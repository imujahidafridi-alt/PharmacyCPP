#include "database/DatabaseManager.h"
#include <QSqlError>
#include <QDebug>
#include <QFileInfo>
#include <QDir>

namespace database {

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager inst;
    return inst;
}

DatabaseManager::~DatabaseManager()
{
    close();
}

core::Result<void, core::AppError> DatabaseManager::initialize(const DbConfig& config)
{
    m_config = config;
    
    QString driverName = (config.type == DatabaseType::PostgreSQL) ? "QPSQL" : "QSQLITE";
    
    if (!QSqlDatabase::isDriverAvailable(driverName)) {
        // If QPSQL is not available, check if we can fallback to SQLite
        if (config.type == DatabaseType::PostgreSQL) {
            return core::Result<void, core::AppError>::err(
                core::AppError(core::ErrorCode::DatabaseConnectionLost,
                               "PostgreSQL driver (QPSQL) is not available in Qt plugins.",
                               "Store database driver is missing. Please contact system support.")
            );
        }
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(driverName);
    if (config.type == DatabaseType::PostgreSQL) {
        db.setHostName(config.host);
        db.setPort(config.port);
        db.setDatabaseName(config.databaseName);
        db.setUserName(config.username);
        db.setPassword(config.password);
    } else {
        // SQLite setup
        QFileInfo fi(config.sqlitePath);
        QDir().mkpath(fi.absolutePath());
        db.setDatabaseName(config.sqlitePath);
    }

    if (!db.open()) {
        QString errText = db.lastError().text();
        return core::Result<void, core::AppError>::err(
            core::AppError::fromSqlError(errText, "Failed to connect to store database.")
        );
    }

    if (config.type == DatabaseType::SQLite) {
        // Enable WAL mode and foreign keys for SQLite
        QSqlQuery pragma(db);
        pragma.exec("PRAGMA journal_mode = WAL;");
        pragma.exec("PRAGMA foreign_keys = ON;");
        pragma.exec("PRAGMA synchronous = NORMAL;");
    }

    m_initialized = true;
    return core::Result<void, core::AppError>::ok();
}

void DatabaseManager::close()
{
    if (m_initialized) {
        QString connName = QSqlDatabase::defaultConnection;
        {
            QSqlDatabase db = QSqlDatabase::database(connName, false);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connName);
        m_initialized = false;
    }
}

bool DatabaseManager::isConnected() const
{
    if (!m_initialized) return false;
    QSqlDatabase db = QSqlDatabase::database();
    return db.isOpen();
}

QSqlDatabase DatabaseManager::connection(const QString& connectionName)
{
    if (connectionName.isEmpty()) {
        return QSqlDatabase::database();
    }
    return QSqlDatabase::database(connectionName);
}

core::Result<void, core::AppError> DatabaseManager::executeTransaction(
    const std::function<core::Result<void, core::AppError>(QSqlDatabase& db)>& work,
    const QString& connName)
{
    QSqlDatabase db = connection(connName);
    if (!db.isOpen()) {
        return core::Result<void, core::AppError>::err(
            core::AppError(core::ErrorCode::DatabaseConnectionLost)
        );
    }

    if (!db.transaction()) {
        return core::Result<void, core::AppError>::err(
            core::AppError::fromSqlError(db.lastError().text(), "Could not start transaction.")
        );
    }

    auto res = work(db);
    if (res.isErr()) {
        db.rollback();
        return res;
    }

    if (!db.commit()) {
        db.rollback();
        return core::Result<void, core::AppError>::err(
            core::AppError::fromSqlError(db.lastError().text(), "Transaction commit failed.")
        );
    }

    return core::Result<void, core::AppError>::ok();
}

core::Result<QSqlQuery, core::AppError> DatabaseManager::executeQuery(
    const QString& queryString,
    const QVariantList& bindValues,
    const QString& connName)
{
    QSqlDatabase db = connection(connName);
    if (!db.isOpen()) {
        return core::Result<QSqlQuery, core::AppError>::err(
            core::AppError(core::ErrorCode::DatabaseConnectionLost)
        );
    }

    QSqlQuery query(db);
    if (!query.prepare(queryString)) {
        return core::Result<QSqlQuery, core::AppError>::err(
            core::AppError::fromSqlError(query.lastError().text())
        );
    }

    for (const auto& val : bindValues) {
        query.addBindValue(val);
    }

    if (!query.exec()) {
        return core::Result<QSqlQuery, core::AppError>::err(
            core::AppError::fromSqlError(query.lastError().text())
        );
    }

    return core::Result<QSqlQuery, core::AppError>::ok(std::move(query));
}

} // namespace database
