#include "services/BackupService.h"
#include "database/DatabaseManager.h"
#include "app/Configuration.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QProcess>

namespace services {

BackupService& BackupService::instance()
{
    static BackupService inst;
    return inst;
}

QString BackupService::defaultBackupDirectory() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString dir = base + "/Pharmacy_Backups";
    QDir().mkpath(dir);
    return dir;
}

core::Result<QString, core::AppError> BackupService::createBackup(const QString& targetDirectory)
{
    QString outDir = targetDirectory.isEmpty() ? defaultBackupDirectory() : targetDirectory;
    QDir().mkpath(outDir);

    auto& dbMgr = database::DatabaseManager::instance();
    QString timestampStr = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

    if (dbMgr.databaseType() == database::DatabaseType::SQLite) {
        QString srcPath = dbMgr.config().sqlitePath;
        QString destPath = QString("%1/PharmacyBackup_%2.db").arg(outDir, timestampStr);
        if (QFile::copy(srcPath, destPath)) {
            return core::Result<QString, core::AppError>::ok(destPath);
        } else {
            return core::Result<QString, core::AppError>::err(
                core::AppError(core::ErrorCode::GeneralDatabaseError, "Could not copy database file for backup.")
            );
        }
    } else {
        // PostgreSQL pg_dump
        QString destPath = QString("%1/PharmacyBackup_%2.sql").arg(outDir, timestampStr);
        QProcess proc;
        QStringList args;
        args << "-h" << dbMgr.config().host
             << "-p" << QString::number(dbMgr.config().port)
             << "-U" << dbMgr.config().username
             << "-F" << "p"
             << "-f" << destPath
             << dbMgr.config().databaseName;

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (!dbMgr.config().password.isEmpty()) {
            env.insert("PGPASSWORD", dbMgr.config().password);
        }
        proc.setProcessEnvironment(env);
        proc.start("pg_dump", args);
        if (proc.waitForFinished(60000) && proc.exitCode() == 0) {
            return core::Result<QString, core::AppError>::ok(destPath);
        } else {
            return core::Result<QString, core::AppError>::err(
                core::AppError(core::ErrorCode::GeneralDatabaseError, proc.readAllStandardError(), "Database backup failed.")
            );
        }
    }
}

core::Result<void, core::AppError> BackupService::restoreBackup(const QString& backupFilePath)
{
    if (!QFile::exists(backupFilePath)) {
        return core::Result<void, core::AppError>::err(
            core::AppError(core::ErrorCode::ValidationFailed, "Backup file not found.")
        );
    }

    // Safety step: Always create pre-restore snapshot first (SRS Section 49)
    createBackup();

    auto& dbMgr = database::DatabaseManager::instance();
    if (dbMgr.databaseType() == database::DatabaseType::SQLite) {
        QString currentDb = dbMgr.config().sqlitePath;
        dbMgr.close();
        QFile::remove(currentDb);
        if (QFile::copy(backupFilePath, currentDb)) {
            dbMgr.initialize(dbMgr.config());
            return core::Result<void, core::AppError>::ok();
        } else {
            dbMgr.initialize(dbMgr.config());
            return core::Result<void, core::AppError>::err(
                core::AppError(core::ErrorCode::GeneralDatabaseError, "Failed to restore database file.")
            );
        }
    }

    return core::Result<void, core::AppError>::ok();
}

std::vector<BackupInfo> BackupService::listBackups(const QString& directory)
{
    QString searchDir = directory.isEmpty() ? defaultBackupDirectory() : directory;
    QDir dir(searchDir);
    QStringList filters;
    filters << "PharmacyBackup_*.db" << "PharmacyBackup_*.sql";
    QFileInfoList list = dir.entryInfoList(filters, QDir::Files, QDir::Time);

    std::vector<BackupInfo> results;
    for (const auto& fi : list) {
        BackupInfo b;
        b.filePath = fi.absoluteFilePath();
        b.timestamp = fi.lastModified();
        b.sizeBytes = fi.size();
        results.push_back(b);
    }
    return results;
}

} // namespace services
