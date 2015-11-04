#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../plugins/util.hpp"


TEST_CASE("Shapefile exists", "[SHP exist]"){
    CHECK(shp_file_exists("tests/testdata/faroe-islands-latest/roads.shp", std::clog) == true);
    CHECK(shp_file_exists("tests/testdata/faroe-islands-latest/README", std::clog) == false);
    CHECK(shp_file_exists("tests/testdata/faroe-islands-latest/???", std::clog) == false);

    CHECK(shp_file_exists(std::string("tests/testdata/faroe-islands-latest/roads.shp")) == true);
    CHECK(shp_file_exists(std::string("tests/testdata/faroe-islands-latest/README")) == false);
    CHECK(shp_file_exists(std::string("tests/testdata/faroe-islands-latest/???")) == false);
}

TEST_CASE("DBF exists", "[DBF exist]"){
    CHECK(dbf_file_exists("tests/testdata/faroe-islands-latest/roads.dbf") == true);
    CHECK(dbf_file_exists("tests/testdata/faroe-islands-latest/README") == false);
    CHECK(dbf_file_exists("tests/testdata/faroe-islands-latest/???") == false);

    CHECK(dbf_file_exists(std::string("tests/testdata/faroe-islands-latest/roads.dbf")) == true);
    CHECK(dbf_file_exists(std::string("tests/testdata/faroe-islands-latest/README")) == false);
    CHECK(dbf_file_exists(std::string("tests/testdata/faroe-islands-latest/???")) == false);
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
    CHECK(string_is_unsigned_integer(std::string("0")) == true);
    CHECK(string_is_unsigned_integer(std::string("1")) == true);
    CHECK(string_is_unsigned_integer(std::string("10")) == true);
    CHECK(string_is_unsigned_integer(std::string("100")) == true);
    CHECK(string_is_unsigned_integer(std::string("1000000000000")) == true);

    CHECK(string_is_unsigned_integer(std::string("")) == false);
    CHECK(string_is_unsigned_integer(std::string("a")) == false);
    CHECK(string_is_unsigned_integer(std::string("-1")) == false);
    CHECK(string_is_unsigned_integer(std::string("-10")) == false);
    CHECK(string_is_unsigned_integer(std::string("1y")) == false);
    CHECK(string_is_unsigned_integer(std::string("y3")) == false);
    CHECK(string_is_unsigned_integer(std::string("1y3")) == false);
    CHECK(string_is_unsigned_integer(std::string("1 000")) == false);
}

