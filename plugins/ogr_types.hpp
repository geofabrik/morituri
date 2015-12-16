/*
 * types.hpp
 *
 *  Created on: 11.12.2015
 *      Author: philip
 */

#ifndef PLUGINS_OGR_TYPES_HPP_
#define PLUGINS_OGR_TYPES_HPP_

#include <bits/unique_ptr.h>
#include <gdal/ogrsf_frmts.h>

typedef std::unique_ptr<OGRFeature> ogr_feature_uptr;
typedef std::unique_ptr<OGRLayer> ogr_layer_uptr;
typedef std::vector<ogr_layer_uptr> ogr_layer_uptr_vector;


typedef std::unique_ptr<OGRGeometry> ogr_geometry_uptr;
typedef std::unique_ptr<OGRLineString> ogr_line_string_uptr;
typedef std::unique_ptr<OGRPolygon> ogr_polygon_uptr;
typedef std::unique_ptr<OGRMultiPolygon> ogr_multi_polygon_uptr;

#endif /* PLUGINS_OGR_TYPES_HPP_ */
