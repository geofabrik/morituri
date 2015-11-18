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
#include <shapefil.h>
#include <osmium/osm/types.hpp>
#include <sstream>
#include "boost/iostreams/stream.hpp"
#include "boost/iostreams/device/null.hpp"

#include "../plugins/comm2osm_exceptions.hpp"
#include "readers.hpp"

#include <geos/io/WKBReader.h>
#include <geos/io/WKBWriter.h>

#include <geos/geom/LineString.h>
#include <geos/operation/buffer/BufferParameters.h>

const int INCH_BASE = 12;
const int POUND_BASE = 2000;
// short ton in metric tons (source: http://wiki.openstreetmap.org/wiki/Key:maxweight)
const double SHORT_TON = 0.90718474;


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
    if (shapedriver == NULL) {
        OGRCleanupAll();
        return false;
    }
    OGRCleanupAll();
    delete shapedriver;
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
        return false;
    }
    DBFClose(handle);
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

uint64_t dbf_get_uint_by_field(DBFHandle handle, int row, const char *field_name) {
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
    if (field_index == -1) std::cerr << field << std::endl;
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

bool string_is_unsigned_integer(std::string s) {
    if (s.empty()) return false;
    for (auto i : s)
        if (!isdigit(i)) return false;
    return true;
}

bool string_is_not_unsigned_integer(std::string s) {
    return !string_is_unsigned_integer(s);
}

template <class T>
std::string kg_to_t(T kilo){
    return std::to_string(kilo/1000.0f);
}

template <class T>
std::string cm_to_m(T meter){
    return std::to_string(meter/100.0f);
}

std::string inch_to_feet(unsigned int inches) {
    return std::to_string((unsigned int) floor(inches / INCH_BASE)) + "'" + std::to_string(inches % INCH_BASE) + "\"";
}

std::string lbs_to_metric_ton(unsigned int lbs){
    float short_ton = lbs / (float) POUND_BASE;
    float metric_ton = short_ton * SHORT_TON;
    return std::to_string(metric_ton);
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
void init_map_at_element(std::map<Type1, Type2> *map, Type1 key, osmium::unsigned_object_id_type id) {
    if (map->find(key) == map->end()) map->insert(std::make_pair(key, id));
}

boost::iostreams::stream<boost::iostreams::null_sink> cnull((boost::iostreams::null_sink()));

/**
 * Following functions convert OGRGeometry to geos::geom::Geometry and vice versa
 */

std::string ogr2wkb(OGRGeometry *ogr_geom) {
    if (!ogr_geom || ogr_geom->IsEmpty()) throw std::runtime_error("geometry is nullptr");
    unsigned char staticbuffer[1024 * 1024];
    unsigned char *buffer = staticbuffer;
    size_t wkb_size = ogr_geom->WkbSize();
    if (wkb_size > sizeof(staticbuffer)) buffer = (unsigned char *) malloc(wkb_size);
    ogr_geom->exportToWkb(wkbXDR, buffer);
    std::string wkb((const char*) buffer, wkb_size);
    if (buffer != staticbuffer) free(buffer);
    return wkb;
}

geos::io::WKBReader* wkb_reader;
geos::geom::Geometry* wkb2geos(std::string wkb) {
    if (!wkb_reader) wkb_reader = new geos::io::WKBReader();
    std::istringstream ss(wkb);
    geos::geom::Geometry *geos_geom = wkb_reader->read(ss);
    if (!geos_geom) throw std::runtime_error("creating geos::geom::Geometry from wkb failed");
    return geos_geom;
}

geos::geom::Geometry* ogr2geos(OGRGeometry* ogr_geom){
    if (!ogr_geom || ogr_geom->IsEmpty()) throw std::runtime_error("geometry is nullptr");
    return wkb2geos(ogr2wkb(ogr_geom));
}

geos::io::WKBWriter* wkb_writer;
std::string geos2wkb(const geos::geom::Geometry *geos_geom) {
    if (!wkb_writer) wkb_writer = new geos::io::WKBWriter();
    std::ostringstream ss;
    wkb_writer->setOutputDimension(geos_geom->getCoordinateDimension());
    wkb_writer->write(*geos_geom, ss);
    return ss.str();
}

OGRGeometry* wkb2ogr(std::string wkb) {
    OGRGeometry *ogr_geom;
    OGRErr res = OGRGeometryFactory::createFromWkb((unsigned char*) (wkb.c_str()), nullptr, &ogr_geom, wkb.size());
    if (res != OGRERR_NONE) throw std::runtime_error("creating OGRGeometry from wkb failed: " + std::to_string(res));
    return ogr_geom;
}

OGRGeometry* geos2ogr(const geos::geom::Geometry *geos_geom){
    return wkb2ogr(geos2wkb(geos_geom));
}
#endif /* UTIL_HPP_ */
