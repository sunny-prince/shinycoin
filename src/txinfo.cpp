// Copyright (c) 2013-2014 The ShinyCoin developers

// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "txinfo.h"

//------------------------------------------

CTxInfo::CTxInfo(const TxInfoRecord &record) : key(record.info.key), value(record.info.value) { };

//------------------------------------------

std::string TxInfoKey::ToString() const
{
    if (type == INFO_ID && keyString == "n")
        return "Name";
    
    std::string res;
    switch (type)
    {
        case INFO_NORMAL:
            res += "n:";
            break;
        case INFO_WRITE_ONCE:
            res += "w:";
            break;
        case INFO_UNIQUE:
            res += "u:";
            break;
        case INFO_ID:
            res += "i:";
            break;
        default:
            return "!Invalid!";
    }
    
    res += keyString;
    return res;
}


bool TxInfoKey::IsValid(std::string &reason) const
{
    if (type > TX_INFO_MAX_VALID_TYPE)
    {
        reason = "Invalid key type";
        return false;
    }
    
    if (keyString.empty())
    {
        reason = "Key string is empty";
        return false;
    }
    
    for (std::string::const_iterator it = keyString.begin(); it != keyString.end(); ++it)
    {
        if (!TxInfoKey::IsValidIDCharacter(*it))
        {
            reason = "Invalid ID character in key";
            return false;
        }
    }
    
    return true;
}

//------------------------------------------

bool CTxInfo::IsValid(std::string &reason) const
{
    if (!key.IsValid(reason))
        return false;
    
    if (value.empty())
    {
        reason = "Invalid value";
        return false;
    }
    
    if (key.type == INFO_ID) {
        for (std::string::const_iterator it = value.begin(); it != value.end(); ++it)
        {
            if (!TxInfoKey::IsValidIDCharacter(*it))
            {
                reason = "Invalid ID character in value";
                return false;
            }
        }
    }
    
    return true;
}

std::string CTxInfo::ToString() const
{
    std::ostringstream oss;
    oss << key.ToString() << ": " << value;
    return oss.str();
}

//------------------------------------------

void CTxInfoStore::Initialize()
{
    db.exec("CREATE TABLE IF NOT EXISTS TxDbEntry ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    is_latest ls tesINTEGER,"
            "    address TEXT,"
            "    key_type INTEGER,"
            "    key TEXT,"
            "    value TEXT,"
            "    block_hash TEXT,"
            "    index_tx INTEGER,"
            "    index_txin INTEGER,"
            "    index_info INTEGER)");
    
    db.exec("CREATE INDEX IF NOT EXISTS address_index    ON TxDbEntry (address);");
    db.exec("CREATE INDEX IF NOT EXISTS key_index        ON TxDbEntry (key_type, key);");
    db.exec("CREATE INDEX IF NOT EXISTS value_index      ON TxDbEntry (value);");
    db.exec("CREATE INDEX IF NOT EXISTS block_hash_index ON TxDbEntry (block_hash);");
}


bool CTxInfoStore::InTransaction() const
{
    return curTransaction != NULL;
}

void CTxInfoStore::BeginTransaction()
{
    if (InTransaction())
        throw std::runtime_error("Database view already has a transaction");
    
    byteEstimate = 0;
    curTransaction = new SQLite::Transaction(db);
}

void CTxInfoStore::Commit()
{
    if (!InTransaction())
        throw std::runtime_error("Can't Commit() without a transaction");
    
    curTransaction->commit();
    delete curTransaction;
    curTransaction = NULL;
    byteEstimate = 0;
}

unsigned int CTxInfoStore::GetCommitByteEstimate() const
{
    if (!InTransaction())
        return 0;
    
    return byteEstimate;
}

void CTxInfoStore::Rollback()
{
    if (!InTransaction())
        throw std::runtime_error("Can't Rollback() without a transaction");
    
    delete curTransaction;
    curTransaction = NULL;
    byteEstimate = 0;
}



bool CTxInfoStore::_Process(const TxInfoRecord &infoRecord, std::string &reason)
{
    if (!IsValid(infoRecord, reason))
        return false;
    
    {
        SQLite::Statement query(db, "UPDATE TxDbEntry "
                                "SET is_latest=0 "
                                "WHERE address=? AND key_type=? AND key=?");
        query.bind(1, infoRecord.addr.ToString());
        query.bind(2, infoRecord.info.key.type);
        query.bind(3, infoRecord.info.key.keyString);
        query.exec();
    }
    
    {
        SQLite::Statement query(db,
                                "INSERT INTO TxDbEntry (is_latest, address, key_type, key, value, "
                                "                       block_hash, index_tx, index_txin, index_info) "
                                "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?)");
        query.bind(1, infoRecord.addr.ToString());
        query.bind(2, infoRecord.info.key.type);
        query.bind(3, infoRecord.info.key.keyString);
        query.bind(4, infoRecord.info.value);
        query.bind(5, infoRecord.blockHash.GetHex(64));
        query.bind(6, infoRecord.indexTx);
        query.bind(7, infoRecord.indexTxIn);
        query.bind(8, infoRecord.indexInfo);
        query.exec();
    }
    
    byteEstimate += 4 + 1 + infoRecord.addr.ToString().size() + 1 + infoRecord.info.key.keyString.size() + infoRecord.info.value.size() + infoRecord.blockHash.GetHex(64).size() + 12;
    
    return true;
}

bool CTxInfoStore::_Undo(const TxInfoRecord &infoRecord, std::string &reason)
{
    {
        SQLite::Statement query(db, "DELETE FROM TxDbEntry "
                                "WHERE is_latest=1 AND address=? AND key_type=? AND key=? AND value=? "
                                "  AND block_hash=? AND index_tx=? AND index_txin=? AND index_info=?");
        query.bind(1, infoRecord.addr.ToString());
        query.bind(2, infoRecord.info.key.type);
        query.bind(3, infoRecord.info.key.keyString);
        query.bind(4, infoRecord.info.value);
        query.bind(5, infoRecord.blockHash.GetHex(64));
        query.bind(6, infoRecord.indexTx);
        query.bind(7, infoRecord.indexTxIn);
        query.bind(8, infoRecord.indexInfo);
        int rowsChanged = query.exec();
        
        if (rowsChanged == 0)
        {
            reason = "Nothing to undo";
            return false;
        }
    }
    
    {
        SQLite::Statement query(db, "UPDATE TxDbEntry "
                                "SET is_latest=1 "
                                "WHERE id=(SELECT MAX(id) FROM TxDbEntry WHERE address=? AND key_type=? AND key=?)");
        query.bind(1, infoRecord.addr.ToString());
        query.bind(2, infoRecord.info.key.type);
        query.bind(3, infoRecord.info.key.keyString);
        query.exec();
    }
    
    return true;
}


bool CTxInfoStore::Process(const TxInfoRecord &infoRecord, std::string &reason)
{
    if (InTransaction())
        return _Process(infoRecord, reason);
    
    CTxInfoView view(this);
    if (_Process(infoRecord, reason))
    {
        view.Commit();
        return true;
    }
    return false;
}

bool CTxInfoStore::Undo(const TxInfoRecord &infoRecord, std::string &reason)
{
    if (InTransaction())
        return _Undo(infoRecord, reason);
    
    CTxInfoView view(this);
    if (_Undo(infoRecord, reason))
    {
        view.Commit();
        return true;
    }
    return false;
}


boost::optional<TxInfoValue> CTxInfoStore::Get(const CBitcoinAddress &addr, const TxInfoKey &key)
{
    SQLite::Statement query(db, "SELECT value "
                            "FROM TxDbEntry "
                            "WHERE id=(SELECT MAX(id) FROM TxDbEntry WHERE address=? AND key_type=? AND key=?)");
    query.bind(1, addr.ToString());
    query.bind(2, key.type);
    query.bind(3, key.keyString);
    
    if (query.executeStep())
    {
        std::string value = query.getColumn(0);
        return boost::optional<TxInfoValue>(value);
    }
    
    return boost::optional<TxInfoValue>();
}


std::vector<CBitcoinAddress> CTxInfoStore::AddressesWithValue(const TxInfoKey &key, const TxInfoValue &value)
{
    SQLite::Statement query(db, "SELECT address "
                            "FROM TxDbEntry "
                            "WHERE key_type=? and key=? and value=? and is_latest=1");
    query.bind(1, key.type);
    query.bind(2, key.keyString);
    query.bind(3, value);
    
    std::vector<CBitcoinAddress> result;
    
    while (query.executeStep())
    {
        std::string encodedAddress = query.getColumn(0);
        result.push_back(CBitcoinAddress(encodedAddress));
    }
    
    return result;
}


boost::optional<CBitcoinAddress> CTxInfoStore::UniqueAddressWithValue(const TxInfoKey &key, const TxInfoValue &value)
{
    if (!(key.type == INFO_UNIQUE || key.type == INFO_ID))
        throw std::runtime_error("Only unique-type keys can have a unique address for a given value");
    
    std::vector<CBitcoinAddress> resultSet = AddressesWithValue(key, value);
    if (resultSet.empty())
        return boost::optional<CBitcoinAddress>();
    if (resultSet.size() > 1)
        throw std::runtime_error("Unique key has multiple addresses for one value!");
    return boost::optional<CBitcoinAddress>(resultSet.front());
}

std::vector<TxInfoRecord> CTxInfoStore::RecordsWithAddress(const CBitcoinAddress &addr)
{
    SQLite::Statement query(db, "SELECT key_type, key, value, block_hash, index_tx, index_txin, index_info "
                            "FROM TxDbEntry "
                            "WHERE address=?");
    query.bind(1, addr.ToString());

    std::vector<TxInfoRecord> vecRecords;
    while (query.executeStep())
    {
        int keyType = query.getColumn(0);
        std::string keyString = query.getColumn(1);
        std::string valueString = query.getColumn(2);
        std::string blockHashString = query.getColumn(3);
        int indexTx = query.getColumn(4);
        int indexTxIn = query.getColumn(5);
        int indexInfo = query.getColumn(6);
        
        TxInfoKey key((TxInfoType)keyType, keyString);
        TxInfoValue value(valueString);
        CTxInfo info(key, value);

        uint256 blockHash;
        blockHash.SetHex(blockHashString);
        
        vecRecords.push_back(TxInfoRecord(addr, info, blockHash, indexTx, indexTxIn, indexInfo));
    }
    
    return vecRecords;
}

std::vector<TxInfoRecord> CTxInfoStore::RecordsWithKey(const TxInfoKey &key)
{
    SQLite::Statement query(db, "SELECT address, value, block_hash, index_tx, index_txin, index_info "
                            "FROM TxDbEntry "
                            "WHERE key_type=? AND key=?");
    query.bind(1, key.type);
    query.bind(2, key.keyString);
    
    std::vector<TxInfoRecord> vecRecords;
    while (query.executeStep())
    {
        std::string addressString = query.getColumn(0);
        std::string valueString = query.getColumn(1);
        std::string blockHashString = query.getColumn(2);
        int indexTx = query.getColumn(3);
        int indexTxIn = query.getColumn(4);
        int indexInfo = query.getColumn(5);

        CBitcoinAddress address(addressString);
        
        uint256 blockHash;
        blockHash.SetHex(blockHashString);
        
        vecRecords.push_back(TxInfoRecord(address, CTxInfo(key, valueString),
                                          blockHash, indexTx, indexTxIn, indexInfo));
    }
    
    return vecRecords;
}


bool CTxInfoStore::IsValid(const TxInfoRecord &record, std::string &reason)
{
    const CTxInfo &txDb = record.info;
    
    if (!txDb.IsValid(reason))
        return false;
    
    if (txDb.key.type == INFO_UNIQUE || txDb.key.type == INFO_ID)
    {
        if (UniqueAddressWithValue(txDb.key, txDb.value))
        {
            reason = "Unique value is already set";
            return false;
        }
    }
    
    if (txDb.key.type == INFO_UNIQUE || txDb.key.type == INFO_ID || txDb.key.type == INFO_WRITE_ONCE)
    {
        SQLite::Statement query(db, "SELECT COUNT(*) "
                                "FROM TxDbEntry "
                                "WHERE address=? AND key_type=? AND key=?");
        query.bind(1, record.addr.ToString());
        query.bind(2, txDb.key.type);
        query.bind(3, txDb.key.keyString);
        int count = 0;
        while (query.executeStep())
        {
            count = query.getColumn(0);
        }
        
        if (count > 0)
        {
            reason = "A non-overwritable value has already been set";
            return false;
        }
    }
    
    return true;
}

void CTxInfoStore::DumpLatestInfos()
{
    SQLite::Statement query(db, "SELECT address, key_type, key, value "
                                "FROM TxDbEntry "
                                "WHERE is_latest=1 "
                                "ORDER BY address");
    while (query.executeStep()) {
        std::string addr = query.getColumn(0);
        int keyType = query.getColumn(1);
        std::string key = query.getColumn(2);
        std::string value = query.getColumn(3);
        
        printf("%s: %s\n", addr.c_str(), CTxInfo(TxInfoKey((TxInfoType)keyType, key), value).ToString().c_str());
    }
}








