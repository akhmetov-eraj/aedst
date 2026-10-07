import GRDB

/// Migrations owned by `AedstPersistence` itself (schema every feature can
/// rely on). Feature-specific tables belong in that feature's own
/// `registerMigrations(in:)`, not here.
enum AedstBaseMigrations {
    static func register(in migrator: inout DatabaseMigrator) {
        migrator.registerMigration("core.v1_createKeyValue") { db in
            try db.create(table: AedstKeyValueRecord.databaseTableName) { table in
                table.primaryKey("key", .text)
                table.column("value", .blob).notNull()
                table.column("updatedAt", .datetime).notNull()
            }
        }
    }
}
