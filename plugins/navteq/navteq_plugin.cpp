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
#include "navteq.hpp"
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

bool navteq_plugin::check_files(boost::filesystem::path dir) {
    if (!shp_file_exists(boost::filesystem::path(dir / STREETS_SHP))) return false;
    if (!shp_file_exists(boost::filesystem::path(dir / ADMINBNDY_1_SHP))) std::cerr << "  administrative boundaries level 1 are missing\n";
    if (!shp_file_exists(boost::filesystem::path(dir / ADMINBNDY_2_SHP))) std::cerr << "  administrative boundaries level 2 are missing\n";
    if (!shp_file_exists(boost::filesystem::path(dir / ADMINBNDY_3_SHP))) std::cerr << "  administrative boundaries level 3 are missing\n";
    if (!shp_file_exists(boost::filesystem::path(dir / ADMINBNDY_4_SHP))) std::cerr << "  administrative boundaries level 4 are missing\n";
    if (!shp_file_exists(boost::filesystem::path(dir / ADMINBNDY_5_SHP))) std::cerr << "  administrative boundaries level 5 are missing\n";

    if (!dbf_file_exists(boost::filesystem::path(dir / MTD_AREA_DBF ))) return false;
    if (!dbf_file_exists(boost::filesystem::path(dir / RDMS_DBF     ))) return false;
    if (!dbf_file_exists(boost::filesystem::path(dir / CDMS_DBF     ))) return false;
    if (!dbf_file_exists(boost::filesystem::path(dir / ZLEVELS_DBF  ))) return false;
    return true;
}

/**
 * \brief Checks wether there is a subdirectory containinig valid data.
 * \param dir directory from which to start recursion
 * \param recur if set non-directories within the root directory are ignored
 * \return Existance of valid data in a subdirectory.
 */

void navteq_plugin::recurse_dir(boost::filesystem::path dir) {
    if (check_files(dir)) sub_dirs.push_back(dir/*.filename()*/);

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
    if (sub_dirs.empty()) return false;

    std::cout << "sub_dirs: " << std::endl;
    for (auto& i : sub_dirs)
        std::cout << i.string() << std::endl;

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
    for (auto sub_dir : sub_dirs){
        process_meta_areas(read_dbf_file(sub_dir / MTD_AREA_DBF));
    }

    for (auto sub_dir : sub_dirs) {
        if (shp_file_exists(sub_dir / ADMINBNDY_1_SHP)) add_admin_shape(sub_dir / ADMINBNDY_1_SHP);
        if (shp_file_exists(sub_dir / ADMINBNDY_2_SHP)) add_admin_shape(sub_dir / ADMINBNDY_2_SHP);
        if (shp_file_exists(sub_dir / ADMINBNDY_3_SHP)) add_admin_shape(sub_dir / ADMINBNDY_3_SHP);
        if (shp_file_exists(sub_dir / ADMINBNDY_4_SHP)) add_admin_shape(sub_dir / ADMINBNDY_4_SHP);
        if (shp_file_exists(sub_dir / ADMINBNDY_5_SHP)) add_admin_shape(sub_dir / ADMINBNDY_5_SHP);
    }
    g_mtd_area_map.clear();
}

void navteq_plugin::execute() {

    add_street_shapes(sub_dirs);
    assert__id_uniqueness();

    add_turn_restrictions(sub_dirs);
    assert__id_uniqueness();

    add_administrative_boundaries();

    if (!output_path.empty()) write_output();

    std::cout << std::endl << "fin" << std::endl;
}
