#ifndef LOOKUPINFODIALOG_H
#define LOOKUPINFODIALOG_H

#include "addresstablemodel.h"
#include "lookupinfomodel.h"

#include <QDialog>
#include <QSortFilterProxyModel>
#include <QMenu>

QT_BEGIN_NAMESPACE
class QDataWidgetMapper;
QT_END_NAMESPACE

namespace Ui
{
    class LookupInfoDialog;
}
class AddressTableModel;

/** Dialog for viewing info transactions on an address
 */
class LookupInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LookupInfoDialog(AddressTableModel *addressModel, QWidget *parent = 0);
    ~LookupInfoDialog();

    void accept();

    QString getAddress() const;
    void setAddress(const QString &address);
private:
    Ui::LookupInfoDialog *ui;
    LookupInfoModel *model;
    QSortFilterProxyModel *proxyModel;    

    QString address;
    
    AddressTableModel *addressModel;
    
    QMenu *contextMenu;
    
private slots:
    void on_addressBookButton_clicked();
    
    void onCopyValueAction();
    void onCopyKeyAction();
    void onCopyDateAction();
    void onTextChanged(const QString &text);

    /** Spawn contextual menu (right mouse menu) for lookup info entry */
    void contextualMenu(const QPoint &point);
};

#endif // LOOKUPINFODIALOG_H
