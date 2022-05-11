/*
 * readers.hpp
 *
 *  Created on: 17.06.2015
 *      Author: philip
 */

#ifndef PLUGINS_READERS_HPP_
#define PLUGINS_READERS_HPP_

#include <iostream>
#include <gdal/ogrsf_frmts.h>
#include <gdal/gdal_priv.h>
#include <boost/filesystem/path.hpp>

#include "ogr_types.hpp"
#include "comm2osm_exceptions.hpp"

/*
 * \brief Checks shapefile existance and validity.
 *
 * \param shp_file path and file name of shapefile.
 *
 * \return Pointer to first layer in Shapefile and the associated dataset.
 * */

gdal_dataset_layer read_shape_file(boost::filesystem::path shp_file, std::ostream& out = std::cerr) {
    RegisterOGRShape();
    out << "reading " << shp_file << std::endl;

    GDALDataset* input_data_source {static_cast<GDALDataset*>(GDALOpenEx(shp_file.c_str(),
                GDAL_OF_VECTOR | GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR, NULL, NULL, NULL))};

    if (input_data_source == NULL) throw(shp_error(shp_file.string()));

    OGRLayer* input_layer {input_data_source->GetLayer(0)};
    if (input_layer == NULL) throw(shp_empty_error(shp_file.string()));

    std::cerr << "GetDriverName: " << input_data_source->GetDriverName() << '\n';
    std::cerr << "GetGeomType: " << input_layer->GetGeomType() << '\n';

    return gdal_dataset_layer{input_data_source, input_layer};
}

DBFHandle read_dbf_file(boost::filesystem::path dbf_file, std::ostream& out = std::cerr) {
    out << "reading " << dbf_file << std::endl;
    DBFHandle handle = DBFOpen(dbf_file.c_str(), "rb");
    if (handle == NULL) throw(dbf_error(dbf_file.string()));
    return handle;
}

#endif /* PLUGINS_READERS_HPP_ */
