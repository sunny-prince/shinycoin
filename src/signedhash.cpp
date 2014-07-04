// Copyright (c) 2013-2014 The ShinyCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signedhash.h"

#include "uint256.h"
#include "db.h"
#include "util.h"
#include "main.h"
#include "net.h"
#include "protocol.h"

const std::string CSignedHash::strMasterPubKey = "04de9cd6a14a2174db54c597a88248022b6bf0d51513ec00f789abe82096db245dc2fe5eb8a8d7163c5005b507da48771b41a65051687865a7ba825705f5eb0e04";

std::string CSignedHash::strMasterPrivKey = "";


namespace SignedHash
{
    static std::map<uint256, uint256> mapPoWCache;
    static std::map<uint256, std::pair<uint256, std::vector<unsigned char> > > mapSigCache;
    
    std::map<uint256, CSignedHash> mapInvSignedHash;
    
    static CCriticalSection cs_caches;
    
    bool GetSignedPoWHash(const uint256 &idHash, uint256 &powHash, std::vector<unsigned char> &vchSig)
    {
        LOCK(cs_caches);
        
        std::map<uint256, std::pair<uint256, std::vector<unsigned char> > >::iterator mi = mapSigCache.find(idHash);
        if (mi != mapSigCache.end())
        {
            powHash = (*mi).second.first;
            vchSig = (*mi).second.second;
            
            return true;
        }
        
        return false;
    }
    
    size_t CountSignedHashes()
    {
        return mapSigCache.size();
    }
    
    bool GetPoWHash(const uint256 &idHash, uint256 &powHash)
    {
        std::vector<unsigned char> vchSig;
        if (GetBoolArg("-usesignedhashes", true) && GetSignedPoWHash(idHash, powHash, vchSig))
        {
            if (fDebug)
                printf("SignedHash::GetPoWHash(): Returning signed hash %s --> %s\n",
                       idHash.ToString().substr(0,16).c_str(),
                       powHash.ToString().substr(0,16).c_str());
            return true;
        }
        
        LOCK(cs_caches);
        
        std::map<uint256, uint256>::iterator mi = mapPoWCache.find(idHash);
        if (mi != mapPoWCache.end())
        {
            powHash = (*mi).second;
            
            if (fDebug)
                printf("SignedHash::GetPoWHash(): Returning unsigned cached hash %s --> %s\n",
                       idHash.ToString().substr(0,16).c_str(),
                       powHash.ToString().substr(0,16).c_str());
            
            return true;
        }
        
        return false;
    }
    
    bool UncheckedAddHash(const uint256 &idHash, const uint256 &powHash)
    {
        LOCK(cs_caches);
        mapPoWCache[idHash] = powHash;
        
        CTxDB txdb;
        bool fSuccess = txdb.WritePoWHash(idHash, powHash);
        txdb.Close();

        if (!fSuccess)
            return error("SignedHash::UncheckedAddHash(): Failed to write PoW hash to txdb");
        
        return true;
    }
    
    bool UncheckedAddSignedHash(const uint256 &idHash, const uint256 &powHash, const std::vector<unsigned char> &vchSig)
    {
        LOCK(cs_caches);
        mapSigCache[idHash] = std::make_pair(powHash, vchSig);
        
        CTxDB txdb;
        bool fSuccess = txdb.WriteSignedHash(idHash, powHash, vchSig);
        txdb.Close();

        if (!fSuccess)
            return error("SignedHash::UncheckedAddSignedHash(): Failed to write PoW hash to txdb");
        
        return true;
    }
    
    bool LoadHashCache()
    {
        LOCK(cs_caches);
        
        CTxDB txdb;
        
        {
            std::string strPubKey = "";
            if (!txdb.ReadSignedHashPubKey(strPubKey) || strPubKey != CSignedHash::strMasterPubKey)
            {
                mapSigCache.clear();
                mapInvSignedHash.clear();
                
                if (!txdb.TxnBegin())
                    return error("LoadHashCache() : failed to start txdb transaction");
                
                if (!txdb.WriteSignedHashPubKey(CSignedHash::strMasterPubKey))
                    return error("LoadHashCache() : failed to write new signed hash master key to db");
                
                for (CBlockIndex* pindex = pindexBest; pindex && pindex->pprev; pindex = pindex->pprev)
                    txdb.EraseSignedHash(pindex->GetBlockIDHash());
                
                if (!txdb.TxnCommit())
                    return error("LoadHashCache() : failed to commit new signed hash master key to db");
                
                return true;
            }
        }
        
        for (CBlockIndex* pindex = pindexBest; pindex && pindex->pprev; pindex = pindex->pprev)
        {
            uint256 idHash = pindex->GetBlockIDHash();
            uint256 powHash;
            std::vector<unsigned char> vchSig;
            
            if (txdb.ReadPoWHash(idHash, powHash))
                mapPoWCache[idHash] = powHash;
            
            if (txdb.ReadSignedHash(idHash, powHash, vchSig))
            {
                CSignedHash signedHash;
                signedHash.SetHashes(idHash, powHash);
                if (signedHash.SetSignature(vchSig))
                {
                    mapSigCache[idHash] = std::make_pair(powHash, vchSig);
                    mapInvSignedHash[signedHash.GetHash()] = signedHash;
                }
                else
                    error("LoadHashCache(): TxDB contains invalid signed hash");
            }
        }
        
        if (fDebug)
            printf("LoadHashCache(): Have %d unsigned hashes, %d signed hashes\n",
                   mapPoWCache.size(), mapSigCache.size());
        
        return true;
    }
    
    bool SetSignedHashPrivKey(std::string strPrivKey)
    {
        std::string strPrevPrivKey = CSignedHash::strMasterPrivKey;
        
        CSignedHash::strMasterPrivKey = strPrivKey;
        
        CSignedHash signedHash;
        
        if (!signedHash.SignHashes(uint256(11111), uint256(22222)))
        {
            CSignedHash::strMasterPrivKey = strPrevPrivKey;
            return false;
        }
        
        return true;
    }
    
    bool SendSignedHash(uint256 idHash)
    {
        std::map<uint256, CBlockIndex*>::iterator it = mapBlockIndex.find(idHash);
        if (it == mapBlockIndex.end())
            return error("SendSignedHash(): Invalid block id");
        
        CBlockIndex *pblockIndex = (*it).second;
        
        uint256 powHash;
        SetNeedCheckBlock(true);
        bool fSuccess = pblockIndex->ComputeBlockPoWHash(powHash);
        SetNeedCheckBlock(false);
        if (!fSuccess)
            return error("SendSignedHash(): Unable to get proof of work hash");

        CSignedHash signedHash;
        if (!signedHash.SignHashes(idHash, powHash))
            return false;
        if (!signedHash.ProcessRelaySignedHash(NULL))
            return error("SendSignedHash: Failed to process and relay signed hash");

        return true;
    }
}

bool CSignedHash::CheckSignature()
{
    CKey key;
    if (!key.SetPubKey(ParseHex(CSignedHash::strMasterPubKey)))
        return error("CSignedHash::CheckSignature() : SetPubKey failed");
    if (!key.Verify(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
        return error("CSignedHash::CheckSignature() : verify signature failed");

    // Now unserialize the data
    CDataStream sMsg(vchMsg, SER_NETWORK, PROTOCOL_VERSION);
    sMsg >> *(CUnsignedHash*)this;
    return true;
}

bool CSignedHash::SetSignature(const std::vector<unsigned char> &vchSigIn)
{
    vchSig = vchSigIn;
    
    if (!CheckSignature())
    {
        vchSig.clear();
        return error("CSignedHash::SetSignature() : Invalid signature");
    }
    
    return true;
}

void CSignedHash::SetHashes(const uint256 &idHashIn, const uint256 &powHashIn)
{
    vchSig.clear();
    
    idHash = idHashIn;
    powHash = powHashIn;
    
    CDataStream sMsg(SER_NETWORK, PROTOCOL_VERSION);
    sMsg << *((CUnsignedHash *)(this));
    vchMsg = std::vector<char>(sMsg.begin(), sMsg.end());
}

bool CSignedHash::SignHashes(const uint256 &idHashIn, const uint256 &powHashIn)
{
    if (CSignedHash::strMasterPrivKey.empty())
        return error("SignHashes: Checkpoint master key unavailable.");
    
    SetHashes(idHashIn, powHashIn);
    
    std::vector<unsigned char> vchPrivKey = ParseHex(CSignedHash::strMasterPrivKey);
    
    CKey key;
    key.SetPrivKey(CPrivKey(vchPrivKey.begin(), vchPrivKey.end())); // if key is not correct openssl may crash
    if (!key.Sign(Hash(vchMsg.begin(), vchMsg.end()), vchSig))
        return error("SendSignedHash: Unable to sign hash, check private key?");
    
    return true;
}

bool CSignedHash::ProcessRelaySignedHash(CNode* pfrom)
{
    if (!CheckSignature())
        return false;
    
    SignedHash::UncheckedAddSignedHash(idHash, powHash, vchSig);
    
    SignedHash::mapInvSignedHash[GetHash()] = *this;
    
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
            pnode->PushInventory(CInv(MSG_SIGNED_HASH, this->GetHash()));
    }
    
    return true;
}


