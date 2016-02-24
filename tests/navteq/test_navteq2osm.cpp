#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "../../plugins/navteq/navteq.hpp"
#include "../../plugins/navteq/navteq_plugin.hpp"

static std::string test_dir = "tests/navteq/";
static std::string tmp_dir = ".tmp_navteq/";

TEST_CASE("Parse oneway", "[oneway]") {
    CHECK(parse_one_way_tag("F") == "yes");
    CHECK(parse_one_way_tag("T") == "-1");
    CHECK(parse_one_way_tag("B") == nullptr);
}

void system_s(std::string s) {
    system(s.c_str());
}

void create_testdata(std::string testdata) {
    system_s("mkdir -p " + tmp_dir);

    std::string test_geojson = tmp_dir + "test.geojson";

    system_s("python " + test_dir + "create_zlvl_geojson.py " + testdata + " > " + test_geojson);
    system_s("ogr2ogr -overwrite " + tmp_dir + "Zlevels.shp " + test_geojson);

    system_s("python " + test_dir + "create_street_geojson.py " + testdata + " > " + test_geojson);
    system_s("ogr2ogr -overwrite " + tmp_dir + "Streets.shp " + test_geojson);

    system_s("python " + test_dir + "create_mtd_area_geojson.py > " + test_geojson);
    system_s("ogr2ogr -overwrite " + tmp_dir + "MtdArea.shp " + test_geojson);

    system_s("rm -f " + test_geojson);
}

struct test_struct {
    std::string z_lvls; // = "0 4 4 0 0 0 5 5 0 0";
    std::vector<int> supposed_way_z_lvls; // {4,0,5,0};
    test_struct(std::string s, std::vector<int> v) {
        z_lvls = s;
        supposed_way_z_lvls = v;
    }
};

TEST_CASE("split subway", "[split subway]") {
    OGRRegisterAll();

    std::vector<test_struct> tests;
    tests.push_back(test_struct("0 1", {1}));
    tests.push_back(test_struct("1 0", {1}));
    tests.push_back(test_struct("1 1", {1}));

    tests.push_back(test_struct("0 0 1", {0, 1}));
    tests.push_back(test_struct("0 1 0", {1}));
    tests.push_back(test_struct("0 1 1", {1}));
    tests.push_back(test_struct("1 0 0", {1, 0}));
    tests.push_back(test_struct("1 0 1", {1}));
    tests.push_back(test_struct("1 1 0", {1}));
    tests.push_back(test_struct("1 1 1", {1}));

    tests.push_back(test_struct("-1 0 -1", {-1}));
    tests.push_back(test_struct("-1 1 -1", {-1}));
    tests.push_back(test_struct("1 -1 1", {1}));
    tests.push_back(test_struct("0 -1 0", {-1}));

    tests.push_back(test_struct("2 1 2", {2}));
    tests.push_back(test_struct("-2 -1 -2", {-2}));
    tests.push_back(test_struct("-2 -1 -3", {-2, -3}));

    tests.push_back(test_struct("0 0 0 1", {0, 1}));
    tests.push_back(test_struct("0 0 1 0", {0, 1}));
    tests.push_back(test_struct("0 0 1 1", {0, 1}));
    tests.push_back(test_struct("0 1 0 0", {1, 0}));
    tests.push_back(test_struct("0 1 0 1", {1}));
    tests.push_back(test_struct("0 1 1 0", {1}));
    tests.push_back(test_struct("0 1 1 1", {1}));
    tests.push_back(test_struct("1 0 0 0", {1, 0}));
    tests.push_back(test_struct("1 0 0 1", {1, 0, 1}));
    tests.push_back(test_struct("1 0 1 0", {1}));
    tests.push_back(test_struct("1 0 1 1", {1}));
    tests.push_back(test_struct("1 1 0 0", {1, 0}));
    tests.push_back(test_struct("1 1 0 1", {1}));
    tests.push_back(test_struct("1 1 1 0", {1}));
    tests.push_back(test_struct("1 1 1 1", {1}));

    tests.push_back(test_struct("1 2 2 1", {2}));
    tests.push_back(test_struct("2 1 1 2", {2, 1, 2}));

    tests.push_back(test_struct("1 0 0 0 1", {1, 0, 1}));
    tests.push_back(test_struct("2 1 0 1 2", {2, 1, 2}));

    tests.push_back(test_struct("2 1 0 0 1 2", {2, 1, 0, 1, 2}));

    tests.push_back(test_struct("0 1 1 0 1 1 0", {1}));

    tests.push_back(test_struct("0 1 0 1 0 1 0 1", {1}));
    tests.push_back(test_struct("0 1 1 0 0 1 0 0", {1, 0, 1, 0}));
    tests.push_back(test_struct("1 0 1 0 1 0 1 0", {1}));

    tests.push_back(test_struct("0 1 0 1 0 0 1 0 1", {1, 0, 1}));

    tests.push_back(test_struct("0 1 1 0 1 1 0 1 1 0", {1}));
    tests.push_back(test_struct("0 4 4 0 0 0 5 5 0 0", {4, 0, 5, 0}));

    for (auto test : tests) {
        std::cout << "testing mapping: node z-levels --> way z-levels [" << test.z_lvls << "] --> ";
        for (auto j : test.supposed_way_z_lvls)
        std::cout << j << " ";
        std::cout << std::endl;

        create_testdata(test.z_lvls.c_str());

        process_meta_areas(tmp_dir, true);
        add_street_shapes(tmp_dir, true);

        std::string osm_z_lvl_string;
        std::string supposed_z_lvl;
        for (auto i : test.supposed_way_z_lvls)
        supposed_z_lvl.append(std::to_string(i) + " ");

        int ctr = 0;
        for (auto& it : g_way_buffer) {
            osmium::OSMObject* obj = static_cast<osmium::OSMObject*>(&it);
            if (obj->type() == osmium::item_type::way) {
                CAPTURE(test.z_lvls);

                const char* osm_z_lvl = obj->get_value_by_key("layer");
                if (!osm_z_lvl) osm_z_lvl = "0";
                CAPTURE(osm_z_lvl);
                CAPTURE(std::stoi(osm_z_lvl));
                CAPTURE(test.supposed_way_z_lvls.at(ctr));
                CAPTURE(ctr);
                CAPTURE(test.supposed_way_z_lvls.size());

                CHECK(std::stoi(osm_z_lvl) == test.supposed_way_z_lvls.at(ctr));
                osm_z_lvl_string.append(osm_z_lvl);
                osm_z_lvl_string.append(" ");
                ctr++;
            }
        }

        CAPTURE(test.z_lvls);
        CAPTURE(supposed_z_lvl);

        CAPTURE(osm_z_lvl_string);
        CAPTURE(ctr);
        CAPTURE(test.supposed_way_z_lvls.size());

        CHECK(ctr == test.supposed_way_z_lvls.size());
        clear_all();
        system_s("rm -rf " + tmp_dir);

    }
}
