#pragma once
#include <QString>
#include "core/Result.h"
#include "core/Errors.h"

namespace database {

class Migrations {
public:
    static core::Result<void, core::AppError> runMigrations();

    static QString getInitialSchemaSql(bool isPostgres);
    static QString getSeedDataSql(bool isPostgres);
};

} // namespace database
