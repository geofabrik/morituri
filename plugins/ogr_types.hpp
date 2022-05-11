/*
 * types.hpp
 *
 *  Created on: 11.12.2015
 *      Author: philip
 */

#ifndef PLUGINS_OGR_TYPES_HPP_
#define PLUGINS_OGR_TYPES_HPP_

#include <iostream>
#include <bits/unique_ptr.h>
#include <gdal/ogrsf_frmts.h>

struct OGRFeatureDeleter {
    void operator()(OGRFeature* feature) {
        OGRFeature::DestroyFeature(feature);
    }
};

typedef std::unique_ptr<OGRFeature, OGRFeatureDeleter> ogr_feature_uptr;
typedef std::unique_ptr<OGRLayer> ogr_layer_uptr;
typedef std::unique_ptr<GDALDataset> gdal_dataset_uptr;
struct gdal_dataset_layer {
    GDALDataset* dataset;
    OGRLayer* layer;

    gdal_dataset_layer() = delete;

    explicit gdal_dataset_layer(GDALDataset* ds, OGRLayer* lyr) :
        dataset(ds), layer(lyr) {
    }

    gdal_dataset_layer(const gdal_dataset_layer& obj) :
        dataset(obj.dataset), layer(obj.layer) {
    }

    void release() {
        delete layer;
        if (dataset) {
            delete dataset;
        }
    }
};
typedef std::vector<gdal_dataset_layer> ogr_layer_uptr_vector;


typedef std::unique_ptr<OGRGeometry> ogr_geometry_uptr;
typedef std::unique_ptr<OGRLineString> ogr_line_string_uptr;
typedef std::unique_ptr<OGRPolygon> ogr_polygon_uptr;
typedef std::unique_ptr<OGRMultiPolygon> ogr_multi_polygon_uptr;

#endif /* PLUGINS_OGR_TYPES_HPP_ */
