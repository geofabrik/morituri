/*
 * navteq_types.hpp
 *
 *  Created on: 04.11.2015
 *      Author: philip
 */

#ifndef PLUGINS_NAVTEQ_NAVTEQ_TYPES_HPP_
#define PLUGINS_NAVTEQ_NAVTEQ_TYPES_HPP_

#include <unordered_map>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/osm/location.hpp>
#include <osmium/osm/types.hpp>


typedef uint64_t cond_id_type;
typedef uint64_t mod_typ_type;
typedef uint64_t mod_val_type;

typedef std::pair<mod_typ_type, mod_val_type> mod_pair_type;

struct mod_group_type{
    const char* lang_code;
    mod_typ_type mod_type;
    mod_val_type mod_val;
    mod_group_type(mod_typ_type mod_type, mod_val_type mod_val, const char* lang_code = nullptr){
        this->lang_code = lang_code;
        this->mod_type = mod_type;
        this->mod_val = mod_val;
    }
};

typedef uint64_t area_id_type;
typedef uint64_t govt_code_type;

typedef std::unordered_map<cond_id_type, mod_group_type> cnd_mod_map_type;

typedef uint64_t link_id_type;
typedef std::multimap<link_id_type, cond_id_type> cdms_map_type;

// vector of osm_ids
typedef std::vector<osmium::unsigned_object_id_type> osm_id_vector_type;

// maps location to node ids
typedef std::map<osmium::Location, osmium::unsigned_object_id_type> node_map_type;

// pair of [Location, osm_id]
typedef std::pair<osmium::Location, osmium::unsigned_object_id_type> loc_osmid_pair_type;
// vector of pairs of [Location, osm_id]
typedef std::vector<loc_osmid_pair_type> node_vector_type;

// maps link ids to a vector of osm_ids
typedef std::map<uint64_t, osm_id_vector_type> link_id_map_type;


/* z-level types */
// type of z-levels (range -4 to +5)
typedef short z_lvl_type;

// maps navteq link_ids to pairs of <index, z_lvl>
typedef std::map<uint64_t, std::vector<std::pair<ushort, z_lvl_type>>>z_lvl_map;

// pair [Location, z_level] identifies nodes precisely
typedef std::pair<osmium::Location, z_lvl_type> node_id_type;
// maps pair [Location, z_level] to osm_id.
typedef std::map<node_id_type, osmium::unsigned_object_id_type> z_lvl_nodes_map_type;


typedef osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, size_t> osm_id_to_offset_map;

#endif /* PLUGINS_NAVTEQ_NAVTEQ_TYPES_HPP_ */
