/* \include
 * navteq.hpp
 *
 *  Created on: 12.06.2015
 *      Author: philip
 *
 * Convert Shapefiles into OSM files.
 */

#ifndef NAVTEQ_HPP_
#define NAVTEQ_HPP_

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <unordered_map>

#include <gdal/ogrsf_frmts.h>
#include <geos/geom/Geometry.h>

#include <shapefil.h>

#include <osmium/osm/types.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/builder/osm_object_builder.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>

#include "comm2osm_exceptions.hpp"
#include "navteq2osm_tag_parser.hpp"
#include "readers.hpp"
#include "util.hpp"
#include "navteq_mappings.hpp"
#include "navteq_types.hpp"

#define DEBUG false

static constexpr int buffer_size = 10 * 1000 * 1000;

// maps location of way end nodes to node ids
node_map_type g_way_end_points_map;

z_lvl_nodes_map_type g_z_lvl_nodes_map;

// stores osm objects, grows if needed.
osmium::memory::Buffer g_node_buffer(buffer_size);
osmium::memory::Buffer g_way_buffer(buffer_size);
osmium::memory::Buffer g_rel_buffer(buffer_size);

// id counter for object creation
osmium::unsigned_object_id_type g_osm_id = 1;

// g_link_id_map maps navteq link_ids to a vector of osm_ids (it will mostly map to a single osm_id)
link_id_map_type g_link_id_map;

// Provides access to elements in m_buffer through offsets
osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, size_t> g_way_offset_map;

// data structure to store admin boundary tags
struct mtd_area_dataset {
    osmium::unsigned_object_id_type area_id;
    std::string admin_lvl;
    std::vector<std::pair<std::string, std::string>> lang_code_2_area_name;

    void print() {
        std::cout << "area_id=" << area_id;
        std::cout << ", admin_lvl=" << admin_lvl;
        std::cout << std::endl;
    }
};
// auxiliary map which maps datasets with tags for administrative boundaries
std::map<osmium::unsigned_object_id_type, mtd_area_dataset> g_mtd_area_map;

// current layer
OGRLayer* cur_layer;

// current feature
OGRFeature* cur_feat;

/**
 * \brief Dummy attributes enable josm to read output xml files.
 *
 * \param obj OSMObject to set attributess to.
 * */
void set_dummy_osm_object_attributes(osmium::OSMObject& obj) {
    obj.set_version(VERSION);
    obj.set_changeset(CHANGESET);
    obj.set_uid(USERID);
    obj.set_timestamp(TIMESTAMP);
}

/**
 * \brief adds mutual node of two ways as via role (required)
 *
 * \param osm_ids vector with osm_ids of ways which belong to the turn restriction
 * \param rml_builder relation member list builder of turn restriction
 */

void add_common_node_as_via(std::vector<osmium::unsigned_object_id_type>* osm_ids,
    osmium::builder::RelationMemberListBuilder& rml_builder) {
    assert(osm_ids->size() == 2);

    const osmium::Way &from_way = g_way_buffer.get<const osmium::Way>(g_way_offset_map.get(osm_ids->at(0)));
    auto from_way_front = from_way.nodes().front().location();
    auto from_way_back = from_way.nodes().back().location();

    const osmium::Way &to_way = g_way_buffer.get<const osmium::Way>(g_way_offset_map.get(osm_ids->at(1)));
    auto to_way_front = to_way.nodes().front().location();
    auto to_way_back = to_way.nodes().back().location();

    if (from_way_front == to_way_front) {
        if (g_way_end_points_map.find(from_way_front) == g_way_end_points_map.end()) {
            std::cerr << "Skipping via node: " << from_way_front << " is not in g_way_end_points_map." << std::endl;
            return;
        }
        rml_builder.add_member(osmium::item_type::node, g_way_end_points_map.at(from_way_front), "via");
    } else if (from_way_front == to_way_back) {
        if (g_way_end_points_map.find(from_way_front) == g_way_end_points_map.end())  {
            std::cerr << "Skipping via node: " << from_way_front << " is not in g_way_end_points_map." << std::endl;
            return;
        }
        rml_builder.add_member(osmium::item_type::node, g_way_end_points_map.at(from_way_front), "via");
    } else if (from_way_back == to_way_front) {
        if (g_way_end_points_map.find(from_way_back) == g_way_end_points_map.end())  {
            std::cerr << "Skipping via node: " << from_way_back << " is not in g_way_end_points_map." << std::endl;
            return;
        }
        rml_builder.add_member(osmium::item_type::node, g_way_end_points_map.at(from_way_back), "via");
    } else {
        if (g_way_end_points_map.find(from_way_back) == g_way_end_points_map.end())  {
            std::cerr << "Skipping via node: " << from_way_back << " is not in g_way_end_points_map." << std::endl;
            return;
        }
        rml_builder.add_member(osmium::item_type::node, g_way_end_points_map.at(from_way_back), "via");
        assert(from_way_back == to_way_back);
    }
}

/**
 * \brief writes turn restrictions as Relation to m_buffer.
 * 			required roles: "from" "via" and "to" (multiple "via"s are allowed)
 * 			the order of *links is important to assign the correct role.
 *
 * \param osm_ids vector with osm_ids of ways which belong to the turn restriction
 * \return Last number of committed bytes to m_buffer before this commit.
 */
size_t write_turn_restriction(std::vector<osmium::unsigned_object_id_type> *osm_ids) {

    osmium::builder::RelationBuilder builder(g_rel_buffer);
    STATIC_RELATION(builder.object()).set_id(std::to_string(g_osm_id++).c_str());
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
    builder.add_user(USER);

    {
        osmium::builder::RelationMemberListBuilder rml_builder(g_rel_buffer, &builder);

        assert(osm_ids->size() >= 2);

        rml_builder.add_member(osmium::item_type::way, osm_ids->at(0), "from");
        for (int i = 1; i < osm_ids->size() - 1; i++)
            rml_builder.add_member(osmium::item_type::way, osm_ids->at(i), "via");
        if (osm_ids->size() == 2) add_common_node_as_via(osm_ids, rml_builder);
        rml_builder.add_member(osmium::item_type::way, osm_ids->at(osm_ids->size() - 1), "to");

        osmium::builder::TagListBuilder tl_builder(g_rel_buffer, &builder);
        // todo get the correct direction of the turn restriction
        tl_builder.add_tag("restriction", "no_straight_on");
        tl_builder.add_tag("type", "restriction");
    }
    return g_rel_buffer.commit();
}

/**
 * \brief parses and writes tags from features with builder
 * \param builder builder to add tags to.
 * \param z_level Sets z_level if valid (-5 is invalid default)
 * \return link id of parsed feature.
 */

uint64_t build_tag_list(osmium::builder::Builder* builder, osmium::memory::Buffer& buf, short z_level = -5) {
    osmium::builder::TagListBuilder tl_builder(buf, builder);

    uint64_t link_id = parse_street_tags(&tl_builder, cur_feat, &g_cdms_map, &g_cnd_mod_map);

    if (z_level != -5) tl_builder.add_tag("layer", std::to_string(z_level).c_str());
    if (link_id == 0) throw(format_error("layers column field '" + std::string(LINK_ID) + "' is missing"));
    return link_id;
}

/**
 * \brief creates Node with given builder.
 * \param location Location of Node being created.
 * \param builder NodeBuilder to create Node.
 * \return id of created Node.
 * */
osmium::unsigned_object_id_type build_node(osmium::Location location, osmium::builder::NodeBuilder* builder) {
    assert(builder != nullptr);
    STATIC_NODE(builder->object()).set_id(g_osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder->object()));
    builder->add_user(USER);
    STATIC_NODE(builder->object()).set_location(location);
    return STATIC_NODE(builder->object()).id();
}

/**
 * \brief creates Node and writes it to m_buffer.
 * \param location Location of Node being created.
 * \return id of created Node.
 * */
osmium::unsigned_object_id_type build_node(osmium::Location location) {
    osmium::builder::NodeBuilder builder(g_node_buffer);
    return build_node(location, &builder);
}

/**
 * \brief gets id from Node with location. creates Node if missing.
 * \param location Location of Node being created.
 * \return id of found or created Node.
 */
osmium::unsigned_object_id_type get_node(osmium::Location location) {
    auto it = g_way_end_points_map.find(location);
    if (it != g_way_end_points_map.end()) return it->second;
    return build_node(location);
}

/**
 * \brief adds WayNode to Way.
 * \param location Location of WayNode
 * \param wnl_builder Builder to create WayNode
 * \param node_ref_map holds Node ids suitable to given Location.
 */
void add_way_node(const osmium::Location& location, osmium::builder::WayNodeListBuilder& wnl_builder,
        node_map_type* node_ref_map) {
    assert(node_ref_map);
    wnl_builder.add_node_ref(osmium::NodeRef(node_ref_map->at(location), location));
}

static std::set<short> z_lvl_set = { -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 };
void test__z_lvl_range(short z_lvl) {
    if (z_lvl_set.find(z_lvl) == z_lvl_set.end())
        throw(out_of_range_exception("z_lvl " + std::to_string(z_lvl) + " is not valid"));
}

/**
 * \brief creates way with tags in m_buffer.
 * \param ogr_ls provides geometry (linestring) for the way.
 * \param node_ref_map provides osm_ids of Nodes to a given location.
 * \param is_sub_linestring true if given linestring is a sublinestring.
 * \param z_lvl z-level of way. initially invalid (-5).
 * \return id of created Way.
 */
osmium::unsigned_object_id_type build_way(OGRLineString* ogr_ls, node_map_type *node_ref_map = nullptr,
        bool is_sub_linestring = false, short z_lvl = -5) {
    if (is_sub_linestring) test__z_lvl_range(z_lvl);

    osmium::builder::WayBuilder builder(g_way_buffer);
    STATIC_WAY(builder.object()).set_id(g_osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));

    builder.add_user(USER);
    osmium::builder::WayNodeListBuilder wnl_builder(g_way_buffer, &builder);
    for (int i = 0; i < ogr_ls->getNumPoints(); i++) {
        osmium::Location location(ogr_ls->getX(i), ogr_ls->getY(i));
        bool is_end_point = i == 0 || i == ogr_ls->getNumPoints() - 1;
        std::map < osmium::Location, osmium::unsigned_object_id_type > *map_containing_node;
        if (!is_sub_linestring) {
            if (is_end_point)
                map_containing_node = &g_way_end_points_map;
            else
                map_containing_node = node_ref_map;
        } else {
            if (node_ref_map->find(location) != node_ref_map->end()) {
                map_containing_node = node_ref_map;
            } else {
                // node has to be in node_ref_map or way_end_points_map
                assert(g_way_end_points_map.find(location) != g_way_end_points_map.end());
                map_containing_node = &g_way_end_points_map;
            }
        }
        add_way_node(location, wnl_builder, map_containing_node);
    }
    uint64_t link_id = build_tag_list(&builder, g_way_buffer, z_lvl);
    assert(link_id != 0);
    if (g_link_id_map.find(link_id) == g_link_id_map.end())
        g_link_id_map.insert(std::make_pair(link_id, std::vector<osmium::unsigned_object_id_type>()));
    g_link_id_map.at(link_id).push_back((osmium::unsigned_object_id_type) STATIC_WAY(builder.object()).id());

    return STATIC_WAY(builder.object()).id();
}

/**
 * \brief  creates a sublinestring from ogr_ls from range [start_index, end_index] inclusive
 * \param ogr_ls Linestring from which the sublinestring is taken from.
 * \param start_index Node index in ogr_ls, where sublinestring will begin.
 * \param end_index Node index in ogr_ls, where sublinestring will end.
 * \return sublinestring from ogr_ls [start_index, end_index] inclusive
 */
OGRLineString create_sublinestring_geometry(OGRLineString* ogr_ls, int start_index, int end_index = -1) {
    assert(start_index < end_index || end_index == -1);
    assert(start_index < ogr_ls->getNumPoints());
    OGRLineString ogr_sub_ls;
    ogr_sub_ls.addSubLineString(ogr_ls, start_index, end_index);
    return ogr_sub_ls;
}

/* helpers for split_way_by_z_level */
/**
 * \brief checks if first z_level is more significant than the other.
 * \param superior First z_level.
 * \param than second z_level.
 * \return true if superior is superior to than.
 */
bool is_superior(short superior, short than) {
    if (abs(superior) > abs(than)) return true;
    return false;
}

/**
 * \brief checks if first z_level is more significant than the other or equal.
 * \param superior first z_level.
 * \param than second z_level.
 * \return true if superior is greater or equal than.
 */
bool is_superior_or_equal(short superior, short than) {
    if (abs(superior) >= abs(than)) return true;
    return false;
}

/**
 * \brief splits a OGRLinestring by index.
 * \param start_index index where sub_way begins.
 * \param end_index index where sub_way ends.
 * \param ogr_ls geometry of index.
 * \param node_ref_map provides osm_ids of Nodes to a given location.
 * \param z_lvl
 */
void build_sub_way_by_index(ushort start_index, ushort end_index, OGRLineString* ogr_ls, node_map_type* node_ref_map,
        short z_lvl = 0) {
    OGRLineString ogr_sub_ls = create_sublinestring_geometry(ogr_ls, start_index, end_index);
    osmium::unsigned_object_id_type way_id = build_way(&ogr_sub_ls, node_ref_map, true, z_lvl);
    g_way_offset_map.set(way_id, g_way_buffer.commit());
}

/**
 * \brief creates a way for each z_level change
 * \param start_index starting index of next way creation
 * \param end_index index where the last z_level is not 0 (not default)
 * \param last_index index of the last node in given way
 * \param link_id for debug only
 * \param node_z_level_vector holds [index, z_level] pairs to process
 * \param ogr_ls given way which has to be splitted
 * \param node_ref_map location to osm_id mapping (to preserve uniqueness of node locations)
 * \return start_index
 */
ushort create_continuing_sub_ways(ushort first_index, ushort start_index, ushort last_index, uint link_id,
        std::vector<std::pair<ushort, short> >* node_z_level_vector, OGRLineString* ogr_ls, node_map_type* node_ref_map) {
    for (auto it = node_z_level_vector->cbegin(); it != node_z_level_vector->cend(); ++it) {
        short z_lvl = it->second;
        test__z_lvl_range(z_lvl);
        bool last_element = node_z_level_vector->cend() - 1 == it;
        bool not_last_element = !last_element;
        ushort index = it->first;
        ushort next_index;
        short next_z_lvl;
        if (not_last_element) {
            auto next_it = it + 1;
            next_index = next_it->first;
            next_z_lvl = next_it->second;
            test__z_lvl_range(next_z_lvl);

        }
        if (DEBUG)
            std::cout << "first_index=" << first_index << "   " << "start_index=" << start_index << "   "
                    << "last_index=" << last_index << "   " << "index=" << index << "   " << "z_lvl=" << z_lvl << "   "
                    << "next_z_lvl=" << next_z_lvl << std::endl;

        if (not_last_element) {
            if (index + 2 == next_index && z_lvl == next_z_lvl) continue;
            bool not_second_last_element = it + 2 != node_z_level_vector->cend();
            if (not_second_last_element) {
                ushort second_next_index = (it + 2)->first;
                short second_next_z_lvl = (it + 2)->second;
                test__z_lvl_range(second_next_z_lvl);
                if (index + 2 == second_next_index && is_superior_or_equal(second_next_z_lvl, next_z_lvl)
                        && z_lvl == second_next_z_lvl) {
                    ++it;
                    continue;
                }
            }
        }

        // checks for gaps within the way
        if (last_element || index + 1 < next_index || z_lvl != next_z_lvl) {
            ushort from = start_index;
            ushort to;
            if (last_element || index + 1 < next_index || is_superior(z_lvl, next_z_lvl))
                to = std::min((ushort)(index + 1), last_index);
            else
                to = index;
            if (DEBUG)
                std::cout << " 2 ## " << link_id << " ## " << from << "/" << last_index << "  -  " << to << "/"
                        << last_index << ": \tz_lvl=" << z_lvl << std::endl;
            if (from < to) {
                build_sub_way_by_index(from, to, ogr_ls, node_ref_map, z_lvl);
                start_index = to;
            }

            if (not_last_element && to < next_index - 1) {
                build_sub_way_by_index(to, next_index - 1, ogr_ls, node_ref_map);
                if (DEBUG)
                    std::cout << " 3 ## " << link_id << " ## " << to << "/" << last_index << "  -  " << next_index - 1
                            << "/" << last_index << ": \tz_lvl=" << 0 << std::endl;
                start_index = next_index - 1;
            }
        }
    }
    return start_index;
}

/**
 * \brief splits a given linestring into sub_ways to be able to apply z_levels.
 * \param ogr_ls Linestring to be splitted.
 * \param node_z_level_vector holds pairs of Node indices in linestring and their z_level.
 * 							  ommited indices imply default value of 0.
 * \param node_ref_map provides osm_ids of Nodes to a given location.
 * \param link_id link_id of processed feature - for debug only.
 */
void split_way_by_z_level(OGRLineString* ogr_ls, std::vector<std::pair<ushort, short>> *node_z_level_vector,
        node_map_type *node_ref_map, uint link_id) {

    ushort first_index = 0, last_index = ogr_ls->getNumPoints() - 1;
    ushort start_index = node_z_level_vector->cbegin()->first;
    if (start_index > 0) start_index--;

    // first_index <= start_index < end_index <= last_index
    assert(first_index <= start_index);
    assert(start_index < last_index);

//	if (DEBUG) print_z_level_map(link_id, true);

    if (first_index != start_index) {
        build_sub_way_by_index(first_index, start_index, ogr_ls, node_ref_map);
        if (DEBUG)
            std::cout << " 1 ## " << link_id << " ## " << first_index << "/" << last_index << "  -  " << start_index
                    << "/" << last_index << ": \tz_lvl=" << 0 << std::endl;
    }

    start_index = create_continuing_sub_ways(first_index, start_index, last_index, link_id, node_z_level_vector, ogr_ls,
            node_ref_map);

    if (start_index < last_index) {
        build_sub_way_by_index(start_index, last_index, ogr_ls, node_ref_map);
        if (DEBUG)
            std::cout << " 4 ## " << link_id << " ## " << start_index << "/" << last_index << "  -  " << last_index
                    << "/" << last_index << ": \tz_lvl=" << 0 << std::endl;
    }
}

/****************************************************
 * processing input: converting from navteq to osm
 * 					 and writing it to osmium
 ****************************************************/

/**
 * \brief determines osm_id for end_point. If it doesn't exist it will be created.
 */

void process_end_point(bool first, ushort index, z_lvl_type z_lvl, OGRLineString *ogr_ls, z_lvl_map *z_level_map,
        std::map<osmium::Location, osmium::unsigned_object_id_type> & node_ref_map) {
    ushort i = first ? 0 : ogr_ls->getNumPoints() - 1;
    osmium::Location location(ogr_ls->getX(i), ogr_ls->getY(i));

    if (z_lvl != 0) {
        node_id_type node_id = std::make_pair(location, z_lvl);
        auto it = g_z_lvl_nodes_map.find(node_id);
        if (it != g_z_lvl_nodes_map.end()) {
            osmium::unsigned_object_id_type osm_id = it->second;
            node_ref_map.insert(std::make_pair(location, osm_id));
        } else {
            osmium::unsigned_object_id_type osm_id = build_node(location);
            node_ref_map.insert(std::make_pair(location, osm_id));
            g_z_lvl_nodes_map.insert(std::make_pair(node_id, osm_id));
        }
    } else if (g_way_end_points_map.find(location) == g_way_end_points_map.end()) {
        // adds all zero z-level end points to g_way_end_points_map
        g_way_end_points_map.insert(std::make_pair(location, build_node(location)));
    }
}

void process_first_end_point(ushort index, z_lvl_type z_lvl, OGRLineString *ogr_ls, z_lvl_map *z_level_map,
        std::map<osmium::Location, osmium::unsigned_object_id_type> & node_ref_map) {
    process_end_point(true, index, z_lvl, ogr_ls, z_level_map, node_ref_map);
}

void process_last_end_point(ushort index, z_lvl_type z_lvl, OGRLineString *ogr_ls, z_lvl_map *z_level_map,
        std::map<osmium::Location, osmium::unsigned_object_id_type> & node_ref_map) {
    process_end_point(false, index, z_lvl, ogr_ls, z_level_map, node_ref_map);
}

void middle_points_preparation(OGRLineString* ogr_ls,
        std::map<osmium::Location, osmium::unsigned_object_id_type>& node_ref_map) {
    // creates remaining nodes required for way
    for (int i = 1; i < ogr_ls->getNumPoints() - 1; i++) {
        osmium::Location location(ogr_ls->getX(i), ogr_ls->getY(i));
        node_ref_map.insert(std::make_pair(location, build_node(location)));
    }
    g_node_buffer.commit();
}

/**
 * \brief creates Way from linestring.
 * 		  creates missing Nodes needed for Way and Way itself.
 * \param ogr_ls linestring which provides the geometry.
 * \param z_level_map holds z_levels to Nodes of Ways.
 */
void process_way(OGRLineString *ogr_ls, z_lvl_map *z_level_map) {
    std::map<osmium::Location, osmium::unsigned_object_id_type> node_ref_map;

    // creates remaining nodes required for way
    middle_points_preparation(ogr_ls, node_ref_map);
    if (ogr_ls->getNumPoints() > 2) assert(node_ref_map.size() > 0);

    uint64_t link_id = get_uint_from_feature(cur_feat, LINK_ID);

    auto it = z_level_map->find(link_id);
    if (it == z_level_map->end()) {
        osmium::unsigned_object_id_type way_id = build_way(ogr_ls, &node_ref_map);
        g_way_offset_map.set(way_id, g_way_buffer.commit());
    } else {
        // way with different z_levels
        auto first_point_with_different_z_lvl = it->second.at(0);
        auto first_index = 0;
        z_lvl_type first_z_lvl;
        if (first_point_with_different_z_lvl.first == first_index) first_z_lvl = first_point_with_different_z_lvl.second;
        else first_z_lvl = 0;
        process_first_end_point(first_index, first_z_lvl, ogr_ls, z_level_map, node_ref_map);

        auto last_point_with_different_z_lvl = it->second.at(it->second.size() - 1);
        auto last_index = ogr_ls->getNumPoints() - 1;
        z_lvl_type last_z_lvl;
        if (last_point_with_different_z_lvl.first == last_index) last_z_lvl = last_point_with_different_z_lvl.second;
        else last_z_lvl = 0;
        process_last_end_point(last_index, last_z_lvl, ogr_ls, z_level_map, node_ref_map);

        g_way_buffer.commit();

        split_way_by_z_level(ogr_ls, &it->second, &node_ref_map, link_id);
    }
}

// \brief writes way end node to way_end_points_map.
void process_way_end_node(osmium::Location location) {
    init_map_at_element(&g_way_end_points_map, location, get_node(location));
}

// \brief gets end nodes of linestring and processes them.
void process_way_end_nodes(OGRLineString *ogr_ls) {
    process_way_end_node(osmium::Location(ogr_ls->getX(0), ogr_ls->getY(0)));
    process_way_end_node(
            osmium::Location(ogr_ls->getX(ogr_ls->getNumPoints() - 1), ogr_ls->getY(ogr_ls->getNumPoints() - 1)));
}

// \brief stores z_levels in z_level_map for later use
z_lvl_map process_z_levels(DBFHandle handle) {

    z_lvl_map z_level_map;

    uint64_t last_link_id;
    std::vector<std::pair<ushort, z_lvl_type>> v;

    for (int i = 0; i < DBFGetRecordCount(handle); i++) {
        uint64_t link_id = dbf_get_uint_by_field(handle, i, LINK_ID);
        ushort point_num = dbf_get_uint_by_field(handle, i, POINT_NUM) - 1;
        assert(point_num >= 0);
        short z_level = dbf_get_uint_by_field(handle, i, Z_LEVEL);

        if (i > 0 && last_link_id != link_id && v.size() > 0) {
            z_level_map.insert(std::make_pair(last_link_id, v));
            v = std::vector<std::pair<ushort, z_lvl_type>>();
        }
        if (z_level != 0) v.push_back(std::make_pair(point_num, z_level));
        last_link_id = link_id;
    }
    if (v.size() > 0) z_level_map.insert(std::make_pair(last_link_id, v));
    return z_level_map;
}

/**
 * \brief creates nodes for administrative boundary
 * \return osm_ids of created nodes
 */
node_vector_type create_admin_boundary_way_nodes(OGRLinearRing* ring) {
    node_vector_type osm_way_node_ids;
    node_map_type admin_nodes;
    for (int i = 0; i < ring->getNumPoints() - 1; i++) {
        osmium::Location location(ring->getX(i), ring->getY(i));
        auto osm_id = build_node(location);
        osm_way_node_ids.push_back(loc_osmid_pair_type(location, osm_id));
        admin_nodes.insert(std::make_pair(location, osm_id));
    }
    osmium::Location location(ring->getX(ring->getNumPoints() - 1), ring->getY(ring->getNumPoints() - 1));
    osm_way_node_ids.push_back(loc_osmid_pair_type(location, admin_nodes.at(location)));
    return osm_way_node_ids;
}

/**
 * \brief creates ways for administrative boundary
 * \return osm_ids of created ways
 */
osm_id_vector_type create_admin_boundary_ways(OGRLinearRing* ring) {
    node_vector_type osm_way_node_ids = create_admin_boundary_way_nodes(ring);

    osm_id_vector_type osm_way_ids;
    int i = 0;
    do {
        osmium::builder::WayBuilder builder(g_way_buffer);
        STATIC_WAY(builder.object()).set_id(g_osm_id++);
        set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
        builder.add_user(USER);
        osmium::builder::WayNodeListBuilder wnl_builder(g_way_buffer, &builder);
        for (int j = i; j < std::min(i + OSM_MAX_WAY_NODES, (int) osm_way_node_ids.size()); j++)
            wnl_builder.add_node_ref(osm_way_node_ids.at(j).second, osm_way_node_ids.at(j).first);
        osm_way_ids.push_back(STATIC_WAY(builder.object()).id());
        i += OSM_MAX_WAY_NODES - 1;
    } while (i < osm_way_node_ids.size());
    return osm_way_ids;
}

/**
 * \brief adds navteq administrative boundary tags to Relation
 */
void create_admin_boundary_taglist(osmium::builder::RelationBuilder& builder) {
    // Mind tl_builder scope!
    osmium::builder::TagListBuilder tl_builder(g_rel_buffer, &builder);
    tl_builder.add_tag("type", "multipolygon");
    tl_builder.add_tag("boundary", "administrative");
    for (int i = 0; i < cur_layer->GetLayerDefn()->GetFieldCount(); i++) {
        OGRFieldDefn* po_field_defn = cur_layer->GetLayerDefn()->GetFieldDefn(i);
        const char* field_name = po_field_defn->GetNameRef();
        const char* field_value = cur_feat->GetFieldAsString(i);
        // admin boundary mapping: see NAVSTREETS Street Data Reference Manual: p.947)
        if (!strcmp(field_name, "AREA_ID")) {
            osmium::unsigned_object_id_type area_id = std::stoi(field_value);
            if (g_mtd_area_map.find(area_id) != g_mtd_area_map.end()) {
                auto d = g_mtd_area_map.at(area_id);
                if (!d.admin_lvl.empty()) tl_builder.add_tag("navteq_admin_level", d.admin_lvl);

                if (!d.admin_lvl.empty())
                    tl_builder.add_tag("admin_level", navteq_2_osm_admin_lvl(d.admin_lvl).c_str());

                for (auto it : d.lang_code_2_area_name)
                    tl_builder.add_tag(std::string("name:" + parse_lang_code(it.first)), it.second);
                //                    if (!d.area_name_tr.empty()) tl_builder.add_tag("int_name", d.area_name_tr);
            } else {
                std::cerr << "skipping unknown navteq_admin_level" << std::endl;
            }
        }
    }
}

void create_relation_members(const osm_id_vector_type& relation_member_osm_ids,
        osmium::builder::RelationBuilder& builder, osmium::item_type osm_type, const char* role) {
    osmium::builder::RelationMemberListBuilder rml_builder(g_rel_buffer, &builder);
    for (osmium::unsigned_object_id_type osm_id : relation_member_osm_ids)
        rml_builder.add_member(osm_type, osm_id, role);
}

osmium::unsigned_object_id_type create_admin_boundary_relation_with_tags(osm_id_vector_type osm_way_ids) {
    osmium::builder::RelationBuilder builder(g_rel_buffer);
    STATIC_RELATION(builder.object()).set_id(g_osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
    builder.add_user(USER);
    create_admin_boundary_taglist(builder);
    create_relation_members(osm_way_ids, builder, osmium::item_type::way, "outer");
    return STATIC_RELATION(builder.object()).id();
}

/**
 * \brief handles administrative boundary multipolygons
 */
void process_admin_boundary_multipolygon() {
    osm_id_vector_type relation_member_ids;

    OGRMultiPolygon* mp = static_cast<OGRMultiPolygon*>(cur_feat->GetGeometryRef());
    for (int i = 0; i < mp->getNumGeometries(); i++){
        OGRLinearRing* ring = static_cast<OGRPolygon*>(mp->getGeometryRef(i))->getExteriorRing();
        osm_id_vector_type osm_way_ids = create_admin_boundary_ways(ring);

        std::move(osm_way_ids.begin(), osm_way_ids.end(), std::back_inserter(relation_member_ids));
    }
    create_admin_boundary_relation_with_tags(relation_member_ids);
}

/**
 * \brief adds administrative boundaries as Relations to m_buffer
 */
void process_admin_boundaries() {
    auto geom_type = cur_feat->GetGeometryRef()->getGeometryType();

    if (geom_type == wkbMultiPolygon) {
        process_admin_boundary_multipolygon();
    } else if (geom_type == wkbPolygon) {
        OGRPolygon* poly = static_cast<OGRPolygon*>(cur_feat->GetGeometryRef());
        OGRLinearRing* ring = poly->getExteriorRing();
        osm_id_vector_type ways = create_admin_boundary_ways(ring);
        create_admin_boundary_relation_with_tags(ways);
    } else {
            throw(std::runtime_error(
                    "Adminboundaries with geometry=" + std::string(cur_feat->GetGeometryRef()->getGeometryName())
                            + " are not yet supported."));
    }

    g_node_buffer.commit();
    g_way_buffer.commit();
    g_rel_buffer.commit();
}

/**
 * \brief adds tags from administrative boundaries to mtd_area_map.
 * 		  adds tags from administrative boundaries to mtd_area_map
 * 		  to be accesible when creating the Relations of
 * 		  administrative boundaries.
 * \param handle file handle to navteq administrative meta data.
 */
void process_meta_areas(DBFHandle handle) {

    uint64_t last_link_id;
    for (int i = 0; i < DBFGetRecordCount(handle); i++) {

        osmium::unsigned_object_id_type area_id = dbf_get_uint_by_field(handle, i, AREA_ID);

        mtd_area_dataset data;
        if (g_mtd_area_map.find(area_id) != g_mtd_area_map.end()) data = g_mtd_area_map.at(area_id);

        data.area_id = area_id;

        std::string admin_lvl = std::to_string(dbf_get_uint_by_field(handle, i, ADMIN_LVL));

        if (data.admin_lvl.empty())
            data.admin_lvl = admin_lvl;
        else if (data.admin_lvl != admin_lvl)
            std::cerr << "entry with area_id=" << area_id << " has multiple admin_lvls:" << data.admin_lvl << ", "
                    << admin_lvl << std::endl;

        std::string lang_code = dbf_get_string_by_field(handle, i, LANG_CODE);
        std::string area_name = dbf_get_string_by_field(handle, i, AREA_NAME);
        data.lang_code_2_area_name.push_back(std::make_pair(lang_code, to_camel_case_with_spaces(area_name)));

        g_mtd_area_map.insert(std::make_pair(area_id, data));
//		data.print();
    }
    DBFClose(handle);
}

std::vector<uint64_t> collect_via_manoeuvre_link_ids(uint64_t link_id, DBFHandle rdms_handle,
        osmium::unsigned_object_id_type cond_id, int& i) {
    std::vector<uint64_t> via_manoeuvre_link_id;
    via_manoeuvre_link_id.push_back(link_id);
    for (int j = 0;; j++) {
        if (i + j == DBFGetRecordCount(rdms_handle)) {
            i += j - 1;
            break;
        }
        osmium::unsigned_object_id_type next_cond_id = dbf_get_uint_by_field(rdms_handle, i + j, COND_ID);
        if (cond_id != next_cond_id) {
            i += j - 1;
            break;
        }
        via_manoeuvre_link_id.push_back(dbf_get_uint_by_field(rdms_handle, i + j, MAN_LINKID));
    }
    return via_manoeuvre_link_id;
}

std::vector<osmium::unsigned_object_id_type> collect_via_manoeuvre_osm_ids(
        const std::vector<uint64_t>& via_manoeuvre_link_id) {
    std::vector<osmium::unsigned_object_id_type> via_manoeuvre_osm_id;

    osmium::Location end_point_front;
    osmium::Location end_point_back;
    osmium::Location curr;
    uint ctr = 0;

    for (auto it : via_manoeuvre_link_id) {
        bool reverse = false;

        if (g_link_id_map.find(it) == g_link_id_map.end()) return std::vector<osmium::unsigned_object_id_type>();

        std::vector<long unsigned int> &osm_id_vector = g_link_id_map.at(it);
        auto first_osm_id = osm_id_vector.at(0);
        const auto &first_way = g_way_buffer.get<const osmium::Way>(g_way_offset_map.get(first_osm_id));
        osmium::Location first_way_front = first_way.nodes().front().location();
        auto last_osm_id = osm_id_vector.at(osm_id_vector.size()-1);
        const auto &last_way = g_way_buffer.get<const osmium::Way>(g_way_offset_map.get(last_osm_id));
        osmium::Location last_way_back = last_way.nodes().back().location();

        // determine end_points
        if (ctr == 0){
            // assume this is correct
            end_point_front = first_way_front;
            end_point_back  = last_way_back;
        } else {
            if (ctr == 1) {
                // checks wether assumption is valid and corrects it if necessary
                if (end_point_front == first_way_front || end_point_front == last_way_back) {
                    // reverse via_manoeuvre_osm_id
                    std::reverse(via_manoeuvre_osm_id.begin(), via_manoeuvre_osm_id.end());
                    // switch end_points
                    auto tmp = end_point_front;
                    end_point_front = end_point_back;
                    end_point_back = tmp;
                }
            }
            // detemine new end_point
            if (end_point_back == last_way_back)
                end_point_back = first_way_front;
            else if (end_point_back == first_way_front)
                end_point_back = last_way_back;
            else assert(false);
        }

        // check wether we have to reverse vector
        if (osm_id_vector.size() > 1) {
            if (end_point_back == first_way_front) reverse = true;
            else
            assert(end_point_back == last_way_back);
        }
        if (reverse)
            via_manoeuvre_osm_id.insert(via_manoeuvre_osm_id.end(), osm_id_vector.rbegin(), osm_id_vector.rend());
        else
            via_manoeuvre_osm_id.insert(via_manoeuvre_osm_id.end(), osm_id_vector.begin(), osm_id_vector.end());

        ctr++;
    } // end link_id loop
    return via_manoeuvre_osm_id;
}

/**
 * \brief reads turn restrictions from DBF file and writes them to osmium.
 * \param handle DBF file handle to navteq manoeuvres.
 * */

void process_turn_restrictions(DBFHandle rdms_handle, DBFHandle cdms_handle) {
    // maps COND_ID to COND_TYPE
    std::map<osmium::unsigned_object_id_type, ushort> cdms_map;
    for (int i = 0; i < DBFGetRecordCount(cdms_handle); i++) {
        osmium::unsigned_object_id_type cond_id = dbf_get_uint_by_field(cdms_handle, i, COND_ID);
        ushort cond_type = dbf_get_uint_by_field(cdms_handle, i, COND_TYPE);
        cdms_map.insert(std::make_pair(cond_id, cond_type));
    }

    for (int i = 0; i < DBFGetRecordCount(rdms_handle); i++) {

        uint64_t link_id = dbf_get_uint_by_field(rdms_handle, i, LINK_ID);
        osmium::unsigned_object_id_type cond_id = dbf_get_uint_by_field(rdms_handle, i, COND_ID);

        auto it = cdms_map.find(cond_id);
        if (it != cdms_map.end() && it->second != RESTRICTED_DRIVING_MANOEUVRE) continue;

        std::vector<uint64_t> via_manoeuvre_link_id = collect_via_manoeuvre_link_ids(link_id, rdms_handle, cond_id, i);

        std::vector<osmium::unsigned_object_id_type> via_manoeuvre_osm_id = collect_via_manoeuvre_osm_ids(
                via_manoeuvre_link_id);

        // only process complete turn relations
        if (via_manoeuvre_osm_id.empty()) continue;

//			// get corresponding osm_way to link
//			const Way &way = m_buffer.get<const Way>(offset_map.get(osm_id));
//			assert(osm_id == way.id());
//			std::cout <<"osm_id="<< osm_id << std::endl;
//			std::cout <<"way_id="<< way.id() << std::endl;

// todo find out which direction turn restriction has and apply
// 		for now: always apply 'no_straight_on'
        write_turn_restriction (&via_manoeuvre_osm_id);
    }
}

/****************************************************
 * adds layers to osmium:
 *      cur_layer and cur_feature have to be set
 ****************************************************/
// unused code
//void add_point_layer_to_osmium(OGRLayer *layer, std::string dir = std::string()) {
//    cur_layer = layer;
//    while ((cur_feat = layer->GetNextFeature()) != NULL) {
//        OGRPoint *p = static_cast<OGRPoint*>(cur_feat->GetGeometryRef());
//
//        osmium::builder::NodeBuilder builder(m_buffer);
//        build_node(osmium::Location(p->getX(), p->getY()), &builder);
//        build_tag_list(&builder);
//        delete cur_feat;
//    }
//    m_buffer.commit();
//}

/**
 * \brief adds streets to m_buffer / osmium.
 * \param layer Pointer to administrative layer.
 */

void add_street_shape_to_osmium(OGRLayer *layer, std::string dir = std::string(), std::string sub_dir = std::string()) {
    assert(layer->GetGeomType() == wkbLineString);

    cur_layer = layer;

// Maps link_ids to pairs of indices and z-levels of waypoints with z-levels not equal 0.
    z_lvl_map z_level_map = process_z_levels(read_dbf_file(dir + sub_dir + ZLEVELS_DBF));

    if ( dbf_file_exists(dir + CND_MOD_DBF)){
        DBFHandle cnd_mod_handle = read_dbf_file(dir + CND_MOD_DBF);
        for (int i = 0; i < DBFGetRecordCount(cnd_mod_handle); i++) {
            cond_id_type cond_id = dbf_get_uint_by_field(cnd_mod_handle, i, COND_ID);
            // std::string lang_code = dbf_get_string_by_field(cnd_mod_handle, i, LANG_CODE);
            mod_typ_type mod_type = dbf_get_uint_by_field(cnd_mod_handle, i, CM_MOD_TYPE);
            mod_val_type mod_val = dbf_get_uint_by_field(cnd_mod_handle, i, CM_MOD_VAL);
            g_cnd_mod_map.insert(std::make_pair(cond_id, mod_group_type(mod_type, mod_val)));
        }
        DBFClose(cnd_mod_handle);
    }

    if (dbf_file_exists(dir + CDMS_DBF)){
        DBFHandle cdms_handle = read_dbf_file(dir + CDMS_DBF);
        for (int i = 0; i < DBFGetRecordCount(cdms_handle); i++) {
            uint64_t link_id = dbf_get_uint_by_field(cdms_handle, i, LINK_ID);
            uint64_t cond_id = dbf_get_uint_by_field(cdms_handle, i, COND_ID);
            g_cdms_map.insert(std::make_pair(link_id, cond_id));
        }
        DBFClose(cdms_handle);
    }

// get all nodes which may be a routable crossing
    while ((cur_feat = cur_layer->GetNextFeature()) != NULL){
        uint64_t link_id = get_uint_from_feature(cur_feat, LINK_ID);
        // omit way end nodes with different z-levels (they have to be handled extra)
        if (z_level_map.find(link_id) == z_level_map.end())
            process_way_end_nodes(static_cast<OGRLineString*>(cur_feat->GetGeometryRef()));
        delete cur_feat;
    }
    g_node_buffer.commit();
    g_way_buffer.commit();

    cur_layer->ResetReading();
    while ((cur_feat = cur_layer->GetNextFeature()) != NULL){
        process_way(static_cast<OGRLineString*>(cur_feat->GetGeometryRef()), &z_level_map);
        delete cur_feat;
    }
    for (auto elem : z_level_map)
        elem.second.clear();
    z_level_map.clear();
    delete layer;
}

/**
 * \brief adds administrative boundaries to m_buffer / osmium.
 * \param layer pointer to administrative layer.
 */

void add_admin_shape_to_osmium(OGRLayer *layer, std::string dir = std::string(), std::string sub_dir = std::string()) {
    cur_layer = layer;
    assert(layer->GetGeomType() == wkbPolygon);

    process_meta_areas(read_dbf_file(dir + sub_dir + MTD_AREA_DBF));

//    std::cout << "feature count = " << layer->GetFeatureCount() << std::endl;

    while ((cur_feat = cur_layer->GetNextFeature()) != NULL) {
        process_admin_boundaries();
        delete cur_feat;
    }
    g_mtd_area_map.clear();
    delete layer;
}

/****************************************************
 * cleanup and assertions
 ****************************************************/

/**
 * \brief clears all global variables.
 */

void clear_all() {
    g_node_buffer.clear();
    g_way_buffer.clear();
    g_rel_buffer.clear();
    g_osm_id = 1;
    g_link_id_map.clear();
    g_way_offset_map.clear();
    g_mtd_area_map.clear();
}

void add_buffer_ids(std::vector<osmium::unsigned_object_id_type>& v, osmium::memory::Buffer& buf) {
    for (auto& it : buf) {
        osmium::OSMObject* obj = static_cast<osmium::OSMObject*>(&it);
        v.push_back((osmium::unsigned_object_id_type) (obj->id()));
    }
}

void assert__id_uniqueness() {
    std::vector < osmium::unsigned_object_id_type > v;
    add_buffer_ids(v, g_node_buffer);
    add_buffer_ids(v, g_way_buffer);
    add_buffer_ids(v, g_rel_buffer);

    std::sort(v.begin(), v.end());
    auto ptr = unique(v.begin(), v.end());
    assert(ptr == v.end());
}

void assert__node_location_uniqueness(node_map_type& loc_z_lvl_map, osmium::memory::Buffer& buffer) {
    for (auto& it : buffer) {
        osmium::OSMObject* obj = static_cast<osmium::OSMObject*>(&it);
        if (obj->type() == osmium::item_type::node) {
            osmium::Node* node = static_cast<osmium::Node*>(obj);
            osmium::Location l = node->location();
            //			std::cout << node->id() << " - "<< l << std::endl;
            if (loc_z_lvl_map.find(l) == loc_z_lvl_map.end()) loc_z_lvl_map.insert(
                    std::make_pair(node->location(), node->id()));
            else {
                // todo read Zlevels.DBF and check if the level differs
                std::cout << "testing double node:";
                std::cout << " 1st: " << node->id();
                std::cout << ", 2nd: " << loc_z_lvl_map.at(l) << " with location " << l;
                std::cout << std::endl;
            }
        }
    }
}

void assert__node_locations_uniqueness() {
    node_map_type loc_z_lvl_map;
    assert__node_location_uniqueness(loc_z_lvl_map, g_node_buffer);
    assert__node_location_uniqueness(loc_z_lvl_map, g_way_buffer);
    assert__node_location_uniqueness(loc_z_lvl_map, g_rel_buffer);
}

#endif /* NAVTEQ_HPP_ */
