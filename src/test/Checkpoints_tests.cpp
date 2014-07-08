//
// Unit tests for block-chain checkpoints
//
#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/test/unit_test.hpp>
#include <boost/foreach.hpp>

#include "../checkpoints.h"
#include "../util.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(Checkpoints_tests)

BOOST_AUTO_TEST_CASE(sanity)
{
    uint256 p0 = uint256("0x69af109a3d4f8be1192f4598bf27413ecde0618df306a8a13d416e3ac5c0b4f9");
    uint256 p4024 = uint256("0xa33be4c6d63d2459ea587a110e21013fef619bfd3cebae1a51839496ab46c883");
    BOOST_CHECK(Checkpoints::CheckHardened(0, p0));
    BOOST_CHECK(Checkpoints::CheckHardened(4024, p4024));

    
    // Wrong hashes at checkpoints should fail:
    BOOST_CHECK(!Checkpoints::CheckHardened(0, p4024));
    BOOST_CHECK(!Checkpoints::CheckHardened(4024, p0));

    // ... but any hash not at a checkpoint should succeed:
    BOOST_CHECK(Checkpoints::CheckHardened(0+1, p4024));
    BOOST_CHECK(Checkpoints::CheckHardened(4024+1, p0));

    BOOST_CHECK(Checkpoints::GetTotalBlocksEstimate() >= 4024);
}    

BOOST_AUTO_TEST_SUITE_END()
