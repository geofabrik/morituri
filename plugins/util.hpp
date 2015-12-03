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
#include <boost/filesystem/path.hpp>

#include "../plugins/comm2osm_exceptions.hpp"
#include "readers.hpp"

#include <geos/io/WKBReader.h>
#include <geos/io/WKBWriter.h>

#include <geos/geom/Point.h>
#include <geos/geom/LineString.h>
#include <geos/operation/buffer/OffsetCurveBuilder.h>
#include <geos/operation/buffer/BufferParameters.h>

const int INCH_BASE = 12;
const int POUND_BASE = 2000;
// short ton in metric tons (source: http://wiki.openstreetmap.org/wiki/Key:maxweight)
const double SHORT_TON = 0.90718474;

const geos::geom::PrecisionModel* npm;
const geos::geom::GeometryFactory* geos_factory;

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

bool shp_file_exists(boost::filesystem::path shp_file) {
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

bool dbf_file_exists(boost::filesystem::path dbf_file) {
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
    std::stringstream stream;
    stream << kilo/1000.0f;
    return stream.str();
}

template <class T>
std::string cm_to_m(T meter){
    std::stringstream stream;
    stream << meter/100.0f;
    return stream.str();
}

std::string inch_to_feet(unsigned int inches) {
    return std::to_string((unsigned int) floor(inches / INCH_BASE)) + "'" + std::to_string(inches % INCH_BASE) + "\"";
}

std::string lbs_to_metric_ton(double lbs){
    double short_ton = lbs / (double) POUND_BASE;
    double metric_ton = short_ton * SHORT_TON;
    std::stringstream stream;
    stream << metric_ton;
    return stream.str();
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
    ogr_geom->exportToWkb(wkbNDR, buffer);
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

geos::geom::Coordinate move_point(const geos::geom::Coordinate& moving_coord, const geos::geom::Coordinate& reference_coord,
        double move_distance) {

    double distance = moving_coord.distance(reference_coord);
    assert(move_distance < distance);

    // intercept theorem
    double ratio = move_distance / distance;
    auto move_x = ratio * (reference_coord.x - moving_coord.x);
    auto move_y = ratio * (reference_coord.y - moving_coord.y);

    return geos::geom::Coordinate (moving_coord.x + move_x, moving_coord.y + move_y);
}

void cut_front(double cut, geos::geom::CoordinateSequence* geos_cs) {
    double node_distance = std::abs(geos_cs->getAt(0).distance(geos_cs->getAt(1)));
    while (cut >= node_distance) {
        geos_cs->deleteAt(0);
        cut -= node_distance;
        node_distance = std::abs(geos_cs->getAt(0).distance(geos_cs->getAt(1)));
    }
    assert(cut >= 0);
    if (cut > 0) geos_cs->setAt(move_point(geos_cs->getAt(0), geos_cs->getAt(1), cut), 0);
}

void cut_back(double cut, geos::geom::CoordinateSequence* geos_cs) {
    auto len = geos_cs->getSize();
    assert(len >= 2);
    auto moving_coord = geos_cs->getAt(len - 1);
    auto reference_coord = geos_cs->getAt(len - 2);
    double node_distance = std::abs(moving_coord.distance(reference_coord));
    while (cut >= node_distance) {
        geos_cs->deleteAt(geos_cs->getSize() - 1);
        cut -= node_distance;
        len = geos_cs->getSize();
        moving_coord = geos_cs->getAt(len - 1);
        reference_coord = geos_cs->getAt(len - 2);
        node_distance = std::abs(moving_coord.distance(reference_coord));
    }
    assert(cut >= 0);
    if (cut > 0) geos_cs->setAt(move_point(moving_coord, reference_coord, cut), len - 1);
}

geos::geom::LineString* cut_caps(geos::geom::LineString* geos_ls) {
    geos::geom::CoordinateSequence* geos_cs = geos_ls->getCoordinates();

    double cut_ratio = 0.1;
    double max_cut = 0.00025;
    double cut = std::min(max_cut, geos_ls->getLength() * cut_ratio);

    assert(cut < geos_ls->getLength() / 2);

    cut_front(cut, geos_cs);
    cut_back(cut, geos_cs);

    return geos_factory->createLineString(geos_cs);
}

OGRLineString* create_offset_curve(OGRLineString* ogr_ls, double offset, bool left) {
    if (!npm) npm = new geos::geom::PrecisionModel();
    if (!geos_factory) geos_factory = new geos::geom::GeometryFactory(npm);

    geos::operation::buffer::OffsetCurveBuilder* offset_curve_builder;
    offset_curve_builder = new geos::operation::buffer::OffsetCurveBuilder(npm,
            geos::operation::buffer::BufferParameters());

    std::vector<geos::geom::CoordinateSequence*> cs_vec;
    offset_curve_builder->getSingleSidedLineCurve(ogr2geos(ogr_ls)->getCoordinates(), offset, cs_vec, left, !left);
    assert(cs_vec.size() == 1);

    auto cs = cs_vec.at(0);

    // getSingleSidedLineCurve() always produces a ring (bug?). therefore: first_coord == last_coord => drop last_coord
    if (cs->getAt(0) == cs->getAt(cs->getSize() - 1)) cs->deleteAt(cs->size() - 1);

    geos::geom::LineString* offset_geos_ls = geos_factory->createLineString(cs);
    offset_geos_ls = cut_caps(offset_geos_ls);
    OGRGeometry* offset_ogr_geom = geos2ogr(offset_geos_ls);
    delete offset_geos_ls;

    return static_cast<OGRLineString*>(offset_ogr_geom);
}

#endif /* UTIL_HPP_ */
