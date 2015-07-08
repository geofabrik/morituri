/*
 * navteq_mappings.hpp
 *
 *  Created on: 22.06.2015
 *      Author: philip
 */

#ifndef PLUGINS_NAVTEQ_NAME_MAPPING_HPP_
#define PLUGINS_NAVTEQ_NAME_MAPPING_HPP_

namespace {

const std::string STREETS_SHP = "Streets.shp";
const std::string ADMINBNDY_2_SHP = "Adminbndy2.shp";
const std::string ADMINBNDY_3_SHP = "Adminbndy3.shp";
const std::string ADMINBNDY_4_SHP = "Adminbndy4.shp";

const std::string MTD_AREA_DBF = "MtdArea.dbf";
const std::string RDMS_DBF = "Rdms.dbf";
const std::string ZLEVELS_DBF = "Zlevels.dbf";

// MTD_AREA_DBF columns
const char* AREA_ID = "AREA_ID";
const char* LANG_CODE = "LANG_CODE";
const char* AREA_NAME = "AREA_NAME";
const char* ADMIN_LVL = "ADMIN_LVL";

// RDMS_DBF columns
const char* LINK_ID = "LINK_ID";
const char* COND_ID = "COND_ID";
const char* MAN_LINKID = "MAN_LINKID";

// ZLEVELS_DBF columns
const char* Z_LEVEL = "Z_LEVEL";
const char* POINT_NUM = "POINT_NUM";

#define YES "yes"
#define NO "no"

#define NAVTEQ_ADMIN_LVL_MIN 1
#define NAVTEQ_ADMIN_LVL_MAX 7

#define OSM_MAX_WAY_NODES 1000

#define STATIC_NODE(x) static_cast<osmium::Node&>(x)
#define STATIC_WAY(x) static_cast<osmium::Way&>(x)
#define STATIC_RELATION(x) static_cast<osmium::Relation&>(x)
#define STATIC_OSMOBJECT(x) static_cast<osmium::OSMObject&>(x)

}

#endif /* PLUGINS_NAVTEQ_MAPPINGS_HPP_ */
