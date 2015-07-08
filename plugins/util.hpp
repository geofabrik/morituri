/*
 * util.hpp
 *
 *  Created on: 17.06.2015
 *      Author: philip
 */

#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <gdal/ogr_api.h>
#include <gdal/ogrsf_frmts.h>
#include "readers.hpp"

/**

 * helpers to check for file existance and validity
 * */

/**
 * \brief Checks shapefile existance and validity
 *
 * \param shp_file path and file name of shapefile
 *
 * \return Returns true if existing and valid
 * */

bool shp_file_exists(const char* shp_file) {

    RegisterOGRShape();
    OGRDataSource *shapedriver = OGRSFDriverRegistrar::Open(shp_file, FALSE);
    OGRCleanupAll();
    if (shapedriver == NULL) {
//		std::cerr << "Open of " << shp_file << " failed" << std::endl;
        return false;
    }
    return true;
}

bool shp_file_exists(std::string shp_file) {
    return shp_file_exists(shp_file.c_str());
}

/**

 * \brief Checks DBF file existance and validity
 *
 * \param dbf_file path and file name of DBF file
 *
 * \return Returns true if existing and valid
 * */
bool dbf_file_exists(const char* dbf_file) {
    DBFHandle handle = DBFOpen(dbf_file, "rb");
    if (handle == NULL) {
        std::cerr << "Open of " << dbf_file << " failed" << std::endl;
        return false;
    }
    return true;
}

bool dbf_file_exists(std::string dbf_file) {
    return dbf_file_exists(dbf_file.c_str());
}

int dbf_get_field_index(DBFHandle handle, int row, const char *field_name) {
    assert(handle);
    assert(field_name);
    assert(row < DBFGetRecordCount(handle));
    // if (row >= DBFGetRecordCount(handle)) throw(std::runtime_error("row=" + std::to_string(row) + " is out of bounds."));
    int index = DBFGetFieldIndex(handle, field_name);
    if (index == -1) throw(std::runtime_error("DBFfile doesnt contain " + std::string(field_name)));
    return index;
}

std::string dbf_get_string_by_field(DBFHandle handle, int row, const char *field_name) {
    return DBFReadStringAttribute(handle, row, dbf_get_field_index(handle, row, field_name));
}

uint64_t dbf_get_int_by_field(DBFHandle handle, int row, const char *field_name) {
    return DBFReadIntegerAttribute(handle, row, dbf_get_field_index(handle, row, field_name));
}

/* getting fields from OGRFeatures -- begin */

/**
 * \brief returns field from OGRFeature
 *        aborts if feature is nullpointer or field key is invalid
 * \param feat feature from which field is read
 * \param field field name as key
 * \return const char* of field value
 */
const char* get_field_from_feature(OGRFeature* feat, const char* field) {
    assert(feat);
    int field_index = feat->GetFieldIndex(field);
    assert(field_index != -1);
    return feat->GetFieldAsString(field_index);
}

/**
 * \brief returns field from OGRFeature
 *        throws exception if field_value is not
 * \param feat feature from which field is read
 * \param field field name as key
 * \return field value as uint
 */
uint64_t get_uint_from_feature(OGRFeature* feat, const char* field) {
    const char* value = get_field_from_feature(feat, field);
    assert(value);
    try {
        return std::stoul(value);
    } catch (const std::invalid_argument &) {
        throw format_error(
                "Could not parse field='" + std::string(field) + "' with value='" + std::string(value) + "'");
    }
}

/* getting fields from OGRFeatures -- end */

template<class Type>
bool is_in_range(Type test, Type from, Type to) {
    if (test < from || test > to) return false;
    return true;
}

bool string_is_integer(std::string s) {
    for (auto i : s)
        if (!isdigit(i)) return false;
    return true;
}

bool string_is_not_integer(std::string s) {
    return !string_is_integer(s);
}

/* unused */

std::string to_lower(std::string s) {
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

template<class Type1, class Type2>
void print_map(std::map<Type1, Type2> map) {
    for (auto i : map)
        std::cout << i.first << " - " << i.second << std::endl;
}

template<class Type1, class Type2>
void init_map_at_element(std::map<Type1, Type2> *map, Type1 element, osmium::unsigned_object_id_type id) {
    if (map->find(element) == map->end()) map->insert(std::make_pair(element, id));
}

#endif /* UTIL_HPP_ */
