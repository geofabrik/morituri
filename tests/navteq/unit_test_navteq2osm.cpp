#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "../../plugins/navteq/navteq.hpp"
#include "../../plugins/navteq/navteq_plugin.hpp"


TEST_CASE("Create nodes for administrative boundaries", "[admin_nodes]") {
    std::vector<int> ring_sizes = { 2, 5, 10, 100, 999, 1000, 1001, 1002, 10000 };
    for (auto ring_size : ring_sizes) {
        OGRLinearRing* ring = new OGRLinearRing();
        OGRPoint* p = new OGRPoint(0, 0);
        ring->addPoint(p);
        for (auto i = 1; i < ring_size; i++) {
            ring->addPoint(new OGRPoint(i, i));
        }
        ring->addPoint(p);

        node_vector_type osm_ids = create_closed_way_nodes(ring);

        CHECK(osm_ids.size() == ring->getNumPoints());
    }
}

TEST_CASE("Create way for administrative boundaries", "[admin_ways]") {
    std::vector<int> ring_sizes = { 2, 5, 10, 100, 999, 1000, 1001, 1002, 10000 };
    for (auto ring_size : ring_sizes) {
        OGRLinearRing* ring = new OGRLinearRing();
        OGRPoint* p = new OGRPoint(0, 0);
        ring->addPoint(p);
        for (auto i = 1; i < ring_size; i++) {
            ring->addPoint(new OGRPoint(i, i));
        }
        ring->addPoint(p);
        osm_id_vector_type osm_way_ids = build_closed_ways(ring);

        int number_of_way_ids = floor(ring->getNumPoints() / OSM_MAX_WAY_NODES)+1;
        CHECK(osm_way_ids.size() == number_of_way_ids);
    }
}

