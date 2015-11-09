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
const std::string ADMINBNDY_1_SHP = "Adminbndy1.shp";
const std::string ADMINBNDY_2_SHP = "Adminbndy2.shp";
const std::string ADMINBNDY_3_SHP = "Adminbndy3.shp";
const std::string ADMINBNDY_4_SHP = "Adminbndy4.shp";
const std::string ADMINBNDY_5_SHP = "Adminbndy5.shp";

const std::string MTD_AREA_DBF = "MtdArea.dbf";
const std::string RDMS_DBF = "Rdms.dbf";
const std::string CDMS_DBF = "Cdms.dbf";
const std::string CND_MOD_DBF = "CndMod.dbf";
const std::string ZLEVELS_DBF = "Zlevels.dbf";


// STREETS columns
const char* LINK_ID = "LINK_ID";
const char* ST_NAME = "ST_NAME";
const char* FUNC_CLASS = "FUNC_CLASS";
const char* SPEED_CAT = "SPEED_CAT";
const char* FR_SPEED_LIMIT = "FR_SPD_LIM";
const char* TO_SPEED_LIMIT = "TO_SPD_LIM";
const char* DIR_TRAVEL = "DIR_TRAVEL";
const char* AR_AUTO = "AR_AUTO";
const char* AR_BUS = "AR_BUS";
const char* AR_TAXIS = "AR_TAXIS";
const char* AR_CARPOOL = "AR_CARPOOL";
const char* AR_PEDESTRIANS = "AR_PEDEST";
const char* AR_EMERVEH = "AR_EMERVEH";
const char* AR_MOTORCYCLES = "AR_MOTOR";
const char* AR_THROUGH_TRAFFIC = "AR_TRAFF";
const char* PAVED = "PAVED";
const char* PRIVATE = "PRIVATE";
const char* BRIDGE = "BRIDGE";
const char* TUNNEL = "TUNNEL";
const char* TOLLWAY = "TOLLWAY";
const char* ROUNDABOUT = "ROUNDABOUT";
const char* FOURWHLDR = "FOURWHLDR";
const char* PHYS_LANES = "PHYS_LANES";
const char* PUB_ACCESS = "PUB_ACCESS";

// MTD_AREA_DBF columns
const char* AREA_ID = "AREA_ID";
const char* LANG_CODE = "LANG_CODE";
const char* AREA_NAME = "AREA_NAME";
const char* ADMIN_LVL = "ADMIN_LVL";

// RDMS_DBF columns
//const char* LINK_ID = "LINK_ID";
const char* COND_ID = "COND_ID";
const char* MAN_LINKID = "MAN_LINKID";

// CDMS_DBF columns
const char* COND_TYPE = "COND_TYPE";
const char* COND_VAL1 = "COND_VAL1";
const char* COND_VAL2 = "COND_VAL2";
const char* COND_VAL3 = "COND_VAL3";
const char* COND_VAL4 = "COND_VAL4";

// condition types (CT)
#define CT_TRANSPORT_ACCESS_RESTRICTION 23
#define CT_TRANSPORT_RESTRICTED_DRIVING_MANOEUVRE 26

// modifier types (MT)
#define MT_HEIGHT_RESTRICTION 41
#define MT_WEIGHT_RESTRICTION 42
#define MT_WEIGHT_PER_AXLE_RESTRICTION 43
#define MT_LENGTH_RESTRICTION 44
#define MT_WIDTH_RESTRICTION 45

// CndMod types (CM)
const char* CM_MOD_TYPE = "MOD_TYPE";
const char* CM_MOD_VAL = "MOD_VAL";

#define RESTRICTED_DRIVING_MANOEUVRE 7

// ZLEVELS_DBF columns
const char* Z_LEVEL = "Z_LEVEL";
const char* POINT_NUM = "POINT_NUM";

#define YES "yes"
#define NO "no"

#define NAVTEQ_ADMIN_LVL_MIN 1
#define NAVTEQ_ADMIN_LVL_MAX 7

static const char* speed_cat_metric[] = {"", ">130", "101-130", "91-100", "71-90", "51-70", "31-50", "11-30", "<11"};

#define OSM_MAX_WAY_NODES 1000

#define STATIC_NODE(x) static_cast<osmium::Node&>(x)
#define STATIC_WAY(x) static_cast<osmium::Way&>(x)
#define STATIC_RELATION(x) static_cast<osmium::Relation&>(x)
#define STATIC_OSMOBJECT(x) static_cast<osmium::OSMObject&>(x)

// default tags for osm nodes ways and relations
#define USER "import"
#define VERSION "1"
#define CHANGESET "1"
#define USERID "1"
#define TIMESTAMP 1
}

#endif /* PLUGINS_NAVTEQ_MAPPINGS_HPP_ */
