/*
 * navteq_plugin.hpp
 *
 *  Created on: 11.06.2015
 *      Author: philip
 */

#ifndef NAVTEQPLUGIN_HPP_
#define NAVTEQPLUGIN_HPP_

#include "../base_plugin.hpp"
#include <boost/filesystem/path.hpp>
#include <string>

class navteq_plugin: public base_plugin {
private:
    bool is_valid_format(std::string format);
    void recurse_dir(boost::filesystem::path dir, bool recur = true);
    bool check_files(boost::filesystem::path dir);
    void write_output();

    std::vector<boost::filesystem::path> sub_dirs;

public:

    navteq_plugin();
    virtual ~navteq_plugin();

    bool check_input(boost::filesystem::path input_path, boost::filesystem::path output_path =
            boost::filesystem::path());
    void execute();

};

#endif /* NAVTEQPLUGIN_HPP_ */
