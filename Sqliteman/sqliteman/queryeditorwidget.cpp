/* Copyright © 2007-2009 Petr Vanek and 2015-2024 Richard Parkins
 *
 * For general Sqliteman copyright and licensing information please refer
 * to the COPYING file provided with the program. Following this notice may exist
 * a copyright and/or license notice that predates the release of Sqliteman
 * for which a new license (GPL+exception) is in place.
 */

#include <QClipboard>
#include <QComboBox>
#include <QLineEdit>
#include <QScrollBar>
#include <QTreeWidgetItem>

#include "database.h"
#include "preferences.h"
#include "queryeditorwidget.h"
#include "querystringmodel.h"
#include "sqlparser.h"
#include "tabletree.h"
#include "utils.h"

/*****************************************************************************
 *
 * This widget is used by QueryEditorDialog directly, and by CreateIndexDialog,
 * CreateTableDialog, and CreateViewDialog through inheritance from
 * TableEditorDialog. AlterTableDialog also inherits from TableEditorDialog,
 * but the tab containing this widget is removed.
 *
 * The user is allowed, by enabling schemaList, to choose a schema
 * to select from if, and only if, there is more than one database, and
 * a) we are a component of CreateTableDialog, or
 * b) we are a component of CreateViewDialog creating a temporary view and
 *    the temp database contains at least one table, or
 * c) we are a component of QueryEditorDialog and no database was initially
 *    selected.
 *
 * If the previously selected database is detached, we have to deselect it
 * in case another database is subsequently attached and given the same name
 * and we are subsequently called with the same database name, but not
 * the same database. This dealt with by LiteManWindow calling
 * schemaGone(name) after detaching database 'name'.
 *
 * If the SQL editor executed a DETACH or an EXEC, which might have contained
 * a DETACH, LitemanWindow calls schemaGone(null string) because it doesn't
 * know the name of the database which was detached.
 *
 * We don't add "temp" to schemaList anywhere here, because we are looking for
 * a table or view to query. If there are any tables or views in "temp",
 * Database::getDatabases().keys() will include them.
 * 
 * This issue has to be dealt with in TableEditorDialog since the temporary
 * database can be created by creating a table or a view in it which selects
 * from a non-temporary table.
 *
 * The user is allowed, by enabling tablesList, to choose a table or view
 * to select from if, and only if, there is more than one table or view
 * in the currently selected schema, and
 * a) we are a component of CreateTableDialog, or
 * b) we are a component of CreateViewDialog and we are not creating a view on
 *    a specific table or view, or
 * c) we are a component of QueryEditorDialog and no table or view was initially
 *    selected.
 *
 * If the currently selected table or view is dropped or altered,
 * we have to deselect it in case another table or view is subsequently created
 * and given the same name, and we are subsequently called with the same
 * table or view name, but it isn't the same table or view.
 *
 * Currently I don't handle LIMIT and OFFSET, just because I haven't written
 * the code: there is no reason in principle why it can't be done.
 * I would need to be careful about propagating LIMIT and OFFSET to
 * m_currentItem().
 *
 *****************************************************************************/

/* This is signaled when user picks a table or a view from the combo box.
 * Also we call it explicitly from some other places.
 * This version should work with views and virtual tables.
 *
 * If it is the same table as before, we don't need to do anything.
 * The special cases of re-use of a table name are dealt with
 * elsewhere when choosing a different schema (setSchema) or
 * dropping a table (tableGone).
 */
void QueryEditorWidget::tableSelected(const QString & table)
{
    if (table.compare(m_table, Qt::CaseInsensitive)) {
        m_table = table;
        columnModel->clear();
        selectModel->clear();
        ui.tabWidget->setCurrentIndex(0);
        ui.termsTab->ui.termsTable->clear();
        ui.termsTab->ui.termsTable->setRowCount(0); // clear() doesn't do this
        ui.ordersTable->clear();
        ui.ordersTable->setRowCount(0); // clear() doesn't do this
        m_columnList = GetColumnList::getColumnList(m_schema, table);
        ui.termsTab->m_columnList = m_columnList.columns;
        columnModel->setStringList(m_columnList.columns);
        ui.termsTab->update();
        resizeWanted = true;
    }
}

// Common bit of code to look up a table in the table list
// and enable or disable user changes.
// Returns the name it is given unless it is not in the list.
void QueryEditorWidget::findTable(QString name)
{
    int n = ui.tablesList->count();
    int i = ui.tablesList->findText(name, Qt::MatchExactly);
    if (i < 0) { // name is not a table in the database
        if (n > 0) {
            i = 0;
            name = ui.tablesList->itemText(0); // default to first entry
        } else { name.clear(); } // empty list => empty name
        // We're changing the table anyway because it was invalid:
        // we allow the user to change it if there is more than one
        m_canChangeTable = (n > 1);
    } else if (n <= 1) {
        m_canChangeTable = false; // can't change if only one entry
        // We don't set it to true if n >= 2 because if we got here
        // our caller passed us a valid table and we should
        // leave the previous value of tableMayChange.
    }
    ui.tablesList->setEnabled(false); // disable before disconnecting
    disconnect(ui.tablesList, SIGNAL(currentIndexChanged(const QString &)),
            this, SLOT(tableSelected(const QString &)));
    ui.tablesList->setCurrentIndex(i); // i will be -1 if n is 0
    if (m_canChangeTable) {
        connect(ui.tablesList, SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(tableSelected(const QString &)),
                Qt::UniqueConnection);
        ui.tablesList->setEnabled(true); // enable after connecting
    }
    tableSelected(name);
}

// This is called externally if a new table was created
// and internally if the schema was changed.
void QueryEditorWidget::resetTableList()
{
	ui.tablesList->clear();
    if (!m_schema.isEmpty()) {
        ui.tablesList->addItems(Database::getObjects("table", m_schema).keys());
		ui.tablesList->addItems(Database::getObjects("view", m_schema).keys());
    }
	ui.tablesList->adjustSize();
}

// overridden from QWidget
void QueryEditorWidget::paintEvent(QPaintEvent * event)
{
	/* This inelegant trick is needed because the widths of
	 * the data in the columns aren't known before the UI
	 * tries to paint.
	 */
	if (resizeWanted)
	{
		resizeWanted = false;
        Utils::setColumnWidths(ui.termsTab->ui.termsTable);
        Utils::setColumnWidths(ui.ordersTable);
	}
	QWidget::paintEvent(event);
}

// overridden from QWidget
void QueryEditorWidget::resizeEvent(QResizeEvent * event)
{
    Utils::setColumnWidths(ui.termsTab->ui.termsTable);
	Utils::setColumnWidths(ui.ordersTable);
	QWidget::resizeEvent(event);
}

void QueryEditorWidget::addAllSelect()
{
	QStringList list(columnModel->stringList());
    QStringList::const_iterator i;
    for (i = list.constBegin(); i != list.constEnd(); ++i) {
        QString s(*i);
		if (s.compare(m_columnList.rowid))
		{
			selectModel->append(s);
			columnModel->removeAll(s);
		}
	}
}

void QueryEditorWidget::addSelect()
{
	QItemSelectionModel *selections = ui.columnView->selectionModel();
	if (selections->hasSelection()) {
        QString val;
        QModelIndexList l = selections->selectedIndexes();
        QModelIndexList::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            val = columnModel->data(*i, Qt::DisplayRole).toString();
            selectModel->append(val);
            columnModel->removeAll(val);
        }
    }
}

void QueryEditorWidget::removeAllSelect()
{
	tableSelected(m_table);
}

void QueryEditorWidget::removeSelect()
{
	QItemSelectionModel *selections = ui.selectView->selectionModel();
	if (selections->hasSelection()) {
        QString val;
        QModelIndexList l = selections->selectedIndexes();
        QModelIndexList::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            val = selectModel->data(*i, Qt::DisplayRole).toString();
            columnModel->append(val);
            selectModel->removeAll(val);
        }
    }
}

void QueryEditorWidget::moreOrders()
{
	int i = ui.ordersTable->rowCount();
	ui.ordersTable->setRowCount(i + 1);
	QComboBox * fields = new QComboBox();
	fields->addItems(ui.termsTab->m_columnList);
	ui.ordersTable->setCellWidget(i, 0, fields);
	QComboBox * collators = new QComboBox();
	collators->addItem("BINARY");
	collators->addItem("NOCASE");
	collators->addItem("RTRIM");
	collators->addItem("LOCALIZED_CASE");
	collators->addItem("LOCALIZED");
	ui.ordersTable->setCellWidget(i, 1, collators);
	QComboBox * directions = new QComboBox();
	directions->addItem("ASC");
	directions->addItem("DESC");
	ui.ordersTable->setCellWidget(i, 2, directions);
	ui.orderLessButton->setEnabled(true);
	resizeWanted = true;
}

void QueryEditorWidget::lessOrders()
{
	int i = ui.ordersTable->rowCount() - 1;
	ui.ordersTable->removeRow(i);
	if (i == 0) { ui.orderLessButton->setEnabled(false); }
}

// This is the slot signaled when user picks a database from the combo box.
// Also called internally if we're given a database or if we pick the default.
void QueryEditorWidget::schemaSelected(const QString & schema)
{
	if (schema != m_schema) {
		m_schema = schema;
		resetTableList();
	}
}

QueryEditorWidget::QueryEditorWidget(QWidget * parent): QWidget(parent)
{
    m_schema = QString();
    m_table = QString();
	ui.setupUi(this);
    ui.schemaList->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    ui.tablesList->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	columnModel = new QueryStringModel(this);
	selectModel = new QueryStringModel(this);
	ui.columnView->setModel(columnModel);
	ui.selectView->setModel(selectModel);
	ui.ordersTable->setColumnCount(3);
	ui.ordersTable->horizontalHeader()->hide();
	ui.ordersTable->verticalHeader()->hide();
	ui.ordersTable->setShowGrid(false);
	connect(ui.addAllButton, SIGNAL(clicked()), this, SLOT(addAllSelect()));
	connect(ui.addButton, SIGNAL(clicked()), this, SLOT(addSelect()));
	connect(ui.removeAllButton, SIGNAL(clicked()),
            this, SLOT(removeAllSelect()));
	connect(ui.removeButton, SIGNAL(clicked()), this, SLOT(removeSelect()));
	connect(ui.columnView, SIGNAL(doubleClicked(const QModelIndex &)),
			this, SLOT(addSelect()));
	connect(ui.selectView, SIGNAL(doubleClicked(const QModelIndex &)),
			this, SLOT(removeSelect()));
	connect(ui.orderMoreButton, SIGNAL(clicked()), this, SLOT(moreOrders()));
	connect(ui.orderLessButton, SIGNAL(clicked()), this, SLOT(lessOrders()));
    connect(ui.schemaList, SIGNAL(currentIndexChanged(const QString)),
            this, SLOT(schemaSelected(const QString)));
	QPushButton * resetButton = new QPushButton("Reset", this);
    resetButton->setObjectName("Reset");
	connect(resetButton, SIGNAL(clicked(bool)),
            this, SLOT(resetSchemaList()));
	ui.tabWidget->setCornerWidget(resetButton, Qt::TopRightCorner);
}

QString QueryEditorWidget::whereClause()
{
    QString sql;
    QTableWidget * terms = ui.termsTab->ui.termsTable;
    int n = terms->rowCount();
	if (n > 0)
	{ // First determine what is the chosen logic word (And/Or)
        QString logicWord =
            (ui.termsTab->ui.andButton->isChecked()) ? ")\nAND (" : ")\nOR (";

		sql += "\nWHERE (";
        bool nocase = !(ui.termsTab->ui.caseCheckBox->isChecked());

		for (int i = 0; i < n; i++)
		{
			QComboBox * fields = qobject_cast<QComboBox *>
				(terms->cellWidget(i, 0));
			QComboBox * relations = qobject_cast<QComboBox *>
				(terms->cellWidget(i, 1));
			QLineEdit * le = qobject_cast<QLineEdit *>
				(terms->cellWidget(i, 2));
			if (fields && relations) {
                QString field = fields->currentText();
                if (nocase) { field = field.toLower(); }
				if (i > 0) { sql += logicWord; }

                QString value;
                if (relations->currentIndex() < 7) {
                    if (le) {
                        value = le->text();
                        if (nocase) { value = value.toLower(); }
                    } else { continue; }
                }
				switch (relations->currentIndex())
				{
					case 0:	// Contains
                        sql += Utils::q(field);
						sql += (" LIKE " + Utils::like(value));
						break;

					case 1: 	// Doesn't contain
                        sql += Utils::q(field);
						sql += (" NOT LIKE "
								+ Utils::like(value));
						break;

					case 2: 	// Starts with
                        sql += Utils::q(field);
						sql += (" LIKE "
								+ Utils::startswith(value));
						break;

					case 3:		// Equals
						// Avoid NULL when comparing with NULL
                        sql += Utils::q(field);
						sql += (" IS " + Utils::q(value, "'"));
						break;

					case 4:		// Not equals
						// Avoid NULL when comparing with NULL
                        sql += Utils::q(field);
						sql += (" IS NOT " + Utils::q(value, "'"));
						break;

					case 5:		// Bigger than
                        sql += Utils::q(field);
						if (SqlParser::isNumber(value)) {
							sql += (" > " + value);
						} else {
							sql += (" > " + Utils::q(value, "'"));
						}
						break;

					case 6:		// Smaller than
                        sql += Utils::q(field);
						if (SqlParser::isNumber(value)) {
							sql += (" < " + value);
						} else {
							sql += (" < " + Utils::q(value, "'"));
						}
						break;

					case 7:		// is null
                        sql += Utils::q(field);
						sql += (" ISNULL");
						break;

					case 8:		// is not null
                        sql += Utils::q(field);
						sql += (" NOTNULL");
						break;

                    case 9:     // is empty (or null)
                        sql += ("( ");
                        sql += Utils::q(field);
						sql += (" ISNULL OR ");
                        sql += Utils::q(field);
                        sql += (" = '' )");
                        break;

                    case 10:    // is not empty (and not null)
                        sql += ("( ");
                        sql += Utils::q(field);
						sql += (" NOTNULL AND ");
                        sql += Utils::q(field);
                        sql += (" != '' )");
                        break;
				}
			}
		}
		sql += ")";
    }
    return sql;
}

//! \brief generates a valid SELECT statement using the values in the dialog'
/* elide is true if the schema name should be elided from the SELECT
 * clause because we're creating a non-temporary view,
 * This avoids a problem if we try to attach this database in a later
 * session, possibly with a different name. A reference in a view to
 * a database name will try to make a view in the attached database
 * onto the database whose name is referenced, which isn't allowed
 * if the refernced name isn't the same as the name with which the
 * database is being attached.
 *
 * The schema name isn't needed in the SELECT statement (apart from
 * temporary views) because the sqlite execute logic knows that any
 * table or view referenced in the view definition can only be in the
 * database in which the view is being created.
 *
 * sqliteman versions before 1.8.1 didn't have this check,
 * and could create databases which couldn't be attached.
 *
 * Currently CreateViewDialog doesn't support creating temporary views,
 * but I might add this at some time in the future.
 *
 * For tables the situation is different. The SELECT clause can
 * reference any table or view in the main databsee or any attached
 * database, becaue the SELECT clause gets executed when the table
 * is created and doesn't get saved in the database.
 */
QString QueryEditorWidget::statement(bool elide)
{
    Preferences * prefs = Preferences::instance();
    int width = prefs->textWidthMarkSize();
    QString sql;
	QString line = "SELECT ";

	// columns
	if (selectModel->rowCount() == 0)
		sql = line + "* ";
	else
	{
        QStringList columns = selectModel->stringList();
        bool first = true;
        while (columns.count() > 0) {
            if (first) { first = false; } else { line += ","; }
            QString s = Utils::q(columns.takeFirst());
            if (line.size() + s.size() > width) {
                sql += line + "\n";
                line = " " + s;
            } else {
                line += " " + s;
            }
        }
		sql += line;
	}

	sql += "\nFROM ";

	// insert table or view name
	/* If (and only if) we're creating a view here,
	 * we don't want to qualify the table name with the schema
	 * because this can make it impossible to attach this database.
	 */
	if (!elide) {
		sql += Utils::q(m_schema) + ".";
	}
	sql += Utils::q(m_table);

	// Optionally add terms
    sql += whereClause();

	// optionally add ORDER BY clauses
	if (ui.ordersTable->rowCount() > 0)
	{
		sql += "\nORDER BY ";
		for (int i = 0; i < ui.ordersTable->rowCount(); i++)
		{
			QComboBox * fields =
				qobject_cast<QComboBox *>(ui.ordersTable->cellWidget(i, 0));
			QComboBox * collators =
				qobject_cast<QComboBox *>(ui.ordersTable->cellWidget(i, 1));
			QComboBox * directions =
				qobject_cast<QComboBox *>(ui.ordersTable->cellWidget(i, 2));
			if (fields && collators && directions)
			{
				if (i > 0) { sql += ",\n "; }
				sql += Utils::q(fields->currentText());
				sql += " COLLATE ";
				sql += collators->currentText() + " ";
				sql += directions->currentText();
			}
		}
	}

	sql += ";";
	return sql;
}

/* Generates a DELETE statement by replacing SELECT ... FROM
 * with DELETE FROM:
 * executing this will delete all the rows found by the SELECT statement.
 * We should only call this if we just ran a built query on a real table.
 */
QString QueryEditorWidget::deleteStatement()
{
	QString sql = "DELETE\n";

	// Add table name
	// It's safe to always prefix the database name here
	// because this doesn't go into the schema.
    sql += ("\nFROM " + Utils::q(m_schema) + "." + Utils::q(m_table));

	// Add terms
    QString terms(whereClause());
    // Safety precaution: use Empty Table to really delete all rows
    if (terms.isEmpty()) { return NULL; }
	return sql + terms + ";";
}


/* Called when a table or a view is altered or dropped.
 * oldName is the old name for an altered table or view
 * or the name of a table or view which was dropped
 * or a null string if if an EXEC might have changed
 * a table or a view, or the schema changed.
 * newName is the new name of an altered table or view
 * or a null string in other cases.
 */
void QueryEditorWidget::tableGone(QString oldName, QString newName)
{
	if (oldName.isEmpty()) {
		// We don't know which tables and views may have changed
		resetTableList();
		/* This can do the wrong thing in the case where some EXEC
		 * code drops a table and creates a new one with the same name,
		 * but this isn't very likely and it doesn't do much harm since
		 * it only results in the dialog showing the column list for the
		 * newly created table - which may be what the user wanted anyway.
		 */
		newName = m_table;
	} else {
		// oldName was dropped or altered
		int n = ui.tablesList->count();
		for (int i = 0; i < n; ++i)
		{
			QString s(ui.tablesList->itemText(i));
			if (s.compare(oldName, Qt::CaseInsensitive) == 0)
			{
				// Found oldName in table list:
				// avoid prematurely calling tableSelected()
				ui.tablesList->setEnabled(false);
				disconnect(ui.tablesList,
						   SIGNAL(currentIndexChanged(const QString &)),
						   this, SLOT(tableSelected(const QString &)));
				if (newName.isEmpty()) {
					// oldname was dropped
					ui.tablesList->removeItem(i);
				} else {
					// oldName was altered to newName
					ui.tablesList->setItemText(i, newName);
				}
				// If there isn't still more than one table or view ...
				if (ui.tablesList->count() <= 1) {
                    m_canChangeTable = false;
                }
				if (m_canChangeTable) {
					// reconnect the user action on picking one
					connect(ui.tablesList,
							SIGNAL(currentIndexChanged(const QString &)),
							this, SLOT(tableSelected(const QString &)),
							Qt::UniqueConnection);
					ui.tablesList->setEnabled(true);
				}
				break;
			}
		}
		if (m_table.compare(oldName, Qt::CaseInsensitive)) {
			// if m_table wasn't the one dropped or altered, we're done
			return;
		}
	}
	findTable(newName); // calls tableSelected
}

/* This is called by our parent to set the QueryEditorWidget up
 * for a specified schema and table or view to query.
 * Also called internally if a schema may have been detached,
 * to update the schemaList and the tablesList if the previous
 * current schema was detached.
 * If we are querying the same table or view as in the last time
 * we were called, we keep the same fields, terms, and orderings.
 * Otherwise we reset.
 */
void QueryEditorWidget::setSchema(QString schema, QString table,
                                  bool schemaMayChange, bool tableMayChange)
{
	m_canChangeSchema = schemaMayChange;
	m_canChangeTable = tableMayChange;
    ui.schemaList->setEnabled(false); // disable before disconnecting
    disconnect(ui.schemaList, SIGNAL(currentIndexChanged(const QString &)),
               this, SLOT(schemaSelected(const QString &)));
    // We need to recreate the schema list because a schema
    // may have been detached sicne we were last called.
	ui.schemaList->clear();
	ui.schemaList->addItems(Database::getDatabases().keys());
	ui.schemaList->adjustSize();
	// MatchFixedString implies CaseInsensitive
	int i = ui.schemaList->findText(m_schema, Qt::MatchFixedString);
	int n = ui.schemaList->count();
    // See if the schema has changed
    if (schema.compare(m_schema, Qt::CaseInsensitive)) {
        m_schema = schema;
        resetTableList(); // previous table list is no longer valid
        m_table.clear(); // previous table is no longer valid
        // check the newly requested schema
        i = ui.schemaList->findText(schema, Qt::MatchFixedString);
    }
    if (i < 0) { // not (now) a database (may have been detached).
        if (n > 0) { // At least one schema in list
            // Default to first schema, but allow user to change it
            // if there is more than one
            i = 0;
            m_schema = ui.schemaList->itemText(0);
            m_canChangeSchema = n > 1;
        } else { // no schemas (started with no file?)
            schema.clear();
        }
        table.clear(); // requested table is not valid
        m_table.clear(); // previous table is no longer valid
        resetTableList(); // previous table list is no longer valid
    } else if (n < 2) {
        m_canChangeSchema = false;
        // We don't set it to true if n >= 2 because if we got here
        // our caller passed us a valid database and we should
        // honour the passed value of schemaMayChange.
    }
	ui.schemaList->setCurrentIndex(i); // i will be -1 if n is 0
    if (m_canChangeSchema) {
        connect(ui.schemaList, SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(schemaSelected(const QString &)),
                Qt::UniqueConnection);
        ui.schemaList->setEnabled(true); // enable after connecting
    }
    findTable(table); // calls tableSelected
}

void QueryEditorWidget::setSchema(QString schema,
                bool schemaMayChange, bool tableMayChange)
{
    setSchema(schema, m_table, schemaMayChange, tableMayChange);
}

void QueryEditorWidget::clear()
{
    ui.schemaList->setEnabled(false); // disable before disconnecting
    disconnect(ui.schemaList, SIGNAL(currentIndexChanged(const QString &)),
               this, SLOT(schemaSelected(const QString &)));
    ui.schemaList->clear();
    m_schema.clear();
    ui.tablesList->setEnabled(false);
    disconnect(ui.tablesList,
                SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(tableSelected(const QString &)));
    ui.tablesList->clear();
    m_table.clear();
}


void QueryEditorWidget::copySql(bool elide)
{
	QApplication::clipboard()->setText(statement(elide));
}

void QueryEditorWidget::copySql()
{
	copySql(!m_columnList.queryingTable);
}

/* Called when we need to reset the query.
 * This can happen if the user clicks reset or if the current
 * query has become invalid because the schema has been removed
 * (or may have been removed by an EXEC)
 * or because the current table has been removed or altered
 * (or may have been removed or altered by an EXEC).
 *
 * Currently it is also called when the dialog is shown because
 * some changes might have occurred while the dialog was not visible,
 * but I hope to make it modeless in the future,
 * so it will need to react to the events listed above.
 */
void QueryEditorWidget::resetSchemaList()
{
	setSchema(m_schema, m_table, m_canChangeSchema, m_canChangeTable);
}
