#include "app/Application.h"
#include "app/Configuration.h"
#include "database/DatabaseManager.h"
#include "database/Migrations.h"
#include "ui/Theme.h"
#include <QMessageBox>

namespace app {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setApplicationName("PakPharmacyPOS");
    setApplicationVersion("1.0.0");
    setOrganizationName("PakPharmacy");
}

Application::~Application()
{
    database::DatabaseManager::instance().close();
}

bool Application::initialize()
{
    // 1. Load configuration
    Configuration::instance().load();

    // 2. Initialize Database Connection
    auto& dbMgr = database::DatabaseManager::instance();
    auto dbResult = dbMgr.initialize(Configuration::instance().settings().dbConfig);
    if (dbResult.isErr()) {
        // If Postgres fails, fallback to SQLite automatically for resilience
        if (Configuration::instance().settings().dbConfig.type == database::DatabaseType::PostgreSQL) {
            Configuration::instance().mutableSettings().dbConfig.type = database::DatabaseType::SQLite;
            dbResult = dbMgr.initialize(Configuration::instance().settings().dbConfig);
        }

        if (dbResult.isErr()) {
            QMessageBox::critical(nullptr, "Database Error", dbResult.error().userMessage());
            return false;
        }
    }

    // 3. Run versioned migrations (tables + seed data)
    auto migResult = database::Migrations::runMigrations();
    if (migResult.isErr()) {
        QMessageBox::critical(nullptr, "Migration Error", migResult.error().userMessage());
        return false;
    }

    // 4. Apply application stylesheet
    setStyleSheet(ui::Theme::getAppStyleSheet());

    // 5. Show Main Window
    m_mainWindow = std::make_unique<ui::MainWindow>();
    m_mainWindow->show();

    return true;
}

} // namespace app
