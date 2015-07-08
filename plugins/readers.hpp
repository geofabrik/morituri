/*
 * readers.hpp
 *
 *  Created on: 17.06.2015
 *      Author: philip
 */

#ifndef PLUGINS_READERS_HPP_
#define PLUGINS_READERS_HPP_

#include <gdal/ogrsf_frmts.h>

/*
 * \brief Checks shapefile existance and validity.
 *
 * \param shp_file path and file name of shapefile.
 *
 * \return Pointer to first layer in Shapefile.
 * */

OGRLayer* read_shape_file(const char* shp_file) {
    OGRDataSource *input_data_source;
//	std::cout << "read shapefile: " << shp_file << std::endl;

    input_data_source = OGRSFDriverRegistrar::Open(shp_file, FALSE);
    if (input_data_source == NULL) throw(shp_error(shp_file));

    OGRLayer *input_layer = input_data_source->GetLayer(0);
    if (input_layer == NULL) throw(shp_empty_error(shp_file));

    return input_layer;
}

OGRLayer* read_shape_file(std::string shp_file) {
    return read_shape_file(shp_file.c_str());
}

DBFHandle read_dbf_file(std::string dbf_file) {
//	std::cout << "reading " << dbf_file << std::endl;
    DBFHandle handle = DBFOpen(dbf_file.c_str(), "rb");
    if (handle == NULL) throw(dbf_error(dbf_file));
    return handle;
}

#endif /* PLUGINS_READERS_HPP_ */
