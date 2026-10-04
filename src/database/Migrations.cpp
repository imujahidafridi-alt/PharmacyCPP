#include "database/Migrations.h"
#include "database/DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace database {

QString Migrations::getInitialSchemaSql(bool isPostgres)
{
    QString idType = isPostgres ? "SERIAL PRIMARY KEY" : "INTEGER PRIMARY KEY AUTOINCREMENT";
    QString textType = isPostgres ? "VARCHAR(255)" : "TEXT";

    return QString(R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            applied_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS users (
            id %1,
            username %2 UNIQUE NOT NULL,
            full_name %2 NOT NULL,
            password_hash %2 NOT NULL,
            role %2 NOT NULL DEFAULT 'Cashier',
            is_active INTEGER DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS categories (
            id %1,
            name %2 UNIQUE NOT NULL,
            description TEXT,
            is_discountable INTEGER NOT NULL DEFAULT 1,
            default_disc_pct REAL NOT NULL DEFAULT 0.0,
            max_discount_pct REAL NOT NULL DEFAULT 15.0
        );

        CREATE TABLE IF NOT EXISTS items (
            id %1,
            code %2 UNIQUE NOT NULL,
            name %2 NOT NULL,
            category_id INTEGER REFERENCES categories(id),
            brand %2,
            barcode %2,
            sale_price_paisa BIGINT NOT NULL DEFAULT 0,
            purchase_cost_paisa BIGINT NOT NULL DEFAULT 0,
            tp_paisa BIGINT NOT NULL DEFAULT 0,
            min_stock_alert INTEGER DEFAULT 10,
            is_active INTEGER DEFAULT 1,
            is_medicine INTEGER DEFAULT 0,
            generic_name %2,
            strength %2,
            dosage_form %2,
            is_prescription_required INTEGER DEFAULT 0,
            pieces_per_strip INTEGER DEFAULT 1,
            strips_per_box INTEGER DEFAULT 1,
            strip_sale_price_paisa BIGINT DEFAULT 0,
            box_sale_price_paisa BIGINT DEFAULT 0,
            is_discountable INTEGER DEFAULT NULL,
            override_disc_pct REAL DEFAULT NULL,
            min_margin_pct REAL DEFAULT 0.0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS item_barcodes (
            id %1,
            item_id INTEGER NOT NULL REFERENCES items(id) ON DELETE CASCADE,
            barcode %2 UNIQUE NOT NULL
        );

        CREATE TABLE IF NOT EXISTS batches (
            id %1,
            item_id INTEGER NOT NULL REFERENCES items(id) ON DELETE RESTRICT,
            batch_number %2 NOT NULL,
            expiry_date DATE NOT NULL,
            cost_price_paisa BIGINT NOT NULL DEFAULT 0,
            sale_price_paisa BIGINT NOT NULL DEFAULT 0,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(item_id, batch_number)
        );

        CREATE TABLE IF NOT EXISTS stock_balances (
            item_id INTEGER NOT NULL REFERENCES items(id) ON DELETE RESTRICT,
            batch_id INTEGER NOT NULL REFERENCES batches(id) ON DELETE RESTRICT,
            quantity INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (item_id, batch_id)
        );

        CREATE TABLE IF NOT EXISTS stock_movements (
            id %1,
            item_id INTEGER NOT NULL REFERENCES items(id),
            batch_id INTEGER REFERENCES batches(id),
            movement_type %2 NOT NULL,
            quantity_delta INTEGER NOT NULL,
            balance_after INTEGER NOT NULL,
            reason TEXT,
            reference_id INTEGER,
            user_id INTEGER REFERENCES users(id),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS customers (
            id %1,
            name %2 NOT NULL,
            phone %2,
            address TEXT,
            baqaya_paisa BIGINT NOT NULL DEFAULT 0,
            notes TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS customer_ledger (
            id %1,
            customer_id INTEGER NOT NULL REFERENCES customers(id),
            reference_type %2 NOT NULL,
            reference_id INTEGER,
            debit_paisa BIGINT DEFAULT 0,
            credit_paisa BIGINT DEFAULT 0,
            balance_after_paisa BIGINT NOT NULL,
            description TEXT,
            user_id INTEGER REFERENCES users(id),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS suppliers (
            id %1,
            name %2 NOT NULL,
            phone %2,
            address TEXT,
            baqaya_paisa BIGINT NOT NULL DEFAULT 0,
            notes TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS supplier_ledger (
            id %1,
            supplier_id INTEGER NOT NULL REFERENCES suppliers(id),
            reference_type %2 NOT NULL,
            reference_id INTEGER,
            debit_paisa BIGINT DEFAULT 0,
            credit_paisa BIGINT DEFAULT 0,
            balance_after_paisa BIGINT NOT NULL,
            description TEXT,
            user_id INTEGER REFERENCES users(id),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS sales (
            id %1,
            bill_number %2 UNIQUE NOT NULL,
            customer_id INTEGER NOT NULL REFERENCES customers(id),
            payment_type %2 NOT NULL DEFAULT 'Cash',
            subtotal_paisa BIGINT NOT NULL,
            discount_paisa BIGINT NOT NULL DEFAULT 0,
            net_total_paisa BIGINT NOT NULL,
            cash_received_paisa BIGINT NOT NULL DEFAULT 0,
            change_given_paisa BIGINT NOT NULL DEFAULT 0,
            status %2 NOT NULL DEFAULT 'Completed',
            cancel_reason TEXT,
            user_id INTEGER REFERENCES users(id),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS sale_items (
            id %1,
            sale_id INTEGER NOT NULL REFERENCES sales(id) ON DELETE CASCADE,
            item_id INTEGER NOT NULL REFERENCES items(id),
            batch_id INTEGER REFERENCES batches(id),
            unit_type %2 NOT NULL DEFAULT 'Piece',
            display_qty INTEGER NOT NULL DEFAULT 1,
            atomic_qty INTEGER NOT NULL DEFAULT 1,
            unit_price_paisa BIGINT NOT NULL,
            total_amount_paisa BIGINT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS purchases (
            id %1,
            invoice_number %2,
            supplier_id INTEGER REFERENCES suppliers(id),
            is_cash_market INTEGER NOT NULL DEFAULT 0,
            total_cost_paisa BIGINT NOT NULL,
            amount_paid_paisa BIGINT NOT NULL DEFAULT 0,
            notes TEXT,
            user_id INTEGER REFERENCES users(id),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS purchase_items (
            id %1,
            purchase_id INTEGER NOT NULL REFERENCES purchases(id) ON DELETE CASCADE,
            item_id INTEGER NOT NULL REFERENCES items(id),
            batch_id INTEGER REFERENCES batches(id),
            atomic_qty INTEGER NOT NULL,
            unit_cost_paisa BIGINT NOT NULL,
            total_cost_paisa BIGINT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS cash_sessions (
            id %1,
            counter_id INTEGER NOT NULL DEFAULT 1,
            user_id INTEGER NOT NULL REFERENCES users(id),
            opening_float_paisa BIGINT NOT NULL DEFAULT 0,
            cash_sales_paisa BIGINT DEFAULT 0,
            cash_received_paisa BIGINT DEFAULT 0,
            cash_returns_paisa BIGINT DEFAULT 0,
            expected_cash_paisa BIGINT DEFAULT 0,
            actual_cash_paisa BIGINT DEFAULT 0,
            difference_paisa BIGINT DEFAULT 0,
            discrepancy_notes TEXT,
            is_open INTEGER NOT NULL DEFAULT 1,
            opened_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            closed_at TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS audit_logs (
            id %1,
            user_id INTEGER,
            action %2 NOT NULL,
            entity %2 NOT NULL,
            entity_id INTEGER,
            old_value TEXT,
            new_value TEXT,
            reason TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS settings (
            key %2 PRIMARY KEY,
            value TEXT
        );
    )").arg(idType, textType);
}

QString Migrations::getSeedDataSql(bool /*isPostgres*/)
{
    return QString(R"(
        INSERT INTO users (id, username, full_name, password_hash, role, is_active)
        VALUES 
            (1, 'admin', 'Administrator', 'admin123', 'Admin', 1),
            (2, 'cashier', 'Counter Cashier', 'cashier123', 'Cashier', 1)
        ON CONFLICT DO NOTHING;

        INSERT INTO customers (id, name, phone, address, baqaya_paisa, notes)
        VALUES 
            (1, 'Walk-in Customer', '', 'Store Counter', 0, 'Default counter customer')
        ON CONFLICT DO NOTHING;

        INSERT INTO suppliers (id, name, phone, address, baqaya_paisa, notes)
        VALUES 
            (1, 'Cash Purchase / Local Market', '', 'Local Wholesale Market', 0, 'General direct cash wholesale')
        ON CONFLICT DO NOTHING;

        INSERT INTO categories (id, name, description, is_discountable, default_disc_pct, max_discount_pct)
        VALUES 
            (1, 'Tablets & Capsules', 'Oral solid pharmaceuticals', 1, 10.0, 15.0),
            (2, 'Syrups & Suspensions', 'Liquid oral pharmaceuticals', 1, 7.0, 12.0),
            (3, 'Injections & Infusions', 'Parenteral medications', 1, 5.0, 10.0),
            (4, 'Creams & Ointments', 'Topical medications', 1, 5.0, 10.0),
            (5, 'Baby Care', 'Pampers, baby food and accessories (FMCG strictly at MRP)', 0, 0.0, 0.0),
            (6, 'Personal Care', 'Soaps, face wash, shampoos (FMCG strictly at MRP)', 0, 0.0, 0.0),
            (7, 'Beverages & Nutrition', 'Energy drinks, juices, supplements (FMCG strictly at MRP)', 0, 0.0, 0.0),
            (8, 'General Retail / First Aid', 'Bandages, cotton, thermometers, retail', 1, 5.0, 10.0)
        ON CONFLICT DO NOTHING;

        -- Default initial settings
        INSERT INTO settings (key, value) VALUES
            ('store_name', 'Bismillah Pharmacy & General Store'),
            ('store_phone', '042-35889900 / 0300-1234567'),
            ('store_address', 'Main Market, Lahore, Pakistan'),
            ('receipt_footer', 'JazakAllah Khair for shopping with us!'),
            ('allow_negative_stock', 'false'),
            ('fefo_enabled', 'true')
        ON CONFLICT DO NOTHING;
    )");
}

core::Result<void, core::AppError> Migrations::runMigrations()
{
    auto& dbMgr = DatabaseManager::instance();
    bool isPostgres = (dbMgr.databaseType() == DatabaseType::PostgreSQL);

    auto result = dbMgr.executeTransaction([isPostgres](QSqlDatabase& db) -> core::Result<void, core::AppError> {
        QSqlQuery q(db);
        
        // Execute schema statements
        QString schemaSql = getInitialSchemaSql(isPostgres);
        QStringList statements = schemaSql.split(';', Qt::SkipEmptyParts);
        for (const QString& stmt : statements) {
            QString trimmed = stmt.trimmed();
            if (trimmed.isEmpty()) continue;
            if (!q.exec(trimmed)) {
                return core::Result<void, core::AppError>::err(
                    core::AppError::fromSqlError(q.lastError().text(), "Schema migration failed on statement.")
                );
            }
        }

        // Schema upgrades for existing databases (idempotent / non-destructive)
        QStringList upgrades = {
            "ALTER TABLE categories ADD COLUMN is_discountable INTEGER NOT NULL DEFAULT 1",
            "ALTER TABLE categories ADD COLUMN default_disc_pct REAL NOT NULL DEFAULT 0.0",
            "ALTER TABLE categories ADD COLUMN max_discount_pct REAL NOT NULL DEFAULT 15.0",
            "ALTER TABLE items ADD COLUMN tp_paisa BIGINT NOT NULL DEFAULT 0",
            "ALTER TABLE items ADD COLUMN is_discountable INTEGER DEFAULT NULL",
            "ALTER TABLE items ADD COLUMN override_disc_pct REAL DEFAULT NULL",
            "ALTER TABLE items ADD COLUMN min_margin_pct REAL DEFAULT 0.0",
            "UPDATE items SET tp_paisa = purchase_cost_paisa WHERE tp_paisa = 0",
            "UPDATE categories SET is_discountable = 0, default_disc_pct = 0.0, max_discount_pct = 0.0 WHERE id IN (5, 6, 7)",
            "UPDATE categories SET is_discountable = 1, default_disc_pct = 10.0, max_discount_pct = 15.0 WHERE id = 1",
            "UPDATE categories SET is_discountable = 1, default_disc_pct = 7.0, max_discount_pct = 12.0 WHERE id = 2",
            "UPDATE categories SET is_discountable = 1, default_disc_pct = 5.0, max_discount_pct = 10.0 WHERE id IN (3, 4, 8)"
        };
        for (const auto& u : upgrades) {
            q.exec(u); // silently succeed or ignore if column already exists
        }

        // Execute seed data
        QString seedSql = getSeedDataSql(isPostgres);
        QStringList seedStatements = seedSql.split(';', Qt::SkipEmptyParts);
        for (const QString& stmt : seedStatements) {
            QString trimmed = stmt.trimmed();
            if (trimmed.isEmpty()) continue;
            if (!q.exec(trimmed)) {
                // If conflict syntax varies on some engines, continue gracefully
                qWarning() << "Seed note:" << q.lastError().text();
            }
        }

        // Record migration version 1
        q.exec("INSERT INTO schema_migrations (version) VALUES (1);");

        return core::Result<void, core::AppError>::ok();
    });

    return result;
}

} // namespace database
