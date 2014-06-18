#ifndef LOOKUPINFOMODEL_H
#define LOOKUPINFOMODEL_H

#include <QAbstractTableModel>
#include <QStringList>

class LookupInfoTablePriv;

/**
   Qt model of the lookup info dialog table.
 */
class LookupInfoModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit LookupInfoModel(const QString &address);
    ~LookupInfoModel();

    enum ColumnIndex
    {
        Date = 0,
        Key = 1,
        Value = 2
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex & index, const QVariant & value, int role);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex & parent) const;
    bool removeRows(int row, int count, const QModelIndex & parent = QModelIndex());
    Qt::ItemFlags flags(const QModelIndex & index) const;

private:
    QString address;
    LookupInfoTablePriv *priv;
    QStringList columns;
    
public slots:
    /* Update info list from core. Invalidates any indices.
     */
    void update();
};

#endif // LOOKUPINFOMODEL_H
