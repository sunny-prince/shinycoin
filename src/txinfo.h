// Copyright (c) 2013-2014 The ShinyCoin developers

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef SHINYCOIN_TXINFO_H
#define SHINYCOIN_TXINFO_H

#include <algorithm>

#include <boost/optional/optional.hpp>
#include <boost/filesystem.hpp>
#include "SQLiteCpp/SQLiteCpp.h"

#include "base58.h"


static std::string ignoreReason;
struct TxInfoRecord;


enum TxInfoType
{
    INFO_NORMAL             = 0,
    INFO_WRITE_ONCE         = 1,
    INFO_UNIQUE             = 2,
    INFO_ID                 = 3,
};
#define TX_INFO_MAX_VALID_TYPE  3

class TxInfoKey
{
public:
    static bool IsValidIDCharacter(char c) {
        
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '-');
    }
    
    unsigned char type;
    std::string keyString;
    
    TxInfoKey() {}
    TxInfoKey(const char *keyIn) : type((unsigned char)INFO_NORMAL), keyString(std::string(keyIn)) {}
    TxInfoKey(std::string keyIn) : type((unsigned char)INFO_NORMAL), keyString(keyIn) {}
    TxInfoKey(TxInfoType typeIn, std::string keyIn) : type((unsigned char)typeIn), keyString(keyIn) {}
    
    std::string ToString() const;
    
    friend bool operator==(const TxInfoKey& a, const TxInfoKey& b)
    {
        return (a.type == b.type &&
                a.keyString == b.keyString);
    }
    
    friend bool operator!=(const TxInfoKey& a, const TxInfoKey& b)
    {
        return !(a == b);
    }
    
    bool IsValid(std::string &reason=ignoreReason) const;
    
    IMPLEMENT_SERIALIZE
    (
        READWRITE(type);
        READWRITE(keyString);
    )
};

typedef std::string TxInfoValue;

class CTxInfo
{
public:
    TxInfoKey key;
    TxInfoValue value;
    
    CTxInfo(TxInfoKey keyIn, TxInfoValue valueIn) : key(keyIn), value(valueIn) {}
    CTxInfo(const TxInfoRecord &record);
    CTxInfo() {}
    
    IMPLEMENT_SERIALIZE
    (
        READWRITE(key);
        READWRITE(value);
    )
    
    friend bool operator==(const CTxInfo& a, const CTxInfo& b)
    {
        return (a.key == b.key &&
                a.value == b.value);
    }
    
    friend bool operator!=(const CTxInfo& a, const CTxInfo& b)
    {
        return !(a == b);
    }
    
    bool IsValid(std::string &reason=ignoreReason) const;
    
    std::string ToString() const;
};

struct TxInfoRecord
{
    CBitcoinAddress addr;
    CTxInfo info;
    uint256 blockHash;
    int indexTx;
    int indexTxIn;
    int indexInfo;
    
    TxInfoRecord(const CBitcoinAddress &addr_in, const CTxInfo &info_in,
                 uint256 blockHash_in=0, int indexTx_in=-1, int indexTxIn_in=-1, int indexInfo_in=-1) :
        addr(addr_in), info(info_in),
        blockHash(blockHash_in), indexTx(indexTx_in), indexTxIn(indexTxIn_in), indexInfo(indexInfo_in) {}
};

class CTxInfoStore
{
private:
    SQLite::Database db;
    SQLite::Transaction *curTransaction;
    unsigned int byteEstimate;
    
    void Initialize();
    bool _Process(const TxInfoRecord &infoRecord, std::string &reason);
    bool _Undo(const TxInfoRecord &infoRecord, std::string &reason);
    
public:
    explicit CTxInfoStore(const boost::filesystem::path &path) :
    db(path.string(), SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE)
    {
        curTransaction = NULL;
        byteEstimate = 0;
        Initialize();
    }
    
    ~CTxInfoStore()
    {
        if (InTransaction())
            Rollback();
    }
    
    void Reset()
    {
        db.exec("DROP TABLE TxDbEntry");
        Initialize();
    }
    
    bool IsValid(const TxInfoRecord &infoRecord, std::string &reason=ignoreReason);
    bool Process(const TxInfoRecord &infoRecord, std::string &reason=ignoreReason);
    bool Undo(const TxInfoRecord &infoRecord, std::string &reason=ignoreReason);
    
    boost::optional<TxInfoValue> Get(const CBitcoinAddress &addr, const TxInfoKey &key);
    std::vector<CBitcoinAddress> AddressesWithValue(const TxInfoKey &key, const TxInfoValue &value);
    boost::optional<CBitcoinAddress> UniqueAddressWithValue(const TxInfoKey &key, const TxInfoValue &value);
    
    std::vector<TxInfoRecord> RecordsWithAddress(const CBitcoinAddress &addr);
    std::vector<TxInfoRecord> RecordsWithKey(const TxInfoKey &key);
    
    bool InTransaction() const;
    void BeginTransaction();
    void Commit();
    void Rollback();
    
    unsigned int GetCommitByteEstimate() const;
    
    void DumpLatestInfos();
};


class CTxInfoView
{
private:
    CTxInfoStore *store;
    bool committed;
    
public:
    explicit CTxInfoView(CTxInfoStore *storeIn) : store(storeIn), committed(false)
    {
        store->BeginTransaction();
    }
    ~CTxInfoView()
    {
        if (!committed)
            store->Rollback();
    }
    
    void Commit()
    {
        if (committed)
            throw std::runtime_error("Can't Commit(), this view already over");
        committed = true;
        store->Commit();
    }
    
    void Rollback()
    {
        if (committed)
            throw std::runtime_error("Can't Rollback(), this view already over");
        committed = true;
        store->Rollback();
    }
};

#endif
