#pragma once
#include <QString>
#include "database/DatabaseManager.h"

namespace app {

struct AppSettings {
    database::DbConfig dbConfig;
    QString storeName{"Bismillah Pharmacy & Retail"};
    QString storePhone{"042-35889900"};
    QString storeAddress{"Main Market, Lahore"};
    QString dslNumber{"DSL-LHR-2024-884"};
    QString ntnNumber{"NTN-9876543-2"};
    bool printBismillahHeader{true};
    QString receiptFooter{"JazakAllah Khair! No refund without bill."};
    int activeCounterId{1};
    bool allowNegativeStock{false};
    bool fefoEnabled{true};
    QString defaultPrinterName;
};

class Configuration {
public:
    static Configuration& instance();

    bool load();
    bool save();

    const AppSettings& settings() const { return m_settings; }
    AppSettings& mutableSettings() { return m_settings; }

private:
    Configuration() = default;
    AppSettings m_settings;
    QString configFilePath() const;
};

} // namespace app
