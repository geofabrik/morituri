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

// Provides access to elements in g_way_buffer through offsets
osm_id_to_offset_map_type g_way_offset_map;

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

// map for conditional modifications
cnd_mod_map_type g_cnd_mod_map;

// map for conditional driving manoeuvres
cdms_map_type g_cdms_map;
std::map<area_id_type, govt_code_type> g_area_to_govt_code_map;
cntry_ref_map_type g_cntry_ref_map;

// current layer
OGRLayer* g_cur_layer;

// current feature
OGRFeature* g_cur_feat;

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
size_t build_turn_restriction(std::vector<osmium::unsigned_object_id_type> *osm_ids) {

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

link_id_type build_tag_list(osmium::builder::Builder* builder, osmium::memory::Buffer& buf, short z_level = -5) {
    osmium::builder::TagListBuilder tl_builder(buf, builder);

    link_id_type link_id = parse_street_tags(&tl_builder, g_cur_feat, &g_cdms_map, &g_cnd_mod_map, &g_area_to_govt_code_map,
            &g_cntry_ref_map);

    if (z_level != -5 && z_level != 0) tl_builder.add_tag("layer", std::to_string(z_level).c_str());
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

osmium::unsigned_object_id_type build_node_with_tag(osmium::Location location, const char* tag_key,
        const char* tag_val) {
    osmium::builder::NodeBuilder node_builder(g_node_buffer);
    auto node_id = build_node(location, &node_builder);
    if (tag_key) {
        osmium::builder::TagListBuilder tl_builder(g_node_buffer, &node_builder);
        tl_builder.add_tag(tag_key, tag_val);
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
    link_id_type link_id = build_tag_list(&builder, g_way_buffer, z_lvl);
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
 * \brief replaces all z-levels by zero, which are not an endpoint
 * \param z_lvl_vec vector containing pairs of [z_lvl_index, z_lvl]
 */
void set_ferry_z_lvls_to_zero(std::vector<std::pair<ushort, z_lvl_type> >& z_lvl_vec) {
    // erase middle z_lvls
    if (z_lvl_vec.size() > 2) z_lvl_vec.erase(z_lvl_vec.begin() + 1, z_lvl_vec.end() - 1);
    // erase first z_lvl if first index references first node
    if (z_lvl_vec.size() > 0 && z_lvl_vec.begin()->first != 0) z_lvl_vec.erase(z_lvl_vec.begin());
    // erase last z_lvl if last index references last node
    OGRLineString* ogr_ls = static_cast<OGRLineString*>(g_cur_feat->GetGeometryRef());
    if (z_lvl_vec.size() > 0 && (z_lvl_vec.end() - 1)->first != ogr_ls->getNumPoints() - 1)
        z_lvl_vec.erase(z_lvl_vec.end());
}

void create_house_numbers(OGRLineString* ogr_ls, bool left){
    const char* ref_addr = left ? L_REFADDR : R_REFADDR;
    const char* nref_addr = left ? L_NREFADDR : R_NREFADDR;
    const char* addr_schema = left ? L_ADDRSCH : R_ADDRSCH;

    if (!strcmp(get_field_from_feature(g_cur_feat, ref_addr),"")) return;
    if (!strcmp(get_field_from_feature(g_cur_feat, nref_addr),"")) return;
    if (!strcmp(get_field_from_feature(g_cur_feat, addr_schema),"")) return;
    if (!strcmp(get_field_from_feature(g_cur_feat, addr_schema),"M")) return;

    OGRLineString* offset_ogr_ls = create_offset_curve(ogr_ls, 0.00005, left);

    osmium::builder::WayBuilder way_builder(g_way_buffer);
    STATIC_WAY(way_builder.object()).set_id(g_osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(way_builder.object()));
    way_builder.add_user(USER);
    osmium::builder::WayNodeListBuilder wnl_builder(g_way_buffer, &way_builder);
    for (int i = 0; i < offset_ogr_ls->getNumPoints(); i++) {
        osmium::Location location(offset_ogr_ls->getX(i), offset_ogr_ls->getY(i));
        assert(location.valid());
        osmium::unsigned_object_id_type node_id;
        if (i == 0) {
            node_id = build_node_with_tag(location, "addr:housenumber", get_field_from_feature(g_cur_feat, ref_addr));
        } else if (i == offset_ogr_ls->getNumPoints() - 1) {
            node_id = build_node_with_tag(location, "addr:housenumber", get_field_from_feature(g_cur_feat, nref_addr));
        } else {
            node_id = build_node(location);
        }

        wnl_builder.add_node_ref(osmium::NodeRef(node_id, location));
    }
    {
        osmium::builder::TagListBuilder tl_builder(g_way_buffer, &way_builder);
        const char* schema = parse_house_number_schema(get_field_from_feature(g_cur_feat, addr_schema));
        tl_builder.add_tag("addr:interpolation", schema);
    }
    g_node_buffer.commit();
    g_way_buffer.commit();
}

void create_house_numbers(OGRLineString* ogr_ls) {
    create_house_numbers(ogr_ls, true);
    create_house_numbers(ogr_ls, false);
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

    link_id_type link_id = get_uint_from_feature(g_cur_feat, LINK_ID);

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

        bool ferry = is_ferry(get_field_from_feature(g_cur_feat, FERRY));
        if (ferry) set_ferry_z_lvls_to_zero(it->second);

        split_way_by_z_level(ogr_ls, &it->second, &node_ref_map, link_id);
    }

    if (!strcmp(get_field_from_feature(g_cur_feat, ADDR_TYPE),"B")){
        create_house_numbers(ogr_ls);
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

/**
 * \brief creates nodes for administrative boundary
 * \return osm_ids of created nodes
 */
node_vector_type create_admin_boundary_way_nodes(OGRLinearRing* ring) {
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
 * \brief creates ways for administrative boundary
 * \return osm_ids of created ways
 */
osm_id_vector_type build_admin_boundary_ways(OGRLinearRing* ring) {
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
void build_admin_boundary_taglist(osmium::builder::RelationBuilder& builder) {
    // Mind tl_builder scope!
    osmium::builder::TagListBuilder tl_builder(g_rel_buffer, &builder);
    tl_builder.add_tag("type", "multipolygon");
    tl_builder.add_tag("boundary", "administrative");
    for (int i = 0; i < g_cur_layer->GetLayerDefn()->GetFieldCount(); i++) {
        OGRFieldDefn* po_field_defn = g_cur_layer->GetLayerDefn()->GetFieldDefn(i);
        const char* field_name = po_field_defn->GetNameRef();
        const char* field_value = g_cur_feat->GetFieldAsString(i);
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

void build_relation_members(osmium::builder::RelationBuilder& builder, const osm_id_vector_type& ext_osm_way_ids,
        const osm_id_vector_type& int_osm_way_ids) {
    osmium::builder::RelationMemberListBuilder rml_builder(g_rel_buffer, &builder);

    for (osmium::unsigned_object_id_type osm_id : ext_osm_way_ids)
        rml_builder.add_member(osmium::item_type::way, osm_id, "outer");

    for (osmium::unsigned_object_id_type osm_id : int_osm_way_ids)
        rml_builder.add_member(osmium::item_type::way, osm_id, "inner");
}

osmium::unsigned_object_id_type build_admin_boundary_relation_with_tags(osm_id_vector_type ext_osm_way_ids,
        osm_id_vector_type int_osm_way_ids) {
    osmium::builder::RelationBuilder builder(g_rel_buffer);
    STATIC_RELATION(builder.object()).set_id(g_osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
    builder.add_user(USER);
    build_admin_boundary_taglist(builder);
    build_relation_members(builder, ext_osm_way_ids, int_osm_way_ids);
    return STATIC_RELATION(builder.object()).id();
}

/**
 * \brief handles administrative boundary multipolygons
 */
void create_admin_boundary_member(OGRMultiPolygon* mp, osm_id_vector_type& mp_ext_ring_osm_ids,
        osm_id_vector_type& mp_int_ring_osm_ids) {

    for (int i = 0; i < mp->getNumGeometries(); i++){
        OGRPolygon* poly = static_cast<OGRPolygon*>(mp->getGeometryRef(i));

        osm_id_vector_type exterior_way_ids = build_admin_boundary_ways(poly->getExteriorRing());
        // append osm_way_ids to relation_member_ids
        std::move(exterior_way_ids.begin(), exterior_way_ids.end(), std::back_inserter(mp_ext_ring_osm_ids));

        for (int i=0; i<poly->getNumInteriorRings(); i++){
            auto interior_way_ids = build_admin_boundary_ways(poly->getInteriorRing(i));
            std::move(interior_way_ids.begin(), interior_way_ids.end(), std::back_inserter(mp_int_ring_osm_ids));
        }
    }
}

void create_admin_boundary_member(OGRPolygon* poly, osm_id_vector_type& exterior_way_ids,
        osm_id_vector_type& interior_way_ids) {
    exterior_way_ids = build_admin_boundary_ways(poly->getExteriorRing());

    for (int i = 0; i < poly->getNumInteriorRings(); i++) {
        auto tmp = build_admin_boundary_ways(poly->getInteriorRing(i));
        std::move(tmp.begin(), tmp.end(), std::back_inserter(interior_way_ids));
    }
}

/**
 * \brief adds administrative boundaries as Relations to m_buffer
 */
void process_admin_boundary() {
    auto geom = g_cur_feat->GetGeometryRef();
    auto geom_type = geom->getGeometryType();

    osm_id_vector_type exterior_way_ids, interior_way_ids;
    if (geom_type == wkbMultiPolygon) {
        create_admin_boundary_member(static_cast<OGRMultiPolygon*>(geom), exterior_way_ids, interior_way_ids);
    } else if (geom_type == wkbPolygon) {
        create_admin_boundary_member(static_cast<OGRPolygon*>(geom), exterior_way_ids, interior_way_ids);
    } else {
        throw(std::runtime_error(
                "Adminboundaries with geometry=" + std::string(geom->getGeometryName())
                        + " are not yet supported."));
    }
    build_admin_boundary_relation_with_tags(exterior_way_ids, interior_way_ids);

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
void process_meta_areas(boost::filesystem::path dir) {
    DBFHandle handle = read_dbf_file(dir / MTD_AREA_DBF);

    link_id_type last_link_id;
    for (int i = 0; i < DBFGetRecordCount(handle); i++) {

        osmium::unsigned_object_id_type area_id = dbf_get_uint_by_field(handle, i, AREA_ID);

        mtd_area_dataset data;
        if (g_mtd_area_map.find(area_id) != g_mtd_area_map.end()){
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

        g_mtd_area_map.insert(std::make_pair(area_id, data));
    }
    DBFClose(handle);
}

std::vector<link_id_type> collect_via_manoeuvre_link_ids(link_id_type link_id, DBFHandle rdms_handle,
        cond_id_type cond_id, int& i) {
    std::vector<link_id_type> via_manoeuvre_link_id;
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

osm_id_vector_type collect_via_manoeuvre_osm_ids(const std::vector<link_id_type>& via_manoeuvre_link_id) {
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
            build_turn_restriction(&via_manoeuvre_osm_id);
        }
    }
}

void init_g_cnd_mod_map(const boost::filesystem::path& dir, std::ostream& out) {
    DBFHandle cnd_mod_handle = read_dbf_file(dir / CND_MOD_DBF, out);
    for (int i = 0; i < DBFGetRecordCount(cnd_mod_handle); i++) {
        cond_id_type cond_id = dbf_get_uint_by_field(cnd_mod_handle, i, COND_ID);
        // std::string lang_code = dbf_get_string_by_field(cnd_mod_handle, i, LANG_CODE);
        mod_typ_type mod_type = dbf_get_uint_by_field(cnd_mod_handle, i, CM_MOD_TYPE);
        mod_val_type mod_val = dbf_get_uint_by_field(cnd_mod_handle, i, CM_MOD_VAL);
        g_cnd_mod_map.insert(std::make_pair(cond_id, mod_group_type(mod_type, mod_val)));
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
        layer_vector.push_back(ogr_layer_uptr(read_shape_file( dir / STREETS_SHP, out)));
    }
    return layer_vector;
}

// \brief stores z_levels in z_level_map for later use. Maps link_ids to pairs of indices and z-levels of waypoints with z-levels not equal 0.
void init_z_level_map(DBFHandle handle, z_lvl_map& z_level_map) {

    link_id_type last_link_id;
    std::vector<std::pair<ushort, z_lvl_type>> v;

    for (int i = 0; i < DBFGetRecordCount(handle); i++) {
        link_id_type link_id = dbf_get_uint_by_field(handle, i, LINK_ID);
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

z_lvl_map process_z_levels(path_vector_type dirs, ogr_layer_uptr_vector& layer_vector, std::ostream& out) {
    z_lvl_map z_level_map;
    for (int i = 0; i < layer_vector.size(); i++) {
        boost::filesystem::path dir = dirs.at(i);
        auto& layer = layer_vector.at(i);
        assert(layer->GetGeomType() == wkbLineString);
//        g_cur_layer = layer;

        init_z_level_map(read_dbf_file(dir / ZLEVELS_DBF, out), z_level_map);
        init_conditional_driving_manoeuvres(dir, out);
        init_country_reference(dir, out);
    }
    return z_level_map;
}

void process_way_end_nodes(ogr_layer_uptr_vector& layer_vector, z_lvl_map& z_level_map) {
    for (int i = 0; i < layer_vector.size(); i++) {
        auto& layer = layer_vector.at(i);
        // get all nodes which may be a routable crossing
        while ((g_cur_feat = layer->GetNextFeature()) != NULL) {
            link_id_type link_id = get_uint_from_feature(g_cur_feat, LINK_ID);
            // omit way end nodes with different z-levels (they have to be handled extra)
            if (z_level_map.find(link_id) == z_level_map.end())
                process_way_end_nodes(static_cast<OGRLineString*>(g_cur_feat->GetGeometryRef()));
            delete g_cur_feat;
        }
        g_node_buffer.commit();
        g_way_buffer.commit();
        layer->ResetReading();
    }
}

void process_way(ogr_layer_uptr_vector& layer_vector, z_lvl_map& z_level_map) {
    for (int i = 0; i < layer_vector.size(); i++) {
        auto& layer = layer_vector.at(i);
        while ((g_cur_feat = layer->GetNextFeature()) != NULL) {
            process_way(static_cast<OGRLineString*>(g_cur_feat->GetGeometryRef()), &z_level_map);
            delete g_cur_feat;
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
//    for (OGRLayer* layer : layer_vector)
//        delete layer;
    for (auto elem : z_level_map)
        elem.second.clear();
    z_level_map.clear();
}

void add_street_shapes(boost::filesystem::path dir, bool test = false) {
    std::vector<boost::filesystem::path> dir_vector;
    dir_vector.push_back(dir);
    add_street_shapes(dir_vector, test);
}

/**
 * \brief adds administrative boundaries to m_buffer / osmium.
 * \param layer pointer to administrative layer.
 */

void add_admin_shape(boost::filesystem::path admin_shape_file, bool test = false) {

    OGRLayer *layer = read_shape_file(admin_shape_file);
    assert(layer->GetGeomType() == wkbPolygon);
    g_cur_layer = layer;

    while ((g_cur_feat = g_cur_layer->GetNextFeature()) != NULL) {
        process_admin_boundary();
        delete g_cur_feat;
    }

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

void add_buffer_ids(osm_id_vector_type& v, osmium::memory::Buffer& buf) {
    for (auto& it : buf) {
        osmium::OSMObject* obj = static_cast<osmium::OSMObject*>(&it);
        v.push_back((osmium::unsigned_object_id_type) (obj->id()));
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
