
# base plugin
BASE_HEADER=plugins/base_plugin.hpp

# plugins
DUMMY_SOURCE=plugins/dummy/dummy_plugin.cpp
DUMMY_HEADER=plugins/dummy/dummy_plugin.hpp

NAVTEQ_SOURCE=plugins/navteq/navteq_plugin.cpp
NAVTEQ_HEADER=plugins/navteq/navteq_plugin.hpp\
		plugins/navteq/navteq.hpp\
		plugins/navteq/navteq2osm_tag_parser.hpp\
		plugins/navteq/navteq_mappings.hpp\
		plugins/navteq/navteq_types.hpp\
		plugins/comm2osm_exceptions.hpp\
		plugins/navteq/navteq_util.hpp\
		plugins/ogr_types.hpp\
		plugins/util.hpp\
		plugins/readers.hpp

# sources of all plugins
SOURCE=comm2osm.cpp\
		${NAVTEQ_SOURCE}\
		${DUMMY_SOURCE}

# header of all plugins
HEADER=${BASE_HEADER}\
		${NAVTEQ_HEADER}\
		${DUMMY_HEADER}

# test sources and headers
NAVTEQ_UNIT_TEST_SOURCE=tests/navteq/unit_test_navteq2osm.cpp
NAVTEQ_UNIT_TEST_HEADER=${NAVTEQ_HEADER}
NAVTEQ_TEST_SOURCE=tests/navteq/test_navteq2osm.cpp
NAVTEQ_TEST_HEADER=${NAVTEQ_HEADER}
UTIL_TEST_SOURCE=tests/unit_test_util.cpp
UTIL_TEST_HEADER=plugins/util.hpp

# includes
OSMIUM_INCLUDE=-I${HOME}/libs/libosmium/include

ifeq ($(TRAVIS),true)
OSMPBF_INCLUDE=-I${HOME}/libs/OSM-binary/include
OSMPBF_LIBRARY=${HOME}/libs/OSM-binary/src/libosmpbf.a
else
OSMPBF_INCLUDE=
OSMPBF_LIBRARY=-losmpbf
endif

INCLUDES=${OSMIUM_INCLUDE} -Iplugins ${OSMPBF_INCLUDE}
LIBS=-lbz2 -lgdal -lexpat -lgeos -lpthread -lz -lprotobuf-lite -lboost_system -lboost_filesystem -licuuc ${OSMPBF_LIBRARY}

CXXFLAGS=-std=c++11 
DEBUG_FLAGS=-O0 -g

all: comm2osm-debug comm2osm tests
.PHONY: all

comm2osm-debug: ${SOURCE} ${HEADER}
	${CXX} ${CXXFLAGS} ${DEBUG_FLAGS} -o comm2osm-debug ${SOURCE} ${INCLUDES} ${LIBS}

comm2osm: ${SOURCE} ${HEADER}
	${CXX} ${CXXFLAGS} -o comm2osm ${SOURCE} ${INCLUDES} ${LIBS}

tests: tests/navteq_test tests/util_test tests/navteq_unit_test
.PHONY: tests

tests/navteq_unit_test: ${NAVTEQ_UNIT_TEST_SOURCE} ${NAVTEQ_UNIT_TEST_HEADER}
	${CXX} ${CXXFLAGS} ${DEBUG_FLAGS} -o tests/navteq_unit_test ${NAVTEQ_UNIT_TEST_SOURCE} ${INCLUDES} ${LIBS}

tests/navteq_test: ${NAVTEQ_TEST_SOURCE} ${NAVTEQ_TEST_HEADER}
	${CXX} ${CXXFLAGS} ${DEBUG_FLAGS} -o tests/navteq_test ${NAVTEQ_TEST_SOURCE} ${INCLUDES} ${LIBS}

tests/util_test: ${UTIL_TEST_SOURCE} ${UTIL_TEST_HEADER}
	${CXX} ${CXXFLAGS} ${DEBUG_FLAGS} -o tests/util_test ${UTIL_TEST_SOURCE} ${INCLUDES} ${LIBS}

test:
	./tests/navteq_unit_test
	./tests/navteq_test
	./tests/util_test

doc: ${SOURCE} ${HEADER} Doxyfile
	doxygen Doxyfile
   
clean: 
	rm -f comm2osm comm2osm-debug test testfiles
	rm -rf .tmp_navteq
	rm -rf doc
	rm -f tests/navteq_test
