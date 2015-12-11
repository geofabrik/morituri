/*
 * navteq_util.hpp
 *
 *  Created on: 11.12.2015
 *      Author: philip
 */

#ifndef PLUGINS_NAVTEQ_NAVTEQ_UTIL_HPP_
#define PLUGINS_NAVTEQ_NAVTEQ_UTIL_HPP_

#include <assert.h>
#include "../util.hpp"
#include "navteq_types.hpp"

/**
 * \brief returns field from OGRFeature
 *        aborts if feature is nullpointer or field key is invalid
 * \param feat feature from which field is read
 * \param field field name as key
 * \return const char* of field value
 */
const char* get_field_from_feature(ogr_feature_uptr& feat, const char* field) {
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
uint64_t get_uint_from_feature(ogr_feature_uptr& feat, const char* field) {
    const char* value = get_field_from_feature(feat, field);
    assert(value);
    try {
        return std::stoul(value);
    } catch (const std::invalid_argument &) {
        throw format_error(
                "Could not parse field='" + std::string(field) + "' with value='" + std::string(value) + "'");
    }
}



#endif /* PLUGINS_NAVTEQ_NAVTEQ_UTIL_HPP_ */
