/*
 * DummyPlugin.h
 *
 *  Created on: 11.06.2015
 *      Author: philip
 */

#ifndef DUMMYPLUGIN_HPP_
#define DUMMYPLUGIN_HPP_

#include "../base_plugin.hpp"

class dummy_plugin: public base_plugin {
public:
    dummy_plugin();
    virtual ~dummy_plugin();

    bool check_input(const char* input_path, const char* output_path = nullptr);
    void execute();
};

#endif /* DUMMYPLUGIN_HPP_ */
