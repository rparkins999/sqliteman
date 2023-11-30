/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef DATABASE_H
#define DATABASE_H

#include <QtCore/QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtCore/QStringList>

#include "sqlite3.h"
#include "sqlparser.h"

/* The connection with this name is created using
 * QSqlDatabase::addDatabase in litemanwindow.cpp.
 * Subseqeuntly the connection is obtained statically
 * when needed by using QSqlDatabase::database(SESSION_NAME).
 */
#define SESSION_NAME "sqliteman-db"

/*! \brief This struct is a sqlite3 table column representation.
Something like a system catalogue item */
typedef struct
{
	int cid;
	QString name;
	QString type;
	bool notnull;
	QString defval;
	bool pk;
	QString comment;
}
DatabaseTableField;

/*! \brief List of the attached databases ("schemas").
Mapping name/filename */
typedef QMap<QString,QString> DbAttach;

//! \brief A map with "object name"/"its parent" - schema
typedef QMap<QString,QString> DbObjects;


/*!
 * @brief The database manager
 * 
 * The %Database class represents a single database connection.
 * Multiple database files of the same kind (this application only uses sqlite)
 * can be attached to a database connection.
 * These are sometimes referred to as databases and sometimes as schemas.
 * 
 * Internally, the class uses the QtSQL API for manipulating the database.
 *
 * All methods here are static so it's not needed to create a Database instance.
 *
 * \author Igor Khanin
 * \author Petr Vanek <petr@scribus.info>
 */
class Database
{
		Q_DECLARE_TR_FUNCTIONS(Database)
				
	public:
		static DbAttach getDatabases();

        /*! \brief Gets correct sqlite_master or sqlite_temp_master.
        \param schema a name of the DB schema
        \retval QString either schema.sqlite_master or sqlite_temp_master
        */
        static QString getMaster(const QString & schema);

		/*! \brief Gather user objects from sqlite_master by type.
		It skips the resrved names "sqlite_%". See getSysObjects().
		\param type a "enum" 'table', 'view', index etc. If it's empty all objects are selected.
		\param schema a name of the DB schema
		\retval DbObjects a map with "object name"/"its parent"
		*/
		static DbObjects getObjects(const QString type = QString(), const QString schema = "main");

		/*! \brief Gather "SYS schema" objects.
		\param schema a string with "attached db" name
		\retval DbObjects with With reserved names "sqlite_%".
		*/
		static DbObjects getSysObjects(const QString & schema = "main");

		/*! \brief Gather "SYS indexes".
		System indexes are indexes created internally for UNIQUE constraints.
		\param table a table name.
		\param schema a string with "attached db" name
		\retval DbObjects with With reserved names "sqlite_%".
		*/
		static QStringList getSysIndexes(const QString & table, const QString & schema);

		/*! \brief Execute a SQL statement which can fail as a result of user error.
			\param statement an SQL statement which should return a table.
			\retval QSqlQuery an executed query
			which can be examined for errors or returned data.
		 */
		static QSqlQuery doSql(QString statement);

		/*! \brief Execute a SQL statement which is not expected to fail.
		 *  failure is a programming error rather than a user error.
			\param statement an SQL statement which should return a table.
			\retval QSqlQuery the executed query
			if there was an error, which has been reported to the user,
			the returned query has been cleared and query.isActive() is false.
		 */
		static QSqlQuery runSql(QString statement);

		//FIXME all calls to this should be replaced by calls to doSql.
		/*! \brief Execute an SQL statement which is not expected to fail.
		 * 	failure is a programming error rather than a user error.
		\param statement an SQl statement which should not return a result.
		\retval bool true on success, false if there was an error,
			which has been reported to the user,
		 */
		static bool execSql(QString statement);

		/*! \brief Create a session name for the new DB connection.
		\param schema a schema name.
		\retval QString string with "SQLITEMAN_schema" format.
		*/
		static QString sessionName(const QString & schema);

		/*!
		@brief Drop a trigger from the database
		\param name The name of the trigger to drop
		\param schema a table own schema
		\retval bool true on success, false on error. Error are reported in this method
		             already. */
		static bool dropTrigger(const QString & name, const QString & schema);

		/*!
		@brief Returns the list of fields in a table
		@param table The table to retrive the fields from
		\param schema a name of the DB schema
		@return The list of fields in \a table
		*/
		static QList<FieldInfo> tableFields(const QString & table,
											const QString & schema);

		/*!
		@brief Returns parsed info for a table
		@param table The table to retreive the fields from
		\param schema a name of the DB schema
		@return a pointer to a new SqlParser: the caller is responsible for
		destroying it
		*/
		static SqlParser * parseTable(const QString & table,
									  const QString & schema);

		//! \brief Returns the list of columns in given index
		static QStringList indexFields(const QString & index, const QString &schema);
		
		/*!
		@brief Drop a view from the database
		\param view The name of the view to drop
		\param schema a table own schema
		\retval bool true on success, false on error. Error are reported in this method
		             already. */
		static bool dropView(const QString & view, const QString & schema);

		/*!
		@brief Drop an index from the database
		\param name The name of the index to drop
		\param schema a table own schema
		\retval bool true on success, false on error. Error are reported in this method
		             already. */
		static bool dropIndex(const QString & name, const QString & schema);

		/*!
		@brief Exports the SQL code of a database to file
		If the file provided by \a fileName exists, it will be overriden.
		@param fileName The file to export the SQL to
		@todo Currently, Only the tables and views are exported. This should be fixed.
		*/
		static bool exportSql(const QString & fileName);

		static bool dumpDatabase(const QString & fileName);

		static QString describeObject(const QString & name,
									  const QString & schema,
									  const QString & type);

		/*! \brief BLOB X'foo' notation. See sqlite3 internals as a reference.
		\param val a raw "encoded" QByteArray (string)
		\retval QString with sqlite encoded X'blah' notation.
		*/
		static QString hex(const QByteArray & val);

		/*! \brief Query the DB for its specified PRAGMA setting.
		\param name a pragma name (PRAGMA name;)
		\retval QString pragma value or "n/a" string if it is not set at all.
		*/
		static QString pragma(const QString & name);

        /*! \brief Prepare Sqlite3 C API handler for usage in Sqliteman.
        \retval sqlite3* handle or 0 on error.
        */
        static sqlite3 * sqlite3handle();

        /*! \brief Enable or disable extension loading.
        \param enable true enables; false disables.
        * This can be called when no database has been opened yet
        */
        static bool setEnableExtensions(bool enable);

		/*! \brief Try to load given extensions
		\param list a QStringList with full paths to load in loop
		\retval QStringList list of the really loaded extensions
        * This can't be called until a database has been opened
		*/
		static QStringList loadExtension(const QStringList & list);

		// get a name which isn't already in use
		static QString getTempName(const QString & schema);

		// are we in autocommit mode = !(did the sql editor do a BEGIN)?
		static bool isAutoCommit();

		static int makeUserFunctions();

	private:
		//! \brief Error feedback to the user.
		static void exception(const QString & message);
};

#endif
