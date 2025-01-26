/* Copyright © 2007-2009 Petr Vanek and 2015-2025 Richard Parkins
 *
 * For general Sqliteman copyright and licensing information please refer
 * to the COPYING file provided with the program. Following this notice may exist
 * a copyright and/or license notice that predates the release of Sqliteman
 * for which a new license (GPL+exception) is in place.
 *
 * If table name contains non-alphanumeric characters, no rows are displayed,
 * although they are actually still there as proved by renaming it back again.
 * This is a QT bug.
 */

#include <time.h>

#include <QApplication>
#include <QColor>
#include <QCursor>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>
#include <QStyle>
#include <QtCore/QVariant>

#include "database.h"
#include "sqlmodels.h"
#include "utils.h"

// Overrides QSqlTableModel::data
QVariant SqlTableModel::data(const QModelIndex & item, int role) const
{
	QVariant rawdata = QSqlTableModel::data(item, Qt::DisplayRole);
	QString curr(rawdata.toString());
	// numbers
	if (role == Qt::TextAlignmentRole)
	{
		bool ok;
		curr.toDouble(&ok);
		if (ok) { return QVariant(Qt::AlignRight | Qt::AlignTop); }
		return QVariant(Qt::AlignTop);
	}

	if (role == Qt::BackgroundColorRole)
    {
		// QSqlTableModel::isDirty() always returns true for an inserted
		// row but we only want to show it as modified if user has changed
		// it since insertion.
		int row = item.row();
		if (m_insertCache.contains(row))
		{
			if (m_insertCache.value(row))
			{

				return QVariant(QColor("cyan"));
			}
		}
		else
		{
	        for (int i = 0; i < columnCount(); ++i)
	        {
	            if (isDirty(index(row, i)))
				{
					return QVariant(QColor("cyan"));
				}
	        }
		}
		if (curr.isNull() && m_prefs->nullHighlight())
		{
			return QVariant(m_prefs->nullHighlightColor());
		}
		if (   (rawdata.type() == QVariant::ByteArray)
                 && (m_prefs->blobHighlight()))
        {
            return QVariant(m_prefs->blobHighlightColor());
        }
        // anything else
		return m_palette.color(QPalette::Base);
    }


	// nulls
	if (curr.isNull())
	{
		if (role == Qt::ToolTipRole)
			return QVariant(tr("NULL value"));
		if ((role == Qt::DisplayRole) && m_prefs->nullHighlight())
			return QVariant(m_prefs->nullHighlightText());
	}

	// blobs
	if (rawdata.type() == QVariant::ByteArray)
	{
		if (role == Qt::ToolTipRole) { return QVariant(tr("BLOB value")); }
		if ((role == Qt::DisplayRole) && (m_prefs->blobHighlight()))
		{
			return QVariant(m_prefs->blobHighlightText());
		}
		else if (role == Qt::DisplayRole)
		{
			return QVariant(Database::hex(rawdata.toByteArray()));
		}
	}

	// advanced tooltips
	if (role == Qt::ToolTipRole)
		return QVariant("<qt>" + curr + "</qt>");

	return QSqlTableModel::data(item, role);
}

// Overrides QSqlTableModel::setData
bool SqlTableModel::setData ( const QModelIndex & ix, const QVariant & value, int role)
{
    // Copying a value to the clipboard sometimes calls setdata
    // (I don't understand why).
    // This test avoids incorrectly marking the database as modified.
    if ((role == Qt::EditRole) && (data(ix, role) == value)) {
        return true; // not changing it, don't mark as changed
    }
    // setData returns false if role != Qt::EditRole
	if (QSqlTableModel::setData(ix, value, role))
	{
        m_pending = true;
        int row = ix.row();
        if (m_insertCache.contains(row))
        {
            m_insertCache.insert(row, true);
        }
		return true;
	}
	else { return false; }
}

// Overrides QSqlTableModel::headerData
QVariant SqlTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		switch (role)
		{
			case (Qt::DecorationRole):
				switch (m_header[section])
				{
					case (SqlTableModel::PK):
						return Utils::getIcon("key.png");
						break;
					case (SqlTableModel::Auto):
						return Utils::getIcon("index.png");
						break;
					case (SqlTableModel::Default):
						return Utils::getIcon("column.png");
						break;
					default:
						return 0;
				}
				break;
			case (Qt::ToolTipRole):
				switch (m_header[section])
				{
					case (SqlTableModel::PK):
						return tr("Primary Key");
						break;
					case (SqlTableModel::Auto):
						return tr("Autoincrement");
						break;
					case (SqlTableModel::Default):
						return tr("Has default value");
						break;
					default:
						return "";
				}
				break;
		}
	}
	return QSqlTableModel::headerData(section, orientation, role);
}

// Overrides QSqlTableModel::insertRowIntoTable
bool SqlTableModel::insertRowIntoTable(const QSqlRecord &values)
{
	bool generated = false;
	for (int i = 0; i < values.count(); ++i)
	{
		generated |= values.isGenerated(i);
	}
	if (generated)
	{
		return QSqlTableModel::insertRowIntoTable(values);
	}
	// QSqlTableModel::insertRowIntoTable will fail, so we do the right thing...
	QString sql("INSERT INTO ");
	sql += Utils::q(m_schema) + "." + Utils::q(objectName());
	sql += " DEFAULT VALUES;";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	return !query.lastError().isValid();
}

QVariant SqlTableModel::evaluate(QString expression) {
    QString sql = "VALUES(" + expression + ");";
    QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
    if (query.first()) { return query.value(0); }
    else { return QVariant(QVariant::String); } // NULL
}

// Throw away changes
void SqlTableModel::reset(QString tableName, bool isNew)
{
	if (isNew) { m_header.clear(); }
	m_deleteCache.clear();
	m_insertCache.clear();
}

// We get the FieldInfo's here and keep them,
// because we modify the defaultKeyValue
// for a field which has an actual or implied UNIQUE constraint.
void SqlTableModel::refreshFields() {
	m_fields = Database::tableFields(objectName(), m_schema);
    bool donePK = false;
    QList<FieldInfo>::iterator i;
	int j;
    for (i = m_fields.begin(), j = 0; i != m_fields.end(); ++i, ++j) {
        if (i->isAutoIncrement)
        { // get the next autoincrement value so we can fake new ones
            QString sql = QString("SELECT seq FROM ") 
                            + Utils::q(m_schema.toLower())
                            + ".sqlite_sequence WHERE lower(name) = "
                            + Utils::q(objectName().toLower())
                            + ";";
            QSqlQuery seqQuery(sql, QSqlDatabase::database(SESSION_NAME));
            if (!(seqQuery.lastError().isValid()))
            {
                if (seqQuery.first()) {
                    i->defaultKeyValue = seqQuery.value(0).toLongLong();
                } else {
                    i->defaultKeyValue = 0; // table has no rows yet
                }
            }
        } else if (   i->isUnique
                   || (i->isPartOfPrimaryKey && !donePK))
        { // this column must be unique, get largest number used
            QString sql = QString("SELECT CAST (") // cast non-integers ...
                            + Utils::q(i->name)
                            + " AS INTEGER)FROM " /// ... to integer
                            + Utils::q(m_schema) + "." + Utils::q(objectName())
                            + " ORDER BY CAST ( " + Utils::q(i->name)
                            + " AS INTEGER ) DESC LIMIT 1;"; // select largest
            QSqlQuery seqQuery(sql, QSqlDatabase::database(SESSION_NAME));
            if (!(seqQuery.lastError().isValid()))
            {
                if (seqQuery.first()) {
                    i->defaultKeyValue = seqQuery.value(0).toLongLong();
                } else {
                    i->defaultKeyValue = 0; // no rows or no numbers yet
                }
                if (i->isPartOfPrimaryKey) { donePK = true; }
            }
        }
    }
}

// Called when creating a new record, handles copying data if wanted.
void SqlTableModel::doPrimeInsert(int row, QSqlRecord & record)
{
    bool prefill = m_prefs->prefillNew();
    QList<FieldInfo>::iterator i;
    int j = 0; // field number
    bool noPKyet = true;
    for (i = m_fields.begin(); i != m_fields.end(); ++i, ++j) {
        if (   prefill
            && (   i->isAutoIncrement
                || (   i->isWholePrimaryKey
                    && (i->type.toLower() == "integer")
                    && !(i->isColumnPkDesc))))
		{
            // This logic can put in a smaller rowid than sqlite would,
            // (if for example some rows were created and then deleted
            // before committing) but it is a valid one unless
            // 9223372036854775807 has been used.
            record.setValue(i->name, QVariant(++(i->defaultKeyValue)));
            record.setGenerated(i->name, true);
            continue;
        }
        QVariant defval = QVariant(); // Invalid QVariant -> NULL field
        bool generated = false; // true if we create a value
        if (!(m_copyThis.isEmpty())) {
            defval = m_copyThis.value(j);
            generated = true;
        } else if (prefill) {
            char s[22];
            time_t dummy;
            bool ok;
            if (i->defaultisQuoted) {
                defval = QVariant(i->defaultValue);
            } else if (i->defaultIsExpression) {
                defval = evaluate(i->defaultValue);
            } else  if (i->defaultValue.compare(
                "CURRENT_TIMESTAMP", Qt::CaseInsensitive) == 0)
            {
                time(&dummy);
                (void)strftime(s, 20, "%F %T", localtime(&dummy));
                defval = QVariant(s);
            } else if (i->defaultValue.compare(
                "CURRENT_TIME", Qt::CaseInsensitive) == 0)
            {
                time(&dummy);
                (void)strftime(s, 20, "%T", localtime(&dummy));
                defval = QVariant(s);
            } else if (i->defaultValue.compare(
                "CURRENT_DATE", Qt::CaseInsensitive) == 0)
            {
                time(&dummy);
                (void)strftime(s, 20, "%F", localtime(&dummy));
                defval = QVariant(s);
            } else if (!(i->defaultValue).isNull()) {
                QString val(i->defaultValue);
                qlonglong j = val.toLongLong(&ok);
                if (ok) {
                    defval = QVariant(j);
                } else {
                    double d = val.toDouble(&ok);
                    if (ok) { defval = QVariant(d); }
                    else { defval = val; }
                }
            }
            if (i->isNotNull && defval.isNull()) {
                defval = QVariant(""); // force a non-null value
                generated = true;
            }
        }
        if (   prefill
            && (   (noPKyet && i->isPartOfPrimaryKey)
                || i->isUnique))
        {
            bool forceValue = false;
            QMap<int,bool>::const_iterator it;
            QSqlQuery query(QSqlDatabase::database(SESSION_NAME));
            if (defval.isNull()) {
                forceValue = true; // need to force a non-NULL value
            } else { // see if defval is already in the table
                QString sql("VALUES ( ? IN ( SELECT "
                            + Utils::q(i->name) + " FROM "
                            + Utils::q(m_schema) + "." + Utils::q(objectName()));
                bool first = true;
                for (it = m_insertCache.constBegin();
                        it != m_insertCache.constEnd(); ++it)
                {
                    if (first) {
                        first = false;
                        sql += " UNION ALL VALUES ( ";
                    } else { sql += ","; }
                    sql += "(?)"; // add args for rows in insert cache
                }
                if (!first) { sql += ")"; }
                sql += "));";
                query.prepare(sql); // construct a query to find it
                query.addBindValue(defval);
                for (it = m_insertCache.constBegin();
                        it != m_insertCache.constEnd(); ++it)
                { // bind values from rows in insert cache
                    QModelIndex mi = createIndex(it.key(), j);
                    query.addBindValue(QSqlTableModel::data(mi));
                }
                query.exec(); // run the query
                if (query.first() && (query.value(0) != 0)) {
                    forceValue = true; // need to force a different value
                }
            }
            if (forceValue) {
                // Construct an integer that isn't in the table.
                QString sql = "VALUES ( 1 + max ( ( SELECT CAST ( "
                            + Utils::q(i->name)
                            + " AS INTEGER ) AS x FROM "
                            + Utils::q(m_schema) + "." + Utils::q(objectName())
                            + " ORDER BY x DESC LIMIT 1)" ;
                for (it = m_insertCache.constBegin();
                        it != m_insertCache.constEnd(); ++it)
                { sql += ", ?"; } // add args for rows in insert cache
                sql += "));";
                query.prepare(sql);
                for (it = m_insertCache.constBegin();
                        it != m_insertCache.constEnd(); ++it)
                { // bind values from rows in insert cache
                    QModelIndex mi = createIndex(it.key(), j);
                    query.addBindValue(QSqlTableModel::data(mi));
                }
                query.exec(); // find an integer not already in table
                query.first(); // must succeed
                defval = query.value(0);
                generated = true;
            }
        }
        record.setValue(i->name, defval);
        record.setGenerated(i->name, generated);
	}
}

// Overrides QSqlTableModel::deleteRowFromTable
// because we need to update the DataViewer if a row was really deleted.
bool SqlTableModel::deleteRowFromTable(int row)
{
	bool result = QSqlTableModel::deleteRowFromTable(row);
	if (result) { emit reallyDeleting(row); }
	return result;
}

SqlTableModel::SqlTableModel(QObject * parent, QSqlDatabase db)
	: QSqlTableModel(parent, db),
	m_pending(false),
	m_schema("")
{
    /* We used to cache the preference values which this class uses,
     * but that used the old value if the user changed a preference
     * after this class has been instantiated. Our singleton preferences
     * class caches all preferences when sqliteman is started, but updates
     * its cache if the user changes a preference, so reading from it
     * gets an up to date value relatively cheaply.
     */
    m_prefs = Preferences::instance();
	m_deleteCache.clear();
	m_insertCache.clear();
    m_copyThis = QSqlRecord();
	connect(this, SIGNAL(primeInsert(int, QSqlRecord &)),
			this, SLOT(doPrimeInsert(int, QSqlRecord &)));
	m_useCount = 1;
}

/*
 * Note this isn't SQL's pending transaction mode (between BEGIN and COMMIT),
 * but our own flag meaning that QT's local copy of the model has pending
 * changes which haven't yet been written to the database.
 */
void SqlTableModel::setPendingTransaction(bool pending)
{
	m_pending = pending;

	if (!pending)
	{
		for (int i = 0; i < m_deleteCache.size(); ++i)
		{
			emit headerDataChanged(
				Qt::Vertical, m_deleteCache[i], m_deleteCache[i]);
		}
		m_deleteCache.clear();
		m_insertCache.clear();
	}
}

bool SqlTableModel::copyRow (int row, QSqlRecord record) {
    m_copyThis = record;
    bool result = insertRows(row, 1, QModelIndex());
    m_copyThis.clear();
    return result;
}

// insert new rows
// count is always 1, but we're overriding QSqlTableModel::insertRows
bool SqlTableModel::insertRows ( int row, int count, const QModelIndex & parent)
{
	if (QSqlTableModel::insertRows(row, count, parent))
	{
		m_pending = true;
		for (int i = 0; i < count; ++i)
		{
			m_insertCache.insert(row + i, false);
		}
		return true;
	}
	else { return false; }
}

// Overrides QSqlTableModel::removeRows
bool SqlTableModel::removeRows ( int row, int count, const QModelIndex & parent)
{
	// this is a workaround to allow mark heading as deletion
	// (as it's propably a bug in Qt QSqlTableModel ManualSubmit handling
	if (QSqlTableModel::removeRows(row, count, parent))
	{
		m_pending = true;
		emit dataChanged(index(row, 0), index(row+count-1, columnCount()-1));
		emit headerDataChanged(Qt::Vertical, row, row+count-1);
		for (int i = 0; i < count; ++i)
		{
			m_deleteCache.append(row+i);
			m_insertCache.remove(row+i);
		}
		return true;
	}
	else { return false; }
}

bool SqlTableModel::isDeleted(int row)
{
	return m_deleteCache.contains(row);
}

bool SqlTableModel::isNewRow(int row)
{
	return m_insertCache.contains(row) && !m_insertCache.value(row);
}

// Overrides QSqlTableModel::setTable
void SqlTableModel::setTable(const QString &tableName)
{
	reset(tableName, true);
	if (m_schema.isEmpty())
	{
		QSqlTableModel::setTable(tableName);
	}
	else
	{
		QSqlTableModel::setTable(m_schema + "." + tableName);
	}
	// For some strange reason QSqlTableModel:i->defaultKeyValue:tableName
	// gives us back schema.table, so we stash the undecorated table name
	// in the object name
	setObjectName(tableName);
    refreshFields();
    QList<FieldInfo>::iterator i;
    int j;
    for (i = m_fields.begin(), j = 0; i != m_fields.end(); ++i, ++j) {
        if (i->isAutoIncrement) {
            m_header[j] = SqlTableModel::Auto; // show autoincrement icon
        } else if (i->isPartOfPrimaryKey) {
            m_header[j] = SqlTableModel::PK; // show primary key icon
        } else if (!i->defaultValue.isEmpty()) {
            m_header[j] = SqlTableModel::Default; // show has default icon
        } else {
            m_header[j] = SqlTableModel::None;
        }
    }
}

void SqlTableModel::detach (SqlTableModel * model)
{
	if (--(model->m_useCount) == 0) { delete model ; }
}

void SqlTableModel::fetchAll()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    bool fetched = false;
	if (rowCount() > 0)
	{
		while (canFetchMore(QModelIndex()))
		{
			QSqlTableModel::fetchMore();
            fetched = true;
		}
	}
	if (fetched)
	{
		refreshFields();
		emit moreFetched();
	}
    QApplication::restoreOverrideCursor();
}

// Overrides QSqlTableModel::select()
bool SqlTableModel::select()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	bool result = QSqlTableModel::select();
    int readRowsCount;
    if (result) {
        switch (m_prefs->rowsToRead())
        {
            case 0: readRowsCount = 256;
            case 1: readRowsCount = 512;
            case 2: readRowsCount = 1024;
            case 3: readRowsCount = 2048;
            case 4: readRowsCount = 4096;
            default: readRowsCount = INT_MAX;
        }

        bool fetched = false;
        while (   canFetchMore(QModelIndex())
               && (rowCount() < readRowsCount))
        {
            QSqlTableModel::fetchMore();
            fetched = true;
        }
        if (fetched)
        {
            refreshFields();
            emit moreFetched();
        }
    }
    QApplication::restoreOverrideCursor();
	return result;
}

// Overrides QSqlTableModel::submitAll() because we need to update the display
// to show no rows have been changed from the database copy now.
bool SqlTableModel::submitAll()
{
	if (QSqlTableModel::submitAll())
	{
		reset(objectName(), false);
        refreshFields();
		return true;
	}
	else
	{
		return false;
	}
}

// Overrides QSqlTableModel::revertAll() because we need to update the display
// to show no rows have been changed from the database copy now.
void SqlTableModel::revertAll()
{
	QSqlTableModel::revertAll();
	reset(objectName(), false);
    refreshFields();
}


SqlQueryModel::SqlQueryModel( QObject * parent)
	: QSqlQueryModel(parent),
	m_useCount(1)
{
    /* We used to cache the preference values which this class uses,
     * but that used the old value if the user changed a preference
     * after this class has been instantiated. Our singleton preferences
     * class caches all preferences when sqliteman is started, but updates
     * its cache if the user changes a preference, so reading from it
     * gets an up to date value relatively cheaply.
     */
    m_prefs = Preferences::instance();
}

// Overrides QSqlQueryModel::data
QVariant SqlQueryModel::data(const QModelIndex & item, int role) const
{
	QVariant rawdata = QSqlQueryModel::data(item, Qt::DisplayRole);
	QString curr(rawdata.toString());

	// numbers
	if (role == Qt::TextAlignmentRole)
	{
		bool ok;
		curr.toDouble(&ok);
		if (ok)
			return QVariant(Qt::AlignRight | Qt::AlignTop);
		return QVariant(Qt::AlignTop);
	}

	if (m_prefs->nullHighlight() && curr.isNull())
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(m_prefs->nullHighlightColor());
		if (role == Qt::ToolTipRole)
			return QVariant(tr("NULL value"));
		if (role == Qt::DisplayRole)
			return QVariant(m_prefs->nullHighlightText());
	}

	if (m_prefs->blobHighlight() && (rawdata.type() == QVariant::ByteArray))
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(m_prefs->blobHighlightColor());
		if (role == Qt::ToolTipRole)
			return QVariant(tr("BLOB value"));
		if (   (role == Qt::DisplayRole)
			|| (role == Qt::EditRole))
		{
			return QVariant(m_prefs->blobHighlightText());
		}
	}

	if (role == Qt::BackgroundColorRole)
	{
		return m_palette.color(QPalette::Base);
	}

	// advanced tooltips
	if (role == Qt::ToolTipRole)
		return QVariant("<qt>" + curr + "</qt>");

	return QSqlQueryModel::data(item, role);
}

void SqlQueryModel::initialRead() {
    if (!lastError().isValid()) {
        info = record(); // force column count to be set
        if (   (columnCount() > 0)
			&& (rowCount() > 0))
		{
			while (canFetchMore(QModelIndex()))
			{
				/* note this is QSqlQueryModel::fetchMore()
				 * since SqlQueryModel doesn't override it.
				 */
				fetchMore();
			}
		}
    }
}

//Overrides QSqlQueryModel::setQuery
void SqlQueryModel::setQuery ( const QSqlQuery & query )
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	QSqlQueryModel::setQuery(query);
    initialRead();
    QApplication::restoreOverrideCursor();
}

//Overrides QSqlQueryModel::setQuery
void SqlQueryModel::setQuery ( const QString & query, const QSqlDatabase & db)
{
	QSqlQueryModel::setQuery(query, db);
    initialRead();
    QApplication::restoreOverrideCursor();
}

void SqlQueryModel::detach (SqlQueryModel * model)
{
	if (--(model->m_useCount) == 0) { delete model ; }
}

void SqlQueryModel::fetchAll()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	if (rowCount() > 0)
	{
		while (canFetchMore(QModelIndex()))
		{
			/* note this is QSqlQueryModel::fetchMore()
			 * since SqlQueryModel doesn't override it.
			 */
			fetchMore();
		}
	}
    QApplication::restoreOverrideCursor();
}
