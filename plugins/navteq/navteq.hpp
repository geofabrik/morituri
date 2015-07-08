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
#include <cstdlib>
#include <getopt.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>

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

#define DEBUG false

#define USER "import"
#define VERSION "1"
#define CHANGESET "1"
#define USERID "1"
#define TIMESTAMP 1

// maps location to node ids
typedef std::map<osmium::Location, osmium::unsigned_object_id_type> node_map;

// maps navteq link_ids to
typedef std::map<uint64_t, std::vector<std::pair<ushort, short>>>z_lvl_map;

static constexpr int buffer_size = 10 * 1000 * 1000;

// maps location of way end nodes to node ids
node_map way_end_points_map;

// stores osm objects, grows if needed.
osmium::memory::Buffer m_buffer(buffer_size);

// id counter for object creation
osmium::unsigned_object_id_type osm_id = 1;

// id_mmap multimap maps navteq link_ids to osm_ids.
std::multimap<uint64_t, osmium::unsigned_object_id_type> link_id_mmap;

// Provides access to elements in m_buffer through offsets
osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, size_t> offset_map;

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
std::map<osmium::unsigned_object_id_type, mtd_area_dataset> mtd_area_map;

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
 * \brief writes turn restrictions as Relation to m_buffer.
 * 			required roles: "from" and "to".
 * 			optional role: "via" (multiple "via"s are allowed)
 * 			the order of *links is important to assign the correct role.
 *
 * \param osm_ids vector with osm_ids of all nodes which belong to the turn restriction
 * \return Last number of committed bytes to m_buffer before this commit.
 */
size_t write_turn_restriction(std::vector<osmium::unsigned_object_id_type> *osm_ids) {

    osmium::builder::RelationBuilder builder(m_buffer);
    STATIC_RELATION(builder.object()).set_id(std::to_string(osm_id++).c_str());
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
    builder.add_user(USER);

    {
        osmium::builder::RelationMemberListBuilder rml_builder(m_buffer, &builder);

        assert(osm_ids->size() >= 2);

        rml_builder.add_member(osmium::item_type::way, osm_ids->at(0), "from");
        for (int i = 1; i < osm_ids->size() - 1; i++)
            rml_builder.add_member(osmium::item_type::way, osm_ids->at(i), "via");
        rml_builder.add_member(osmium::item_type::way, osm_ids->at(osm_ids->size() - 1), "to");

        osmium::builder::TagListBuilder tl_builder(m_buffer, &builder);
        // todo get the correct direction of the turn restriction
        tl_builder.add_tag("restriction", "no_straight_on");
    }
    return m_buffer.commit();
}

/**
 * \brief parses and writes tags from features with builder
 * \param builder builder to add tags to.
 * \param z_level Sets z_level if valid (-5 is invalid default)
 * \return link id of parsed feature.
 */

uint64_t build_tag_list(osmium::builder::Builder* builder, short z_level = -5) {
    uint64_t link_id = 0;
    osmium::builder::TagListBuilder tl_builder(m_buffer, builder);

    for (int i = 0; i < cur_layer->GetLayerDefn()->GetFieldCount(); i++) {
        OGRFieldDefn* field_definition = cur_layer->GetLayerDefn()->GetFieldDefn(i);
        const char* field_name = field_definition->GetNameRef();
        const char* field_value = cur_feat->GetFieldAsString(i);

        if (!strcmp(field_name, LINK_ID)) {
            tl_builder.add_tag(field_name, field_value);
            link_id = std::stoul(field_value);
            continue;
        }

        // decode fields of shapefile into osm format
        parse_street_tag(&tl_builder, field_name, field_value);
    }

    if (z_level != -5) tl_builder.add_tag("level", std::to_string(z_level).c_str());
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
    STATIC_NODE(builder->object()).set_id(osm_id++);
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
    osmium::builder::NodeBuilder builder(m_buffer);
    return build_node(location, &builder);
}

/**
 * \brief gets id from Node with location. creates Node if missing.
 * \param location Location of Node being created.
 * \return id of found or created Node.
 */
osmium::unsigned_object_id_type get_node(osmium::Location location) {
    auto it = way_end_points_map.find(location);
    if (it != way_end_points_map.end()) return it->second;
    return build_node(location);
}

/**
 * \brief adds WayNode to Way.
 * \param location Location of WayNode
 * \param wnl_builder Builder to create WayNode
 * \param node_ref_map holds Node ids suitable to given Location.
 */
void add_way_node(const osmium::Location& location, osmium::builder::WayNodeListBuilder& wnl_builder,
        node_map* node_ref_map) {
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
osmium::unsigned_object_id_type create_way_with_tag_list(OGRLineString* ogr_ls, node_map *node_ref_map = nullptr,
        bool is_sub_linestring = false, short z_lvl = -5) {
    if (is_sub_linestring) test__z_lvl_range(z_lvl);

    osmium::builder::WayBuilder builder(m_buffer);
    STATIC_WAY(builder.object()).set_id(osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));

    builder.add_user(USER);
    osmium::builder::WayNodeListBuilder wnl_builder(m_buffer, &builder);
    for (int i = 0; i < ogr_ls->getNumPoints(); i++) {
        osmium::Location location(ogr_ls->getX(i), ogr_ls->getY(i));
        bool is_end_point = i == 0 || i == ogr_ls->getNumPoints() - 1;
        std::map < osmium::Location, osmium::unsigned_object_id_type > *map_containing_node;
        if (!is_sub_linestring) {
            if (is_end_point)
                map_containing_node = &way_end_points_map;
            else
                map_containing_node = node_ref_map;
        } else {
            if (node_ref_map->find(location) != node_ref_map->end())
                map_containing_node = node_ref_map;
            else if (way_end_points_map.find(location) != way_end_points_map.end())
                map_containing_node = &way_end_points_map;
            else
                assert(false); // node has to be in node_ref_map or way_end_points_map
        }
        add_way_node(location, wnl_builder, map_containing_node);
    }
    uint64_t link_id = build_tag_list(&builder, z_lvl);
    assert(link_id != 0);
    link_id_mmap.insert(std::make_pair(link_id, (osmium::unsigned_object_id_type) STATIC_WAY(builder.object()).id()));

    return STATIC_WAY(builder.object()).id();
}

/**
 * \brief  creates a sublinestring from ogr_ls from range [start_index, end_index] inclusive
 * \param ogr_ls Linestring from which the sublinestring is taken from.
 * \param start_index Node index in ogr_ls, where sublinestring will begin.
 * \param end_index Node index in ogr_ls, where sublinestring will end.
 * \return sublinestring from ogr_ls [start_index, end_index] inclusive
 */
OGRLineString create_ogr_sublinestring(OGRLineString* ogr_ls, int start_index, int end_index = -1) {
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
void create_sub_way_by_index(ushort start_index, ushort end_index, OGRLineString* ogr_ls, node_map* node_ref_map,
        short z_lvl = 0) {
    OGRLineString ogr_sub_ls = create_ogr_sublinestring(ogr_ls, start_index, end_index);
    osmium::unsigned_object_id_type way_id = create_way_with_tag_list(&ogr_sub_ls, node_ref_map, true, z_lvl);
    offset_map.set(way_id, m_buffer.commit());
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
 */
ushort create_continuing_sub_ways(ushort first_index, ushort start_index, ushort last_index, uint link_id,
        std::vector<std::pair<ushort, short> >* node_z_level_vector, OGRLineString* ogr_ls, node_map* node_ref_map) {
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
                create_sub_way_by_index(from, to, ogr_ls, node_ref_map, z_lvl);
                start_index = to;
            }

            if (not_last_element && to < next_index - 1) {
                create_sub_way_by_index(to, next_index - 1, ogr_ls, node_ref_map);
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
        node_map *node_ref_map, uint link_id) {

    ushort first_index = 0, last_index = ogr_ls->getNumPoints() - 1;
    ushort start_index = node_z_level_vector->cbegin()->first;
    if (start_index > 0) start_index--;

    // first_index <= start_index < end_index <= last_index
    assert(first_index <= start_index);
    assert(start_index < last_index);

//	if (DEBUG) print_z_level_map(link_id, true);

    if (first_index != start_index) {
        create_sub_way_by_index(first_index, start_index, ogr_ls, node_ref_map);
        if (DEBUG)
            std::cout << " 1 ## " << link_id << " ## " << first_index << "/" << last_index << "  -  " << start_index
                    << "/" << last_index << ": \tz_lvl=" << 0 << std::endl;
    }

    start_index = create_continuing_sub_ways(first_index, start_index, last_index, link_id, node_z_level_vector, ogr_ls,
            node_ref_map);

    if (start_index < last_index) {
        create_sub_way_by_index(start_index, last_index, ogr_ls, node_ref_map);
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
 * \brief creates Way from linestring.
 * 		  creates missing Nodes needed for Way and Way itself.
 * \param ogr_ls linestring which provides the geometry.
 * \param z_level_map holds z_levels to Nodes of Ways.
 */
void process_way(OGRLineString *ogr_ls, z_lvl_map *z_level_map) {
    std::map < osmium::Location, osmium::unsigned_object_id_type > node_ref_map;

    // creates remaining nodes required for way
    for (int i = 1; i < ogr_ls->getNumPoints() - 1; i++) {
        osmium::Location location(ogr_ls->getX(i), ogr_ls->getY(i));
        node_ref_map.insert(std::make_pair(location, build_node(location)));
    }
    m_buffer.commit();

    if (ogr_ls->getNumPoints() > 2) assert(node_ref_map.size() > 0);

    uint64_t link_id = get_uint_from_feature(cur_feat, LINK_ID);

    auto it = z_level_map->find(link_id);
    if (it == z_level_map->end()) {
        osmium::unsigned_object_id_type way_id = create_way_with_tag_list(ogr_ls, &node_ref_map);
        offset_map.set(way_id, m_buffer.commit());
    } else
        split_way_by_z_level(ogr_ls, &it->second, &node_ref_map, link_id);
}

// \brief writes way end node to way_end_points_map.
void process_way_end_node(osmium::Location location) {
    init_map_at_element(&way_end_points_map, location, get_node(location));
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
    std::vector<std::pair<ushort, short>> v;

    for (int i = 0; i < DBFGetRecordCount(handle); i++) {
        uint64_t link_id = dbf_get_int_by_field(handle, i, LINK_ID);
        ushort point_num = dbf_get_int_by_field(handle, i, POINT_NUM) - 1;
        assert(point_num >= 0);
        short z_level = dbf_get_int_by_field(handle, i, Z_LEVEL);

        if (i > 0 && last_link_id != link_id && v.size() > 0) {
            z_level_map.insert(std::make_pair(last_link_id, v));
            v = std::vector<std::pair<ushort, short>>();
        }
        if (z_level != 0) v.push_back(std::make_pair(point_num, z_level));
        last_link_id = link_id;
    }
    if (v.size() > 0) z_level_map.insert(std::make_pair(last_link_id, v));
//	print_z_level_map();
    return z_level_map;
}

/**
 * \brief adds administrative boundaries as Relations to m_buffer
 */
void process_admin_boundaries() {
    OGRLinearRing *ring = static_cast<OGRPolygon*>(cur_feat->GetGeometryRef())->getExteriorRing();

    // todo handle interior rings

    // create nodes for admin boundaries
    std::vector<std::pair<osmium::Location, osmium::unsigned_object_id_type>> osm_way_node_ids;
    for (int i = 0; i < ring->getNumPoints(); i++) {
        osmium::Location location(ring->getX(i), ring->getY(i));
        osm_way_node_ids.push_back(std::make_pair(location, get_node(location)));
    }

    std::vector < osmium::unsigned_object_id_type > osm_way_ids;
    int i = 0;
    do {
        osmium::builder::WayBuilder builder(m_buffer);
        STATIC_WAY(builder.object()).set_id(osm_id++);
        set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
        builder.add_user(USER);
        osmium::builder::WayNodeListBuilder wnl_builder(m_buffer, &builder);
        for (int j = i; j < std::min(i + OSM_MAX_WAY_NODES, (int) osm_way_node_ids.size()); j++)
            wnl_builder.add_node_ref(osm_way_node_ids.at(j).second, osm_way_node_ids.at(j).first);
        osm_way_ids.push_back(STATIC_WAY(builder.object()).id());
        i += OSM_MAX_WAY_NODES - 1;
    } while (i < osm_way_node_ids.size());

    osmium::builder::RelationBuilder builder(m_buffer);
    STATIC_RELATION(builder.object()).set_id(osm_id++);
    set_dummy_osm_object_attributes(STATIC_OSMOBJECT(builder.object()));
    builder.add_user(USER);

    // adds navteq administrative boundary tags to Relation
    { // Mind tl_builder scope!
        osmium::builder::TagListBuilder tl_builder(m_buffer, &builder);
        tl_builder.add_tag("type", "multipolygon");
        tl_builder.add_tag("boundary", "administrative");

        for (int i = 0; i < cur_layer->GetLayerDefn()->GetFieldCount(); i++) {
            OGRFieldDefn* po_field_defn = cur_layer->GetLayerDefn()->GetFieldDefn(i);
            const char* field_name = po_field_defn->GetNameRef();
            const char* field_value = cur_feat->GetFieldAsString(i);

            // admin boundary mapping: see NAVSTREETS Street Data Reference Manual: p.947)
            if (!strcmp(field_name, "AREA_ID")) {
                osmium::unsigned_object_id_type area_id = std::stoi(field_value);

                if (mtd_area_map.find(area_id) != mtd_area_map.end()) {
                    auto d = mtd_area_map.at(area_id);
                    if (!d.admin_lvl.empty()) tl_builder.add_tag("navteq_admin_level", d.admin_lvl);
                    if (!d.admin_lvl.empty()) tl_builder.add_tag("admin_level", navteq_2_osm_admin_lvl(d.admin_lvl));
                    for (auto it : d.lang_code_2_area_name)
                        tl_builder.add_tag(std::string("name:" + parse_lang_code(it.first)), it.second);

//                    if (!d.area_name_tr.empty()) tl_builder.add_tag("int_name", d.area_name_tr);

                } else {
                    std::cerr << "skipping unknown navteq_admin_level" << std::endl;
                }
            }
        }
    }

    osmium::builder::RelationMemberListBuilder rml_builder(m_buffer, &builder);
    for (osmium::unsigned_object_id_type way_id : osm_way_ids)
        rml_builder.add_member(osmium::item_type::way, way_id, "outer");
    m_buffer.commit();

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

        osmium::unsigned_object_id_type area_id = dbf_get_int_by_field(handle, i, AREA_ID);

        mtd_area_dataset data;
        if (mtd_area_map.find(area_id) != mtd_area_map.end()) data = mtd_area_map.at(area_id);

        data.area_id = area_id;

        std::string admin_lvl = std::to_string(dbf_get_int_by_field(handle, i, ADMIN_LVL));

        if (data.admin_lvl.empty())
            data.admin_lvl = admin_lvl;
        else if (data.admin_lvl != admin_lvl)
            std::cerr << "entry with area_id=" << area_id << " has multiple admin_lvls:" << data.admin_lvl << ", "
                    << admin_lvl << std::endl;

        std::string lang_code = dbf_get_string_by_field(handle, i, LANG_CODE);
        std::string area_name = dbf_get_string_by_field(handle, i, AREA_NAME);
        data.lang_code_2_area_name.push_back(std::make_pair(lang_code, to_camel_case_with_spaces(area_name)));

        mtd_area_map.insert(std::make_pair(area_id, data));
//		data.print();
    }
}

/**
 * \brief reads turn restrictions from DBF file and writes them to osmium.
 * \param handle DBF file handle to navteq manoeuvres.
 * */

void process_turn_restrictions(DBFHandle handle) {
    for (int i = 0; i < DBFGetRecordCount(handle); i++) {

        uint64_t link_id = dbf_get_int_by_field(handle, i, LINK_ID);
        std::vector < uint64_t > via_manoeuvre_link_id;

        int j;
        via_manoeuvre_link_id.push_back(link_id);
        for (j = 0;; j++) {
            if (i + j == DBFGetRecordCount(handle)) {
                i += j - 1;
                break;
            }
            osmium::unsigned_object_id_type cond_id = dbf_get_int_by_field(handle, i, COND_ID);
            osmium::unsigned_object_id_type next_cond_id = dbf_get_int_by_field(handle, i + j, COND_ID);
            if (cond_id != next_cond_id) {
                i += j - 1;
                break;
            }
            via_manoeuvre_link_id.push_back(dbf_get_int_by_field(handle, i + j, MAN_LINKID));
        }

        std::vector < osmium::unsigned_object_id_type > via_manoeuvre_osm_id;
        bool complete = true;

        for (auto it : via_manoeuvre_link_id) {
            auto range = link_id_mmap.equal_range(it);
            if (range.first == range.second) {
                complete = false;
                std::cerr << "link_id " << it << " is missing in Streets.dbf" << std::endl;
                break;
            }
            for (auto it2 = range.first; it2 != range.second; ++it2) {
                via_manoeuvre_osm_id.push_back(it2->second);
            }
        }
        // only process complete turn relations
        if (!complete) continue;

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

void add_point_layer_to_osmium(OGRLayer *layer, std::string dir = std::string()) {
    cur_layer = layer;
    while ((cur_feat = layer->GetNextFeature()) != NULL) {
        OGRPoint *p = static_cast<OGRPoint*>(cur_feat->GetGeometryRef());

        osmium::builder::NodeBuilder builder(m_buffer);
        build_node(osmium::Location(p->getX(), p->getY()), &builder);
        build_tag_list(&builder);
    }
    m_buffer.commit();
}

/**
 * \brief adds streets to m_buffer / osmium.
 * \param layer Pointer to administrative layer.
 */

void add_street_shape_to_osmium(OGRLayer *layer, std::string dir = std::string(), std::string sub_dir = std::string()) {
    assert(layer->GetGeomType() == wkbLineString);

    cur_layer = layer;

// Maps link_ids to pairs of indices and z-levels of waypoints with z-levels not equal 0.
    z_lvl_map z_level_map = process_z_levels(read_dbf_file(dir + sub_dir + ZLEVELS_DBF));

// get all nodes which may be a routable crossing
    while ((cur_feat = cur_layer->GetNextFeature()) != NULL)
        process_way_end_nodes(static_cast<OGRLineString*>(cur_feat->GetGeometryRef()));
    m_buffer.commit();

    cur_layer->ResetReading();
    while ((cur_feat = cur_layer->GetNextFeature()) != NULL)
        process_way(static_cast<OGRLineString*>(cur_feat->GetGeometryRef()), &z_level_map);
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

    while ((cur_feat = layer->GetNextFeature()) != NULL) {
        process_admin_boundaries();
    }
}

/****************************************************
 * cleanup and assertions
 ****************************************************/

/**
 * \brief clears all global variables.
 */

void clear_all() {
    int cleared = m_buffer.clear();
    osm_id = 1;
    link_id_mmap.clear();
    offset_map.clear();
    mtd_area_map.clear();
}

void assert__id_uniqueness() {
    std::vector < osmium::unsigned_object_id_type > v;
    for (auto& it : m_buffer) {
        osmium::OSMObject* obj = static_cast<osmium::OSMObject*>(&it);
        v.push_back((osmium::unsigned_object_id_type) obj->id());
    }
    std::sort(v.begin(), v.end());
    auto ptr = unique(v.begin(), v.end());
    assert(ptr == v.end());
}

void assert__node_locations_uniqueness() {
    std::map < osmium::Location, osmium::unsigned_object_id_type > loc_z_lvl_map;
    for (auto& it : m_buffer) {
        osmium::OSMObject* obj = static_cast<osmium::OSMObject*>(&it);
        if (obj->type() == osmium::item_type::node) {
            osmium::Node* node = static_cast<osmium::Node*>(obj);
            osmium::Location l = node->location();
//			std::cout << node->id() << " - "<< l << std::endl;
            if (loc_z_lvl_map.find(l) == loc_z_lvl_map.end())
                loc_z_lvl_map.insert(std::make_pair(node->location(), node->id()));
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

#endif /* NAVTEQ_HPP_ */
