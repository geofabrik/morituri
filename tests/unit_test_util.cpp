#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../plugins/util.hpp"

TEST_CASE("Shapefile exists", "[SHP exist]"){
    CHECK(shp_file_exists("tests/testdata/faroe-islands-latest/roads.shp") == true);
    CHECK(shp_file_exists("tests/testdata/faroe-islands-latest/README") == false);
    CHECK(shp_file_exists("tests/testdata/faroe-islands-latest/???") == false);

    CHECK(shp_file_exists(std::string("tests/testdata/faroe-islands-latest/roads.shp")) == true);
    CHECK(shp_file_exists(std::string("tests/testdata/faroe-islands-latest/README")) == false);
    CHECK(shp_file_exists(std::string("tests/testdata/faroe-islands-latest/???")) == false);

    CHECK(shp_file_exists(boost::filesystem::path("tests/testdata/faroe-islands-latest/roads.shp")) == true);
    CHECK(shp_file_exists(boost::filesystem::path("tests/testdata/faroe-islands-latest/README")) == false);
    CHECK(shp_file_exists(boost::filesystem::path("tests/testdata/faroe-islands-latest/???")) == false);
}

TEST_CASE("DBF exists", "[DBF exist]"){
    CHECK(dbf_file_exists("tests/testdata/faroe-islands-latest/roads.dbf") == true);
    CHECK(dbf_file_exists("tests/testdata/faroe-islands-latest/README") == false);
    CHECK(dbf_file_exists("tests/testdata/faroe-islands-latest/???") == false);

    CHECK(dbf_file_exists(std::string("tests/testdata/faroe-islands-latest/roads.dbf")) == true);
    CHECK(dbf_file_exists(std::string("tests/testdata/faroe-islands-latest/README")) == false);
    CHECK(dbf_file_exists(std::string("tests/testdata/faroe-islands-latest/???")) == false);

    CHECK(dbf_file_exists(boost::filesystem::path("tests/testdata/faroe-islands-latest/roads.dbf")) == true);
    CHECK(dbf_file_exists(boost::filesystem::path("tests/testdata/faroe-islands-latest/README")) == false);
    CHECK(dbf_file_exists(boost::filesystem::path("tests/testdata/faroe-islands-latest/???")) == false);
}

TEST_CASE("is_in_range", "[is_in_range]"){
    CHECK(is_in_range(2, 1, 3) == true);
    CHECK(is_in_range(1, 1, 3) == true);
    CHECK(is_in_range(3, 1, 3) == true);
    CHECK(is_in_range(9, 1, 9) == true);

    CHECK(is_in_range(4, 1, 3) == false);
    CHECK(is_in_range(0, 1, 3) == false);
    CHECK(is_in_range(-1, 1, 3) == false);
}

TEST_CASE("string_is_integer", "[string_is_int]"){
    CHECK(string_is_unsigned_integer(std::string("0")));
    CHECK(string_is_unsigned_integer(std::string("1")));
    CHECK(string_is_unsigned_integer(std::string("10")));
    CHECK(string_is_unsigned_integer(std::string("100")));
    CHECK(string_is_unsigned_integer(std::string("1000000000000")));

    CHECK_FALSE(string_is_unsigned_integer(std::string("")));
    CHECK_FALSE(string_is_unsigned_integer(std::string("a")));
    CHECK_FALSE(string_is_unsigned_integer(std::string("-1")));
    CHECK_FALSE(string_is_unsigned_integer(std::string("-10")));
    CHECK_FALSE(string_is_unsigned_integer(std::string("1y")));
    CHECK_FALSE(string_is_unsigned_integer(std::string("y3")));
    CHECK_FALSE(string_is_unsigned_integer(std::string("1y3")));
    CHECK_FALSE(string_is_unsigned_integer(std::string("1 000")));
}

TEST_CASE("string_is_not_integer", "[string_is_not_int]"){
    CHECK_FALSE(string_is_not_unsigned_integer(std::string("0")));
    CHECK_FALSE(string_is_not_unsigned_integer(std::string("1")));
    CHECK_FALSE(string_is_not_unsigned_integer(std::string("10")));
    CHECK_FALSE(string_is_not_unsigned_integer(std::string("100")));
    CHECK_FALSE(string_is_not_unsigned_integer(std::string("1000000000000")));

    CHECK(string_is_not_unsigned_integer(std::string("")));
    CHECK(string_is_not_unsigned_integer(std::string("a")));
    CHECK(string_is_not_unsigned_integer(std::string("-1")));
    CHECK(string_is_not_unsigned_integer(std::string("-10")));
    CHECK(string_is_not_unsigned_integer(std::string("1y")));
    CHECK(string_is_not_unsigned_integer(std::string("y3")));
    CHECK(string_is_not_unsigned_integer(std::string("1y3")));
    CHECK(string_is_not_unsigned_integer(std::string("1 000")));
}

TEST_CASE("kg_to_ton", "[kg_to_ton]"){
    CHECK(kg_to_t(0) == std::string("0"));
    CHECK(kg_to_t(1) == std::string("0.001"));
    CHECK(kg_to_t(10) == std::string("0.01"));
    CHECK(kg_to_t(100) == std::string("0.1"));
    CHECK(kg_to_t(1000) == std::string("1"));
    CHECK(kg_to_t(10000) == std::string("10"));
    CHECK(kg_to_t(100000) == std::string("100"));
    CHECK(kg_to_t(1000000) == std::string("1000"));
    CHECK(kg_to_t(1000000000) == std::string("1e+06"));
    CHECK(kg_to_t(999999999) == std::string("1e+06"));
    CHECK(kg_to_t(999999499) == std::string("1e+06"));
    CHECK(kg_to_t(999994999) == std::string("999995"));
    CHECK(kg_to_t(999998999) == std::string("999999"));
    CHECK(kg_to_t(99999999) == std::string("100000"));
    CHECK(kg_to_t(9999999) == std::string("10000"));
    CHECK(kg_to_t(999999) == std::string("999.999"));
    CHECK(kg_to_t(99999) == std::string("99.999"));

    CHECK(kg_to_t(1234) == std::string("1.234"));
    CHECK(kg_to_t(1500) == std::string("1.5"));
    CHECK(kg_to_t(123456789) == std::string("123457"));

    CHECK(kg_to_t(-1) == std::string("-0.001"));
}

TEST_CASE("cm_to_m", "[cm_to_m]"){
    CHECK(cm_to_m(0) == std::string("0"));
    CHECK(cm_to_m(1) == std::string("0.01"));
    CHECK(cm_to_m(10) == std::string("0.1"));
    CHECK(cm_to_m(100) == std::string("1"));
    CHECK(cm_to_m(1000) == std::string("10"));
    CHECK(cm_to_m(1000000) == std::string("10000"));
    CHECK(cm_to_m(1000000000) == std::string("1e+07"));

    CHECK(cm_to_m(123) == std::string("1.23"));
    CHECK(cm_to_m(150) == std::string("1.5"));
    CHECK(cm_to_m(12345678) == std::string("123457"));
    CHECK(cm_to_m(123456789) == std::string("1.23457e+06"));

    CHECK(cm_to_m(-1) == std::string("-0.01"));
}

TEST_CASE("inch_to_feet", "[inch_to_feet]"){
    CHECK(inch_to_feet(0) == std::string("0'0\""));
    CHECK(inch_to_feet(1) == std::string("0'1\""));
    CHECK(inch_to_feet(2) == std::string("0'2\""));
    CHECK(inch_to_feet(12) == std::string("1'0\""));
}

TEST_CASE("lbs_to_metric_ton", "[lbs_to_metric_ton]"){
    CHECK(lbs_to_metric_ton(0) == std::string("0"));
    CHECK(lbs_to_metric_ton(2204.6228) == std::string("1"));
    CHECK(lbs_to_metric_ton(2204.623) == std::string("1"));
    CHECK_FALSE(lbs_to_metric_ton(2204.62) == std::string("1"));
    CHECK_FALSE(lbs_to_metric_ton(2204.6) == std::string("1"));

    CHECK(lbs_to_metric_ton(0.5)    == std::string("0.000226796"));
    CHECK(lbs_to_metric_ton(1)      == std::string("0.000453592"));
    CHECK(lbs_to_metric_ton(10)     == std::string("0.00453592"));
    CHECK(lbs_to_metric_ton(100)    == std::string("0.0453592"));
    CHECK(lbs_to_metric_ton(1000)   == std::string("0.453592"));
    CHECK(lbs_to_metric_ton(10000)  == std::string("4.53592"));
}

TEST_CASE("init_map_at_element", "[init_map_at_element]"){
    std::map<int,int> m;
    init_map_at_element(&m, 1, 2);
    CHECK(m.find(1) != m.end());
    CHECK(m.at(1) == 2);

    init_map_at_element(&m, -1, 3);
    CHECK(m.find(-1) != m.end());
    CHECK(m.at(-1) == 3);
}

TEST_CASE("ogr2wkb", "[ogr2wkb]"){
    // create OGRGeometry
    OGRGeometry* ogr_geom;
    char* wkt = new char[1024];
    strcpy(wkt,"LINESTRING (30 10, 10 30, 40 40)");
    std::string wkb_reference = "0102000000030000000000000000003e4000000000000024400000000000002440000000000000"
            "3e400000000000004440000000000000444000000000000000";
    // in postgis: select st_astext('_____wkb_reference_____'::geometry) => LINESTRING (30 10, 10 30, 40 40);

    OGRErr res = OGRGeometryFactory::createFromWkt(&wkt, nullptr, &ogr_geom);
    assert(res == OGRERR_NONE);
    std::string wkb_res = ogr2wkb(ogr_geom);
    const char* wkb_cc = wkb_res.c_str();

    std::string wkb_test;
    for (int i = 0; i < sizeof(wkb_cc)*8; i++){
        char buffer[2];
        sprintf(buffer, "%02x", wkb_cc[i]);
        wkb_test.append(buffer);
    }

    CHECK(wkb_test == wkb_reference);
}

TEST_CASE("to_camel_case_with_spaces", "[to_camel_case_with_spaces]"){
    CHECK(to_camel_case_with_spaces("SIBIRIEN") == std::string("Sibirien"));
    CHECK(to_camel_case_with_spaces("FUCHSBERGER DAMM") == std::string("Fuchsberger Damm"));
    CHECK(to_camel_case_with_spaces("KÖNIGSBERGER STRASSE") == std::string("Königsberger Strasse"));
    CHECK(to_camel_case_with_spaces("KØBENHAVN") == std::string("København"));
    CHECK(to_camel_case_with_spaces("") == std::string(""));
}