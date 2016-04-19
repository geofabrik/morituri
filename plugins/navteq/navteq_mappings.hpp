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
static const boost::filesystem::path WATER_SEG_SHP = "WaterSeg.shp";
static const boost::filesystem::path WATER_POLY_SHP = "WaterPoly.shp";
static const boost::filesystem::path LAND_USE_A_SHP = "LandUseA.shp";
static const boost::filesystem::path LAND_USE_B_SHP = "LandUseB.shp";

static const boost::filesystem::path MTD_CNTRY_REF_DBF = "MtdCntryRef.dbf";
static const boost::filesystem::path MTD_AREA_DBF = "MtdArea.dbf";
static const boost::filesystem::path RDMS_DBF = "Rdms.dbf";
static const boost::filesystem::path CDMS_DBF = "Cdms.dbf";
static const boost::filesystem::path CND_MOD_DBF = "CndMod.dbf";
static const boost::filesystem::path ZLEVELS_DBF = "Zlevels.dbf";
static const boost::filesystem::path MAJ_HWYS_DBF = "MajHwys.dbf";
static const boost::filesystem::path SEC_HWYS_DBF = "SecHwys.dbf";
static const boost::filesystem::path NAMED_PLC_DBF = "NamedPlc.dbf";
static const boost::filesystem::path ALT_STREETS_DBF = "AltStreets.dbf";


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
const char* CONTRACC = "CONTRACC";
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
const char* AREA_CODE_1 = "AREACODE_1";
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

// MAJ_HWYS columns
//const char* LINK_ID = "LINK_ID";
const char* HIGHWAY_NM = "HIGHWAY_NM";

// NAMED_PLC columns
//const char* LINK_ID = "LINK_ID";
const char* POI_NAME = "POI_NAME";
const char* FAC_TYPE = "FAC_TYPE";
const char* POI_NMTYPE = "POI_NMTYPE";
const char* POPULATION = "POPULATION";
const char* CAPITAL = "CAPITAL";

// WaterSeg and WaterPoly columns
const char* POLYGON_NM = "POLYGON_NM";
const char* FEAT_COD = "FEAT_COD";

// condition types (CT)
#define CT_TRANSPORT_ACCESS_RESTRICTION 23
#define CT_TRANSPORT_RESTRICTED_DRIVING_MANOEUVRE 26

// modifier types (MT)
#define MT_HAZARDOUS_RESTRICTION 39
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
const char* UNCLASSIFIED = "unclassified";
const char* RESIDENTIAL = "residential";
const char* TRACK = "track";
const char* PATH = "path";
const char* FOOTWAY = "footway";


// if using OpenstreetMap Carto, highways are being rendered like here:
// http://wiki.openstreetmap.org/wiki/Key:highway#Roads
// TODO: Untested mapping. Add remaining countries and test.
std::map<int, std::vector<std::string>> const HWY_ROUTE_TYPE_MAP = {
	{   0 /*"DEFAULT"*/, { "", TRUNK, TRUNK, PRIMARY, SECONDARY, TERTIARY, UNCLASSIFIED } },
	{   1 /*"ITA"*/, { "", TRUNK, TRUNK, PRIMARY, SECONDARY, "", "" } },
        {   2 /*"FRA"*/, { "", TRUNK, TRUNK, PRIMARY, SECONDARY, TERTIARY, UNCLASSIFIED } },
	{   3 /*"GER"*/, { "", TRUNK, TRUNK, PRIMARY, SECONDARY, TERTIARY, UNCLASSIFIED } }, // tested
	{   5 /*"BEL"*/, { "", TRUNK, TRUNK, PRIMARY, "", "", PRIMARY } },          //exceptional handling for type 3
        {   6 /*"NLD"*/, { "", TRUNK, TRUNK, PRIMARY, "", "", SECONDARY } },
      //{   6 /*"???"*/, { "", TRUNK, TRUNK, PRIMARY, SECONDARY, TERTIARY, UNCLASSIFIED } },
	{   7 /*"LUX"*/, { "", TRUNK, TRUNK, PRIMARY, SECONDARY, "", "" } },
        {   8 /*"CHE"*/, { "", TRUNK, TRUNK, PRIMARY, "", "", "" } },
        {   9 /*"AUT"*/, { "", TRUNK, TRUNK, TRUNK, PRIMARY, TERTIARY, "" } },      //exceptional handling for type 4 and 5 
      //{ 15 /*"LIE"*/, { "", "", "", PRIMARY, "", "", "" } },
        {  18 /*"ESP"*/, { "", TRUNK, TRUNK, PRIMARY, PRIMARY, SECONDARY, TERTIARY } },
        {  19 /*"???"*/, { "", TRUNK, TRUNK, PRIMARY, SECONDARY, TERTIARY, UNCLASSIFIED } },
        /* 22 - for 22 (northern Ireland) look after UK entries after 112 */
        {  23 /*"IRL"*/, { "", TRUNK, PRIMARY, SECONDARY, TERTIARY, "", "", "" } }, //exceptional handling for type 2
	{  30 /*"HUN"*/, { "", TRUNK, TRUNK, PRIMARY, SECONDARY, "", "" } },
        {  43 /*"POL"*/, { "", TRUNK, TRUNK, TRUNK, PRIMARY, SECONDARY, TERTIARY } },
        { 107 /*"SWE"*/, { "", TRUNK, PRIMARY, SECONDARY, "", "", "", ""} },
	{ 108 /*"DEN"*/, { "", TRUNK, PRIMARY, SECONDARY, "", "", ""} },
        /* NOTE: Meaning of routeType 2 in UK depends on color of A-road sign (green=trunk, white=primary)
         * But it seems there is no way to distinguish both types */
	{ 109 /*UK - Wales*/, { "", TRUNK, PRIMARY, SECONDARY, "", "", "", "" } },
	{ 110 /*UK - England*/, { "", TRUNK, PRIMARY, SECONDARY, "", "", "", "" } },
	{ 112 /*UK - Scotland*/, { "", TRUNK, PRIMARY, SECONDARY, "", "", "", "" } },
	{  22 /*UK - Northern Ireland*/, { "", TRUNK, TRUNK, SECONDARY, "", "", "", "" } }, //Northern Ireland has no white A-roads shields
	{ 120 /*"NOR"*/, { "", TRUNK, PRIMARY, SECONDARY, TERTIARY, "", "" } },
	{ 123 /*Isle Of Man*/, { "", TRUNK, PRIMARY, SECONDARY, TERTIARY, "", "", "" } },
	{ 124 /*Channel Island*/, { "", TRUNK, PRIMARY, SECONDARY, TERTIARY, "", "", "" } },
	{ 130 /*"UKR"*/, { "", TRUNK, TRUNK, PRIMARY, PRIMARY, SECONDARY, "" } }
};

std::map<int, std::vector<std::string>> const HWY_FUNC_CLASS_MAP = {
        /* Applied with functional classes:
         *                         1,       2,         3,        4,        5 + rural,    5 + urban */
	{   0 /*"DEFAULT"*/, { "", PRIMARY, SECONDARY, TERTIARY, TERTIARY, UNCLASSIFIED, RESIDENTIAL } },
	{   3 /*"GER"*/, { "", PRIMARY, SECONDARY, TERTIARY, TERTIARY, UNCLASSIFIED, RESIDENTIAL } },
        {   8 /*"CHE"*/, { "", PRIMARY, SECONDARY, SECONDARY, TERTIARY, UNCLASSIFIED, RESIDENTIAL } },
	{ 108 /*"DEN"*/, { "", PRIMARY, SECONDARY, TERTIARY, TERTIARY, UNCLASSIFIED, RESIDENTIAL } },
        { 107 /*"SWE"*/, { "", PRIMARY, SECONDARY, TERTIARY, TERTIARY, UNCLASSIFIED, RESIDENTIAL } }
};

const double HOUSENUMBER_CURVE_OFFSET = 0.00005;

#endif /* PLUGINS_NAVTEQ_MAPPINGS_HPP_ */
