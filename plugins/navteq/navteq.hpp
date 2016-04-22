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

#include <iostream>

#include <gdal/ogrsf_frmts.h>

#include <osmium/osm/item_type.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/builder/osm_object_builder.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include "comm2osm_exceptions.hpp"
#include "navteq2osm_tag_parser.hpp"
#include "../readers.hpp"
#include "navteq_util.hpp"
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

// g_route_type_map maps navteq link_ids to the lowest occurring route_type value
link_id_route_type_map g_route_type_map;

// g_hwys_ref_map maps navteq link_ids to a vector of highway names
link_id_to_names_map g_hwys_ref_map;

// Provides access to elements in g_way_buffer through offsets
osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, size_t> g_way_offset_map;

// auxiliary map which maps datasets with tags for administrative boundaries
mtd_area_map_type g_mtd_area_map;

// map for conditional modifications
cnd_mod_map_type g_cnd_mod_map;

// map for conditional driving manoeuvres
cdms_map_type g_cdms_map;
std::map<area_id_type, govt_code_type> g_area_to_govt_code_map;
cntry_ref_map_type g_cntry_ref_map;

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

void add_common_node_as_via(const osm_id_vector_type& osm_ids, osmium::builder::RelationMemberListBuilder& rml_builder) {
    assert(osm_ids.size() == 2);

    const osmium::Way &from_way = g_way_buffer.get<const osmium::Way>(g_way_offset_map.get(osm_ids.at(0)));
    auto from_way_front = from_way.nodes().front().location();
    auto from_way_back = from_way.nodes().back().location();

    const osmium::Way &to_way = g_way_buffer.get<const osmium::Way>(g_way_offset_map.get(osm_ids.at(1)));
    auto to_way_front = to_way.nodes().front().location();
    auto to_way_back = to_way.nodes().back().location();

    if (from_way_front == to_way_front) {
        if (g_way_end_points_map.find(from_way_front) == g_way_end_points_map.end()) {
            std::cerr << "Skipping via node: " << from_way_front << " is not in g_way_end_points_map." << std::endl;
            return;
        }
        rml_builder.add_member(osmium::item_type::node, g_way_end_points_map.at(from_way_front), "via");
    } else if (from_way_front == to_way_back) {
        if (g_way_end_points_map.find(from_way_front) == g_way_end_points_map.end()) {
            std::cerr << "Skipping via node: " << from_way_front << " is not in g_way_end_points_map." << std::endl;
            return;
        }
        rml_builder.add_member(osmium::item_type::node, g_way_end_points_map.at(from_way_front), "via");
    } else if (from_way_back == to_way_front) {
        if (g_way_end_points_map.find(from_way_back) == g_way_end_points_map.end()) {
            std::cerr << "Skipping via node: " << from_way_back << " is not in g_way_end_points_map." << std::endl;
            return;
        }
        rml_builder.add_member(osmium::item_type::node, g_way_end_points_map.at(from_way_back), "via");
    } else {
        if (g_way_end_points_map.find(from_way_back) == g_way_end_points_map.end()) {
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
size_t build_turn_restriction(const osm_id_vector_type& osm_ids) {

    osmium::builder::RelationBuilder builder(g_rel_buffer);
    STATIC_RELATION(builder.object()).set_id(std::to_string(g_osm_id++).c_str());
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
    builder.add_user(USER);

    {
        osmium::builder::RelationMemberListBuilder rml_builder(g_rel_buffer, &builder);

        assert(osm_ids.size() >= 2);

        rml_builder.add_member(osmium::item_type::way, osm_ids.at(0), "from");
        for (int i = 1; i < osm_ids.size() - 1; i++)
            rml_builder.add_member(osmium::item_type::way, osm_ids.at(i), "via");
        if (osm_ids.size() == 2) add_common_node_as_via(osm_ids, rml_builder);
        rml_builder.add_member(osmium::item_type::way, osm_ids.at(osm_ids.size() - 1), "to");

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

link_id_type build_tag_list(ogr_feature_uptr& feat, osmium::builder::Builder* builder, osmium::memory::Buffer& buf,
        short z_level = -5) {
    osmium::builder::TagListBuilder tl_builder(buf, builder);

    link_id_type link_id = parse_street_tags(&tl_builder, feat, &g_cdms_map, &g_cnd_mod_map, 
            &g_area_to_govt_code_map, &g_cntry_ref_map, &g_mtd_area_map, &g_route_type_map, &g_hwys_ref_map);

    if (z_level != -5 && z_level != 0) tl_builder.add_tag("layer", std::to_string(z_level).c_str());
    if (link_id == 0) throw(format_error("layers column field '" + std::string(LINK_ID) + "' is missing"));
    return link_id;
}

/**
 * \brief creates Node with given builder.
 * \param location Location of Node being created.
 * \param builder NodeBuilder to create Node.
 * \return id of created Node.
 */
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
 */
osmium::unsigned_object_id_type build_node(osmium::Location location) {
    osmium::builder::NodeBuilder builder(g_node_buffer);
    return build_node(location, &builder);
}

/**
 * \brief creates Node with tags and writes it to m_buffer.
 * \param location Location of Node being created.
 * \param kv_vector holds tags as key value pairs
 * \return id of created Node.
 */
osmium::unsigned_object_id_type build_node_with_tags(osmium::Location location, const std::vector<key_val_pair_type>& kv_vector) {
    osmium::builder::NodeBuilder node_builder(g_node_buffer);
    auto node_id = build_node(location, &node_builder);

    osmium::builder::TagListBuilder tl_builder(g_node_buffer, &node_builder);
    for (auto kv_pair : kv_vector){
		auto tag_key = kv_pair.first;
		auto tag_val = kv_pair.second;
		if (tag_key) {
			tl_builder.add_tag(tag_key, tag_val);
		}
    }
    return node_id;
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
osmium::unsigned_object_id_type build_way(ogr_feature_uptr& feat, ogr_line_string_uptr& ogr_ls, node_map_type *node_ref_map =
        nullptr, bool is_sub_linestring = false, short z_lvl = -5) {

    if (is_sub_linestring) test__z_lvl_range(z_lvl);

    osmium::builder::WayBuilder builder(g_way_buffer);
    STATIC_WAY(builder.object()).set_id(g_osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));

    builder.add_user(USER);
    osmium::builder::WayNodeListBuilder wnl_builder(g_way_buffer, &builder);
    for (int i = 0; i < ogr_ls->getNumPoints(); i++) {
        osmium::Location location(ogr_ls->getX(i), ogr_ls->getY(i));
        bool is_end_point = i == 0 || i == ogr_ls->getNumPoints() - 1;
        std::map<osmium::Location, osmium::unsigned_object_id_type> *map_containing_node;
        if (!is_sub_linestring) {
            if (is_end_point) map_containing_node = &g_way_end_points_map;
            else map_containing_node = node_ref_map;
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

    link_id_type link_id = build_tag_list(feat, &builder, g_way_buffer, z_lvl);
    assert(link_id != 0);
    if (g_link_id_map.find(link_id) == g_link_id_map.end())
        g_link_id_map.insert(std::make_pair(link_id, osm_id_vector_type()));
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
OGRLineString create_sublinestring_geometry(ogr_line_string_uptr& ogr_ls, int start_index, int end_index = -1) {
    assert(start_index < end_index || end_index == -1);
    assert(start_index < ogr_ls->getNumPoints());
    OGRLineString ogr_sub_ls;
    ogr_sub_ls.addSubLineString(ogr_ls.get(), start_index, end_index);
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
void build_sub_way_by_index(ogr_feature_uptr& feat, ogr_line_string_uptr& ogr_ls, ushort start_index, ushort end_index,
        node_map_type* node_ref_map, short z_lvl = 0) {
    ogr_line_string_uptr ogr_sub_ls_uptr(
            new OGRLineString(create_sublinestring_geometry(ogr_ls, start_index, end_index)));
    osmium::unsigned_object_id_type way_id = build_way(feat, ogr_sub_ls_uptr, node_ref_map, true, z_lvl);

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
ushort create_continuing_sub_ways(ogr_feature_uptr& feat, ogr_line_string_uptr& ogr_ls, ushort first_index, ushort start_index, ushort last_index,
        uint link_id, const index_z_lvl_vector_type& node_z_level_vector,
        node_map_type* node_ref_map) {

    for (auto it = node_z_level_vector.cbegin(); it != node_z_level_vector.cend(); ++it) {
        short z_lvl = it->second;
        test__z_lvl_range(z_lvl);
        bool last_element = node_z_level_vector.cend() - 1 == it;
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
            bool not_second_last_element = it + 2 != node_z_level_vector.cend();
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
            if (last_element || index + 1 < next_index || is_superior(z_lvl, next_z_lvl)) to = std::min(
                    (ushort) (index + 1), last_index);
            else to = index;
            if (DEBUG)
                std::cout << " 2 ## " << link_id << " ## " << from << "/" << last_index << "  -  " << to << "/"
                        << last_index << ": \tz_lvl=" << z_lvl << std::endl;
            if (from < to) {
                build_sub_way_by_index(feat, ogr_ls, from, to, node_ref_map, z_lvl);
                start_index = to;
            }

            if (not_last_element && to < next_index - 1) {
                build_sub_way_by_index(feat, ogr_ls, to, next_index - 1, node_ref_map);
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
void split_way_by_z_level(ogr_feature_uptr& feat, ogr_line_string_uptr& ogr_ls,
        const index_z_lvl_vector_type& node_z_level_vector, node_map_type *node_ref_map, uint link_id) {

    ushort first_index = 0, last_index = ogr_ls->getNumPoints() - 1;
    ushort start_index = node_z_level_vector.cbegin()->first;
    if (start_index > 0) start_index--;

    // first_index <= start_index < end_index <= last_index
    assert(first_index <= start_index);
    assert(start_index < last_index);

//	if (DEBUG) print_z_level_map(link_id, true);
    if (first_index != start_index) {
        build_sub_way_by_index(feat, ogr_ls, first_index, start_index, node_ref_map);
        if (DEBUG)
            std::cout << " 1 ## " << link_id << " ## " << first_index << "/" << last_index << "  -  " << start_index
                    << "/" << last_index << ": \tz_lvl=" << 0 << std::endl;
    }

    start_index = create_continuing_sub_ways(feat, ogr_ls, first_index, start_index, last_index, link_id, node_z_level_vector,
            node_ref_map);

    if (start_index < last_index) {
        build_sub_way_by_index(feat, ogr_ls, start_index, last_index, node_ref_map);
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

void process_end_point(bool first, ushort index, z_lvl_type z_lvl, ogr_line_string_uptr& ogr_ls, z_lvl_map *z_level_map,
        node_map_type& node_ref_map) {
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

void process_first_end_point(ushort index, z_lvl_type z_lvl, ogr_line_string_uptr& ogr_ls, z_lvl_map *z_level_map,
        node_map_type & node_ref_map) {
    process_end_point(true, index, z_lvl, ogr_ls, z_level_map, node_ref_map);
}

void process_last_end_point(ushort index, z_lvl_type z_lvl, ogr_line_string_uptr& ogr_ls, z_lvl_map *z_level_map,
        node_map_type & node_ref_map) {
    process_end_point(false, index, z_lvl, ogr_ls, z_level_map, node_ref_map);
}

void middle_points_preparation(ogr_line_string_uptr& ogr_ls, node_map_type& node_ref_map) {
    // creates remaining nodes required for way
    for (int i = 1; i < ogr_ls->getNumPoints() - 1; i++) {
        osmium::Location location(ogr_ls->getX(i), ogr_ls->getY(i));
        node_ref_map.insert(std::make_pair(location, build_node(location)));
    }
    g_node_buffer.commit();
}

/**
 * \brief replaces all z-levels by zero, which are not an endpoint
 * \param z_lvl_vec vector containing pairs of [z_lvl_index, z_lvl]
 */
void set_ferry_z_lvls_to_zero(ogr_feature_uptr& feat, index_z_lvl_vector_type& z_lvl_vec) {
    // erase middle z_lvls
    if (z_lvl_vec.size() > 2) z_lvl_vec.erase(z_lvl_vec.begin() + 1, z_lvl_vec.end() - 1);
    // erase first z_lvl if first index references first node
    if (z_lvl_vec.size() > 0 && z_lvl_vec.begin()->first != 0) z_lvl_vec.erase(z_lvl_vec.begin());
    // erase last z_lvl if last index references last node
    OGRLineString* ogr_ls = static_cast<OGRLineString*>(feat->GetGeometryRef());
    if (z_lvl_vec.size() > 0 && (z_lvl_vec.end() - 1)->first != ogr_ls->getNumPoints() - 1)
        z_lvl_vec.erase(z_lvl_vec.end());
}

/**
 * \brief creates interpolated house numbers alongside of a given linestring if feature holds suitable tags
 * \param feat feature which holds the tags
 * \param ogr_ls linestring which receives the interpolated house numbers
 * \param left specifies on which side of the linestring the house numbers will be applied
 */
void create_house_numbers(ogr_feature_uptr& feat, ogr_line_string_uptr& ogr_ls, bool left) {
    const char* ref_addr = left ? L_REFADDR : R_REFADDR;
    const char* nref_addr = left ? L_NREFADDR : R_NREFADDR;
    const char* addr_schema = left ? L_ADDRSCH : R_ADDRSCH;

    if (!strcmp(get_field_from_feature(feat, ref_addr), "")) return;
    if (!strcmp(get_field_from_feature(feat, nref_addr), "")) return;
    if (!strcmp(get_field_from_feature(feat, addr_schema), "")) return;
    if (!strcmp(get_field_from_feature(feat, addr_schema), "M")) return;

	ogr_line_string_uptr offset_ogr_ls(create_offset_curve(ogr_ls.get(), HOUSENUMBER_CURVE_OFFSET, left));
    assert(ogr_ls);
    osmium::builder::WayBuilder way_builder(g_way_buffer);
    STATIC_WAY(way_builder.object()).set_id(g_osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(way_builder.object()));
    way_builder.add_user(USER);
    osmium::builder::WayNodeListBuilder wnl_builder(g_way_buffer, &way_builder);
    for (int i = 0; i < offset_ogr_ls->getNumPoints(); i++) {
        osmium::Location location(offset_ogr_ls->getX(i), offset_ogr_ls->getY(i));
        assert(location.valid());

        std::vector<key_val_pair_type> tags;
        if (i == 0 || i == offset_ogr_ls->getNumPoints() - 1){
			if (i == 0){
				tags.push_back(key_val_pair_type("addr:housenumber", get_field_from_feature(feat, left ? ref_addr : nref_addr)));
			} else if (i == offset_ogr_ls->getNumPoints() - 1) {
				tags.push_back(key_val_pair_type("addr:housenumber", get_field_from_feature(feat, left ? nref_addr : ref_addr)));
			}
			tags.push_back(key_val_pair_type("addr:street", strdup(to_camel_case_with_spaces(get_field_from_feature(feat, ST_NAME)).c_str())));
        }
        osmium::unsigned_object_id_type node_id = build_node_with_tags(location, tags);
        wnl_builder.add_node_ref(osmium::NodeRef(node_id, location));
    }
    {
        osmium::builder::TagListBuilder tl_builder(g_way_buffer, &way_builder);
        const char* schema = parse_house_number_schema(get_field_from_feature(feat, addr_schema));
        tl_builder.add_tag("addr:interpolation", schema);
    }
    g_node_buffer.commit();
    g_way_buffer.commit();
}

void create_house_numbers(ogr_feature_uptr& feat, ogr_line_string_uptr& ogr_ls) {
    create_house_numbers(feat, ogr_ls, true);
    create_house_numbers(feat, ogr_ls, false);
}

/**
 * \brief creates Way from linestring.
 * 		  creates missing Nodes needed for Way and Way itself.
 * \param ogr_ls linestring which provides the geometry.
 * \param z_level_map holds z_levels to Nodes of Ways.
 */
void process_way(ogr_feature_uptr&& feat, z_lvl_map *z_level_map) {

    node_map_type node_ref_map;

    // caution! ogr_ls refers to a geometry which is part of feat => you mustn't cleanup
    ogr_line_string_uptr ogr_ls(static_cast<OGRLineString*>(feat->GetGeometryRef()));

    // creates remaining nodes required for way
    middle_points_preparation(ogr_ls, node_ref_map);
    if (ogr_ls->getNumPoints() > 2) assert(node_ref_map.size() > 0);

    link_id_type link_id = get_uint_from_feature(feat, LINK_ID);

    auto it = z_level_map->find(link_id);
    if (it == z_level_map->end()) {
        osmium::unsigned_object_id_type way_id = build_way(feat, ogr_ls, &node_ref_map);
        g_way_offset_map.set(way_id, g_way_buffer.commit());
    } else {
        index_z_lvl_vector_type& index_z_lvl_vector = it->second;

        // way with different z_levels
        auto first_point_with_different_z_lvl = index_z_lvl_vector.at(0);
        auto first_index = 0;
        z_lvl_type first_z_lvl;
        if (first_point_with_different_z_lvl.first == first_index) first_z_lvl =
                first_point_with_different_z_lvl.second;
        else first_z_lvl = 0;
        process_first_end_point(first_index, first_z_lvl, ogr_ls, z_level_map, node_ref_map);

        auto last_point_with_different_z_lvl = index_z_lvl_vector.at(index_z_lvl_vector.size() - 1);
        auto last_index = ogr_ls->getNumPoints() - 1;
        z_lvl_type last_z_lvl;
        if (last_point_with_different_z_lvl.first == last_index) last_z_lvl = last_point_with_different_z_lvl.second;
        else last_z_lvl = 0;
        process_last_end_point(last_index, last_z_lvl, ogr_ls, z_level_map, node_ref_map);

        g_way_buffer.commit();

        bool ferry = is_ferry(get_field_from_feature(feat, FERRY));
        if (ferry) set_ferry_z_lvls_to_zero(feat, index_z_lvl_vector);

        split_way_by_z_level(feat, ogr_ls, index_z_lvl_vector, &node_ref_map, link_id);
    }

    if (!strcmp(get_field_from_feature(feat, ADDR_TYPE), "B")) {
        create_house_numbers(feat, ogr_ls);
    }
    // ogr_ls will be cleaned alongside with feat
    ogr_ls.release();
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

/**
 * \brief creates nodes for closed ways (i.e. for administrative boundary)
 * \return osm_ids of created nodes
 */
node_vector_type create_closed_way_nodes(OGRLinearRing* ring) {
    node_vector_type osm_way_node_ids;
    loc_osmid_pair_type first_node;
    for (int i = 0; i < ring->getNumPoints() - 1; i++) {
        osmium::Location location(ring->getX(i), ring->getY(i));
        auto osm_id = build_node(location);
        osm_way_node_ids.push_back(loc_osmid_pair_type(location, osm_id));
        if (i == 0) first_node = loc_osmid_pair_type(location, osm_id);
    }
    // first and last node are the same in rings, hence add first node_id and skip last node.
    osmium::Location last_location(ring->getX(ring->getNumPoints() - 1), ring->getY(ring->getNumPoints() - 1));
    if (last_location != first_node.first)
        throw format_error("admin boundary ring is invalid. First and last node don't match");
    osm_way_node_ids.push_back(first_node);
    return osm_way_node_ids;
}

/**
 * \brief creates nodes for open ways
 * \return osm_ids of created nodes
 */
node_vector_type create_open_way_nodes(OGRLineString* line) {
    node_vector_type osm_way_node_ids;
    for (int i = 0; i < line->getNumPoints(); i++) {
        osmium::Location location(line->getX(i), line->getY(i));
        auto osm_id = build_node(location);
        osm_way_node_ids.push_back(loc_osmid_pair_type(location, osm_id));
    }
    return osm_way_node_ids;
}

/**
 * \brief creates closed ways (i.e. for administrative boundary)
 * \return osm_ids of created ways
 */
osm_id_vector_type build_closed_ways(OGRLinearRing* ring) {
    node_vector_type osm_way_node_ids = create_closed_way_nodes(ring);

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
void build_admin_boundary_taglist(osmium::builder::RelationBuilder& builder, ogr_layer_uptr& layer,
        ogr_feature_uptr& feat) {
    // Mind tl_builder scope!
    osmium::builder::TagListBuilder tl_builder(g_rel_buffer, &builder);
    tl_builder.add_tag("type", "multipolygon");
    tl_builder.add_tag("boundary", "administrative");
    for (int i = 0; i < layer->GetLayerDefn()->GetFieldCount(); i++) {
        OGRFieldDefn* po_field_defn = layer->GetLayerDefn()->GetFieldDefn(i);
        const char* field_name = po_field_defn->GetNameRef();
        const char* field_value = feat->GetFieldAsString(i);
        // admin boundary mapping: see NAVSTREETS Street Data Reference Manual: p.947)
        if (!strcmp(field_name, AREA_ID)) {
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
                std::cerr << "Skipping unknown navteq_admin_level" << std::endl;
            }
        } else if (!strcmp(field_name, FEAT_COD)) {
            osmium::unsigned_object_id_type feat_code = std::stoi(field_value);
            // FEAT_TYPE 'SETTLEMENT'
            if (feat_code = 900156)
                tl_builder.add_tag("landuse", "residential");
        }
    }
}

/**
 * \brief adds navteq water tags to Relation
 */
void build_water_poly_taglist(osmium::builder::RelationBuilder& builder, ogr_layer_uptr& layer,
        ogr_feature_uptr& feat) {
    // Mind tl_builder scope!
    osmium::builder::TagListBuilder tl_builder(g_rel_buffer, &builder);
    tl_builder.add_tag("type", "multipolygon");
    tl_builder.add_tag("natural", "water");

    for (int i = 0; i < layer->GetLayerDefn()->GetFieldCount(); i++) {
        OGRFieldDefn* po_field_defn = layer->GetLayerDefn()->GetFieldDefn(i);
        const char* field_name = po_field_defn->GetNameRef();
        const char* field_value = feat->GetFieldAsString(i);
        
        if (!strcmp(field_name, POLYGON_NM)) {
            if(field_value && field_value[0]) {
                std::string waters_name = to_camel_case_with_spaces(field_value);
                if (!waters_name.empty()) tl_builder.add_tag("name", waters_name);
            }
        }
        
        if (!strcmp(field_name, FEAT_COD)) {
            if (!strcmp(field_value, "500412")) {
                // FEAT_TYPE 'RIVER'
                tl_builder.add_tag("water", "river");
            } else if (!strcmp(field_value, "500414")) {
                // FEAT_TYPE 'CANAL/WATER CHANNEL'
                tl_builder.add_tag("water", "canal");
            } else if (!strcmp(field_value, "500421")) {
                // FEAT_TYPE 'LAKE'
                tl_builder.add_tag("water", "lake");
            } else if (!strcmp(field_value, "507116")) {
                // Type 'BAY/HARBOUR' just gets the 'natural=water' tag
            } else {
                std::cerr << "Skipping unknown water poly type " << field_value << std::endl;
            }
        }
    }
}

/**
 * \brief adds navteq water tags to way
 */
void build_water_way_taglist(osmium::builder::WayBuilder& builder, ogr_layer_uptr& layer,
        ogr_feature_uptr& feat) {
    // Mind tl_builder scope!
    osmium::builder::TagListBuilder tl_builder(g_way_buffer, &builder);
//    tl_builder.add_tag("natural", "water");
    for (int i = 0; i < layer->GetLayerDefn()->GetFieldCount(); i++) {
        OGRFieldDefn* po_field_defn = layer->GetLayerDefn()->GetFieldDefn(i);
        const char* field_name = po_field_defn->GetNameRef();
        const char* field_value = feat->GetFieldAsString(i);
        
        if (!strcmp(field_name, POLYGON_NM)) {
            if(field_value && field_value[0]) {
                std::string waters_name = to_camel_case_with_spaces(field_value);
                if (!waters_name.empty()) tl_builder.add_tag("name", waters_name);
            }
        }
        
        if (!strcmp(field_name, FEAT_COD)) {
            if (!strcmp(field_value, "500412")) {
                // FEAT_TYPE 'RIVER'
                tl_builder.add_tag("waterway", "river");
            } else if (!strcmp(field_value, "500414")) {
                // FEAT_TYPE 'CANAL/WATER CHANNEL'
                tl_builder.add_tag("waterway", "canal");
            } else if (!strcmp(field_value, "500421")) {
                // FEAT_TYPE 'LAKE'
                std::cerr << "Skipping water way as type LAKE should only exist as polygon" << std::endl;
            } else if (!strcmp(field_value, "507116")) {
                // FEAT_TYPE 'BAY/HARBOUR'
                std::cerr << "Skipping water way as type BAY/HARBOUR should only exist as polygon" << std::endl;
            } else {
                std::cerr << "Skipping unknown water way type " << field_value << std::endl;
            }
        }
    }
}

/**
 * \brief adds navteq landuse tags to Relation
 */
void build_landuse_taglist(osmium::builder::RelationBuilder& builder, ogr_layer_uptr& layer,
        ogr_feature_uptr& feat) {
    // Mind tl_builder scope!
    osmium::builder::TagListBuilder tl_builder(g_rel_buffer, &builder);
    tl_builder.add_tag("type", "multipolygon");
    
    for (int i = 0; i < layer->GetLayerDefn()->GetFieldCount(); i++) {
        OGRFieldDefn* po_field_defn = layer->GetLayerDefn()->GetFieldDefn(i);
        const char* field_name = po_field_defn->GetNameRef();
        const char* field_value = feat->GetFieldAsString(i);
        
        if (!strcmp(field_name, POLYGON_NM)) {
            if(field_value && field_value[0]) {
                std::string poly_name = to_camel_case_with_spaces(field_value);
                if (!poly_name.empty()) tl_builder.add_tag("name", poly_name);
            }
        }
        
        if (!strcmp(field_name, FEAT_COD)) {
            
            // Land Use A types
            if (!strcmp(field_value, "509998")) {
                // FEAT_TYPE 'BEACH'
                tl_builder.add_tag("natural", "beach");
            } else if (!strcmp(field_value, "900103")) {
                // FEAT_TYPE 'PARK/MONUMENT (NATIONAL)'
                tl_builder.add_tag("boundary", "national_park");
            } else if (!strcmp(field_value, "900130")) {
                // FEAT_TYPE 'PARK (STATE)'
                // In many cases this is meant to be a national park or
                // protected area but this is not guaranteed
                tl_builder.add_tag("leisure", "park");
            } else if (!strcmp(field_value, "900140")) {
                // FEAT_TYPE 'PARK IN WATER'
                tl_builder.add_tag("boundary", "national_park");
            } else if (!strcmp(field_value, "900150")) {
                // FEAT_TYPE 'PARK (CITY/COUNTY)'
                tl_builder.add_tag("leisure", "park");
            } else if (!strcmp(field_value, "900159")) {
                // FEAT_TYPE 'UNDEFINED TRAFFIC AREA'
                // Possible handling: area=yes, highway=pedestrian
            } else if (!strcmp(field_value, "900202")) {
                // FEAT_TYPE 'WOODLAND'
                tl_builder.add_tag("landuse", "forest");
            } else if (!strcmp(field_value, "1700215")) {
                // FEAT_TYPE 'PARKING LOT'
                tl_builder.add_tag("amenity", "parking");
            } else if (!strcmp(field_value, "1900403")) {
                // FEAT_TYPE 'AIRPORT'
                tl_builder.add_tag("aeroway", "aerodrome");
            } else if (!strcmp(field_value, "2000124")) {
                // FEAT_TYPE 'SHOPPING CENTRE'
                tl_builder.add_tag("shop", "mall");
                tl_builder.add_tag("building", "retail");
            } else if (!strcmp(field_value, "2000200")) {
                // FEAT_TYPE 'INDUSTRIAL COMPLEX'
                // Could also be landuse=commercial
                tl_builder.add_tag("landuse", "industrial");
            } else if (!strcmp(field_value, "2000403")) {
                // FEAT_TYPE 'UNIVERSITY/COLLEGE'
                tl_builder.add_tag("amenity", "university");
            } else if (!strcmp(field_value, "2000408")) {
                // FEAT_TYPE 'HOSPITAL'
                tl_builder.add_tag("amenity", "hospital");
            } else if (!strcmp(field_value, "2000420")) {
                // FEAT_TYPE 'CEMETERY'
                tl_builder.add_tag("landuse", "cemetery");
            } else if (!strcmp(field_value, "2000457")) {
                // FEAT_TYPE 'SPORTS COMPLEX'
                tl_builder.add_tag("leisure", "stadium");
                //tl_builder.add_tag("building", "yes");
            } else if (!strcmp(field_value, "2000460")) {
                // FEAT_TYPE 'AMUSEMENT PARK'
                tl_builder.add_tag("leisure", "park");
                tl_builder.add_tag("tourism", "theme_park");
            } // Not implemented so far due to missing sample in data: 
              // "ANIMAL PARK" (2000461), MILITARY BASE (900108), NATIVE AMERICAN RESERVATION (900107), RAILYARD (9997007)
            // Land Use B types
            else if (!strcmp(field_value, "900158")) {
                // FEAT_TYPE 'PEDESTRIAN ZONE'
                tl_builder.add_tag("highway", "pedestrian");
            } else if (!strcmp(field_value, "1907403")) {
                // FEAT_TYPE 'AIRCRAFT ROADS'
                tl_builder.add_tag("aeroway", "runway");
            } else if (!strcmp(field_value, "2000123")) {
                // FEAT_TYPE 'GOLF COURSE'
                tl_builder.add_tag("leisure", "golf_course");
                tl_builder.add_tag("sport", "golf");
            } else if (!strcmp(field_value, "9997004")) {
                // FEAT_TYPE 'CONGESTION ZONE'
                // skipping due to no osm equivalent
            } else if (!strcmp(field_value, "9997010")) {
                // FEAT_TYPE 'ENVIRONMENTAL ZONE'
                tl_builder.add_tag("boundary", "low_emission_zone");
                tl_builder.add_tag("type", "boundary");
            } 
            // Unknown if Land Use A or B types, but seems to appear somewhere
            else if (!strcmp(field_value, "509997")) {
                // FEAT_TYPE 'GLACIER'
                tl_builder.add_tag("natural", "glacier");
            } else {
                std::cerr << "Skipping unknown landuse type " << field_value << " " << std::endl;
            }
        }
    }
}

osm_id_vector_type build_water_ways_with_tagList(ogr_layer_uptr& layer, ogr_feature_uptr& feat,
        OGRLineString* line) {
    node_vector_type osm_way_node_ids = create_open_way_nodes(line);

    osm_id_vector_type osm_way_ids;
    int i = 0;
    do {
        osmium::builder::WayBuilder builder(g_way_buffer);
        STATIC_WAY(builder.object()).set_id(g_osm_id++);
        set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
        builder.add_user(USER);
        build_water_way_taglist(builder, layer, feat);
        osmium::builder::WayNodeListBuilder wnl_builder(g_way_buffer, &builder);
        for (int j = i; j < std::min(i + OSM_MAX_WAY_NODES, (int) osm_way_node_ids.size()); j++)
            wnl_builder.add_node_ref(osm_way_node_ids.at(j).second, osm_way_node_ids.at(j).first);
        osm_way_ids.push_back(STATIC_WAY(builder.object()).id());
        i += OSM_MAX_WAY_NODES - 1;
    } while (i < osm_way_node_ids.size());
    return osm_way_ids;
}

void build_relation_members(osmium::builder::RelationBuilder& builder, const osm_id_vector_type& ext_osm_way_ids,
        const osm_id_vector_type& int_osm_way_ids) {
    osmium::builder::RelationMemberListBuilder rml_builder(g_rel_buffer, &builder);

    for (osmium::unsigned_object_id_type osm_id : ext_osm_way_ids)
        rml_builder.add_member(osmium::item_type::way, osm_id, "outer");

    for (osmium::unsigned_object_id_type osm_id : int_osm_way_ids)
        rml_builder.add_member(osmium::item_type::way, osm_id, "inner");
}

osmium::unsigned_object_id_type build_admin_boundary_relation_with_tags(ogr_layer_uptr& layer, ogr_feature_uptr& feat,
        osm_id_vector_type ext_osm_way_ids, osm_id_vector_type int_osm_way_ids) {
    osmium::builder::RelationBuilder builder(g_rel_buffer);
    STATIC_RELATION(builder.object()).set_id(g_osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
    builder.add_user(USER);
    build_admin_boundary_taglist(builder, layer, feat);
    build_relation_members(builder, ext_osm_way_ids, int_osm_way_ids);
    return STATIC_RELATION(builder.object()).id();
}

osmium::unsigned_object_id_type build_water_relation_with_tags(ogr_layer_uptr& layer, ogr_feature_uptr& feat,
        osm_id_vector_type ext_osm_way_ids, osm_id_vector_type int_osm_way_ids) {
    osmium::builder::RelationBuilder builder(g_rel_buffer);
    STATIC_RELATION(builder.object()).set_id(g_osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
    builder.add_user(USER);
    build_water_poly_taglist(builder, layer, feat);
    build_relation_members(builder, ext_osm_way_ids, int_osm_way_ids);
    return STATIC_RELATION(builder.object()).id();
}

osmium::unsigned_object_id_type build_landuse_relation_with_tags(ogr_layer_uptr& layer, ogr_feature_uptr& feat,
        osm_id_vector_type ext_osm_way_ids, osm_id_vector_type int_osm_way_ids) {
    osmium::builder::RelationBuilder builder(g_rel_buffer);
    STATIC_RELATION(builder.object()).set_id(g_osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
    builder.add_user(USER);
    build_landuse_taglist(builder, layer, feat);
    build_relation_members(builder, ext_osm_way_ids, int_osm_way_ids);
    return STATIC_RELATION(builder.object()).id();
}

/**
 * \brief handles administrative boundary multipolygons
 */
void create_multi_polygon(ogr_multi_polygon_uptr&& mp, osm_id_vector_type& mp_ext_ring_osm_ids,
        osm_id_vector_type& mp_int_ring_osm_ids) {

    for (int i = 0; i < mp->getNumGeometries(); i++) {
        OGRPolygon* poly = static_cast<OGRPolygon*>(mp->getGeometryRef(i));

        osm_id_vector_type exterior_way_ids = build_closed_ways(poly->getExteriorRing());
        // append osm_way_ids to relation_member_ids
        std::move(exterior_way_ids.begin(), exterior_way_ids.end(), std::back_inserter(mp_ext_ring_osm_ids));

        for (int i = 0; i < poly->getNumInteriorRings(); i++) {
            auto interior_way_ids = build_closed_ways(poly->getInteriorRing(i));
            std::move(interior_way_ids.begin(), interior_way_ids.end(), std::back_inserter(mp_int_ring_osm_ids));
        }
    }
}

void create_polygon(ogr_polygon_uptr&& poly, osm_id_vector_type& exterior_way_ids,
        osm_id_vector_type& interior_way_ids) {
    exterior_way_ids = build_closed_ways(poly->getExteriorRing());

    for (int i = 0; i < poly->getNumInteriorRings(); i++) {
        auto tmp = build_closed_ways(poly->getInteriorRing(i));
        std::move(tmp.begin(), tmp.end(), std::back_inserter(interior_way_ids));
    }
}

/**
 * \brief adds administrative boundaries as Relations to m_buffer
 */
void process_admin_boundary(ogr_layer_uptr& layer, ogr_feature_uptr& feat) {
    ogr_geometry_uptr geom(feat->GetGeometryRef());
    auto geom_type = geom->getGeometryType();

    osm_id_vector_type exterior_way_ids, interior_way_ids;
    if (geom_type == wkbMultiPolygon) {
        create_multi_polygon(ogr_multi_polygon_uptr(static_cast<OGRMultiPolygon*>(geom.release())),
                exterior_way_ids, interior_way_ids);

    } else if (geom_type == wkbPolygon) {
        create_polygon(ogr_polygon_uptr(static_cast<OGRPolygon*>(geom.release())), exterior_way_ids,
                interior_way_ids);
    } else {
        throw(std::runtime_error(
                "Adminboundaries with geometry=" + std::string(geom->getGeometryName()) + " are not yet supported."));
    }
    build_admin_boundary_relation_with_tags(layer, feat, exterior_way_ids, interior_way_ids);

    g_node_buffer.commit();
    g_way_buffer.commit();
    g_rel_buffer.commit();
    geom.release();
}

/**
 * \brief adds water polygons as Relations to m_buffer
 */
void process_water(ogr_layer_uptr& layer, ogr_feature_uptr& feat) {
    ogr_geometry_uptr geom(feat->GetGeometryRef());
    auto geom_type = geom->getGeometryType();

    if (geom_type == wkbLineString) {
        OGRLineString* line = static_cast<OGRLineString*> (geom.release());
        build_water_ways_with_tagList(layer, feat, line);
    } else {
        osm_id_vector_type exterior_way_ids, interior_way_ids;
        if (geom_type == wkbMultiPolygon) {
            create_multi_polygon(ogr_multi_polygon_uptr(static_cast<OGRMultiPolygon*> (geom.release())),
                    exterior_way_ids, interior_way_ids);
        } else if (geom_type == wkbPolygon) {
            create_polygon(ogr_polygon_uptr(static_cast<OGRPolygon*> (geom.release())),
                    exterior_way_ids, interior_way_ids);
        } else {
            throw (std::runtime_error(
                    "Water item with geometry=" + std::string(geom->getGeometryName()) + " is not yet supported."));
        }
        build_water_relation_with_tags(layer, feat, exterior_way_ids, interior_way_ids);
    }
   
    g_node_buffer.commit();
    g_way_buffer.commit();
    g_rel_buffer.commit();
    geom.release();
}

/**
 * \brief adds landuse polygons as Relations to m_buffer
 */
void process_landuse(ogr_layer_uptr& layer, ogr_feature_uptr& feat) {
    ogr_geometry_uptr geom(feat->GetGeometryRef());
    auto geom_type = geom->getGeometryType();
    
    osm_id_vector_type exterior_way_ids, interior_way_ids;
    if (geom_type == wkbMultiPolygon) {
        create_multi_polygon(ogr_multi_polygon_uptr(static_cast<OGRMultiPolygon*> (geom.release())),
                exterior_way_ids, interior_way_ids);
    } else if (geom_type == wkbPolygon) {
        create_polygon(ogr_polygon_uptr(static_cast<OGRPolygon*> (geom.release())), 
                exterior_way_ids, interior_way_ids);
    } else {
        throw (std::runtime_error(
                "Landuse item with geometry=" + std::string(geom->getGeometryName()) + " is not yet supported."));
    }
    
    build_landuse_relation_with_tags(layer, feat, exterior_way_ids, interior_way_ids);
    
    g_node_buffer.commit();
    g_way_buffer.commit();
    g_rel_buffer.commit();
    geom.release();
}

/**
 * \brief adds tags from administrative boundaries to mtd_area_map.
 * 		  adds tags from administrative boundaries to mtd_area_map
 * 		  to be accesible when creating the Relations of
 * 		  administrative boundaries.
 * \param handle file handle to navteq administrative meta data.
 */
void process_meta_areas(boost::filesystem::path dir, bool test = false) {
    DBFHandle handle = read_dbf_file(dir / MTD_AREA_DBF, test ? cnull : std::cerr);

    link_id_type last_link_id;
    for (int i = 0; i < DBFGetRecordCount(handle); i++) {

        osmium::unsigned_object_id_type area_id = dbf_get_uint_by_field(handle, i, AREA_ID);

        mtd_area_dataset data;
        if (g_mtd_area_map.find(area_id) != g_mtd_area_map.end()) {
            data = g_mtd_area_map.at(area_id);
        }
        data.area_id = area_id;

        std::string admin_lvl = std::to_string(dbf_get_uint_by_field(handle, i, ADMIN_LVL));
        if (data.admin_lvl.empty()) {
            data.admin_lvl = admin_lvl;
        } else if (data.admin_lvl != admin_lvl) {
            std::cerr << "entry with area_id=" << area_id << " has multiple admin_lvls:" << data.admin_lvl << ", "
                    << admin_lvl << std::endl;
        }

        std::string lang_code = dbf_get_string_by_field(handle, i, LANG_CODE);
        std::string area_name = dbf_get_string_by_field(handle, i, AREA_NAME);
        data.lang_code_2_area_name.push_back(std::make_pair(lang_code, to_camel_case_with_spaces(area_name)));
        data.area_code_1 = dbf_get_uint_by_field(handle, i, AREA_CODE_1);

        g_mtd_area_map.insert(std::make_pair(area_id, data));
    }
    DBFClose(handle);
}

void preprocess_meta_areas(path_vector_type dirs, bool test = false) {
    for (auto dir : dirs){
        process_meta_areas(dir);
    }
}

link_id_vector_type collect_via_manoeuvre_link_ids(link_id_type link_id, DBFHandle rdms_handle,
        cond_id_type cond_id, int& i) {
    link_id_vector_type via_manoeuvre_link_id;
    via_manoeuvre_link_id.push_back(link_id);
    for (int j = 0;; j++) {
        if (i + j == DBFGetRecordCount(rdms_handle)) {
            i += j - 1;
            break;
        }
        cond_id_type next_cond_id = dbf_get_uint_by_field(rdms_handle, i + j, COND_ID);
        if (cond_id != next_cond_id) {
            i += j - 1;
            break;
        }
        via_manoeuvre_link_id.push_back(dbf_get_uint_by_field(rdms_handle, i + j, MAN_LINKID));
    }
    return via_manoeuvre_link_id;
}

osm_id_vector_type collect_via_manoeuvre_osm_ids(const link_id_vector_type& via_manoeuvre_link_id) {
    osm_id_vector_type via_manoeuvre_osm_id;

    osmium::Location end_point_front;
    osmium::Location end_point_back;
    osmium::Location curr;
    uint ctr = 0;

    for (auto it : via_manoeuvre_link_id) {
        bool reverse = false;

        if (g_link_id_map.find(it) == g_link_id_map.end()) return osm_id_vector_type();

        osm_id_vector_type &osm_id_vector = g_link_id_map.at(it);
        auto first_osm_id = osm_id_vector.at(0);
        const auto &first_way = g_way_buffer.get<const osmium::Way>(g_way_offset_map.get(first_osm_id));
        osmium::Location first_way_front = first_way.nodes().front().location();
        auto last_osm_id = osm_id_vector.at(osm_id_vector.size() - 1);
        const auto &last_way = g_way_buffer.get<const osmium::Way>(g_way_offset_map.get(last_osm_id));
        osmium::Location last_way_back = last_way.nodes().back().location();

        // determine end_points
        if (ctr == 0) {
            // assume this is correct
            end_point_front = first_way_front;
            end_point_back = last_way_back;
        } else {
            if (ctr == 1) {
                // checks whether assumption is valid and corrects it if necessary
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
            if (end_point_back == last_way_back) end_point_back = first_way_front;
            else if (end_point_back == first_way_front) end_point_back = last_way_back;
            else assert(false);
        }

        // check wether we have to reverse vector
        if (osm_id_vector.size() > 1) {
            if (end_point_back == first_way_front) reverse = true;
            else
            assert(end_point_back == last_way_back);
        }
        if (reverse) via_manoeuvre_osm_id.insert(via_manoeuvre_osm_id.end(), osm_id_vector.rbegin(),
                osm_id_vector.rend());
        else via_manoeuvre_osm_id.insert(via_manoeuvre_osm_id.end(), osm_id_vector.begin(), osm_id_vector.end());

        ctr++;
    } // end link_id loop
    return via_manoeuvre_osm_id;
}

void init_cdms_map(DBFHandle cdms_handle, std::map<osmium::unsigned_object_id_type, ushort>& cdms_map) {
    for (int i = 0; i < DBFGetRecordCount(cdms_handle); i++) {
        osmium::unsigned_object_id_type cond_id = dbf_get_uint_by_field(cdms_handle, i, COND_ID);
        ushort cond_type = dbf_get_uint_by_field(cdms_handle, i, COND_TYPE);
        cdms_map.insert(std::make_pair(cond_id, cond_type));
    }
}

/**
 * \brief reads turn restrictions from DBF file and writes them to osmium.
 * \param handle DBF file handle to navteq manoeuvres.
 * */

void add_turn_restrictions(path_vector_type dirs) {
    // maps COND_ID to COND_TYPE
    std::map<osmium::unsigned_object_id_type, ushort> cdms_map;
    for (auto dir : dirs)
        init_cdms_map(read_dbf_file(dir / CDMS_DBF), cdms_map);

    for (auto dir : dirs) {
        DBFHandle rdms_handle = read_dbf_file(dir / RDMS_DBF);
        for (int i = 0; i < DBFGetRecordCount(rdms_handle); i++) {

            link_id_type link_id = dbf_get_uint_by_field(rdms_handle, i, LINK_ID);
            cond_id_type cond_id = dbf_get_uint_by_field(rdms_handle, i, COND_ID);

            auto it = cdms_map.find(cond_id);
            if (it != cdms_map.end() && it->second != RESTRICTED_DRIVING_MANOEUVRE) continue;

            link_id_vector_type via_manoeuvre_link_id = collect_via_manoeuvre_link_ids(link_id, rdms_handle, cond_id,
                    i);

            osm_id_vector_type via_manoeuvre_osm_id = collect_via_manoeuvre_osm_ids(via_manoeuvre_link_id);

            // only process complete turn relations
            if (via_manoeuvre_osm_id.empty()) continue;

            // todo find out which direction turn restriction has and apply. For now: always apply 'no_straight_on'
            build_turn_restriction(via_manoeuvre_osm_id);
        }
        DBFClose(rdms_handle);
    }
}

void add_city_nodes(path_vector_type dirs) {

    for (auto dir : dirs) {
        DBFHandle name_place_handle = read_dbf_file(dir / NAMED_PLC_DBF);
        for (int i = 0; i < DBFGetRecordCount(name_place_handle); i++) {

            link_id_type link_id = dbf_get_uint_by_field(name_place_handle, i, LINK_ID);
            
            if (g_link_id_map.find(link_id) == g_link_id_map.end()) {
                std::cerr << "Skipping city node because of missing link id" << std::endl;
                continue;
            }
            
            uint fac_type = dbf_get_uint_by_field(name_place_handle, i, FAC_TYPE);
            if ( fac_type != 4444 ) {
                std::cerr << "Skipping city node because of wrong POI type" << std::endl;
                continue;
            }
            
            std::string name_type = dbf_get_string_by_field(name_place_handle, i, POI_NMTYPE);
            if ( name_type != "B" ) {
                // Skip this entry as it's just a translated namePlc of former one
                continue;
            }
            
            //Read location from stored way
            osm_id_vector_type &osm_id_vector = g_link_id_map.at(link_id);
            auto first_osm_id = osm_id_vector.at(0);
            const auto &first_way = g_way_buffer.get<const osmium::Way>(g_way_offset_map.get(first_osm_id));
            osmium::Location first_way_front = first_way.nodes().front().location();

            //Add new node
            osmium::builder::NodeBuilder node_builder(g_node_buffer);
            build_node(first_way_front, &node_builder);
            osmium::builder::TagListBuilder tl_builder(g_node_buffer, &node_builder);
                    
            std::string name = dbf_get_string_by_field(name_place_handle, i, POI_NAME);
            tl_builder.add_tag("name", to_camel_case_with_spaces( name ));
            
            uint population = dbf_get_uint_by_field(name_place_handle, i, POPULATION);
            if (population > 0) tl_builder.add_tag("population", std::to_string(population));
            uint capital = dbf_get_uint_by_field(name_place_handle, i, CAPITAL);
            tl_builder.add_tag("place", get_place_value(population, capital) );
            
        }
        DBFClose(name_place_handle);
    }
    g_node_buffer.commit();
}

void init_g_cnd_mod_map(const boost::filesystem::path& dir, std::ostream& out) {
    DBFHandle cnd_mod_handle = read_dbf_file(dir / CND_MOD_DBF, out);
    for (int i = 0; i < DBFGetRecordCount(cnd_mod_handle); i++) {
        cond_id_type cond_id = dbf_get_uint_by_field(cnd_mod_handle, i, COND_ID);
        std::string lang_code = dbf_get_string_by_field(cnd_mod_handle, i, LANG_CODE);
        mod_typ_type mod_type = dbf_get_uint_by_field(cnd_mod_handle, i, CM_MOD_TYPE);
        mod_val_type mod_val = dbf_get_uint_by_field(cnd_mod_handle, i, CM_MOD_VAL);
        g_cnd_mod_map.insert(std::make_pair(cond_id, mod_group_type(mod_type, mod_val, lang_code)));
    }
    DBFClose(cnd_mod_handle);
}

void init_g_cdms_map(const boost::filesystem::path& dir, std::ostream& out) {
    DBFHandle cdms_handle = read_dbf_file(dir / CDMS_DBF, out);
    for (int i = 0; i < DBFGetRecordCount(cdms_handle); i++) {
        link_id_type link_id = dbf_get_uint_by_field(cdms_handle, i, LINK_ID);
        cond_id_type cond_id = dbf_get_uint_by_field(cdms_handle, i, COND_ID);
        g_cdms_map.insert(std::make_pair(link_id, cond_id));
    }
    DBFClose(cdms_handle);
}

void init_g_area_to_govt_code_map(const boost::filesystem::path& dir, std::ostream& out) {
    DBFHandle mtd_area_handle = read_dbf_file(dir / MTD_AREA_DBF, out);
    for (int i = 0; i < DBFGetRecordCount(mtd_area_handle); i++) {
        area_id_type area_id = dbf_get_uint_by_field(mtd_area_handle, i, AREA_ID);
        govt_code_type govt_code = dbf_get_uint_by_field(mtd_area_handle, i, GOVT_CODE);
        g_area_to_govt_code_map.insert(std::make_pair(area_id, govt_code));
    }
    DBFClose(mtd_area_handle);
}

void init_g_cntry_ref_map(const boost::filesystem::path& dir, std::ostream& out) {
    DBFHandle cntry_ref_handle = read_dbf_file(dir / MTD_CNTRY_REF_DBF, out);
    for (int i = 0; i < DBFGetRecordCount(cntry_ref_handle); i++) {
        govt_code_type govt_code = dbf_get_uint_by_field(cntry_ref_handle, i, GOVT_CODE);
        auto unit_measure = dbf_get_string_by_field(cntry_ref_handle, i, UNTMEASURE);
        auto speed_limit_unit = dbf_get_string_by_field(cntry_ref_handle, i, SPEEDLIMITUNIT);
        auto iso_code = dbf_get_string_by_field(cntry_ref_handle, i, ISO_CODE);
        auto cntry_ref = cntry_ref_type(unit_measure.c_str()[0], speed_limit_unit.c_str(), iso_code.c_str());
        g_cntry_ref_map.insert(std::make_pair(govt_code, cntry_ref));
    }
    DBFClose(cntry_ref_handle);
}

ogr_layer_uptr_vector init_street_layers(const path_vector_type& dirs, std::ostream& out) {
    ogr_layer_uptr_vector layer_vector;
    for (auto dir : dirs) {
        layer_vector.push_back(ogr_layer_uptr(read_shape_file(dir / STREETS_SHP, out)));
    }
    return layer_vector;
}

// \brief stores z_levels in z_level_map for later use. Maps link_ids to pairs of indices and z-levels of waypoints with z-levels not equal 0.
void init_z_level_map(boost::filesystem::path dir, std::ostream& out, z_lvl_map& z_level_map) {
    DBFHandle handle = read_dbf_file(dir / ZLEVELS_DBF, out);

    link_id_type last_link_id;
    index_z_lvl_vector_type v;

    for (int i = 0; i < DBFGetRecordCount(handle); i++) {
        link_id_type link_id = dbf_get_uint_by_field(handle, i, LINK_ID);
        ushort point_num = dbf_get_uint_by_field(handle, i, POINT_NUM) - 1;
        assert(point_num >= 0);
        short z_level = dbf_get_uint_by_field(handle, i, Z_LEVEL);

        if (i > 0 && last_link_id != link_id && v.size() > 0) {
            z_level_map.insert(std::make_pair(last_link_id, v));
            v = index_z_lvl_vector_type();
        }
        if (z_level != 0) v.push_back(std::make_pair(point_num, z_level));
        last_link_id = link_id;
    }

    if (!v.empty()) z_level_map.insert(std::make_pair(last_link_id, v));
}

void init_conditional_driving_manoeuvres(const boost::filesystem::path& dir, std::ostream& out) {
    if (dbf_file_exists(dir / CND_MOD_DBF) && dbf_file_exists(dir / CDMS_DBF)) {
        init_g_cnd_mod_map(dir, out);
        init_g_cdms_map(dir, out);
    }
}

void init_country_reference(const boost::filesystem::path& dir, std::ostream& out) {
    if (dbf_file_exists(dir / MTD_AREA_DBF) && dbf_file_exists(dir / MTD_CNTRY_REF_DBF)) {
        init_g_area_to_govt_code_map(dir, out);
        init_g_cntry_ref_map(dir, out);
    }
}

void parse_highway_names(const boost::filesystem::path& dbf_file ) {
    DBFHandle maj_hwys_handle = read_dbf_file(dbf_file);
    for (int i = 0; i < DBFGetRecordCount(maj_hwys_handle); i++) {

        link_id_type link_id = dbf_get_uint_by_field(maj_hwys_handle, i, LINK_ID);
        std::string hwy_name = dbf_get_string_by_field(maj_hwys_handle, i, HIGHWAY_NM);

        if (!fits_street_ref(hwy_name)) continue;

        if (g_hwys_ref_map.find(link_id) == g_hwys_ref_map.end())
            g_hwys_ref_map.insert(std::make_pair(link_id, std::vector<std::string>()));
        g_hwys_ref_map.at(link_id).push_back(hwy_name);
    }
    DBFClose(maj_hwys_handle);
}

void init_highway_names(const boost::filesystem::path& dir, std::ostream& out) {
    if (dbf_file_exists(dir / MAJ_HWYS_DBF)) parse_highway_names(dir / MAJ_HWYS_DBF);
    if (dbf_file_exists(dir / SEC_HWYS_DBF)) parse_highway_names(dir / SEC_HWYS_DBF);
}

z_lvl_map process_z_levels(const path_vector_type& dirs, ogr_layer_uptr_vector& layer_vector, std::ostream& out) {
    assert(layer_vector.size() == dirs.size());
    z_lvl_map z_level_map;
    for (int i = 0; i < layer_vector.size(); i++) {
        boost::filesystem::path dir = dirs.at(i);
        auto& layer = layer_vector.at(i);
        assert(layer->GetGeomType() == wkbLineString);

        init_z_level_map(dir, out, z_level_map);
        init_conditional_driving_manoeuvres(dir, out);
        init_country_reference(dir, out);
        init_highway_names(dir, out);
    }
    return z_level_map;
}

void process_way_end_nodes(ogr_layer_uptr_vector& layer_vector, z_lvl_map& z_level_map) {
    for (int i = 0; i < layer_vector.size(); i++) {
        auto& layer = layer_vector.at(i);
        // get all nodes which may be a routable crossing

        int feature_count = layer->GetFeatureCount(false);
        assert(feature_count >= 0);
        for (auto j = 0; j < feature_count; j++) {
            auto&& feat = ogr_feature_uptr(layer->GetFeature(j));
            link_id_type link_id = get_uint_from_feature(feat, LINK_ID);
            // omit way end nodes with different z-levels (they have to be handled extra)
            if (z_level_map.find(link_id) == z_level_map.end())
                process_way_end_nodes(static_cast<OGRLineString*>(feat->GetGeometryRef()));
        }
        g_node_buffer.commit();
        g_way_buffer.commit();
        layer->ResetReading();
    }
}

void process_way(ogr_layer_uptr_vector& layer_vector, z_lvl_map& z_level_map) {
    for (int i = 0; i < layer_vector.size(); i++) {
        auto& layer = layer_vector.at(i);
        int feature_count = layer->GetFeatureCount(false);
        assert(feature_count >= 0);
        for (auto j = 0; j < feature_count; j++) {
            process_way(ogr_feature_uptr(layer->GetFeature(j)), &z_level_map);
        }
    }
}

/**
 * \brief Parses AltStreets.dbf for route type values.
 */
void process_alt_steets_route_types(path_vector_type dirs) {
    for (auto dir : dirs) {
        DBFHandle alt_streets_handle = read_dbf_file(dir / ALT_STREETS_DBF);
        for (int i = 0; i < DBFGetRecordCount(alt_streets_handle); i++) {

            if (dbf_get_string_by_field(alt_streets_handle, i, ROUTE).empty())
                continue;

            osmium::unsigned_object_id_type link_id = dbf_get_uint_by_field(alt_streets_handle, i, LINK_ID);
            ushort route_type = dbf_get_uint_by_field(alt_streets_handle, i, ROUTE);

            if (g_route_type_map.find(link_id) == g_route_type_map.end()) {
                g_route_type_map.insert(std::make_pair(link_id, route_type));
            } else if (g_route_type_map.at(link_id) > route_type) {
                //As link id's aren't unique in AltStreets.dbf
                //just store the lowest route type
                g_route_type_map[link_id] = route_type;
            }
        }
    }
}

/****************************************************
 * adds layers to osmium:
 *      cur_layer and cur_feature have to be set
 ****************************************************/

/**
 * \brief adds streets to m_buffer / osmium.
 * \param layer Pointer to administrative layer.
 */

void add_street_shapes(path_vector_type dirs, bool test = false) {

    std::ostream& out = test ? cnull : std::cerr;

    ogr_layer_uptr_vector layer_vector = init_street_layers(dirs, out);

    out << " processing z-levels" << std::endl;
    z_lvl_map z_level_map = process_z_levels(dirs, layer_vector, out);

    out << " processing way end points" << std::endl;
    process_way_end_nodes(layer_vector, z_level_map);

    out << " processing ways" << std::endl;
    process_way(layer_vector, z_level_map);

    out << " clean" << std::endl;
    for (auto elem : z_level_map)
        elem.second.clear();
    z_level_map.clear();
    
    // now we can clear some maps:
    g_hwys_ref_map.clear();
    g_cdms_map.clear();
    g_cnd_mod_map.clear();
    g_area_to_govt_code_map.clear();
    g_cntry_ref_map.clear();
    g_z_lvl_nodes_map.clear();
    g_route_type_map.clear();
}

void add_street_shapes(boost::filesystem::path dir, bool test = false) {
    path_vector_type dir_vector;
    dir_vector.push_back(dir);
    add_street_shapes(dir_vector, test);
}

/**
 * \brief adds administrative boundaries to m_buffer / osmium.
 * \param layer pointer to administrative layer.
 */

void add_admin_shape(boost::filesystem::path admin_shape_file, bool test = false) {

    ogr_layer_uptr layer(read_shape_file(admin_shape_file));
    assert(layer->GetGeomType() == wkbPolygon);

    int feature_count = layer->GetFeatureCount(false);
    assert(feature_count >= 0);
    for (auto i = 0; i < feature_count; i++) {
        ogr_feature_uptr feat(layer->GetFeature(i));
        process_admin_boundary(layer, feat);
        feat.release();
    }
}

void add_water_shape(boost::filesystem::path water_shape_file) {

    ogr_layer_uptr layer(read_shape_file(water_shape_file));
    assert(layer->GetGeomType() == wkbPolygon || layer->GetGeomType() == wkbLineString);

    int feature_count = layer->GetFeatureCount(false);
    assert(feature_count >= 0);
    for (auto i = 0; i < feature_count; i++) {
        ogr_feature_uptr feat(layer->GetFeature(i));
        process_water(layer, feat);
        feat.release();
    }
}

void add_landuse_shape(boost::filesystem::path water_shape_file) {

    ogr_layer_uptr layer(read_shape_file(water_shape_file));
    assert(layer->GetGeomType() == wkbPolygon);

    int feature_count = layer->GetFeatureCount(false);
    assert(feature_count >= 0);
    for (auto i = 0; i < feature_count; i++) {
        ogr_feature_uptr feat(layer->GetFeature(i));
        process_landuse(layer, feat);
        feat.release();
    }
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
    g_hwys_ref_map.clear();
    g_way_offset_map.clear();
    g_mtd_area_map.clear();
}

void add_buffer_ids(osm_id_vector_type& v, osmium::memory::Buffer& buf) {
    for (auto& it : buf) {
        v.push_back(static_cast<osmium::OSMObject*>(&it)->id());
    }
}

void assert__id_uniqueness() {
    osm_id_vector_type v;
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
