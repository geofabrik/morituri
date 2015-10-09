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
 * \brief apply camel case with spaces to string
 */
std::string& to_camel_case_with_spaces(std::string& camel) {
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
    return camel;
}

/**
 * \brief duplicate const char* value to change
 */
std::string to_camel_case_with_spaces(const char* camel) {
    std::string s(camel);
    to_camel_case_with_spaces(s);
    return s;
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
    else if (!strcmp(field, "ST_NAME"))
        builder->add_tag("name", to_camel_case_with_spaces(value).c_str());
    else if (!strcmp(field, "DIR_TRAVEL"))
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
// http://www.loc.gov/standards/iso639-2/ISO-639-2_utf-8.txt
// ISO-639 conversion
std::map<std::string, std::string> g_lang_code_map;
void parse_lang_code_file() {
    std::ifstream file("plugins/navteq/ISO-639-2_utf-8.txt");
    assert(file.is_open());
    std::string line;
    std::string delim = "|";
    if (file.is_open()){
        while (getline(file, line, '\n')){
            std::vector<std::string> lv;
            auto start = 0u;
            auto end = line.find(delim);
            while (end != std::string::npos){
                lv.push_back(line.substr(start, end-start));
                start = end + delim.length();
                end = line.find(delim, start);
            }
            std::string iso_639_2 = lv.at(0);
            std::string iso_639_1 = lv.at(2);
            std::cout << "inserting "<< iso_639_2 << "-->" << iso_639_1 << std::endl;;
            if (!iso_639_1.empty()) g_lang_code_map.insert(std::make_pair(iso_639_2, iso_639_1));
        }
        file.close();
    }
}

std::string parse_lang_code(std::string lang_code){
    std::transform(lang_code.begin(), lang_code.end(), lang_code.begin(), ::tolower);
    if (g_lang_code_map.empty()) parse_lang_code_file();
    if (g_lang_code_map.find(lang_code) != g_lang_code_map.end()) return g_lang_code_map.at(lang_code);
    std::cerr << lang_code << " not found!" << std::endl;
    throw std::runtime_error("Language code '" + lang_code + "' not found");
}

std::string navteq_2_osm_admin_lvl(std::string navteq_admin_lvl) {
    if (string_is_not_integer(navteq_admin_lvl)) throw std::runtime_error("admin level contains invalid character");

    int navteq_admin_lvl_int = stoi(navteq_admin_lvl);

    if (!is_in_range(navteq_admin_lvl_int, NAVTEQ_ADMIN_LVL_MIN, NAVTEQ_ADMIN_LVL_MAX))
        throw std::runtime_error(
                "invalid admin level. admin level '" + std::to_string(navteq_admin_lvl_int) + "' is out of range.");
    return std::to_string(2 * std::stoi(navteq_admin_lvl)).c_str();
}

#endif /* NAVTEQ2OSMTAGPARSE_HPP_ */

