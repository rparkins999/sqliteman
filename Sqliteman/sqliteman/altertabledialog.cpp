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

#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>

#include <sys/types.h>
#include <unistd.h>

#include "altertabledialog.h"
#include "litemanwindow.h"
#include "mylineedit.h"
#include "preferences.h"
#include "sqlparser.h"
#include "tabletree.h"
#include "utils.h"

namespace {
    // Static variable for createNew, prevents failure when using it
    // 21 times in a single sqliteman session.
    static int createCount = 0;
}

void AlterTableDialog::addField(FieldInfo field, bool isNew)
{
	int i = ui.columnTable->rowCount();
	TableEditorDialog::addField(field);
	QTableWidgetItem * it = new QTableWidgetItem();
	it->setIcon(Utils::getIcon("move-up.png"));
	it->setToolTip("move up");
	ui.columnTable->setItem(i, 4, it);
	it = new QTableWidgetItem();
	it->setIcon(Utils::getIcon("move-down.png"));
	it->setToolTip("move down");
	ui.columnTable->setItem(i, 5, it);
	it =new QTableWidgetItem();
	it->setIcon(Utils::getIcon("delete.png"));
	it->setToolTip("remove");
	ui.columnTable->setItem(i, 6, it);
	m_isIndexed.append(false);
	m_oldColumn.append(isNew ? -1 : i);
}

void AlterTableDialog::addField()
{
    FieldInfo field;
    field.name = QString();
	addField(field, true);
    updateButtons();
    fudge();
}

void AlterTableDialog::resetClicked()
{
    ui.resultEdit->clear();
    m_originalName = Utils::q(m_databaseName) + "." + Utils::q(m_tableName);
    if (m_item == NULL) {
        // we've done a rebuildTableTree(), find the item again
        m_item = m_creator->findTreeItem(m_databaseName, m_tableName);
        if (m_item == NULL) { reject(); } // can't continue this dialog
    }
	// Initialize fields
	SqlParser * parsed = Database::parseTable(m_item->text(0), m_item->text(1));
	m_fields = parsed->m_fields;
	ui.columnTable->clearContents();
	ui.columnTable->setRowCount(0);
	int n = m_fields.size();
	m_isIndexed.fill(false, n);
	m_oldColumn.resize(0);
	int i;
	for (i = 0; i < n; i++)
	{
		addField(m_fields[i], false);
	}
	updateButtons();

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
    fudge();
}

AlterTableDialog::AlterTableDialog(LiteManWindow * parent,
								   QTreeWidgetItem * item,
								   const bool isActive)
	: TableEditorDialog(parent),
	  m_item(item)
{
    ui.gridLayout->removeItem(ui.onTableBox);
    delete ui.onTableBox;
    delete ui.onTableLabel;
    delete ui.tableCombo;
	setWindowTitle(tr("Alter Table"));
    m_tableName = m_item->text(0);
	m_tableOrView = "TABLE";
	m_dubious = false;
	m_alteringActive = isActive;
	ui.databaseCombo->addItems(m_creator->visibleDatabases());
    m_noTemp = (ui.databaseCombo->findText("temp", Qt::MatchFixedString) < 0);
    if (m_noTemp) { ui.databaseCombo->addItem("temp"); }
    int n = ui.databaseCombo->count();
    int i;
    bool databaseMayChange;
    if (item) {
        m_databaseName = item->text(1);
		i = ui.databaseCombo->findText(m_databaseName, Qt::MatchFixedString);
        if (i < 0) {
            if (n > 0) {
                i = 0;
                m_databaseName = ui.databaseCombo->itemText(0);
                databaseMayChange = true;
            } else {
                m_databaseName.clear();
                databaseMayChange = false;
            }
        } else {
            databaseMayChange = m_noTemp;
        }
    } else {
        i = 0;
        m_databaseName = ui.databaseCombo->itemText(0);
        databaseMayChange = true;
    }
    if (item->type() == TableTree::TableType) {
        m_tableName = item->text(0);
        databaseMayChange = false;
    }
    ui.databaseCombo->setCurrentIndex(i);
    databaseMayChange &= (n > 1);
    if (databaseMayChange) {
        connect(ui.databaseCombo, SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(databaseChanged(const QString &)));
    }
    ui.databaseCombo->setEnabled(databaseMayChange);
	ui.nameEdit->setText(m_tableName);
    ui.nameEdit->setToolTip(QString("Original name: ") + m_tableName);
	QPushButton * resetButton = new QPushButton("Reset", this);
	ui.tabWidget->setCornerWidget(resetButton, Qt::TopRightCorner);
	connect(resetButton, SIGNAL(clicked(bool)), this, SLOT(resetClicked()));
    disconnect(ui.removeButton, 0, 0, 0);
	ui.removeButton->setEnabled(false);
	ui.removeButton->hide();
	ui.tabWidget->removeTab(2);
	ui.tabWidget->removeTab(1);
    fixTabWidget();
	ui.columnTable->insertColumn(4);
	ui.columnTable->setHorizontalHeaderItem(4, new QTableWidgetItem());
	ui.columnTable->insertColumn(5);
	ui.columnTable->setHorizontalHeaderItem(5, new QTableWidgetItem());
	ui.columnTable->insertColumn(6);
	ui.columnTable->setHorizontalHeaderItem(6, new QTableWidgetItem());
	connect(ui.columnTable, SIGNAL(cellClicked(int, int)),
			this, SLOT(cellClicked(int,int)));
	ui.adviceLabel->hide();
	m_alterButton =
		ui.buttonBox->addButton("Alte&r", QDialogButtonBox::ApplyRole);
	m_alterButton->setDisabled(true);
	connect(m_alterButton, SIGNAL(clicked(bool)),
			this, SLOT(alterButton_clicked()));
    Preferences * prefs = Preferences::instance();
	resize(prefs->altertableWidth(), prefs->altertableHeight());
	resetClicked();
}

AlterTableDialog::~AlterTableDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setaltertableHeight(height());
    prefs->setaltertableWidth(width());
}

// Called when
// all old rows have been added by resetClicked(), or
// a single new row has been added by the add button, or
// a row has been moved up or down or removed.
void AlterTableDialog::updateButtons()
{
    int n = ui.columnTable->rowCount();
    for (int i = 0; i < n; ++i) {
        // move up is enabled except for first row
        ui.columnTable->item(i, 4)->setFlags(
            (i == 0) ? Qt::NoItemFlags : Qt::ItemIsEnabled);
        // move down is enabled except for last row
        ui.columnTable->item(i, 5)->setFlags(
            (i == n - 1) ? Qt::NoItemFlags : Qt::ItemIsEnabled);
        // remove is enabled if there is more than one row
        ui.columnTable->item(i, 6)->setFlags(
            (n <= 1) ? Qt::NoItemFlags : Qt::ItemIsEnabled);
    }
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
		QString ftype(m_fields[j].type);
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
			|| ((ctype != ftype) && !(ctype.isEmpty() && ftype.isEmpty()))
			|| (fextra != cextra)
			|| (defval->text() != SqlParser::defaultToken(m_fields[j])))
		{
			m_altered = true;
		}
	} else { m_altered = true; }
	return false;
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
    updateButtons();
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
    updateButtons();
	m_dropped = true;
	checkChanges();
}

void AlterTableDialog::cellClicked(int row, int column)
{
    // The tests on "n" and "row" here should be unnecessary because we
    // disable the button if its action isn't allowed,
    // but we check anyway just to be on the safe side.
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
			if (n > 1) { drop(row); }
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
// Subsequently creating the missing table or column changes the view
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
	QString sql("CREATE TABLE ");
    sql.append(Utils::q(m_databaseName)).append(".")
       .append(Utils::q(newTableName)).append(" (")
       .append(getSQLfromDesign());
	QString message(tr("Cannot create table ") + newTableName);
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
    sql = "DROP TABLE ";
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
// FIXME Use ALTER TABLE RENAME COLUMN where appropriate
void AlterTableDialog::alterButton_clicked()
{
    ui.resultEdit->clear();
    // If we're altering the active table and there are unsaved changes,
    // invite the user to save, discard, or abandon the alter table action.
	if (m_alteringActive && !(m_creator && m_creator->checkForPending()))
	{
		return;
	}

    if (m_item == NULL) {
        // we've done a rebuildTableTree(), find the item again
        m_item = m_creator->findTreeItem(m_databaseName, m_tableName);
        if (m_item == NULL) { reject(); } // can't continue this dialog
    }
	QString newTableName(ui.nameEdit->text());
	m_tableName = m_item->text(0);
    m_databaseName = m_item->text(1);
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
		m_updated = true;
        m_tableName = newTableName;

        // if there is nothing else to do, we're done.
		if (!m_altered) {
            m_item->setText(0, newTableName);
            m_originalName = Utils::q(m_databaseName)
                             + "." + Utils::q(m_tableName);
            checkChanges();
            resultAppend(tr("Table successfully altered"));
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
        m_updated = true;
        m_item->setText(0, m_tableName);
        emit rebuildTableTree(ui.databaseCombo->currentText());
        m_item = NULL;
    }
    resetClicked();
    resultAppend(tr("Table successfully altered"));
}

void AlterTableDialog::checkChanges()
{
	m_dubious = false; // true if some column has an empty name
	QString newName(ui.nameEdit->text());
	m_altered = m_hadRowid == ui.withoutRowid->isChecked();
	m_altered |= m_dropped;
	bool ok = checkOk(); // side-effect on m_dubious and m_altered
    // Alter button is enabled if the current table definition is valid
    // and something has changed,
    // even if newName differs from m_tableName in case only.
	m_alterButton->setEnabled(   ok
							  && (   m_altered
								  || (newName != m_tableName)));
    ui.resultEdit->clear();
}
