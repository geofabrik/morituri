/*
 * navteq_plugin.cpp
 *
 *  Created on: 11.06.2015
 *      Author: philip
 */

#include "navteq_plugin.hpp"
#include "navteq.hpp"
#include <gdal/ogr_api.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include "../util.hpp"

/*

 Convert navteq data into routable OSM files.

 */

navteq_plugin::navteq_plugin() {
    name = "Navteq Plugin";
}

navteq_plugin::~navteq_plugin() {
}

bool navteq_plugin::is_valid_format(std::string filename) {
    if (filename.length() > 3) filename = filename.substr(filename.length() - 3);
    for (auto i : filename)
        i = std::tolower(i);
    if (filename == "pbf") return true;
    if (filename == "osm") return true;
    return false;
}

bool navteq_plugin::check_input(const char* input_path, const char* output_file) {
    if (!boost::filesystem::is_directory(input_path))
        throw(std::runtime_error("directory " + std::string(input_path) + " does not exist"));
    if (output_file) {
        std::string output_path = boost::filesystem::path(output_file).parent_path().string();
        if (!boost::filesystem::is_directory(output_path))
            throw(std::runtime_error("output directory " + output_path + " does not exist"));
        if (!is_valid_format(std::string(output_file)))
            throw(format_error("unknown format for outputfile: " + std::string(output_file)));
    }

    if (!shp_file_exists(input_path + STREETS_SHP)) return false;
    if (!dbf_file_exists(input_path + RDMS_DBF)) return false;
    if (!dbf_file_exists(input_path + ZLEVELS_DBF)) return false;
    if (!dbf_file_exists(input_path + MTD_AREA_DBF)) return false;

    this->plugin_setup(input_path, output_file);
    return true;
}

void navteq_plugin::execute() {
    RegisterOGRShape();

//    base_dir = input_path;

    add_street_shape_to_osmium(read_shape_file(input_path + STREETS_SHP), input_path);
    assert__id_uniqueness();

    process_turn_restrictions(read_dbf_file(input_path + RDMS_DBF));
    assert__id_uniqueness();

//    assert__node_locations_uniqueness();

// todo admin-levels only apply to the US => more generic for all countries
    add_admin_shape_to_osmium(read_shape_file(input_path + ADMINBNDY_2_SHP), input_path);
    add_admin_shape_to_osmium(read_shape_file(input_path + ADMINBNDY_3_SHP), input_path);
    add_admin_shape_to_osmium(read_shape_file(input_path + ADMINBNDY_4_SHP), input_path);

    std::string output = output_path;
    if (!output.empty()) {
        std::cout << "writing... " << output << std::endl;

        osmium::io::File outfile(output);
        osmium::io::Header hdr;
        hdr.set("generator", "osmium");
        hdr.set("xml_josm_upload", "false");

        osmium::io::Writer writer(outfile, hdr, osmium::io::overwrite::allow);
        writer(std::move(m_buffer));
        writer.close();
    }

    std::cout << std::endl << "fin" << std::endl;
}
