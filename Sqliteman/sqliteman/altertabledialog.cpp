/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME loses some constraints
	multiple primary key are not allowed
	multiple not null are allowed
	multiple unique are allowed if any on conflict clauses are the same
	multiple different checks are allowed
	multiple default are possible, last one is used
	multiple collate are possible even with different collation names
*/

#include <QtCore/qmath.h>
#include <QCheckBox>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>

#include <sys/types.h>
#include <unistd.h>

#include "altertabledialog.h"
#include "litemanwindow.h"
#include "mylineedit.h"
#include "sqlparser.h"
#include "utils.h"

namespace {
    // Static variable for createNew, prevents failure when using it
    // 21 times in a single sqliteman session.
    static int createCount = 0;
}

void AlterTableDialog::addField(QString oldName, QString oldType,
								int x, QString oldDefault)
{
	int i = ui.columnTable->rowCount();
	TableEditorDialog::addField(oldName, oldType, x, oldDefault);
	if (i > 0)
	{
		ui.columnTable->item(i - 1, 5)->setFlags(Qt::ItemIsEnabled);
	}
	QTableWidgetItem * it = new QTableWidgetItem();
	it->setIcon(Utils::getIcon("move-up.png"));
	it->setToolTip("move up");
	it->setFlags((i == 0) ? Qt::NoItemFlags : Qt::ItemIsEnabled);
	ui.columnTable->setItem(i, 4, it);
	it = new QTableWidgetItem();
	it->setIcon(Utils::getIcon("move-down.png"));
	it->setToolTip("move down");
	it->setFlags(Qt::NoItemFlags);
	ui.columnTable->setItem(i, 5, it);
	it =new QTableWidgetItem();
	it->setIcon(Utils::getIcon("delete.png"));
	it->setToolTip("remove");
	it->setFlags(Qt::ItemIsEnabled);
	ui.columnTable->setItem(i, 6, it);
	m_isIndexed.append(false);
	m_oldColumn.append(-1);
}

void AlterTableDialog::addField()
{
	addField(QString(), QString(), 0, QString());
}

void AlterTableDialog::resetClicked()
{
	// Initialize fields
	SqlParser * parsed = Database::parseTable(m_item->text(0), m_item->text(1));
	m_fields = parsed->m_fields;
	ui.columnTable->clearContents();
	ui.columnTable->setRowCount(0);
	int n = m_fields.size();
	m_isIndexed.fill(false, n);
	m_oldColumn.resize(n);
	int i;
	for (i = 0; i < n; i++)
	{
		m_oldColumn[i] = i;
		QString name(m_fields[i].name);
		int x = m_fields[i].isAutoIncrement ? 3
				: ( m_fields[i].isPartOfPrimaryKey ? 2
					: (m_fields[i].isNotNull ? 1 : 0));

		addField(name, m_fields[i].type, x,
				 SqlParser::defaultToken(m_fields[i]));
	}

	// obtain all indexed columns for DROP COLUMN checks
	QMap<QString,QStringList> columnIndexMap;
    QList<QString> objects =
        Database::getObjects("index", m_item->text(1)).values(m_item->text(0));
    QList<QString>::const_iterator it;
    for (it = objects.constBegin(); it != objects.constEnd(); ++it) {
        QStringList columns = Database::indexFields(*it, m_item->text(1));
        QStringList::const_iterator j;
        for (j = columns.constBegin(); j != columns.constEnd(); ++j) {
            QString indexColumn = *j;
			for (int k = 0; k < n; k++)
			{
				if (!(m_fields[k].name.compare(
					indexColumn, Qt::CaseInsensitive)))
				{
					m_isIndexed[k] = true;
				}
			}
		}
	}

	m_hadRowid = parsed->m_hasRowid;
	ui.withoutRowid->setChecked(!m_hadRowid);
	delete parsed;
	m_dropped = false;
	checkChanges();
}

AlterTableDialog::AlterTableDialog(LiteManWindow * parent,
								   QTreeWidgetItem * item,
								   const bool isActive)
	: TableEditorDialog(parent),
	m_item(item)
{
	m_alteringActive = isActive;
	ui.removeButton->setEnabled(false);
	ui.removeButton->hide();
	setWindowTitle(tr("Alter Table"));
	m_tableOrView = "TABLE";
	m_dubious = false;
	m_oldWidget = ui.designTab;

	m_alterButton =
		ui.buttonBox->addButton("Alte&r", QDialogButtonBox::ApplyRole);
	m_alterButton->setDisabled(true);
	connect(m_alterButton, SIGNAL(clicked(bool)),
			this, SLOT(alterButton_clicked()));
	QPushButton * resetButton = new QPushButton("Reset", this);
	connect(resetButton, SIGNAL(clicked(bool)), this, SLOT(resetClicked()));
	ui.tabWidget->setCornerWidget(resetButton, Qt::TopRightCorner);

	// item must be valid and a table, otherwise we don't get called
    m_originalName = Utils::q(m_item->text(1))
		   + "."
		   + Utils::q(m_item->text(0));
	ui.nameEdit->setText(m_item->text(0));
	int i = ui.databaseCombo->findText(m_item->text(1),
		Qt::MatchFixedString | Qt::MatchCaseSensitive);
	if (i >= 0)
	{
		ui.databaseCombo->setCurrentIndex(i);
		ui.databaseCombo->setDisabled(true);
	}

	ui.tabWidget->removeTab(2);
	ui.tabWidget->removeTab(1);
	m_tabWidgetIndex = ui.tabWidget->currentIndex();
	ui.adviceLabel->hide();

	ui.columnTable->insertColumn(4);
	ui.columnTable->setHorizontalHeaderItem(4, new QTableWidgetItem());
	ui.columnTable->insertColumn(5);
	ui.columnTable->setHorizontalHeaderItem(5, new QTableWidgetItem());
	ui.columnTable->insertColumn(6);
	ui.columnTable->setHorizontalHeaderItem(6, new QTableWidgetItem());

	connect(ui.columnTable, SIGNAL(cellClicked(int, int)),
			this, SLOT(cellClicked(int,int)));

	resetClicked();
}

// on success, return true
// on fail, print message if non-null, and return false
bool AlterTableDialog::execSql(const QString & statement,
							   const QString & message)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
        if (!message.isNull()) {
            QString errtext = message
                            + ":<br/><span style=\" color:#ff0000;\">"
                            + query.lastError().text()
                            + "<br/></span>" + tr("using sql statement:")
                            + "<br/><tt>" + statement;
            resultAppend(errtext);
        }
        query.clear();
		return false;
	}
	query.clear();
	return true;
}

bool AlterTableDialog::doRollback(QString message)
{
	bool result = execSql("ROLLBACK TO ALTER_TABLE;", message);
	if (result)
	{
		// rollback does not cancel the savepoint
		if (execSql("RELEASE ALTER_TABLE;", NULL))
		{
			return true;
		}
	}
    QString errtext = message
                        + ":<br/><span style=\" color:#ff0000;\">"
                        + tr("Database may be left with a pending savepoint.")
                        + "<br/></span>";
    resultAppend(errtext);
	return result;
}

QStringList AlterTableDialog::originalSource(QString tableName)
{
	QString ixsql = QString("select sql from ")
					+ Database::getMaster(m_item->text(1))
					+ " where type = 'trigger' and tbl_name = "
					+ Utils::q(tableName)
					+ ";";
	QSqlQuery query(ixsql, QSqlDatabase::database(SESSION_NAME));

	if (query.lastError().isValid())
	{
		QString errtext = tr("Cannot get trigger list for ")
						  + m_item->text(0)
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + query.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + ixsql;
		resultAppend(errtext);
        query.clear();
		return QStringList();
	}
	QStringList ret;
	while (query.next())
		{ ret.append(query.value(0).toString()); }
	query.clear();
	return ret;
}

QList<SqlParser *> AlterTableDialog::originalIndexes(QString tableName)
{
	QList<SqlParser *> ret;
	QString ixsql = QString("select sql from ")
					+ Database::getMaster(m_item->text(1))
					+ " where type = 'index' and tbl_name = "
					+ Utils::q(tableName)
					+ " and name not like 'sqlite_autoindex_%' ;";
	QSqlQuery query(ixsql, QSqlDatabase::database(SESSION_NAME));

	if (query.lastError().isValid())
	{
		QString errtext = tr("Cannot get index list for ")
						  + m_item->text(0)
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + query.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + ixsql;
		resultAppend(errtext);
	}
	else
	{
		while (query.next())
		{
			ret.append(new SqlParser(query.value(0).toString()));
		}
	}
	query.clear();
	return ret;
}

QString AlterTableDialog::createNew (QString databaseName, QString createBody)
{
    QSqlQuery query;
    QString sql;
    for (int i = 0; i < 20; ++i) {
        sql = "CREATE TABLE ";
        if (!databaseName.isEmpty()) {
            sql.append(Utils::q(databaseName)).append(".");
        }
        QString name =
            QString("sqliteman%1Z%2").arg(getpid()).arg(createCount++);
        sql.append(name).append(" ").append(createBody);
        query = QSqlQuery(sql, QSqlDatabase::database(SESSION_NAME));
        if (!query.lastError().isValid()) {
            query.clear();
            return name;
        }
    }
    QString errtext = tr("Cannot create table after 20 tries")
                    + ":<br/><span style=\" color:#ff0000;\">"
                    + query.lastError().text()
                    + "<br/></span>" + tr("using sql statement:")
                    + "<br/><tt>" + sql;
	resultAppend(errtext);
    query.clear();
    return NULL;
}

// This version should work correctly if multiple tasks are using it
// simultaneously.
QString AlterTableDialog::renameTemp(QString oldTableName) {
    QSqlQuery query;
    QString sql;
    for (int i = 0; i < 20; ++i) {
        QString name = QString("liteman%1tmp%2").arg(getpid()).arg(i);
        sql = QString(
            "ALTER TABLE %1.%2 RENAME TO %3 ;")
            .arg(Utils::q(m_item->text(1)))
            .arg(Utils::q(oldTableName))
            .arg(Utils::q(name));
        query = QSqlQuery(sql, QSqlDatabase::database(SESSION_NAME));
        if (!(query.lastError().isValid())) {
            query.clear();
            return name;
        }
    }
    QString errtext = tr("Cannot rename to temporary table after 20 tries")
                    + ":<br/><span style=\" color:#ff0000;\">"
                    + query.lastError().text()
                    + "<br/></span>" + tr("using sql statement:")
                    + "<br/><tt>" + sql;
	resultAppend(errtext);
    query.clear();
    return NULL;
}

bool AlterTableDialog::renameTable(QString oldTableName, QString newTableName)
{
    if (oldTableName == newTableName) {
        return true; // nothing to do
    }
    QString tmpName = oldTableName;

    // sqlite won't rename a table if only the case of (some letters of) the
    // name has changed, so we rename to a temporary table first.
    if (newTableName.compare(oldTableName, Qt::CaseInsensitive) == 0) {
        tmpName = renameTemp(m_item->text(0));
        if (tmpName.isNull()) {
            return false; // couldn't rename to a temporary
        }
    }
	QString sql = QString("ALTER TABLE ")
				  + Utils::q(m_item->text(1))
				  + "."
				  + Utils::q(tmpName)
				  + " RENAME TO "
				  + Utils::q(newTableName)
				  + ";";
	if (!execSql(sql, tr("Cannot rename table"))) {
        if (tmpName != oldTableName) {
            // We renamed it to a temporary name, try to undo that
            QString sql = QString("ALTER TABLE ")
                        + Utils::q(m_item->text(1))
                        + "."
                        + Utils::q(tmpName)
                        + " RENAME TO "
                        + Utils::q(oldTableName)
                        + ";";
            execSql(sql, tr("Cannot rename table back to original name"));
        }
        return false;
    } else {
        return true;
    }
}

bool AlterTableDialog::checkColumn(int i, QString cname,
								   QString ctype, QString cextra)
{
	int j = m_oldColumn[i];
	if (j >= 0)
	{
		bool useNull = m_prefs->nullHighlight();
		QString nullText = m_prefs->nullHighlightText();

		QString ftype(m_fields[j].type);
		if (ftype.isEmpty())
		{
			if (useNull && !nullText.isEmpty())
			{
				ftype = nullText;
			}
			else
			{
				ftype = "NULL";
			}
		}
		QString fextra;
		if (m_fields[j].isAutoIncrement)
		{
			if (!fextra.isEmpty()) { fextra.append(" "); }
			fextra.append("AUTOINCREMENT");
		}
		if (m_fields[j].isPartOfPrimaryKey)
		{
			if (!m_fields[j].isAutoIncrement)
			{
				if (!fextra.isEmpty()) { fextra.append(" "); }
				fextra.append("PRIMARY KEY");
			}
		}
		if (m_fields[j].isNotNull)
		{
			if (!fextra.isEmpty()) { fextra.append(" "); }
			fextra.append("NOT NULL");
		}
		QLineEdit * defval =
			qobject_cast<QLineEdit*>(ui.columnTable->cellWidget(i, 3));
		if (   (j != i)
			|| (cname != m_fields[j].name)
			|| (ctype != ftype)
			|| (fextra != cextra)
			|| (defval->text() != SqlParser::defaultToken(m_fields[j])))
		{
			m_altered = true;
		}
	}
	else
	{
		m_altered = true;
	}
	return false;
}

// special version for column with only icons
void AlterTableDialog::resizeTable()
{
	QTableWidget * tv = ui.columnTable;
	int widthView = tv->viewport()->width();
	int widthLeft = widthView;
	int widthUsed = 0;
	int columns = tv->horizontalHeader()->count();
	int columnsLeft = columns;
	QVector<int> wantedWidths(columns);
	QVector<int> gotWidths(columns);
	tv->resizeColumnsToContents();
	int i;
	for (i = 0; i < columns; ++i)
	{
		if (i < columns - 3)
		{
			wantedWidths[i] = tv->columnWidth(i);
		}
		else
		{
			wantedWidths[i] = 28;
		}
		gotWidths[i] = 0;
	}
	i = 0;
	/* First give all "small" columns what they want. */
	while (i < columns)
	{
		int w = wantedWidths[i];
		if ((gotWidths[i] == 0) && (w <= widthLeft / columnsLeft ))
		{
			gotWidths[i] = w;
			widthLeft -= w;
			widthUsed += w;
			columnsLeft -= 1;
			i = 0;
			continue;
		}
		++i;
	}
	/* Now allocate to other columns, giving smaller ones a larger proportion
	 * of what they want;
	 */
	for (i = 0; i < columns; ++i)
	{
		if (gotWidths[i] == 0)
		{
			int w = (int)qSqrt((qreal)(
				wantedWidths[i] * widthLeft / columnsLeft));
			gotWidths[i] = w;
			widthUsed += w;
		}
	}
	/* If there is space left, make all columns proportionately wider to fill
	 * it.
	 */
	if (widthUsed < widthView)
	{
		for (i = 0; i < columns; ++i)
		{
			tv->setColumnWidth(i, gotWidths[i] * widthView / widthUsed);
		}
	}
	else
	{
		for (i = 0; i < columns; ++i)
		{
			tv->setColumnWidth(i, gotWidths[i]);
		}
	}
}

void AlterTableDialog::swap(int i, int j)
{
	// Much of this unpleasantness is caused by the lack of a
	// QTableWidget::takeCellWidget() function.
	MyLineEdit * iEd =
		qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(i, 0));
	MyLineEdit * jEd =
		qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(j, 0));
	QString name(iEd->text());
	iEd->setText(jEd->text());
	jEd->setText(name);
	QComboBox * iBox =
		qobject_cast<QComboBox *>(ui.columnTable->cellWidget(i, 1));
	QComboBox * jBox =
		qobject_cast<QComboBox *>(ui.columnTable->cellWidget(j, 1));
	QComboBox * iNewBox = new QComboBox(this);
	QComboBox * jNewBox = new QComboBox(this);
	int k;
	for (k = 0; k < iBox->count(); ++k)
	{
		jNewBox->addItem(iBox->itemText(k));
	}
	for (k = 0; k < jBox->count(); ++k)
	{
		iNewBox->addItem(jBox->itemText(k));
	}
	jNewBox->setCurrentIndex(iBox->currentIndex());
	iNewBox->setCurrentIndex(jBox->currentIndex());
	jNewBox->setEditable(true);
	iNewBox->setEditable(true);
	connect(jNewBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(checkChanges()));
	connect(iNewBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(checkChanges()));
	connect(jNewBox, SIGNAL(editTextChanged(const QString &)),
			this, SLOT(checkChanges()));
	connect(iNewBox, SIGNAL(editTextChanged(const QString &)),
			this, SLOT(checkChanges()));
	ui.columnTable->setCellWidget(i, 1, iNewBox); // destroys old one
	ui.columnTable->setCellWidget(j, 1, jNewBox); // destroys old one
	iBox = qobject_cast<QComboBox *>(ui.columnTable->cellWidget(i, 2));
	jBox =
		qobject_cast<QComboBox *>(ui.columnTable->cellWidget(j, 2));
	iNewBox = new QComboBox(this);
	jNewBox = new QComboBox(this);
	for (k = 0; k < iBox->count(); ++k)
	{
		jNewBox->addItem(iBox->itemText(k));
	}
	for (k = 0; k < jBox->count(); ++k)
	{
		iNewBox->addItem(jBox->itemText(k));
	}
	jNewBox->setCurrentIndex(iBox->currentIndex());
	iNewBox->setCurrentIndex(jBox->currentIndex());
	jNewBox->setEditable(false);
	iNewBox->setEditable(false);
	connect(jNewBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(checkChanges()));
	connect(iNewBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(checkChanges()));
	ui.columnTable->setCellWidget(i, 2, iNewBox); // destroys old one
	ui.columnTable->setCellWidget(j, 2, jNewBox); // destroys old one
	iEd = qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(i, 3));
	jEd = qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(j, 3));
	name = iEd->text();
	iEd->setText(jEd->text());
	jEd->setText(name);
	k = m_oldColumn[i];
	m_oldColumn[i] = m_oldColumn[j];
	m_oldColumn[j] = k;
	checkChanges();
}

void AlterTableDialog::drop(int row)
{
	if (m_isIndexed[row])
	{
		MyLineEdit * edit = qobject_cast<MyLineEdit*>(
			ui.columnTable->cellWidget(row, 0));
		int ret = QMessageBox::question(this, "Sqliteman",
			tr("Column ") + edit->text()
			+ tr(" is indexed:\ndropping it will delete the index\n"
				 "Are you sure you want to drop it?\n\n"
				 "Yes to drop it anyway, Cancel to keep it."),
			QMessageBox::Yes, QMessageBox::Cancel);
		if (ret == QMessageBox::Cancel)
		{
			return;
		}
	}
	m_isIndexed.remove(row);
	m_oldColumn.remove(row);
	ui.columnTable->setCurrentCell(row, 0);
	removeField();
	ui.columnTable->item(0, 4)->setFlags(0);
	ui.columnTable->item(ui.columnTable->rowCount() - 1, 5)->setFlags(0);
	m_dropped = true;
	checkChanges();
}

void AlterTableDialog::cellClicked(int row, int column)
{
	int n = ui.columnTable->rowCount();
	switch (column)
	{
		case 4: // move up
			if (row > 0) { swap(row - 1, row);  }
			break;
		case 5: // move down
			if (row < n - 1) { swap(row, row + 1); }
			break;
		case 6: // delete
			drop(row);
			break;
		default: return; // cell handles its editing
	}
}

void AlterTableDialog::restorePragmas() {
    if (m_oldPragmaForeignKeys != 0) {
        execSql("PRAGMA foreign_keys = 1;", "");
    }
    if (m_oldPragmaAlterTable == 0) {
        execSql("PRAGMA legacy_alter_table = 0;", "");
    }
}

// Creating a view referencing a table that does not exist creates the view,
// but it is of course empty.
// Creating a view referencing a column that does not exist in a table that
// does exist creates the view, but it is also empty.
// Subsequently creating the missing table or column chnages the view
// to show the data selected.
// Note however that sqliteman's view creator GUI quotes column names and
// a quoted column name that does not match a column name in the referenced
// table will be treated as a text string and creates a column with that
// text string as its name and the same text string in every row.
// Any ALTER TABLE statement when the schema contains a view on a table
// which does not exist will fail if PRAGMA legacy_alter_table is 0.
// Renaming a column updates any foreign key references to it, regardless of
// the settings of PRAGMAs legacy_alter_table and foreign_keys

// This does the transaction inside alterButton_clicked()
// so that cleanup happens in only one place.
// returns true if we need to roll back
bool AlterTableDialog::doit(QString newTableName) {
	// Save indexes and triggers on the original table
	QStringList originalSrc = originalSource(m_tableName);
	QList<SqlParser *> originalIx(originalIndexes(m_tableName));

    // If we aren't changing the table name, we need to do so
    // because we can't create a new table with the same name.
	if (newTableName == m_tableName)
	{
		// generate unique temporary tablename
		QString tmpName = renameTemp(m_tableName);
		if (tmpName.isNull())
		{
			return true;
		}
		m_tableName = tmpName;
	}

	// Here we have column changes to make.
	// Create the new table
	QString message(tr("Cannot create table ") + newTableName);
	QString sql(getFullName() + " ( " + getSQLfromGUI());
	if (!execSql(sql, message))
	{
		return true;
	}

	// insert old data
	QMap<QString,QString> columnMap; // old => new
	QString insert;
	QString select;
	bool first = true;
	for (int i = 0; i < ui.columnTable->rowCount(); ++i)
	{
		int j = m_oldColumn[i];
		if (j >= 0)
		{
			if (first)
			{
				first = false;
				insert += (QString("INSERT OR ABORT INTO %1.%2 (")
						   .arg(Utils::q(ui.databaseCombo->currentText()),
								Utils::q(newTableName)));
                select += " ) SELECT ";
			}
			else
			{
				insert += ", ";
				select += ", ";
			}
			QLineEdit * nameItem =
				qobject_cast<QLineEdit *>(ui.columnTable->cellWidget(i, 0));
			insert += Utils::q(nameItem->text());
	        select += Utils::q(m_fields[j].name);
			columnMap.insert(m_fields[j].name, nameItem->text());
		}
	}
	if (!insert.isEmpty())
	{
		select += " FROM "
				  + Utils::q(m_item->text(1))
				  + "."
				  + Utils::q(m_tableName)
				  + ";";
		message = tr("Cannot insert data into ")
				  + newTableName;
		if (!execSql(insert + select, message))
		{
            sql = "DROP TABLE ";
            sql += Utils::q(m_item->text(1))
                + "."
                + Utils::q(newTableName)
                + ";";
            message = tr("Cannot drop table ")
                    + newTableName;
            execSql(sql, message);
            return true;
		}
	}

	// drop old table
	sql = "DROP TABLE ";
	sql += Utils::q(m_item->text(1))
		   + "."
		   + Utils::q(m_tableName)
		   + ";";
	message = tr("Cannot drop table ")
			  + m_tableName;

    if (!execSql(sql, message)) {
        return true;
    }
	m_tableName = newTableName;

	// restoring original triggers
	// FIXME fix up if columns dropped or renamed
    QStringList::const_iterator it;
    for (it = originalSrc.constBegin(); it != originalSrc.constEnd(); ++it) {
		// continue after failure here
		(void)execSql(*it, tr("Cannot recreate original trigger"));
	}

	// recreate indices
	while (!originalIx.isEmpty())
	{
		SqlParser * parser = originalIx.takeFirst();
		if (parser->replace(columnMap, ui.nameEdit->text()))
		{
			// continue after failure here
			(void)execSql(parser->toString(),
						  tr("Cannot recreate index ") + parser->m_indexName);
		}
		delete parser;
	}

	if (!execSql("RELEASE ALTER_TABLE;", tr("Cannot release savepoint")))
	{
		return true;
	}
	return false;
}

// User clicked on "Alter", go ahead and do it
// FIXME Use ALTER TABLE REANME COLUMN where appropriate
void AlterTableDialog::alterButton_clicked()
{
    // If we're altering the active table and there are unsaved changes,
    // invite the user to save, discard, or abandon the alter table action.
	if (m_alteringActive && !(creator && creator->checkForPending()))
	{
		return;
	}

	QString newTableName(ui.nameEdit->text());
	m_tableName = m_item->text(0);
	if (m_dubious)
	{
		int ret = QMessageBox::question(this, "Sqliteman",
			tr("A table with an empty column name "
			   "will not display correctly.\n"
			   "Are you sure you want to go ahead?\n\n"
			   "Yes to do it anyway, Cancel to try another name."),
			QMessageBox::Yes, QMessageBox::Cancel);
		if (ret == QMessageBox::Cancel) { return; }
	}
	if (newTableName.contains(QRegExp
		("\\s|-|\\]|\\[|[`!\"%&*()+={}:;@'~#|\\\\<,>.?/^]")))
	{
		int ret = QMessageBox::question(this, "Sqliteman",
			tr("A table named ")
			+ newTableName
			+ tr(" will not display correctly.\n"
				 "Are you sure you want to rename it?\n\n"
				 "Yes to rename, Cancel to try another name."),
			QMessageBox::Yes, QMessageBox::Cancel);
		if (ret == QMessageBox::Cancel) { return; }
	}
	ui.resultEdit->setHtml("");

    m_oldPragmaAlterTable = Database::pragma("legacy_alter_table").toInt();
    m_oldPragmaForeignKeys = Database::pragma("foreign_keys").toInt();
    bool needPragmaChanges =   (m_oldPragmaAlterTable == 0)
                               || (m_oldPragmaForeignKeys != 0);

    // Changing pragmas doesn't work inside a transaction, so if we will
    // need to do that, we can't do anything other than just renaming the table.
	if (needPragmaChanges && m_altered && !(Database::isAutoCommit())) {
        QString errtext = ":<br/><span style=\" color:#ff0000;\">"
                        + tr("Cannot do this inside a transaction.")
                        + "<br/></span>";
        resultAppend(errtext);
        return;
    }

    // If we are only renaming the table, we can do it now.
    // If we are renaming the table and foreign key checking is enabled,
    //     we need to rename it now (even though we will do so again) so that
    //     foreign key references to this table from other tables are
    //     correctly updated to the new name.
    // If we are renaming the table and legacy_alter_table is not enabled,
    //     we need to rename it now (even though we will do so again) so that
    //     views, indexes, and triggers referring to this table are
    //     correctly updated to the new name.
    if (   newTableName.compare(m_tableName, Qt::CaseSensitive)
        && (needPragmaChanges || !m_altered))
	{
		if (!renameTable(m_tableName, newTableName))
		{
			return;
		}
		updated = true;
        m_tableName = newTableName;

        // if there is nothing else to do, we're done.
		if (!m_altered) {
            m_item->setText(0, newTableName);
            return;
        }
	}
	
    // Need to do this outside any transaction because saving and restoring
    // pragmas doesn't generally work inside a transaction
	if (!execSql("PRAGMA legacy_alter_table = 1;",
        tr("Cannot set PRAGMA legacy_alter_table")))
	{
        // reverse the rename if we did it
        renameTable(m_tableName, m_item->text(0));
		return;
	}
    if (!execSql("PRAGMA foreign_keys = 0;",
        tr("Cannot unset PRAGMA foreign_keys")))
	{
        if (m_oldPragmaAlterTable == 0) {
            execSql("PRAGMA legacy_alter_table = 1;", NULL);
        }
        // reverse the rename if we did it
        renameTable(m_tableName, m_item->text(0));
		return;
	}

	if (!execSql("SAVEPOINT ALTER_TABLE;", tr("Cannot create savepoint")))
	{
        restorePragmas();
        // reverse the rename if we did it
        renameTable(m_tableName, m_item->text(0));
        return;
	}
	QString savePointTableName(m_tableName);
	bool failed = doit(newTableName);
	if (failed) {
		doRollback(tr("Cannot roll back after error"));
    }
    restorePragmas();
    if (failed) {
        // reverse the rename if we did it
        renameTable(savePointTableName, m_item->text(0));
    } else {
        updated = true;
        m_item->setText(0, m_tableName);
        emit rebuildTableTree(ui.databaseCombo->currentText(),
						 ui.nameEdit->text());
    }
}

void AlterTableDialog::checkChanges()
{
	m_dubious = false;
	QString newName(ui.nameEdit->text());
	m_altered = m_hadRowid == ui.withoutRowid->isChecked();
	m_altered |= m_dropped;
	bool ok = checkOk(newName); // side-effect on m_altered
	m_alterButton->setEnabled(   ok
							  && (   m_altered
								  || (newName != m_item->text(0))));
}
