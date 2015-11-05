/*
 * navteq_types.hpp
 *
 *  Created on: 04.11.2015
 *      Author: philip
 */

#ifndef PLUGINS_NAVTEQ_NAVTEQ_TYPES_HPP_
#define PLUGINS_NAVTEQ_NAVTEQ_TYPES_HPP_


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

typedef std::unordered_map<cond_id_type, mod_group_type> cnd_mod_map_type;
cnd_mod_map_type g_cnd_mod_map;

typedef uint64_t link_id_type;
typedef std::multimap<link_id_type, cond_id_type> cdms_map_type;
cdms_map_type g_cdms_map;


#endif /* PLUGINS_NAVTEQ_NAVTEQ_TYPES_HPP_ */
