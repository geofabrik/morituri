/*
 * ogr_util.hpp
 *
 *  Created on: 11.12.2015
 *      Author: philip
 */

#ifndef PLUGINS_OGR_UTIL_HPP_
#define PLUGINS_OGR_UTIL_HPP_

#include <assert.h>
#include <boost/iostreams/stream.hpp>

#include <shapefil.h>
#include <geos/io/WKBReader.h>
#include <geos/io/WKBWriter.h>

#include <geos/geom/Point.h>
#include <geos/geom/LineString.h>
#include <geos/operation/buffer/OffsetCurveBuilder.h>
#include <geos/operation/buffer/BufferParameters.h>

#include "ogr_types.hpp"


const geos::geom::PrecisionModel* npm;
const geos::geom::GeometryFactory* geos_factory;


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



#endif /* PLUGINS_OGR_UTIL_HPP_ */
