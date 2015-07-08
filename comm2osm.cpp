/*

 Convert Shapefiles into OSM files.

 */

#include <getopt.h>
#include <iostream>
#include <vector>
#include <gdal/ogrsf_frmts.h>

#include "plugins/base_plugin.hpp"
#include "plugins/navteq/navteq_plugin.hpp"
#include "plugins/dummy/dummy_plugin.hpp"

std::string input_path, output_path;

void print_help() {
    std::cout << "comm2osm [OPTIONS] [INFILE [OUTFILE]]\n\n"
            << "If INFILE or OUTFILE is not given stdin/stdout is assumed.\n"
            << "File format is autodetected from file name suffix.\n";
//			<< "Use -f and -t options to force file format.\n"
//			<< "\nFile types:\n" << "  osm        normal OSM file\n"
//			<< "  osc        OSM change file\n"
//			<< "  osh        OSM file with history information\n"
//			<< "\nFile format:\n" << "  (default)  XML encoding\n"
//			<< "  pbf        binary PBF encoding\n"
//			<< "  opl        OPL encoding\n" << "\nFile compression\n"
//			<< "  gz         compressed with gzip\n"
//			<< "  bz2        compressed with bzip2\n"
//			<< "\nOptions:\n"
//			<< "  -h, --help                This help message\n"
//			<< "  -f, --from-format=FORMAT  Input format\n"
//			<< "  -t, --to-format=FORMAT    Output format\n";
}

void check_args_and_setup(int argc, char* argv[]) {
    // options
    static struct option long_options[] = { { "help", no_argument, 0, 'h' }, { 0, 0 } };

    while (true) {
        int c = getopt_long(argc, argv, "dhf:t:", long_options, 0);
        if (c == -1) {
            break;
        }

        switch (c) {
            case 'h':
                print_help();
                exit(0);
            default:
                exit(1);
        }
    }

    int remaining_args = argc - optind;
    if (remaining_args < 2) {
        std::cerr << "Usage: " << argv[0] << " [OPTIONS] [INFILE [OUTFILE]]" << std::endl;
        exit(1);
    } else if (remaining_args == 2) {
        input_path = argv[optind];
        output_path = argv[optind + 1];
    } else if (remaining_args == 1) {
        input_path = argv[optind];
    }
}

int main(int argc, char* argv[]) {

    check_args_and_setup(argc, argv);

    std::vector<base_plugin*> plugins;

    plugins.push_back(new dummy_plugin());
    plugins.push_back(new navteq_plugin());

    for (auto plugin : plugins) {
        if (plugin->check_input(input_path.c_str(), output_path.c_str())) {
            std::cout << "executing plugin " << plugin->get_name() << std::endl;
            plugin->execute();
        }
    }
}
