#include "lookupinfodialog.h"
#include "ui_lookupinfodialog.h"
#include "addressbookpage.h"
#include "guiutil.h"

#include <QDataWidgetMapper>
#include <QMessageBox>

LookupInfoDialog::LookupInfoDialog(AddressTableModel *addressModelIn, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LookupInfoDialog), model(0), proxyModel(0), address(""), addressModel(addressModelIn)
{
    ui->setupUi(this);

    GUIUtil::setupAddressWidget(ui->addressEdit, this);
    
    setWindowTitle(tr("Lookup Address Info"));
    ui->addressEdit->setEnabled(true);
    
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Context menu actions
    QAction *copyValueAction = new QAction(tr("Copy Value"), this);
    QAction *copyDateAction = new QAction(tr("Copy Date"), this);
    QAction *copyKeyAction = new QAction(tr("Copy Key"), this);
    
    contextMenu = new QMenu();
    contextMenu->addAction(copyValueAction);
    contextMenu->addAction(copyDateAction);
    contextMenu->addAction(copyKeyAction);
    
    connect(copyValueAction, SIGNAL(triggered()), this, SLOT(onCopyValueAction()));
    connect(copyDateAction, SIGNAL(triggered()), this, SLOT(onCopyDateAction()));
    connect(copyKeyAction, SIGNAL(triggered()), this, SLOT(onCopyKeyAction()));
    
    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
    connect(ui->addressEdit, SIGNAL(textChanged(QString)), this, SLOT(onTextChanged(QString)));
}

LookupInfoDialog::~LookupInfoDialog()
{
    if (model) {
        ui->tableView->setModel(0);
        
        delete proxyModel;
        delete model;
    }
    
    delete ui;
}

void LookupInfoDialog::accept()
{
    QDialog::accept();
}

QString LookupInfoDialog::getAddress() const
{
    return address;
}

void LookupInfoDialog::setAddress(const QString &address)
{
    this->address = address;
    if (ui->addressEdit->text() != address)
    {
        ui->addressEdit->setText(address);
    }
    
    if (model)
    {
        ui->tableView->setModel(0);
        delete proxyModel;
        delete model;
    }
    
    model = new LookupInfoModel(address);
    
    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortRole(Qt::EditRole);
    
    ui->tableView->setModel(proxyModel);
    
    ui->tableView->horizontalHeader()->resizeSection(LookupInfoModel::Date, 160);
    ui->tableView->horizontalHeader()->resizeSection(LookupInfoModel::Key, 120);
    ui->tableView->horizontalHeader()->setResizeMode(LookupInfoModel::Value, QHeaderView::Stretch);
    
    ui->tableView->sortByColumn(0, Qt::AscendingOrder);
}

void LookupInfoDialog::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void LookupInfoDialog::on_addressBookButton_clicked()
{
    if(!addressModel)
        return;
    
    AddressBookPage dlg(AddressBookPage::ForLookup, AddressBookPage::SendingTab, this);
    dlg.setModel(addressModel);
    if(dlg.exec())
    {
        setAddress(dlg.getReturnValue());
    }
}

void LookupInfoDialog::onCopyValueAction()
{
    GUIUtil::copyEntryData(ui->tableView, LookupInfoModel::Value, Qt::DisplayRole);
}

void LookupInfoDialog::onCopyDateAction()
{
    GUIUtil::copyEntryData(ui->tableView, LookupInfoModel::Date, Qt::DisplayRole);
}

void LookupInfoDialog::onCopyKeyAction()
{
    GUIUtil::copyEntryData(ui->tableView, LookupInfoModel::Key, Qt::DisplayRole);
}

void LookupInfoDialog::onTextChanged(const QString &text)
{
    setAddress(text);
}











