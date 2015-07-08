/*
 * base_plugin.h
 *
 *  Created on: 11.06.2015
 *      Author: philip
 */

#ifndef BASEPLUGIN_HPP_
#define BASEPLUGIN_HPP_

#include <string>
#include <assert.h>

class base_plugin {
public:
    const char* name;
    const char* input_path;
    const char* output_path;

    virtual ~base_plugin() {
    }
    ;

    const char* get_name() {
        return name;
    }
    ;

    /*
     * \brief	Sets input_path and output_path.
     *
     * 			Sets the variables input_path and output_path in BasePlugin
     *
     * \param	inputh_path_rhs	input path to set
     * \param	output_path_rhs	output path to set (may be ommited)
     * */
    void plugin_setup(const char* input_path_rhs, const char* output_path_rhs = nullptr) {
        input_path = input_path_rhs;
        assert(input_path);
        if (output_path_rhs) {
            output_path = output_path_rhs;
        }
    }

    /**
     * \brief	Checks validity of input.
     *
     * 			This function is pure virtual and has to be implemented by every plugin.
     * 			It should assure that all necessary files are existing and valid.
     *
     * \param    input_path    path in which the input_files are
     * \param   output_path    path, filename and suffix (e.g. /path/to/file.osm)
     *                         to which the result will be written.
     *                         All suffixes supported by libosmium may be used.
     *                         Currently supported formats are:
     * 	                       		.osm (XML),
     *		                        .pbf (PBF),
     *      	                    .opl (OPL),
     *           	        	    .gz (gzip),
     *           			        .bz2 (bzip2).
     *                         If ommited a standard value can be applied.
     *
     * \return returns true if input is existing and valid
     *  */
    virtual bool check_input(const char* input_path, const char* output_path = nullptr) = 0;

    /**
     * \brief	Converts input to OSM files
     *
     *			This function is pure virtual and has to be implemented by every plugin.
     * 			It does the actual conversion to the OSM-format.
     * */
    virtual void execute() = 0;
};

#endif /* BASEPLUGIN_HPP_ */
