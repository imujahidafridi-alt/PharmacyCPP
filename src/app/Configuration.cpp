#include "app/Configuration.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace app {

Configuration& Configuration::instance()
{
    static Configuration inst;
    return inst;
}

QString Configuration::configFilePath() const
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return appData + "/pharmacy_config.ini";
}

bool Configuration::load()
{
    QSettings s(configFilePath(), QSettings::IniFormat);
    
    // Database settings
    QString dbTypeStr = s.value("database/type", "sqlite").toString().toLower();
    if (dbTypeStr == "postgresql" || dbTypeStr == "postgres") {
        m_settings.dbConfig.type = database::DatabaseType::PostgreSQL;
    } else {
        m_settings.dbConfig.type = database::DatabaseType::SQLite;
    }
    m_settings.dbConfig.host = s.value("database/host", "127.0.0.1").toString();
    m_settings.dbConfig.port = s.value("database/port", 5432).toInt();
    m_settings.dbConfig.databaseName = s.value("database/name", "pharmacy_store").toString();
    m_settings.dbConfig.username = s.value("database/username", "postgres").toString();
    m_settings.dbConfig.password = s.value("database/password", "").toString();
    
    QString appDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_settings.dbConfig.sqlitePath = s.value("database/sqlite_path", appDir + "/pharmacy.db").toString();

    // Store settings
    m_settings.storeName = s.value("store/name", "Bismillah Pharmacy & Retail").toString();
    m_settings.storePhone = s.value("store/phone", "042-35889900").toString();
    m_settings.storeAddress = s.value("store/address", "Main Market, Lahore").toString();
    m_settings.dslNumber = s.value("store/dsl_number", "DSL-LHR-2024-884").toString();
    m_settings.ntnNumber = s.value("store/ntn_number", "NTN-9876543-2").toString();
    m_settings.printBismillahHeader = s.value("store/print_bismillah", true).toBool();
    m_settings.receiptFooter = s.value("store/receipt_footer", "JazakAllah Khair! No refund without bill.").toString();
    m_settings.activeCounterId = s.value("pos/counter_id", 1).toInt();
    m_settings.allowNegativeStock = s.value("pos/allow_negative_stock", false).toBool();
    m_settings.fefoEnabled = s.value("pos/fefo_enabled", true).toBool();
    m_settings.defaultPrinterName = s.value("printing/printer_name", "").toString();

    return true;
}

bool Configuration::save()
{
    QSettings s(configFilePath(), QSettings::IniFormat);

    s.setValue("database/type", (m_settings.dbConfig.type == database::DatabaseType::PostgreSQL) ? "postgresql" : "sqlite");
    s.setValue("database/host", m_settings.dbConfig.host);
    s.setValue("database/port", m_settings.dbConfig.port);
    s.setValue("database/name", m_settings.dbConfig.databaseName);
    s.setValue("database/username", m_settings.dbConfig.username);
    s.setValue("database/password", m_settings.dbConfig.password);
    s.setValue("database/sqlite_path", m_settings.dbConfig.sqlitePath);

    s.setValue("store/name", m_settings.storeName);
    s.setValue("store/phone", m_settings.storePhone);
    s.setValue("store/address", m_settings.storeAddress);
    s.setValue("store/dsl_number", m_settings.dslNumber);
    s.setValue("store/ntn_number", m_settings.ntnNumber);
    s.setValue("store/print_bismillah", m_settings.printBismillahHeader);
    s.setValue("store/receipt_footer", m_settings.receiptFooter);
    s.setValue("pos/counter_id", m_settings.activeCounterId);
    s.setValue("pos/allow_negative_stock", m_settings.allowNegativeStock);
    s.setValue("pos/fefo_enabled", m_settings.fefoEnabled);
    s.setValue("printing/printer_name", m_settings.defaultPrinterName);

    s.sync();
    return true;
}

} // namespace app
