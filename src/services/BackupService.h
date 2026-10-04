#pragma once
#include <QString>
#include <QDateTime>
#include "core/Result.h"
#include "core/Errors.h"

namespace services {

struct BackupInfo {
    QString filePath;
    QDateTime timestamp;
    qint64 sizeBytes{0};
};

class BackupService {
public:
    static BackupService& instance();

    // Perform immediate backup
    core::Result<QString, core::AppError> createBackup(const QString& targetDirectory = QString());

    // Restore backup with automatic safety pre-restore backup
    core::Result<void, core::AppError> restoreBackup(const QString& backupFilePath);

    // Query recent backups
    std::vector<BackupInfo> listBackups(const QString& directory = QString());

    QString defaultBackupDirectory() const;

private:
    BackupService() = default;
};

} // namespace services
