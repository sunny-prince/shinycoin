// Copyright (c) 2013-2014 The ShinyCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SHINYCOIN_SIGNEDHASH_H
#define SHINYCOIN_SIGNEDHASH_H

#include "uint256.h"
#include "util.h"
#include "serialize.h"

#include <boost/tuple/tuple.hpp>

#include <string>
#include <map>
#include <utility>
#include <vector>


class CBlockIndex;
class CTxDB;
class CNode;
class CBlock;

class CSignedHash;

namespace SignedHash
{
    extern std::map<uint256, CSignedHash> mapInvSignedHash;
    
    size_t CountSignedHashes();
    
    bool GetSignedPoWHash(const uint256 &idHash, uint256 &powHash, std::vector<unsigned char> &vchSig);
    bool GetPoWHash(const uint256 &idHash, uint256 &powHash);
    bool UncheckedAddHash(const uint256 &idHash, const uint256 &powHash);
    bool UncheckedAddSignedHash(const uint256 &idHash, const uint256 &powHash, const std::vector<unsigned char> &vchSig);
    
    bool LoadHashCache();
    
    bool SetSignedHashPrivKey(std::string strPrivKey);
    
    bool SendSignedHash(uint256 idHash);
}

class CUnsignedHash
{
public:
    int nVersion;
    uint256 idHash;
    uint256 powHash;

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(idHash);
        READWRITE(powHash);
    )

    void SetNull()
    {
        nVersion = 1;
        idHash = 0;
        powHash = 0;
    }

    std::string ToString() const
    {
        return strprintf(
                "CUnsignedHash(\n"
                "    nVersion       = %d\n"
                "    idHash         = %s\n"
                "    powHash        = %s\n"
                ")\n",
            nVersion,
            idHash.GetHex(64).c_str(),
            powHash.GetHex(64).c_str());
    }

    void print() const
    {
        printf("%s", ToString().c_str());
    }
};

class CSignedHash : public CUnsignedHash
{
public:
    static const std::string strMasterPubKey;
    static std::string strMasterPrivKey;

    std::vector<char> vchMsg;
    std::vector<unsigned char> vchSig;

    CSignedHash()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(vchMsg);
        READWRITE(vchSig);
        {
            CUnsignedHash uns;
         
            CDataStream sMsg(vchMsg, SER_NETWORK, PROTOCOL_VERSION);
            sMsg >> *((CUnsignedHash *)(this));
        }
    )

    void SetNull()
    {
        CUnsignedHash::SetNull();
        vchMsg.clear();
        vchSig.clear();
    }
    
    bool IsNull() const
    {
        return (idHash == 0);
    }
    
    uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

    void SetHashes(const uint256 &idHashIn, const uint256 &powHashIn);
    bool SignHashes(const uint256 &idHashIn, const uint256 &powHashIn);

    bool CheckSignature();
    bool SetSignature(const std::vector<unsigned char> &vchSigIn);
    
    bool ProcessRelaySignedHash(CNode* pfrom);
};


#endif

