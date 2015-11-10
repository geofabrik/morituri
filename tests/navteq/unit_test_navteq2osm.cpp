#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "../../plugins/navteq/navteq.hpp"
#include "../../plugins/navteq/navteq_plugin.hpp"


TEST_CASE("Create nodes for administrative boundaries", "[admin_nodes]") {

    int ring_size = 10005;

    OGRLinearRing* ring = new OGRLinearRing();
    OGRPoint* p = new OGRPoint(0,0);
    ring->addPoint(p);
    for (auto i=1; i<ring_size; i++){
        ring->addPoint(new OGRPoint(i,i));
    }
    ring->addPoint(p);

    node_vector_type osm_ids = create_admin_boundary_way_nodes(ring);


    CHECK(osm_ids.size() == ring->getNumPoints());

}

