#pragma once
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <functional>
#include <memory>
#include "core/Result.h"
#include "core/Errors.h"

namespace database {

enum class DatabaseType {
    PostgreSQL,
    SQLite
};

struct DbConfig {
    DatabaseType type{DatabaseType::SQLite};
    QString host{"127.0.0.1"};
    int port{5432};
    QString databaseName{"pharmacy_store"};
    QString username{"postgres"};
    QString password;
    QString sqlitePath{"pharmacy.db"};
};

class DatabaseManager {
public:
    static DatabaseManager& instance();

    core::Result<void, core::AppError> initialize(const DbConfig& config);
    void close();

    bool isConnected() const;
    DatabaseType databaseType() const { return m_config.type; }
    const DbConfig& config() const { return m_config; }

    QSqlDatabase connection(const QString& connectionName = QString());
    
    // RAII Transaction helper: rolls back automatically if callable returns an error or throws
    core::Result<void, core::AppError> executeTransaction(
        const std::function<core::Result<void, core::AppError>(QSqlDatabase& db)>& work,
        const QString& connName = QString()
    );

    // Run query with automatic error mapping
    core::Result<QSqlQuery, core::AppError> executeQuery(
        const QString& queryString,
        const QVariantList& bindValues = {},
        const QString& connName = QString()
    );

private:
    DatabaseManager() = default;
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    DbConfig m_config;
    bool m_initialized{false};
};

} // namespace database
