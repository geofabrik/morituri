/*
 * comm2osm_exceptions.hpp
 *
 *  Created on: 16.06.2015
 *      Author: philip
 */

#ifndef EXCEPTIONS_HPP_
#define EXCEPTIONS_HPP_

#include <osmium/io/error.hpp>

/**
 * Exception thrown when there was a problem with parsing the SHP format of
 * a file.
 */
struct shp_error: public osmium::io_error {

    shp_error(const std::string& what) :
            osmium::io_error(std::string("Could not read Shapefile: ") + what) {
    }

    shp_error(const char* what) :
            osmium::io_error(std::string("Could not read Shapefile: ") + what) {
    }

};
// struct shp_error

/**
 * Exception thrown when the parsed shapefile was emtpy
 */
struct shp_empty_error: public osmium::io_error {

    shp_empty_error(const std::string& what) :
            osmium::io_error(std::string("Shapefile is empty: ") + what) {
    }

    shp_empty_error(const char* what) :
            osmium::io_error(std::string("Shapefile is empty: ") + what) {
    }

};
// struct shp_empty_error

/**
 * Exception thrown when there was a problem with parsing the DBF format of
 * a file.
 */
struct dbf_error: public osmium::io_error {

    dbf_error(const std::string& what) :
            osmium::io_error(std::string("Could not read DBF-file: ") + what) {
    }

    dbf_error(const char* what) :
            osmium::io_error(std::string("Could not read DBF-file: ") + what) {
    }

};

struct format_error: public std::runtime_error {
    format_error(const std::string& what) :
            std::runtime_error(std::string("Could not read format: ") + what) {
    }

    format_error(const char* what) :
            std::runtime_error(std::string("Could not read format: ") + what) {
    }
};

struct out_of_range_exception: public std::runtime_error {
    out_of_range_exception(const std::string& what) :
            std::runtime_error(std::string("Out_of_range: ") + what) {
    }

    out_of_range_exception(const char* what) :
            std::runtime_error(std::string("Out_of_range: ") + what) {
    }
};

// struct dbf_error

#endif /* EXCEPTIONS_HPP_ */
