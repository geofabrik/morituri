/*
 * navteq_plugin.cpp
 *
 *  Created on: 11.06.2015
 *      Author: philip
 */

#include <osmium/io/any_input.hpp>
#include <osmium/io/any_output.hpp>

#include <gdal/ogr_api.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include "navteq_plugin.hpp"
#include "navteq_util.hpp"
#include "navteq.hpp"

/*

 Convert navteq data into routable OSM files.

 */

navteq_plugin::navteq_plugin(boost::filesystem::path executable_path) :
        base_plugin::base_plugin("Navteq Plugin", executable_path) {
    // setting executable_path in navteq2osm_tag_parser.hpp for reading ISO-file
    g_executable_path = this->executable_path;
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

bool navteq_plugin::check_files(boost::filesystem::path dir) {
    if (!shp_file_exists(dir / STREETS_SHP)) return false;

    if (!dbf_file_exists(dir / MTD_AREA_DBF )) return false;
    if (!dbf_file_exists(dir / RDMS_DBF     )) return false;
    if (!dbf_file_exists(dir / CDMS_DBF     )) return false;
    if (!dbf_file_exists(dir / ZLEVELS_DBF  )) return false;
    if (!dbf_file_exists(dir / MAJ_HWYS_DBF )) return false;
    if (!dbf_file_exists(dir / SEC_HWYS_DBF )) return false;
    if (!dbf_file_exists(dir / NAMED_PLC_DBF)) return false;
    if (!dbf_file_exists(dir / ALT_STREETS_DBF)) return false;

    if (!shp_file_exists(dir / ADMINBNDY_1_SHP)) std::cerr << "  administrative boundaries level 1 are missing\n";
    if (!shp_file_exists(dir / ADMINBNDY_2_SHP)) std::cerr << "  administrative boundaries level 2 are missing\n";
    if (!shp_file_exists(dir / ADMINBNDY_3_SHP)) std::cerr << "  administrative boundaries level 3 are missing\n";
    if (!shp_file_exists(dir / ADMINBNDY_4_SHP)) std::cerr << "  administrative boundaries level 4 are missing\n";
    if (!shp_file_exists(dir / ADMINBNDY_5_SHP)) std::cerr << "  administrative boundaries level 5 are missing\n";
    return true;
}

/**
 * \brief Checks wether there is a subdirectory containinig valid data.
 * \param dir directory from which to start recursion
 * \param recur if set non-directories within the root directory are ignored
 * \return Existance of valid data in a subdirectory.
 */

void navteq_plugin::recurse_dir(boost::filesystem::path dir) {
    if (check_files(dir)) dirs.push_back(dir);

    for (auto& itr : boost::make_iterator_range(boost::filesystem::directory_iterator(dir), { })) {
        if (boost::filesystem::is_directory(itr)){
            recurse_dir(itr);
        }
    }
}

bool navteq_plugin::check_input(boost::filesystem::path input_path, boost::filesystem::path output_file) {
    if (!boost::filesystem::is_directory(input_path))
        throw(std::runtime_error("directory " + input_path.string() + " does not exist"));

    if (!output_file.empty()) {
        boost::filesystem::path output_path = output_file.parent_path();
        if (!boost::filesystem::is_directory(output_path))
            throw(std::runtime_error("output directory " + output_path.string() + " does not exist"));
        if (!is_valid_format(output_file.string()))
            throw(format_error("unknown format for outputfile: " + output_file.string()));
    }

    recurse_dir(input_path);

    if (dirs.empty()) return false;

    std::cout << "dirs: " << std::endl;
    for (auto& dir : dirs)
        std::cout << dir << std::endl;

    this->plugin_setup(input_path, output_file);
    return true;
}

void navteq_plugin::write_output() {
    std::cout << "writing... " << output_path << std::endl;
    osmium::io::File outfile(output_path.string());
    osmium::io::Header hdr;
    hdr.set("generator", "osmium");
    hdr.set("xml_josm_upload", "false");
    osmium::io::Writer writer(outfile, hdr, osmium::io::overwrite::allow);
    writer(std::move(g_node_buffer));
    writer(std::move(g_way_buffer));
    writer(std::move(g_rel_buffer));
    writer.close();
}

void navteq_plugin::add_administrative_boundaries() {
    // todo admin-levels only apply to the US => more generic for all countries

    for (auto dir : dirs) {
        if (shp_file_exists(dir / ADMINBNDY_1_SHP)) add_admin_shape(dir / ADMINBNDY_1_SHP);
        if (shp_file_exists(dir / ADMINBNDY_2_SHP)) add_admin_shape(dir / ADMINBNDY_2_SHP);
        if (shp_file_exists(dir / ADMINBNDY_3_SHP)) add_admin_shape(dir / ADMINBNDY_3_SHP);
        if (shp_file_exists(dir / ADMINBNDY_4_SHP)) add_admin_shape(dir / ADMINBNDY_4_SHP);
        if (shp_file_exists(dir / ADMINBNDY_5_SHP)) add_admin_shape(dir / ADMINBNDY_5_SHP);
    }
    g_mtd_area_map.clear();
}

void navteq_plugin::add_water() {
    for (auto dir : dirs) {
        if (shp_file_exists(dir / WATER_POLY_SHP)) add_water_shape(dir / WATER_POLY_SHP);
        if (shp_file_exists(dir / WATER_SEG_SHP )) add_water_shape(dir / WATER_SEG_SHP );
    }
}

void navteq_plugin::add_landuse() {
    for (auto dir : dirs) {
        if (shp_file_exists(dir / LAND_USE_A_SHP)) add_landuse_shape(dir / LAND_USE_A_SHP);
        if (shp_file_exists(dir / LAND_USE_B_SHP)) add_landuse_shape(dir / LAND_USE_B_SHP);
    }
}

void navteq_plugin::execute() {

    preprocess_meta_areas(dirs);

    process_alt_steets_route_types(dirs);

    add_street_shapes(dirs);
    assert__id_uniqueness();

    add_turn_restrictions(dirs);
    assert__id_uniqueness();

    add_administrative_boundaries();
    
    add_water();
    
    add_landuse();
    
    add_city_nodes(dirs);

    if (!output_path.empty()) write_output();

    std::cout << std::endl << "fin" << std::endl;
}
