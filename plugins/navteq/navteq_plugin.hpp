/*
 * navteq_plugin.hpp
 *
 *  Created on: 11.06.2015
 *      Author: philip
 */

#ifndef NAVTEQPLUGIN_HPP_
#define NAVTEQPLUGIN_HPP_

#include "../base_plugin.hpp"

class navteq_plugin: public base_plugin {
private:
    bool is_valid_format(std::string format);

public:

    navteq_plugin();
    virtual ~navteq_plugin();

    bool check_input(const char* input_path, const char* output_path = nullptr);
    void execute();

};

#endif /* NAVTEQPLUGIN_HPP_ */
