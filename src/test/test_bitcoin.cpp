#define BOOST_TEST_MODULE Bitcoin Test Suite
#include <boost/test/unit_test.hpp>

#include "main.h"
#include "wallet.h"
#include "checkpoints.h"

CWallet* pwalletMain;

extern bool fPrintToConsole;
struct TestingSetup {
    TestingSetup() {
        fPrintToConsole = true; // don't want to write to debug.log file
        pwalletMain = new CWallet();
        RegisterWallet(pwalletMain);

        hashGenesisBlock = uint256("0x69af109a3d4f8be1192f4598bf27413ecde0618df306a8a13d416e3ac5c0b4f9");
        hashGenesisMerkle = uint256("0x41aae881eacb1d04fd8dd23cdd9e61a80ec80c2f300ec465e44a233f97e6496b");
        strGenesisTimestampString = "Times UK 19-JUN-2014 Felipe VI was proclaimed King of Spain this morning";
        nGenesisTime = 1403189935;
        nGenesisNonce = 1;
        
        Checkpoints::InitMapCheckpoints();
    }
    ~TestingSetup()
    {
        delete pwalletMain;
        pwalletMain = NULL;
    }
};

BOOST_GLOBAL_FIXTURE(TestingSetup);

void Shutdown(void* parg)
{
  exit(0);
}

void StartShutdown()
{
  exit(0);
}

