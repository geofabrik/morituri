/*
 * DummyPlugin.cpp
 *
 *  Created on: 11.06.2015
 *      Author: philip
 */

#include "dummy_plugin.hpp"
#include <iostream>

dummy_plugin::dummy_plugin() {
    name = "Dummy Plugin";
}

dummy_plugin::~dummy_plugin() {
}

bool dummy_plugin::check_input(const char* input_path, const char* output_path) {
    return true;
}

void dummy_plugin::execute() {
    std::cout << "dummy has nothing to execute" << std::endl;
}
