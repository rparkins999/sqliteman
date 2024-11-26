/* Copyright © 2024 Richard Parkins
 *
 * This is the single static member function of GetColumnList.
 * It returns a struct ColumnList containing a list of column names
 * and a boolean flag idicating whether the
 */

#include <QSqlQuery>
#include <QSqlRecord>

#include "database.h"
#include "getcolumnlist.h"
#include "utils.h"

struct ColumnList GetColumnList::getColumnList(QString schema, QString table) {
    struct ColumnList result = {QStringList(), QString(), false};

    if (!table.isEmpty()) {
        /* This PRAGMA returns a nonempty table only if the table
         * or view referenced is a WITHOUT ROWID table
         */
        QString stmt("PRAGMA "
                     + Utils::q(schema) +
                     ".index_info("
                     + Utils::q(table)
                     + ");");
        QSqlQuery query = Database::doSql(stmt);
        if (query.isActive())
        {
            bool without_rowid = query.first();
            // This may not be necessary, but just to be on the safe side...
            query.clear();
            /* This rather silly SELECT statement returns a nonempty table
             * if the parameter table is in fact a table, and not a view.
             */
            stmt = "SELECT type from "
                   + Database::getMaster(schema)
                   + " WHERE name = "
                   + Utils::q(table)
                   + " AND type = \"table\";";
            query = Database::doSql(stmt);
            if (query.isActive())
            {
                result.queryingTable = query.first();
                // This may not be necessary, but just to be on the safe side...
                query.clear();
                /* This rather silly SELECT statement just gets the column names.
                 * It should work for views and virtual tables
                 * as well as plain tables.
                 */
                stmt = "SELECT * FROM "
                       + Utils::q(schema)
                       + "."
                       + Utils::q(table)
                       + " LIMIT 0;";
                query = Database::doSql(stmt);
                if (query.isActive())
                {
                    QSqlRecord rec = query.record();
                    int n = rec.count();
                    bool rowid = true; // no column named rowid
                    bool _rowid_ = true; // no column named _rowid_
                    bool oid = true; // no column named oid
                    for (int i = 0; i < n; ++i)
                    {
                        QString name(rec.fieldName(i));
                        result.columns << name;
                        if (name.compare("rowid", Qt::CaseInsensitive) == 0)
                            { rowid = false; }
                        if (name.compare("_rowid_", Qt::CaseInsensitive) == 0)
                            { _rowid_ = false; }
                        if (name.compare("oid", Qt::CaseInsensitive) == 0)
                            { oid = false; }
                    }
                    if (result.queryingTable && !without_rowid)
                    {
                        /* If this is a table, but not a WITHOUT ROWID table,
                         * and a rowid alias is available
                         * (not the name of another column),
                         * we include it at the start of the column list.
                         */
                        if (rowid)
                        {
                            result.columns.insert(0, QString("rowid"));
                            result.rowid = "rowid";
                        } else if (_rowid_)
                        {
                            result.columns.insert(0, QString("_rowid_")); result.rowid = "_rowid_";
                        } else if (oid)
                        {
                            result.columns.insert(0, QString("oid"));
                            result.rowid = "oid";
                        }
                    }
                }
            }
        }
    }
    return  result;
}
