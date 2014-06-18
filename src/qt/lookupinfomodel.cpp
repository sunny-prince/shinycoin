#include "lookupinfomodel.h"
#include "guiutil.h"

#include "txinfo.h"
#include "wallet.h"

#include <QFont>
#include <QColor>

#include <boost/algorithm/string.hpp>

struct LookupInfoEntry
{
    int nTime;
    CTxInfo info;

    LookupInfoEntry() {}
    LookupInfoEntry(int nTimeIn, const CTxInfo &infoIn) : nTime(nTimeIn), info(infoIn) {}
};

// Private implementation
class LookupInfoTablePriv
{
private:
    QString &address;
    
public:
    QList<LookupInfoEntry> cachedInfoTable;

    LookupInfoTablePriv(QString &addressIn) : address(addressIn) {}

    void refreshInfoTable()
    {
        cachedInfoTable.clear();

        {
            LOCK(cs_main);
            
            std::vector<TxInfoRecord> vecRecords = ptxinfoStore->RecordsWithAddress(address.toStdString());
            BOOST_FOREACH(const TxInfoRecord &record, vecRecords)
            {
                if (mapBlockIndex.count(record.blockHash) == 0)
                    continue;
                
                CBlockIndex *pindex = mapBlockIndex[record.blockHash];
                
                cachedInfoTable.append(LookupInfoEntry(pindex->nTime, record.info));
            }
        }
    }

    int size()
    {
        return cachedInfoTable.size();
    }

    LookupInfoEntry *index(int idx)
    {
        if(idx >= 0 && idx < cachedInfoTable.size())
        {
            return &cachedInfoTable[idx];
        }
        else
        {
            return 0;
        }
    }
};

LookupInfoModel::LookupInfoModel(const QString &addressIn) :
    address(addressIn)
{
    priv = new LookupInfoTablePriv(address);
    
    columns << tr("Date Added") << tr("Key") << tr("Value");
    priv->refreshInfoTable();
}

LookupInfoModel::~LookupInfoModel()
{
    delete priv;
}

int LookupInfoModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int LookupInfoModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant LookupInfoModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    LookupInfoEntry *rec = static_cast<LookupInfoEntry*>(index.internalPointer());

    if (role == Qt::DisplayRole)
    {
        switch(index.column())
        {
        case Date:
            return GUIUtil::dateTimeStr(rec->nTime);
        case Key:
            return rec->info.key.ToString().c_str();
        case Value:
            return rec->info.value.c_str();
        }
    }
    else if (role == Qt::EditRole) // sort
    {
        switch(index.column())
        {
        case Date:
            return rec->nTime;
        case Key:
            return rec->info.key.ToString().c_str();
        case Value:
            std::string val = rec->info.value;
            boost::to_lower(val);
            return val.c_str();
        }
    }
    else if (role == Qt::ForegroundRole) {
        return QColor(0, 0, 0);
    }
    
    return QVariant();
}

bool LookupInfoModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    return false;
}

QVariant LookupInfoModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags LookupInfoModel::flags(const QModelIndex & index) const
{
    if(!index.isValid())
        return 0;
    
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

QModelIndex LookupInfoModel::index(int row, int column, const QModelIndex & parent) const
{
    Q_UNUSED(parent);
    LookupInfoEntry *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

void LookupInfoModel::update()
{
    beginResetModel();
    priv->refreshInfoTable();
    endResetModel();
}

bool LookupInfoModel::removeRows(int row, int count, const QModelIndex & parent)
{
    return false;
}




