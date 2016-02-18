/*
 * navteq_mappings.hpp
 *
 *  Created on: 22.06.2015
 *      Author: philip
 */

#ifndef PLUGINS_NAVTEQ_NAME_MAPPING_HPP_
#define PLUGINS_NAVTEQ_NAME_MAPPING_HPP_

#include <osmium/osm.hpp>
#include <osmium/osm/object.hpp>

namespace {

static const boost::filesystem::path STREETS_SHP = "Streets.shp";
static const boost::filesystem::path ADMINBNDY_1_SHP = "Adminbndy1.shp";
static const boost::filesystem::path ADMINBNDY_2_SHP = "Adminbndy2.shp";
static const boost::filesystem::path ADMINBNDY_3_SHP = "Adminbndy3.shp";
static const boost::filesystem::path ADMINBNDY_4_SHP = "Adminbndy4.shp";
static const boost::filesystem::path ADMINBNDY_5_SHP = "Adminbndy5.shp";

static const boost::filesystem::path MTD_CNTRY_REF_DBF = "MtdCntryRef.dbf";
static const boost::filesystem::path MTD_AREA_DBF = "MtdArea.dbf";
static const boost::filesystem::path RDMS_DBF = "Rdms.dbf";
static const boost::filesystem::path CDMS_DBF = "Cdms.dbf";
static const boost::filesystem::path CND_MOD_DBF = "CndMod.dbf";
static const boost::filesystem::path ZLEVELS_DBF = "Zlevels.dbf";


// STREETS columns
const char* LINK_ID = "LINK_ID";
const char* ST_NAME = "ST_NAME";

const char* ADDR_TYPE = "ADDR_TYPE";
const char* L_REFADDR = "L_REFADDR";
const char* L_NREFADDR = "L_NREFADDR";
const char* L_ADDRSCH = "L_ADDRSCH";
const char* L_ADDRFORM = "L_ADDRFORM";
const char* R_REFADDR = "R_REFADDR";
const char* R_NREFADDR = "R_NREFADDR";
const char* R_ADDRSCH = "R_ADDRSCH";
const char* R_ADDRFORM = "R_ADDRFORM";

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
const char* AR_TRUCKS = "AR_TRUCKS";
const char* AR_EMERVEH = "AR_EMERVEH";
const char* AR_MOTORCYCLES = "AR_MOTOR";
const char* AR_THROUGH_TRAFFIC = "AR_TRAFF";
const char* PAVED = "PAVED";
const char* PRIVATE = "PRIVATE";
const char* BRIDGE = "BRIDGE";
const char* TUNNEL = "TUNNEL";
const char* TOLLWAY = "TOLLWAY";
const char* ROUNDABOUT = "ROUNDABOUT";
const char* FERRY = "FERRY_TYPE";
const char* URBAN = "URBAN";
const char* ROUTE = "ROUTE_TYPE";
const char* FOURWHLDR = "FOURWHLDR";
const char* PHYS_LANES = "PHYS_LANES";
const char* PUB_ACCESS = "PUB_ACCESS";
const char* L_AREA_ID = "L_AREA_ID";
const char* R_AREA_ID = "R_AREA_ID";
const char* L_POSTCODE = "L_POSTCODE";
const char* R_POSTCODE = "R_POSTCODE";

const char* AREA_NAME_LANG_CODE = "NM_LANGCD";

// MTD_AREA_DBF columns
const char* AREA_ID = "AREA_ID";
const char* LANG_CODE = "LANG_CODE";
const char* AREA_NAME = "AREA_NAME";
const char* ADMIN_LVL = "ADMIN_LVL";
const char* GOVT_CODE = "GOVT_CODE";

// MTD_CNTRY_REF columns
const char* UNTMEASURE = "UNTMEASURE";
//const char* MAX_ADMINLEVEL = "MAX_ADMINLEVEL";
const char* SPEEDLIMITUNIT = "SPDLIMUNIT";
const char* ISO_CODE = "ISO_CODE";


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

// highway OSM tags
const char* MOTORWAY = "motorway";
const char* TRUNK = "trunk";
const char* PRIMARY = "primary";
const char* SECONDARY = "secondary";
const char* TERTIARY = "tertiary";
const char* RESIDENTIAL = "residential";
const char* UNCLASSIFIED = "unclassified";

// if using OpenstreetMap Carto, highways are being rendered like here:
// http://wiki.openstreetmap.org/wiki/Key:highway#Roads
// TODO: Untested mapping. Add remaining countries and test.
std::map<std::string, std::vector<std::string>> const HWY_LVL_MAP = {
	{ "default", { "", MOTORWAY, TRUNK, PRIMARY, SECONDARY, TERTIARY, UNCLASSIFIED } },
	{ "GER", { "", MOTORWAY, MOTORWAY, PRIMARY,SECONDARY, TERTIARY, UNCLASSIFIED } }, // tested
	{ "DEN", { "", MOTORWAY, SECONDARY, TERTIARY, "", "", ""} },
	{ "SWE", { "", MOTORWAY, PRIMARY, SECONDARY, "", "", "", ""} },
	{ "NOR", { "", MOTORWAY, TRUNK, PRIMARY,SECONDARY, "", "" } }
};



#endif /* PLUGINS_NAVTEQ_MAPPINGS_HPP_ */
