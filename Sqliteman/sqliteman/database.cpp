/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QtCore/QTextStream>
#include <QtCore/QVariant>
#include <QtCore/QFile>
#include <QMessageBox>

#include "database.h"
#include "preferences.h"
#include "sqlparser.h"
#include "utils.h"

/* Internal error handling
 * User errors should be reported in the status area.
 */
void Database::exception(const QString & message)
{
	QMessageBox::critical(0, tr("Sqliteman internal Error"),
		message
		+ tr("\nPlease report in ")
		+ "https://github.com/rparkins999/sqliteman/issues "
		+ tr("with as much information as possible about what you were doing when the error occurred."));
}

QSqlQuery Database::doSql(QString statement)
{
	return QSqlQuery(statement, QSqlDatabase::database(SESSION_NAME));
}
QSqlQuery Database::runSql(QString statement)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		exception(query.lastQuery());
		query.clear();
	}
	return query;
}

bool Database::execSql(QString statement)
{
	QSqlQuery query = runSql(statement);
	return !query.isActive();
}

QString Database::sessionName(const QString & schema)
{
	return QString("%1_%2").arg(SESSION_NAME).arg(schema);
}

DbAttach Database::getDatabases()
{
	DbAttach ret;
	QSqlQuery query("PRAGMA database_list;", QSqlDatabase::database(SESSION_NAME));

	if (query.lastError().isValid())
	{
		exception(tr("Cannot get databases list. %1")
				  .arg(query.lastError().text()));
		return ret;
	}
	while(query.next())
		ret.insertMulti(query.value(1).toString(), query.value(2).toString());
	return ret;
}

SqlParser * Database::parseTable(const QString & table, const QString & schema)
{
	// Build a query string to SELECT the CREATE statement from sqlite_master
	QString createSQL = QString("SELECT sql FROM ")
						+ getMaster(schema)
						+ " WHERE lower(name) = "
						+ Utils::q(table.toLower())
						+ ";";
	// Run the query

	QSqlQuery createQuery(createSQL, QSqlDatabase::database(SESSION_NAME));
	// Make sure the query ran successfully
	if(createQuery.lastError().isValid()) {
		exception(tr("Error grabbing CREATE statement: ")
				  + table
				  + ": "
				  + createQuery.lastError().text());
	}
	// Position the Query on the first (only) result
	createQuery.first();
	// Grab the complete CREATE statement
	QString createStatement = createQuery.value(0).toString();

	// Parse the CREATE statement
	return new SqlParser(createStatement);
}

QList<FieldInfo> Database::tableFields(const QString & table, const QString & schema)
{
	SqlParser * parser = parseTable(table, schema);
	QList<FieldInfo> result(parser->m_fields);
	delete parser;
	return result;
}
QStringList Database::indexFields(const QString & index, const QString &schema)
{
	QString sql = QString("PRAGMA ")
				  + Utils::q(schema)
				  + ".INDEX_INFO("
				  + Utils::q(index)
				  + ");";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	QStringList fields;

	if (query.lastError().isValid())
	{
        exception(tr("Error while getting the fields of ")
        		  + index
        		  + ": "
        		  + query.lastError().text());
		return fields;
	}

	while (query.next())
		fields.append(query.value(2).toString());

	return fields;
}

DbObjects Database::getObjects(const QString type, const QString schema)
{
	DbObjects objs;

	QString sql;
	if (type.isNull())
	{
        sql = QString("SELECT name, tbl_name FROM ")
			  + getMaster(schema)
			  + ";";
	}
	else
	{
        sql = QString("SELECT name, tbl_name FROM ")
			  + getMaster(schema)
			  + " WHERE lower(type) = "
			  + Utils::q(type.toLower())
			  + " and name not like 'sqlite_%';";
	}

	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	while(query.next())
		objs.insertMulti(query.value(1).toString(), query.value(0).toString());

	if(query.lastError().isValid())
		exception(tr("Error getting the list of ")
				  + type
				  + ": "
				  + query.lastError().text());

	return objs;
}

QStringList Database::getSysIndexes(const QString & table, const QString & schema)
{
	QStringList orig = Database::getObjects("index", schema).values(table);
	// really all indexes
	QStringList sysIx;
	QSqlQuery query(QString("PRAGMA ")
					+ Utils::q(schema)
					+ ".index_list("
					+ Utils::q(table)
					+ ");", QSqlDatabase::database(SESSION_NAME));

	QString curr;
	while(query.next())
	{
		curr = query.value(1).toString();
		if (!orig.contains(curr))
			sysIx.append(curr);
	}

	if(query.lastError().isValid())
		exception(tr("Error getting the list of indexes: ")
				  + query.lastError().text());

	return sysIx;
}

DbObjects Database::getSysObjects(const QString & schema)
{
	DbObjects objs;

    QSqlQuery query(QString("SELECT name, tbl_name FROM %1 "
							"WHERE type = 'table' and name like 'sqlite_%';")
					.arg(getMaster(schema)),
					QSqlDatabase::database(SESSION_NAME));

	if (schema.compare("temp", Qt::CaseInsensitive))
	{
		objs.insert("sqlite_master", "");
	}
	else
	{
		objs.insert("sqlite_temp_master", "");
	}
	while(query.next())
		objs.insertMulti(query.value(1).toString(), query.value(0).toString());

	if(query.lastError().isValid())
		exception(tr("Error getting the system catalogue: %1.").arg(query.lastError().text()));

	return objs;
}

bool Database::dropView(const QString & view, const QString & schema)
{
	QString sql = QString("DROP VIEW ")
				  + Utils::q(schema)
				  + "."
				  + Utils::q(view)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		exception(tr("Error while dropping the view ")
				  + view
				  + ": "
				  + query.lastError().text());
		return false;
	}
	return true;
}

bool Database::dropIndex(const QString & name, const QString & schema)
{
	QString sql = QString("DROP INDEX ")
				  + Utils::q(schema)
				  + "."
				  + Utils::q(name)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		exception(tr("Error while dropping the index ")
				  + name
				  + ": "
				  + query.lastError().text());
		return false;
	}
	return true;
}

bool Database::exportSql(const QString & fileName)
{
	QFile file(fileName);
	
	if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		exception(tr("Unable to open file %1 for writing.").arg(fileName));
		return false;
	}

	QTextStream stream(&file);
	
	// Run query for whole schema
	QString sql = "SELECT sql FROM sqlite_master;";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if (query.lastError().isValid())
	{
		exception(tr(
			"Error while reading sqlite_master: %1."
        ).arg(query.lastError().text()));
		return false;
	}
	
	stream << "BEGIN TRANSACTION;\n";
	while(query.next())
    {
        QString row = query.value(0).toString();
        row.replace("CREATE INDEX", "CREATE INDEX IF NOT EXISTS");
        row.replace("CREATE TABLE", "CREATE TABLE IF NOT EXISTS");
        row.replace("CREATE TRIGGER", "CREATE TRIGGER IF NOT EXISTS");
        row.replace("CREATE VIEW", "CREATE VIEW IF NOT EXISTS");
		stream << row << ";\n";
    }
	stream << "COMMIT;\n";
	
	file.close();
	return true;
}

// TODO/FIXME: it definitely requires worker thread - to unfreeze GUI
bool Database::dumpDatabase(const QString & fileName)
{
	QFile file(fileName);
	
	if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		exception(tr("Unable to open file %1 for writing.").arg(fileName));
		return false;
	}

	QTextStream stream(&file);
	
	// Run query for whole schema
	QString sql = "SELECT sql FROM sqlite_master;";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if (query.lastError().isValid())
	{
		exception(tr(
			"Error while reading sqlite_master: %1."
        ).arg(query.lastError().text()));
		return false;
	}
	
	stream << "BEGIN TRANSACTION;\n";
	while(query.next())
    {
        QString row = query.value(0).toString();
        row.replace("CREATE INDEX", "CREATE INDEX IF NOT EXISTS");
        row.replace("CREATE TABLE", "CREATE TABLE IF NOT EXISTS");
        row.replace("CREATE TRIGGER", "CREATE TRIGGER IF NOT EXISTS");
        row.replace("CREATE VIEW", "CREATE VIEW IF NOT EXISTS");
		stream << row << ";\n";
    }

    // Run query on each table
	sql = "SELECT name, sql FROM sqlite_master WHERE type = \"table\";";
	QSqlQuery q1(sql, QSqlDatabase::database(SESSION_NAME));
	if (q1.lastError().isValid())
	{
		exception(tr(
			"Error while reading table names: %1."
        ).arg(q1.lastError().text()));
		return false;
	}
	while(q1.next())
    {
        QString tablename = q1.value(0).toString();
        sql = "SELECT * FROM ";
        sql.append(Utils::q(tablename)).append(";");
        QSqlQuery q2(sql, QSqlDatabase::database(SESSION_NAME));
        if (q2.lastError().isValid())
        {
            exception(tr(
                "Error while reading table %1: %2."
            ).arg(Utils::q(tablename)).arg(q2.lastError().text()));
            return false;
        }
        QString fnames;
        while(q2.next())
        {
            int i;
            QSqlRecord rec = q2.record();
            QString sep = "";
            if (fnames.isNull())
            {
                fnames = "INSERT OR REPLACE INTO ";
                fnames.append(Utils::q(tablename)).append(" ( ");
                QString sep = "";
                for (i = 0; i < rec.count(); ++i)
                {
                    fnames.append(sep).append(Utils::q(rec.fieldName(i)));
                    sep = ", ";
                }
                fnames.append(" ) VALUES ( ");
            }
            stream << fnames;
            sep = "";
            for (i = 0; i < rec.count(); ++i)
            {
                QVariant v =rec.value(i);
                if (v.isNull())
                {
                    stream << sep << "NULL";
                }
                else if (v.type() == QVariant::LongLong)
                {
                    stream << sep << v.toString();
                }
                else
                {
                    stream << sep << Utils::q(v.toString(), "'");
                }
                sep = ", ";
            }
            stream << " );\n";
        }
    }
	stream << "COMMIT;\n";
	
	file.close();
	return true;
}

QString Database::describeObject(const QString & name,
								 const QString & schema,
								 const QString & type)
{
    QString sql = QString("select sql from ")
    			  + getMaster(schema)
    			  + " where lower(name) = "
    			  + Utils::q(name.toLower(), "'")
    			  + " and lower(type) = "
				  + Utils::q(type.toLower())
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if (query.lastError().isValid())
	{
		exception(tr("Error while describing object ")
				  + name
				  + ": "
				  + query.lastError().text());
		return "";
	}
	
	while(query.next())
		return query.value(0).toString();
	
	return "";
}

bool Database::dropTrigger(const QString & name, const QString & schema)
{
	QString sql = QString("DROP TRIGGER ")
				  + Utils::q(schema)
				  + "."
				  + Utils::q(name)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		exception(tr("Error while dropping the trigger ")
				  + name
				  + ": "
				  + query.lastError().text());
		return false;
	}
	return true;
}

QString Database::hex(const QByteArray & val)
{
	const char hexdigits[] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
	};
	QString ret("X'");
	for (int i = 0; i < val.size(); ++i)
	{
		ret += hexdigits[(val.at(i)>>4)&0xf];
		ret += hexdigits[val.at(i)&0xf];
	}
	return ret + "'";
}

QString Database::pragma(const QString & name)
{
	QString statement;
	if (name.compare("case_sensitive_like", Qt::CaseInsensitive) == 0)
	{
		statement = QString("VALUES ('a' NOT LIKE 'A') ;");
	}
	else
	{
		statement = QString("PRAGMA main.%1;").arg(name);
	}
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		exception(tr("Error executing: %1.").arg(query.lastError().text()));
		return "error";
	}

	while(query.next())
		return query.value(0).toString();
	return tr("Not Set");
}

sqlite3 * Database::sqlite3handle()
{
	QVariant v = QSqlDatabase::database(SESSION_NAME).driver()->handle();
	if (!v.isValid())
	{
		exception(tr("DB driver is not valid"));
		return 0;
	}
	if (qstrcmp(v.typeName(), "sqlite3*") != 0)
	{
		exception(tr("DB type name does not equal sqlite3"));
		return 0;
	}

	sqlite3 *handle = *static_cast<sqlite3 **>(v.data());
	if (handle == 0)
		exception(tr("DB handler is not valid"));

	return handle;
}

bool Database::setEnableExtensions(bool enable)
{
	QVariant v = QSqlDatabase::database(SESSION_NAME).driver()->handle();
	if (v.isValid())
	{
		return false;
	}
	sqlite3 * handle = Database::sqlite3handle();
	if (handle && sqlite3_enable_load_extension(handle, enable ? 1 : 0) != SQLITE_OK)
	{
		if (enable)
			exception(tr("Failed to enable extension loading"));
		else
			exception(tr("Failed to disable extension loading"));
		return false;
	}
	return true;
}

QStringList Database::loadExtension(const QStringList & list)
{
	sqlite3 * handle = Database::sqlite3handle();
	if (!handle)
		return QStringList();

	QStringList retval;

    QStringList::const_iterator i;
    for (i = list.constBegin(); i != list.constEnd(); ++i) {
        const char *zProc = 0;
		char *zErrMsg = 0;
		int rc;

		rc = sqlite3_load_extension(
            handle, i->toUtf8().data(), zProc, &zErrMsg);
		if (rc != SQLITE_OK)
		{
			exception(tr("Error loading extension\n")
					  + *i
					  + "\n"
					  + QString::fromLocal8Bit(zErrMsg));
			sqlite3_free(zErrMsg);
		}
		else
			retval.append(*i);
	}
	return retval;
}

QString Database::getMaster(const QString &schema)
{
    if (schema.compare(QString("temp"), Qt::CaseInsensitive)==0)
	{
		return QString("sqlite_temp_master");
	}
	else
	{
		return QString("%1.sqlite_master").arg(Utils::q(schema));
	}
}

bool Database::isAutoCommit()
{
    // can't use sqlite3handle() because there may be no database open
    QVariant v = QSqlDatabase::database(SESSION_NAME).driver()->handle();
    if (v.isNull())
    {
        return 1;
    }
    else
    {
        sqlite3 *handle = *static_cast<sqlite3 **>(v.data());
        return sqlite3_get_autocommit(handle) != 0;
    }
}

struct do_exec_state {
	int count;
	char * name;
	sqlite3 * db;
	char * errmsg;
	char * result;
};

extern "C" int do_exec_callback(void * p, int n, char ** data, char **names)
{
	struct do_exec_state * state = (struct do_exec_state *)p;
	if (state->errmsg) { return 0; }
	bool first = true;
	int i;
	if (state->name)
	{
		if (state->count == 0)
		{
			// First result row, create the table
			QString create(QString("CREATE TABLE %1 (")
						   .arg(state->name));
			for (i = 0; i < n; ++i)
			{
				if (first) { first = false; }
				else { create += ","; }
				create += Utils::q(names[i]);
			}
			create += ");"; 
			sqlite3_exec(state->db, create.toUtf8().data(),
						 NULL, NULL, &(state->errmsg));
			if (state->errmsg) { return 0; }
		}
		state->count++;
		// add the data row
		QString insert(QString("INSERT INTO %1 VALUES (")
					   .arg(state->name));
		first = true;
		for (i = 0; i < n; ++i)
		{
			if (first) { first = false; }
			else { insert += ","; }
			insert += Utils::q(data[i], "'");
		}
		insert += ");"; 
		sqlite3_exec(state->db, insert.toUtf8().data(),
					 NULL, NULL, &(state->errmsg));
		if (state->errmsg)
		{
			// Oh dear, we failed after creating the table: try to drop it again
			QString drop(QString("DROP TABLE %1 ;").arg(state->name));
			sqlite3_exec(state->db, drop.toUtf8().data(), NULL, NULL, NULL);
		}
	}
	else
	{
		if (state->count == 0)
		{
			state->result = strdup(*data);
		}
		state->count++;
	}
	return 0;
}

extern "C" void do_exec(sqlite3_context* context,
						  int n, sqlite3_value** value)
{
	struct do_exec_state state;
	state.count = 0;
	if (sqlite3_value_type(value[0]) == SQLITE_NULL)
	{
		state.name = NULL;
	}
	else
	{
		state.name = strdup((const char *)sqlite3_value_text(value[0]));
	}
	state.db = sqlite3_context_db_handle(context);
	state.errmsg = NULL;
	state.result = NULL;
	char * errmsg = NULL;
	char * sql = strdup((const char *)sqlite3_value_text(value[1]));
	sqlite3_exec(state.db, sql, do_exec_callback, &state, &errmsg);
	if (state.errmsg)
	{
		sqlite3_result_error(context, state.errmsg, -1);
		sqlite3_free(state.errmsg);
	}
	else if (state.name == NULL)
	{
		sqlite3_result_text(context, state.result, -1, SQLITE_TRANSIENT);
	}
	else if (errmsg)
	{
		sqlite3_result_text(context, errmsg, -1, SQLITE_TRANSIENT);
	}
	else
	{
		sqlite3_result_null(context);
	}
	sqlite3_free(errmsg);
	free(state.name);
	free(state.result);
	free(sql);
}

extern "C" int do_localized(void * unused, int n1, const void *v1,
							int n2, const void *v2)
{
	QString s1(QLocale().toLower(QString((const QChar *)v1, n1/2)));
	QString s2(QLocale().toLower(QString((const QChar *)v2, n2/2)));
	return s1.localeAwareCompare(s2);
}

extern "C" int do_localized_case(void * unused, int n1, const void *v1,
							int n2, const void *v2)
{
	QString s1((const QChar *)v1, n1/2);
	QString s2((const QChar *)v2, n2/2);
	return s1.localeAwareCompare(s2);
}

int Database::makeUserFunctions()
{
	sqlite3 * handle = sqlite3handle();
	int res = sqlite3_create_function(
		handle, "exec", 2, SQLITE_UTF8, NULL, do_exec, NULL, NULL);
	if (res != SQLITE_OK) { return res; }
	res = sqlite3_create_collation(
		handle, "LOCALIZED", SQLITE_UTF16, NULL, do_localized);
	if (res != SQLITE_OK) { return res; }
	return sqlite3_create_collation(
		handle, "LOCALIZED_CASE", SQLITE_UTF16, NULL, do_localized_case);
}
