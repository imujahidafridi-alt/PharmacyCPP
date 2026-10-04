#include "ui/settings/SettingsWindow.h"
#include "app/Configuration.h"
#include "services/BackupService.h"
#include "database/DatabaseManager.h"
#include "database/Migrations.h"
#include "ui/components/AppTextInput.h"
#include "ui/components/AppDropdown.h"
#include "ui/components/AppButton.h"
#include "ui/components/AppToast.h"
#include "ui/components/ReceiptPreviewDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QPrinterInfo>
#include <QSqlError>

namespace ui {

SettingsWindow::SettingsWindow(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 12, 14, 12);
    mainLayout->setSpacing(10);

    auto* topColumns = new QHBoxLayout();
    topColumns->setSpacing(10);

    // 1. Store Profile & Receipt Group
    auto* storeGroup = new QGroupBox("Store Profile & Receipt Branding", this);
    storeGroup->setStyleSheet("QGroupBox { font-size: 11px; font-weight: 700; color: #334155; }");
    auto* storeLayout = new QFormLayout(storeGroup);
    storeLayout->setContentsMargins(10, 10, 10, 10);
    storeLayout->setSpacing(6);

    m_storeNameEdit = new AppTextInput(AppTextInput::Size::Small, storeGroup);
    m_storePhoneEdit = new AppTextInput(AppTextInput::Size::Small, storeGroup);
    m_storeAddressEdit = new AppTextInput(AppTextInput::Size::Small, storeGroup);
    m_dslEdit = new AppTextInput(AppTextInput::Size::Small, storeGroup);
    m_ntnEdit = new AppTextInput(AppTextInput::Size::Small, storeGroup);
    m_bismillahCheck = new QCheckBox("Print Bismillah Header on Invoices", storeGroup);
    m_bismillahCheck->setStyleSheet("font-size: 11px; font-weight: 600; color: #475569;");
    m_receiptFooterEdit = new AppTextInput(AppTextInput::Size::Small, storeGroup);

    storeLayout->addRow("Store Name:", m_storeNameEdit);
    storeLayout->addRow("Phone Numbers:", m_storePhoneEdit);
    storeLayout->addRow("Address:", m_storeAddressEdit);
    storeLayout->addRow("Drug License (DSL):", m_dslEdit);
    storeLayout->addRow("NTN / STRN #:", m_ntnEdit);
    storeLayout->addRow("", m_bismillahCheck);
    storeLayout->addRow("Receipt Policy/Footer:", m_receiptFooterEdit);

    auto* previewReceiptBtn = new AppButton("Preview Thermal Receipt (80mm)", AppButton::Variant::Info, AppButton::Size::Small, storeGroup);
    storeLayout->addRow("", previewReceiptBtn);
    topColumns->addWidget(storeGroup, 1);

    // 2. Hardware & Rules Group
    auto* hwGroup = new QGroupBox("Hardware & Operational Rules", this);
    hwGroup->setStyleSheet("QGroupBox { font-size: 11px; font-weight: 700; color: #334155; }");
    auto* hwLayout = new QFormLayout(hwGroup);
    hwLayout->setContentsMargins(10, 10, 10, 10);
    hwLayout->setSpacing(8);

    m_printerCombo = new AppDropdown(AppDropdown::Size::Small, hwGroup);
    m_printerCombo->addItem("None / Screen Preview Only", "");
    for (const auto& printer : QPrinterInfo::availablePrinters()) {
        m_printerCombo->addItem(printer.printerName(), printer.printerName());
    }

    m_allowNegativeStockCheck = new QCheckBox("Allow Negative Stock (Fast counter sales when stock count lagging)", hwGroup);
    m_allowNegativeStockCheck->setStyleSheet("font-size: 11px; font-weight: 600; color: #475569;");
    m_fefoCheck = new QCheckBox("Enforce FEFO (Automatically sell earliest expiry batches first)", hwGroup);
    m_fefoCheck->setStyleSheet("font-size: 11px; font-weight: 600; color: #475569;");
    m_fefoCheck->setChecked(true);

    hwLayout->addRow("Receipt Printer:", m_printerCombo);
    hwLayout->addRow("", m_allowNegativeStockCheck);
    hwLayout->addRow("", m_fefoCheck);
    topColumns->addWidget(hwGroup, 1);

    mainLayout->addLayout(topColumns);

    // 3. Database Connection Switcher & Multi-Counter LAN Mode
    auto* dbGroup = new QGroupBox("Database Engine & Multi-Counter Network Setup", this);
    dbGroup->setStyleSheet("QGroupBox { font-size: 11px; font-weight: 700; color: #334155; }");
    auto* dbLayout = new QVBoxLayout(dbGroup);
    dbLayout->setContentsMargins(10, 8, 10, 8);
    dbLayout->setSpacing(6);

    auto* dbHeaderRow = new QHBoxLayout();
    auto* dbTypeLbl = new QLabel("Database Engine:", dbGroup);
    dbTypeLbl->setStyleSheet("font-size: 11px; font-weight: 700; color: #334155;");
    m_dbTypeCombo = new AppDropdown(AppDropdown::Size::Small, dbGroup);
    m_dbTypeCombo->addItem("SQLite (Single PC / Local Embedded)", "sqlite");
    m_dbTypeCombo->addItem("PostgreSQL (Multi-Counter Store LAN)", "postgresql");
    m_dbTypeCombo->setFixedWidth(260);

    dbHeaderRow->addWidget(dbTypeLbl);
    dbHeaderRow->addWidget(m_dbTypeCombo);
    dbHeaderRow->addStretch();
    dbLayout->addLayout(dbHeaderRow);

    // PostgreSQL LAN server fields container
    m_pgFieldsContainer = new QWidget(dbGroup);
    auto* pgGrid = new QGridLayout(m_pgFieldsContainer);
    pgGrid->setContentsMargins(0, 4, 0, 4);
    pgGrid->setSpacing(6);

    m_dbHostEdit = new AppTextInput(AppTextInput::Size::Small, m_pgFieldsContainer);
    m_dbHostEdit->setPlaceholderText("e.g. 192.168.1.100 or localhost");
    m_dbPortEdit = new AppTextInput(AppTextInput::Size::Small, m_pgFieldsContainer);
    m_dbPortEdit->setText("5432");
    m_dbNameEdit = new AppTextInput(AppTextInput::Size::Small, m_pgFieldsContainer);
    m_dbNameEdit->setText("pharmacy_store");
    m_dbUserEdit = new AppTextInput(AppTextInput::Size::Small, m_pgFieldsContainer);
    m_dbUserEdit->setText("postgres");
    m_dbPassEdit = new AppTextInput(AppTextInput::Size::Small, m_pgFieldsContainer);
    m_dbPassEdit->setEchoMode(QLineEdit::Password);

    auto addPgField = [pgGrid](int r, int c, const QString& label, QWidget* w) {
        auto* l = new QLabel(label);
        l->setStyleSheet("font-size: 10px; font-weight: 700; color: #64748B;");
        pgGrid->addWidget(l, r * 2, c);
        pgGrid->addWidget(w, r * 2 + 1, c);
    };

    addPgField(0, 0, "SERVER IP / HOST", m_dbHostEdit);
    addPgField(0, 1, "PORT", m_dbPortEdit);
    addPgField(0, 2, "DATABASE NAME", m_dbNameEdit);
    addPgField(1, 0, "USERNAME", m_dbUserEdit);
    addPgField(1, 1, "PASSWORD", m_dbPassEdit);

    auto* testConnBtn = new AppButton("Test Connection", AppButton::Variant::Secondary, AppButton::Size::Small, m_pgFieldsContainer);
    auto* initDbBtn = new AppButton("Initialize Remote Tables", AppButton::Variant::Info, AppButton::Size::Small, m_pgFieldsContainer);
    
    auto* pgActionRow = new QHBoxLayout();
    pgActionRow->addWidget(testConnBtn);
    pgActionRow->addWidget(initDbBtn);
    pgActionRow->addStretch();
    pgGrid->addLayout(pgActionRow, 3, 2);

    dbLayout->addWidget(m_pgFieldsContainer);
    mainLayout->addWidget(dbGroup);

    // 4. Backup & Recovery Card
    auto* backupGroup = new QGroupBox("Backup & Recovery", this);
    backupGroup->setStyleSheet("QGroupBox { font-size: 11px; font-weight: 700; color: #334155; }");
    auto* backupLayout = new QHBoxLayout(backupGroup);
    backupLayout->setContentsMargins(10, 8, 10, 8);
    m_lastBackupLabel = new QLabel("Last Backup: Ready", backupGroup);
    m_lastBackupLabel->setStyleSheet("font-size: 11px; font-weight: 600; color: #475569;");

    auto* backupNowBtn = new AppButton("Create Backup Snapshot Now", AppButton::Variant::Secondary, AppButton::Size::Small, backupGroup);
    auto* restoreBtn = new AppButton("Restore Database...", AppButton::Variant::Danger, AppButton::Size::Small, backupGroup);

    backupLayout->addWidget(m_lastBackupLabel);
    backupLayout->addStretch();
    backupLayout->addWidget(backupNowBtn);
    backupLayout->addWidget(restoreBtn);
    mainLayout->addWidget(backupGroup);

    // Save All Settings Button
    auto* bottomLayout = new QHBoxLayout();
    auto* saveBtn = new AppButton("Save All Settings", AppButton::Variant::Primary, AppButton::Size::Medium, this);
    saveBtn->setMinimumWidth(160);
    saveBtn->setFixedHeight(32);
    bottomLayout->addStretch();
    bottomLayout->addWidget(saveBtn);
    mainLayout->addLayout(bottomLayout);

    connect(saveBtn, &QPushButton::clicked, this, &SettingsWindow::handleSaveSettings);
    connect(backupNowBtn, &QPushButton::clicked, this, &SettingsWindow::handleBackupNow);
    connect(restoreBtn, &QPushButton::clicked, this, &SettingsWindow::handleRestoreBackup);
    connect(previewReceiptBtn, &QPushButton::clicked, this, &SettingsWindow::handlePreviewReceipt);
    connect(testConnBtn, &QPushButton::clicked, this, &SettingsWindow::handleTestConnection);
    connect(initDbBtn, &QPushButton::clicked, this, &SettingsWindow::handleInitDatabase);
    connect(m_dbTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsWindow::handleDbTypeChanged);

    loadSettings();
}

void SettingsWindow::loadSettings()
{
    const auto& s = app::Configuration::instance().settings();
    m_storeNameEdit->setText(s.storeName);
    m_storePhoneEdit->setText(s.storePhone);
    m_storeAddressEdit->setText(s.storeAddress);
    m_dslEdit->setText(s.dslNumber);
    m_ntnEdit->setText(s.ntnNumber);
    m_bismillahCheck->setChecked(s.printBismillahHeader);
    m_receiptFooterEdit->setText(s.receiptFooter);

    int idx = m_printerCombo->findData(s.defaultPrinterName);
    if (idx >= 0) m_printerCombo->setCurrentIndex(idx);

    m_allowNegativeStockCheck->setChecked(s.allowNegativeStock);
    m_fefoCheck->setChecked(s.fefoEnabled);

    int dbIdx = (s.dbConfig.type == database::DatabaseType::PostgreSQL) ? 1 : 0;
    m_dbTypeCombo->setCurrentIndex(dbIdx);
    m_dbHostEdit->setText(s.dbConfig.host);
    m_dbPortEdit->setText(QString::number(s.dbConfig.port));
    m_dbNameEdit->setText(s.dbConfig.databaseName);
    m_dbUserEdit->setText(s.dbConfig.username);
    m_dbPassEdit->setText(s.dbConfig.password);

    handleDbTypeChanged(dbIdx);

    auto backups = services::BackupService::instance().listBackups();
    if (!backups.empty()) {
        m_lastBackupLabel->setText(QString("Last Backup: %1").arg(backups.front().timestamp.toString("dd-MMM-yyyy hh:mm AP")));
    }
}

void SettingsWindow::handleDbTypeChanged(int index)
{
    bool isPostgres = (index == 1);
    m_pgFieldsContainer->setVisible(isPostgres);
}

void SettingsWindow::handleTestConnection()
{
    QString host = m_dbHostEdit->text().trimmed();
    int port = m_dbPortEdit->text().toInt();
    QString dbName = m_dbNameEdit->text().trimmed();
    QString user = m_dbUserEdit->text().trimmed();
    QString pass = m_dbPassEdit->text();

    if (host.isEmpty() || dbName.isEmpty() || user.isEmpty()) {
        AppToast::showWarning(this, "Please fill in Server Host, Database Name, and Username.");
        return;
    }

    // Attempt test connection using a temporary QSqlDatabase instance
    {
        QString testConnName = "test_pg_connection";
        QSqlDatabase testDb = QSqlDatabase::addDatabase("QPSQL", testConnName);
        testDb.setHostName(host);
        testDb.setPort(port > 0 ? port : 5432);
        testDb.setDatabaseName(dbName);
        testDb.setUserName(user);
        testDb.setPassword(pass);

        if (testDb.open()) {
            AppToast::showSuccess(this, QString("Connection successful! Connected to PostgreSQL at %1").arg(host));
            testDb.close();
        } else {
            AppToast::showError(this, QString("Connection failed: %1").arg(testDb.lastError().text()));
        }
    }
    QSqlDatabase::removeDatabase("test_pg_connection");
}

void SettingsWindow::handleInitDatabase()
{
    auto confirm = QMessageBox::question(
        this, "Initialize Remote Database",
        "This will apply table migrations to the specified PostgreSQL server.\nProceed?",
        QMessageBox::Yes | QMessageBox::No
    );
    if (confirm != QMessageBox::Yes) return;

    handleSaveSettings();
    auto migRes = database::Migrations::runMigrations();
    if (migRes.isOk()) {
        AppToast::showSuccess(this, "Remote database schema initialized successfully!");
    } else {
        AppToast::showError(this, QString("Migration error: %1").arg(migRes.error().userMessage()));
    }
}

void SettingsWindow::handlePreviewReceipt()
{
    // Generate sample preview bill
    domain::Sale sampleSale;
    sampleSale.billNumber = "BILL-PREVIEW-001";
    sampleSale.createdAt = QDateTime::currentDateTime();
    sampleSale.customerName = "Sample Customer";
    sampleSale.cashierName = "Counter Cashier";
    sampleSale.paymentType = domain::PaymentType::Cash;

    domain::CartItem it1;
    it1.itemName = "Panadol 500mg (Strip)";
    it1.displayQty = 2;
    it1.unitPrice = core::Money::fromRupees(40.0);
    it1.totalAmount = core::Money::fromRupees(80.0);

    domain::CartItem it2;
    it2.itemName = "Augmentin 625mg (Box)";
    it2.displayQty = 1;
    it2.unitPrice = core::Money::fromRupees(350.0);
    it2.totalAmount = core::Money::fromRupees(350.0);

    sampleSale.items = {it1, it2};
    sampleSale.subtotal = core::Money::fromRupees(430.0);
    sampleSale.discount = core::Money::fromRupees(30.0);
    sampleSale.netTotal = core::Money::fromRupees(400.0);
    sampleSale.cashReceived = core::Money::fromRupees(500.0);
    sampleSale.changeGiven = core::Money::fromRupees(100.0);

    ReceiptPreviewDialog dlg(sampleSale, this);
    dlg.exec();
}

void SettingsWindow::handleSaveSettings()
{
    auto& cfg = app::Configuration::instance().mutableSettings();
    cfg.storeName = m_storeNameEdit->text().trimmed();
    cfg.storePhone = m_storePhoneEdit->text().trimmed();
    cfg.storeAddress = m_storeAddressEdit->text().trimmed();
    cfg.dslNumber = m_dslEdit->text().trimmed();
    cfg.ntnNumber = m_ntnEdit->text().trimmed();
    cfg.printBismillahHeader = m_bismillahCheck->isChecked();
    cfg.receiptFooter = m_receiptFooterEdit->text().trimmed();
    cfg.defaultPrinterName = m_printerCombo->currentData().toString();
    cfg.allowNegativeStock = m_allowNegativeStockCheck->isChecked();
    cfg.fefoEnabled = m_fefoCheck->isChecked();

    cfg.dbConfig.type = (m_dbTypeCombo->currentData().toString() == "postgresql") 
        ? database::DatabaseType::PostgreSQL 
        : database::DatabaseType::SQLite;
    cfg.dbConfig.host = m_dbHostEdit->text().trimmed();
    cfg.dbConfig.port = m_dbPortEdit->text().toInt() > 0 ? m_dbPortEdit->text().toInt() : 5432;
    cfg.dbConfig.databaseName = m_dbNameEdit->text().trimmed();
    cfg.dbConfig.username = m_dbUserEdit->text().trimmed();
    cfg.dbConfig.password = m_dbPassEdit->text();

    app::Configuration::instance().save();
    AppToast::showSuccess(this, "Settings saved successfully.");
}

void SettingsWindow::handleBackupNow()
{
    auto res = services::BackupService::instance().createBackup();
    if (res.isErr()) {
        AppToast::showError(this, QString("Backup failed: %1").arg(res.error().userMessage()));
        return;
    }

    AppToast::showSuccess(this, QString("Backup snapshot created successfully:\n%1").arg(res.value()));
    loadSettings();
}

void SettingsWindow::handleRestoreBackup()
{
    QString backupDir = services::BackupService::instance().defaultBackupDirectory();
    QString filePath = QFileDialog::getOpenFileName(this, "Select Backup File to Restore", backupDir, "Database Backups (*.db *.sql)");
    if (filePath.isEmpty()) return;

    auto confirm = QMessageBox::warning(
        this, "Confirm Restore",
        "Restoring a backup will replace current store data.\nA safety backup will be made automatically before restoring.\n\nContinue?",
        QMessageBox::Yes | QMessageBox::No
    );

    if (confirm != QMessageBox::Yes) return;

    auto res = services::BackupService::instance().restoreBackup(filePath);
    if (res.isErr()) {
        AppToast::showError(this, QString("Restore failed: %1").arg(res.error().userMessage()));
        return;
    }

    AppToast::showSuccess(this, "Database restored successfully. Application is ready.");
}

} // namespace ui
