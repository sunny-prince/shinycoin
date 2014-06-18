// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>
#include <boost/foreach.hpp>

#include "base58.h"
#include "bitcoinrpc.h"
#include "db.h"
#include "init.h"
#include "net.h"
#include "wallet.h"

using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace json_spirit;

Object JSONRPCError(int code, const string& message);
void SyncWithWallets(const CTransaction& tx, const CBlock* pblock = NULL, bool fUpdate = false, bool fConnect = true);

Value getrawmempool(const Array& params, bool fHelp)
{
	if (fHelp || params.size() != 0)
		throw runtime_error(
		"getrawmempool\n"
		"\nReturns all transaction ids in memory pool as a json array of string transaction ids.\n"
		"\nResult:\n"
		"[                       (json array of string)\n"
		"  \"transactionid\"     (string) The transaction id\n"
		"  ,...\n"
		"]"
		);

	vector<uint256> vtxid;
	mempool.queryHashes(vtxid);

	Array a;
	BOOST_FOREACH(const uint256& hash, vtxid)
		a.push_back(hash.ToString());

	return a;
}

Value getrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getrawtransaction <txid> [verbose=0]\n"
            "If verbose=0, returns a string that is\n"
            "serialized, hex-encoded data for <txid>.\n"
            "If verbose is non-zero, returns an Object\n"
            "with information about <txid>.");

    std::string strHash = params[0].get_str();
    uint256 hash(strHash);

    bool fVerbose = false;
    if (params.size() > 1)
        fVerbose = (params[1].get_int() != 0);

    CTransaction tx;

    {
        LOCK(mempool.cs);
        if (!mempool.exists(hash))
            throw JSONRPCError(-22, "No information available about transaction");
        tx = mempool.lookup(hash);
    }

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;
    string strHex = HexStr(ssTx.begin(), ssTx.end());

    if (!fVerbose)
        return strHex;

    throw JSONRPCError(-22, "Verbose: Not supported");
}

Value sendrawtransaction(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            "sendrawtransaction <hex string>\n"
            "Submits raw transaction (serialized, hex-encoded) to local node and network.");

    //RPCTypeCheck(params, list_of(str_type));

    // parse hex string from parameter
    vector<unsigned char> txData(ParseHex(params[0].get_str()));
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;

    // deserialize binary data stream
    try {
        ssData >> tx;
    }
    catch (std::exception &e) {
        throw JSONRPCError(-22, "TX decode failed");
    }
    uint256 hashTx = tx.GetHash();

    // See if the transaction is already in a block
    // or in the memory pool:
    CTransaction existingTx;
    uint256 hashBlock = 0;
    //if (GetTransaction(hashTx, existingTx, hashBlock))
    //{
    // if (hashBlock != 0)
    // throw JSONRPCError(-5, string("transaction already in block ")+hashBlock.GetHex());
    // // Not in block, but already in the memory pool; will drop
    // // through to re-relay it.
    //}
    //else
    {
        // push to local node
        CTxDB txdb("r");
        if (!tx.AcceptToMemoryPool(txdb))
            throw JSONRPCError(-22, "TX rejected");
        SyncWithWallets(tx, NULL, true);
    }
    RelayMessage(CInv(MSG_TX, hashTx), tx);

    return hashTx.GetHex();
}
