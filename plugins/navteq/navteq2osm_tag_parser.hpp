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
void add_highway_tag(osmium::builder::TagListBuilder* builder, const char* value) {
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

void add_one_way_tag(osmium::builder::TagListBuilder* builder, const char* value) {
    const char* one_way = "oneway";
    const char* parsed_value = parse_one_way_tag(value);
    builder->add_tag(one_way, parsed_value);
}

void add_access_tags(osmium::builder::TagListBuilder* builder, OGRFeature* f) {
    builder->add_tag("motorcar", parse_bool(get_field_from_feature(f, AR_AUTO)) ? YES : NO);
    builder->add_tag("bus", parse_bool(get_field_from_feature(f, AR_BUS)) ? YES : NO);
    builder->add_tag("taxi", parse_bool(get_field_from_feature(f, AR_TAXIS)) ? YES : NO);
    builder->add_tag("hov", parse_bool(get_field_from_feature(f, AR_CARPOOL)) ? YES : NO);
    builder->add_tag("foot", parse_bool(get_field_from_feature(f, AR_PEDESTRIANS)) ? YES : NO);
    builder->add_tag("emergency", parse_bool(get_field_from_feature(f, AR_EMERVEH)) ? YES : NO);
    builder->add_tag("motorcycle", parse_bool(get_field_from_feature(f, AR_MOTORCYCLES)) ? YES : NO);
    if (parse_bool(get_field_from_feature(f, AR_THROUGH_TRAFFIC))) builder->add_tag("access", "destination");
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
 * \return link id of processed feature.
 */
uint64_t parse_street_tags(osmium::builder::TagListBuilder *builder, OGRFeature* f){
    const char* link_id_s = get_field_from_feature(f, LINK_ID);
    uint64_t link_id = std::stoul(link_id_s);
    builder->add_tag(LINK_ID, link_id_s); // tag for debug purpose

    builder->add_tag("name", to_camel_case_with_spaces(get_field_from_feature(f, ST_NAME)).c_str());
    add_highway_tag(builder, get_field_from_feature(f, FUNC_CLASS));
    add_one_way_tag(builder, get_field_from_feature(f, DIR_TRAVEL));
    add_access_tags(builder, f);

    if (!parse_bool(get_field_from_feature(f, PUB_ACCESS)) || parse_bool(get_field_from_feature(f, PRIVATE)))
        add_tag_access_private(builder);

    if (parse_bool(get_field_from_feature(f, PAVED))) builder->add_tag("surface", "paved");
    if (parse_bool(get_field_from_feature(f, BRIDGE))) builder->add_tag("bridge", YES);
    if (parse_bool(get_field_from_feature(f, TUNNEL))) builder->add_tag("tunnel", YES);
    if (parse_bool(get_field_from_feature(f, TOLLWAY))) builder->add_tag("toll", YES);
    if (parse_bool(get_field_from_feature(f, ROUNDABOUT))) builder->add_tag("junction", "roundabout");
    if (parse_bool(get_field_from_feature(f, FOURWHLDR))) builder->add_tag("4wd_only", YES);

    const char* number_of_physical_lanes = get_field_from_feature(f, PHYS_LANES);
    if( strcmp(number_of_physical_lanes, "0") ) builder->add_tag("lanes", number_of_physical_lanes);

    return link_id;
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

