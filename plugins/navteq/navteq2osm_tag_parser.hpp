#ifndef NAVTEQ2OSMTAGPARSE_HPP_
#define NAVTEQ2OSMTAGPARSE_HPP_

#include <iostream>
#include <getopt.h>

#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>
#include <osmium/geom/coordinates.hpp>

#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>

#include "util.hpp"
#include "navteq_mappings.hpp"

// helper
bool parse_bool(const char* value) {
    if (!strcmp(value, "Y")) return true;
    return false;
}

// match 'functional class' to 'highway' tag
void add_highway_tag(const char* value, osmium::builder::TagListBuilder* builder) {
    const char* highway = "highway";

    switch (std::stoi(value)) {
        case 1:
            builder->add_tag(highway, "motorway");
            break;
        case 2:
            builder->add_tag(highway, "primary");
            break;
        case 3:
            builder->add_tag(highway, "secondary");
            break;
        case 4:
            builder->add_tag(highway, "tertiary");
            break;
        case 5:
            builder->add_tag(highway, "residential");
            break;
        default:
            throw(format_error("functional class '" + std::string(value) + "' not valid"));
    }
}

const char* parse_one_way_tag(const char* value) {
    const char* one_way = "oneway";
    if (!strcmp(value, "F"))				// F --> FROM reference node
        return YES;
    else if (!strcmp(value, "T"))			// T --> TO reference node
        return "-1";    // todo reverse way instead using "-1"
    else if (!strcmp(value, "B"))			// B --> BOTH ways are allowed
        return NO;
    throw(format_error("value '" + std::string(value) + "' for oneway not valid"));
}

void add_one_way_tag(const char* value, osmium::builder::TagListBuilder* builder) {
    const char* one_way = "oneway";
    const char* parsed_value = parse_one_way_tag(value);
    builder->add_tag(one_way, parsed_value);
}

void add_access_tags(const char* field, const char* value, osmium::builder::TagListBuilder* builder) {
    const char* osm_value = parse_bool(value) ? YES : NO;
    if (!strcmp(field, "AR_AUTO"))
        builder->add_tag("motorcar", osm_value);
    else if (!strcmp(field, "AR_BUS"))
        builder->add_tag("bus", osm_value);
    else if (!strcmp(field, "AR_TAXIS"))
        builder->add_tag("taxi", osm_value);
    else if (!strcmp(field, "AR_CARPOOLS"))
        builder->add_tag("hov", osm_value);
    else if (!strcmp(field, "AR_PEDESTRIANS"))
        builder->add_tag("foot", osm_value);
    //	else if (!strcmp(field, "AR_TRUCKS"))
    else if (!strcmp(field, "AR_EMERVEH"))
        builder->add_tag("emergency", osm_value);
    else if (!strcmp(field, "AR_MOTORCYCLES"))
        builder->add_tag("motorcycle", osm_value);
    else if (!strcmp(field, "AR_THROUGHTRAFFIC") && !strcmp(osm_value, YES)) builder->add_tag("access", "destination");
}

/**
 * \brief apply camel case with spaces to char*
 */
const char* to_camel_case_with_spaces(char* camel) {
    bool new_word = true;
    for (char *i = camel; *i; i++) {
        if (std::isalpha(*i)) {
            if (new_word) {
                *i = std::toupper(*i);
                new_word = false;
            } else {
                *i = std::tolower(*i);
            }
        } else
            new_word = true;
    }
    return camel;
}

/**
 * \brief duplicate const char* value to change
 */
const char* to_camel_case_with_spaces(const char* camel) {
    return to_camel_case_with_spaces(strdup(camel));
}

/**
 * \brief apply camel case with spaces to string
 */
const char* to_camel_case_with_spaces(std::string camel) {
    bool new_word = true;
    for (auto it = camel.begin(); it != camel.end(); ++it) {
        if (std::isalpha(*it)) {
            if (new_word) {
                *it = std::toupper(*it);
                new_word = false;
            } else {
                *it = std::tolower(*it);
            }
        } else
            new_word = true;
    }
    return camel.c_str();
}

/**
 * \brief adds tag "access: private"
 */
void add_tag_access_private(osmium::builder::TagListBuilder* builder) {
    builder->add_tag("access", "private");
}

/**
 * \brief maps navteq tags for access, tunnel, bridge, etc. to osm tags
 */
void parse_street_tag(osmium::builder::TagListBuilder *builder, const char* field, const char* value) {
    if (!strcmp(field, "FUNC_CLASS"))
        add_highway_tag(value, builder);
    else if (!strcmp(field, "ST_NAME")) {
        const char* camel_value = to_camel_case_with_spaces(value);
        builder->add_tag("name", camel_value);
        delete camel_value;
    } else if (!strcmp(field, "DIR_TRAVEL"))
        add_one_way_tag(value, builder);
    else if (!strncmp(field, "AR_", 3))
        add_access_tags(field, value, builder);
    else if (!strcmp(field, "PUB_ACCESS") && !parse_bool(value))
        add_tag_access_private(builder);
    else if (!strcmp(field, "PRIVATE") && parse_bool(value))
        add_tag_access_private(builder);
    else if (!strcmp(field, "PAVED") && parse_bool(value))
        builder->add_tag("surface", "paved");
    else if (!strcmp(field, "BRIDGE") && parse_bool(value))
        builder->add_tag("bridge", YES);
    else if (!strcmp(field, "TUNNEL") && parse_bool(value))
        builder->add_tag("tunnel", YES);
    else if (!strcmp(field, "TOLLWAY") && parse_bool(value))
        builder->add_tag("toll", YES);
    else if (!strcmp(field, "ROUNDABOUT") && parse_bool(value))
        builder->add_tag("junction", "roundabout");
    else if (!strcmp(field, "FOURWHLDR") && parse_bool(value))
        builder->add_tag("4wd_only", YES);
    else if (!strcmp(field, "PHYS_LANES") && strcmp(value, "0")) builder->add_tag("lanes", value);
}

// matching from http://www.loc.gov/standards/iso639-2/php/code_list.php
// ISO-639 conversion
std::string parse_lang_code(std::string lang_code) {
    if (lang_code == "ABK") return "ab";
    if (lang_code == "AFR") return "af";
    if (lang_code == "AKA") return "ak";
    if (lang_code == "AMH") return "am";
    if (lang_code == "ARA") return "ar";
    if (lang_code == "ARG") return "an";
    if (lang_code == "ASM") return "as";
    if (lang_code == "AVA") return "av";
    if (lang_code == "AVE") return "ae";
    if (lang_code == "AYM") return "ay";
    if (lang_code == "AZE") return "az";
    if (lang_code == "BAK") return "ba";
    if (lang_code == "BAM") return "bm";
    if (lang_code == "BEL") return "be";
    if (lang_code == "BEN") return "bn";
    if (lang_code == "BIH") return "bh";
    if (lang_code == "BIS") return "bi";
    if (lang_code == "BOS") return "bs";
    if (lang_code == "BRE") return "br";
    if (lang_code == "BUL") return "bg";
    if (lang_code == "CAT") return "ca";
    if (lang_code == "CHA") return "ch";
    if (lang_code == "CHE") return "ce";
    if (lang_code == "CHU") return "cu";
    if (lang_code == "CHV") return "cv";
    if (lang_code == "COR") return "kw";
    if (lang_code == "COS") return "co";
    if (lang_code == "CRE") return "cr";
    if (lang_code == "DAN") return "da";
    if (lang_code == "DIV") return "dv";
    if (lang_code == "DZO") return "dz";
    if (lang_code == "ENG") return "en";
    if (lang_code == "EPO") return "eo";
    if (lang_code == "EST") return "et";
    if (lang_code == "EWE") return "ee";
    if (lang_code == "FAO") return "fo";
    if (lang_code == "FIJ") return "fj";
    if (lang_code == "FIN") return "fi";
    if (lang_code == "FRY") return "fy";
    if (lang_code == "FUL") return "ff";
    if (lang_code == "GLA") return "gd";
    if (lang_code == "GLE") return "ga";
    if (lang_code == "GLG") return "gl";
    if (lang_code == "GLV") return "gv";
    if (lang_code == "GRN") return "gn";
    if (lang_code == "GUJ") return "gu";
    if (lang_code == "HAT") return "ht";
    if (lang_code == "HAU") return "ha";
    if (lang_code == "HEB") return "he";
    if (lang_code == "HER") return "hz";
    if (lang_code == "HIN") return "hi";
    if (lang_code == "HMO") return "ho";
    if (lang_code == "HRV") return "hr";
    if (lang_code == "HUN") return "hu";
    if (lang_code == "IBO") return "ig";
    if (lang_code == "IDO") return "io";
    if (lang_code == "III") return "ii";
    if (lang_code == "IKU") return "iu";
    if (lang_code == "ILE") return "ie";
    if (lang_code == "INA") return "ia";
    if (lang_code == "IND") return "id";
    if (lang_code == "IPK") return "ik";
    if (lang_code == "ITA") return "it";
    if (lang_code == "JAV") return "jv";
    if (lang_code == "JPN") return "ja";
    if (lang_code == "KAL") return "kl";
    if (lang_code == "KAN") return "kn";
    if (lang_code == "KAS") return "ks";
    if (lang_code == "KAU") return "kr";
    if (lang_code == "KAZ") return "kk";
    if (lang_code == "KHM") return "km";
    if (lang_code == "KIK") return "ki";
    if (lang_code == "KIN") return "rw";
    if (lang_code == "KIR") return "ky";
    if (lang_code == "KOM") return "kv";
    if (lang_code == "KON") return "kg";
    if (lang_code == "KOR") return "ko";
    if (lang_code == "KUA") return "kj";
    if (lang_code == "KUR") return "ku";
    if (lang_code == "LAO") return "lo";
    if (lang_code == "LAT") return "la";
    if (lang_code == "LAV") return "lv";
    if (lang_code == "LIM") return "li";
    if (lang_code == "LIN") return "ln";
    if (lang_code == "LIT") return "lt";
    if (lang_code == "LTZ") return "lb";
    if (lang_code == "LUB") return "lu";
    if (lang_code == "LUG") return "lg";
    if (lang_code == "MAH") return "mh";
    if (lang_code == "MAL") return "ml";
    if (lang_code == "MAR") return "mr";
    if (lang_code == "MLG") return "mg";
    if (lang_code == "MLT") return "mt";
    if (lang_code == "MON") return "mn";
    if (lang_code == "NAU") return "na";
    if (lang_code == "NAV") return "nv";
    if (lang_code == "NBL") return "nr";
    if (lang_code == "NDE") return "nd";
    if (lang_code == "NDO") return "ng";
    if (lang_code == "NEP") return "ne";
    if (lang_code == "NNO") return "nn";
    if (lang_code == "NOB") return "nb";
    if (lang_code == "NOR") return "no";
    if (lang_code == "NYA") return "ny";
    if (lang_code == "OCI") return "oc";
    if (lang_code == "OJI") return "oj";
    if (lang_code == "ORI") return "or";
    if (lang_code == "ORM") return "om";
    if (lang_code == "OSS") return "os";
    if (lang_code == "PAN") return "pa";
    if (lang_code == "PLI") return "pi";
    if (lang_code == "POL") return "pl";
    if (lang_code == "POR") return "pt";
    if (lang_code == "PUS") return "ps";
    if (lang_code == "QUE") return "qu";
    if (lang_code == "ROH") return "rm";
    if (lang_code == "RUN") return "rn";
    if (lang_code == "RUS") return "ru";
    if (lang_code == "SAG") return "sg";
    if (lang_code == "SAN") return "sa";
    if (lang_code == "SIN") return "si";
    if (lang_code == "SLV") return "sl";
    if (lang_code == "SME") return "se";
    if (lang_code == "SMO") return "sm";
    if (lang_code == "SNA") return "sn";
    if (lang_code == "SND") return "sd";
    if (lang_code == "SOM") return "so";
    if (lang_code == "SOT") return "st";
    if (lang_code == "SPA") return "es";
    if (lang_code == "SRD") return "sc";
    if (lang_code == "SRP") return "sr";
    if (lang_code == "SSW") return "ss";
    if (lang_code == "SUN") return "su";
    if (lang_code == "SWA") return "sw";
    if (lang_code == "SWE") return "sv";
    if (lang_code == "TAH") return "ty";
    if (lang_code == "TAM") return "ta";
    if (lang_code == "TAT") return "tt";
    if (lang_code == "TEL") return "te";
    if (lang_code == "TGK") return "tg";
    if (lang_code == "TGL") return "tl";
    if (lang_code == "THA") return "th";
    if (lang_code == "TIR") return "ti";
    if (lang_code == "TON") return "to";
    if (lang_code == "TSN") return "tn";
    if (lang_code == "TSO") return "ts";
    if (lang_code == "TUK") return "tk";
    if (lang_code == "TUR") return "tr";
    if (lang_code == "TWI") return "tw";
    if (lang_code == "UIG") return "ug";
    if (lang_code == "UKR") return "uk";
    if (lang_code == "URD") return "ur";
    if (lang_code == "UZB") return "uz";
    if (lang_code == "VEN") return "ve";
    if (lang_code == "VIE") return "vi";
    if (lang_code == "VOL") return "vo";
    if (lang_code == "WLN") return "wa";
    if (lang_code == "WOL") return "wo";
    if (lang_code == "XHO") return "xh";
    if (lang_code == "YID") return "yi";
    if (lang_code == "YOR") return "yo";
    if (lang_code == "ZHA") return "za";
    if (lang_code == "ZUL") return "zu";

    std::cerr << lang_code << " not found!" << std::endl;
    throw std::runtime_error("Language code '" + lang_code + "' not found");
    return "";
}

const char* navteq_2_osm_admin_lvl(std::string navteq_admin_lvl) {
    if (string_is_not_integer(navteq_admin_lvl)) throw std::runtime_error("admin level contains invalid character");

    int navteq_admin_lvl_int = stoi(navteq_admin_lvl);

    if (!is_in_range(navteq_admin_lvl_int, NAVTEQ_ADMIN_LVL_MIN, NAVTEQ_ADMIN_LVL_MAX))
        throw std::runtime_error(
                "invalid admin level. admin level '" + std::to_string(navteq_admin_lvl_int) + "' is out of range.");
    return std::to_string(2 * std::stoi(navteq_admin_lvl)).c_str();
}

#endif /* NAVTEQ2OSMTAGPARSE_HPP_ */

