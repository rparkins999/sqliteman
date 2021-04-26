/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QClipboard>
#include <QComboBox>
#include <QLineEdit>
#include <QScrollBar>
#include <QTreeWidgetItem>

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
 * schemaList is enabled if, and only if, there is more than one database, and
 * a) we are a component of CreateTableDialog, or
 * b) we are a component of CreateViewDialog creating a temporary view and
 *    the temp database contains at least one table, or
 * c) we are a component of QueryEditorDialog and no database was initially
 *    selected, or
 * d) we are a component of a currently invisible QueryEditorDialog and a new
 *    database is attached.
 *
 * Except for case (d) and the test for more than one database, all of these
 * are handled by our parent passing suitable parameters to setSchema().
 * Case (d) is handled by a call to addSchema().
 *
 * We don't add "temp" to schemaList anywhere here, because we are looking for
 * a table or view to query. If there are any tables or views in "temp",
 * Database::getDatabases().keys() will include it.
 * 
 * This issue has to be dealt with in TableEditorDialog since the temporary
 * database can be created by creating a table or a view in it which selects
 * from a non-temporary table.
 *
 * tablesList is enabled if, and only if, there is more than one table in the
 * currently selected schema, and
 * a) we are a component of CreateTableDialog, or
 * b) we are a component of CreateViewDialog and we are not creating a view on
 *    a specific table or view, or
 * c) we are a component of QueryEditorDialog and no table was initially
 *    selected.
 *
 * Except for the test for more than one table, all of these are handled by
 * our parent passing suitable parameters to setSchema().
 *
 * The current version of this widget does not yet support creating a view
 * on a view.
 * FIXME we don't need to be able to parse a SELECT statement to do this:
 * we can get a column list on a table OR a view by doing SELECT * on it and
 * examining a QSqlrecord: this works even if there are no rows in it.
 *
 *****************************************************************************/

// Common bit of code to look up a table in the table list
// and enable or disable user changes.
// Returns the name it is given unless it is not in the list.
QString QueryEditorWidget::findTable(QString name, bool allowChange)
{
    int n = ui.tablesList->count();
    int i = ui.tablesList->findText(name, Qt::MatchExactly);
    if (i < 0) { // name is not a table in the database
        if (n > 0) {
            i = 0;
            name = ui.tablesList->itemText(0); // default to first entry
            allowChange = true; // but allow user to change it
        } else { name.clear(); } // empty list => empty name
    }
    if (allowChange && (n > 1)) { // can't change if only one entry
        ui.tablesList->setCurrentIndex(i);
        connect(ui.tablesList, SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(tableSelected(const QString &)),
                Qt::UniqueConnection);
        ui.tablesList->setEnabled(true); // enable after connecting
    } else {
        ui.tablesList->setEnabled(false); // disable before disconnecting
        disconnect(ui.tablesList, SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(tableSelected(const QString &)));
        ui.tablesList->setCurrentIndex(i); // i will be -1 if n is 0
    }
    return name;
}

// This is the slot signaled when user picks a table from the combo box.
void QueryEditorWidget::tableSelected(const QString & table)
{
	columnModel->clear();
	selectModel->clear();
	ui.tabWidget->setCurrentIndex(0);
	ui.termsTab->ui.termsTable->clear();
	ui.termsTab->ui.termsTable->setRowCount(0); // clear() doesn't do this
	ui.ordersTable->clear();
	ui.ordersTable->setRowCount(0); // clear() doesn't do this
    m_table = table;
    if (!table.isEmpty()) {
        QStringList columns;
        SqlParser * parser = Database::parseTable(m_table, m_schema);
        QList<FieldInfo> l = parser->m_fields;
        QList<FieldInfo>::const_iterator i;
        bool rowid = true; // no column named rowid
        bool _rowid_ = true; // no column named _rowid_
        bool oid = true; // no column named oid
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            columns << i->name;
            if (i->name.compare("rowid", Qt::CaseInsensitive) == 0)
                { rowid = false; }
            if (i->name.compare("_rowid_", Qt::CaseInsensitive) == 0)
                { _rowid_ = false; }
            if (i->name.compare("oid", Qt::CaseInsensitive) == 0)
                { oid = false; }
        }
        if (parser->m_hasRowid)
        {
            if (rowid)
                { columns.insert(0, QString("rowid")); m_rowid = "rowid"; }
            else if (_rowid_)
                { columns.insert(0, QString("_rowid_")); m_rowid = "_rowid_"; }
            else if (oid)
                { columns.insert(0, QString("oid")); m_rowid = "oid"; }
        }
        delete parser;
        ui.termsTab->m_columnList = columns;
        columnModel->setStringList(columns);
    }
	ui.termsTab->update();
	resizeWanted = true;
}

// This is called internally
// a) if we're given a table and the user may change it
//      ("name" is the table and "allowchange" is true)
// b) if we're given a table and the user may not change it
//      ("name" is the table and "allowchange" is false)
// c) if the active database changed and we must choose a default table
//    which the user may change
//      ("name" is empty)
// In case (a) or (b) if we can't find the table in the database,
// we choose the default, but allow the user to change it.
// If there are no tables in the database we can't open one at all
// and we just deselect the previously selected table.
void QueryEditorWidget::resetTableList(QString name, bool allowChange)
{
	ui.tablesList->clear();
    if (!m_schema.isEmpty()) {
        ui.tablesList->addItems(Database::getObjects("table", m_schema).keys());
// FIXME need to fix tableSelected() before this will work
//	 tablesList->addItems(Database::getObjects("view", m_schema).keys());
    }
	ui.tablesList->adjustSize();
    tableSelected(findTable(name, allowChange));
    resizeWanted = true;
}

// This is the slot signaled when user picks a database from the combo box.
// Also called internally if we're given a database or if we pick the default.
void QueryEditorWidget::schemaSelected(const QString & schema)
{
    m_schema = schema;
	resetTableList(QString(), true);
}

void QueryEditorWidget::addAllSelect()
{
	QStringList list(columnModel->stringList());
    QStringList::const_iterator i;
    for (i = list.constBegin(); i != list.constEnd(); ++i) {
        QString s(*i);
		if (s.compare(m_rowid))
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

void QueryEditorWidget::copySql()
{
	QApplication::clipboard()->setText(statement());
}

// overridden from QWidget
void QueryEditorWidget::paintEvent(QPaintEvent * event)
{
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
	connect(resetButton, SIGNAL(clicked(bool)),
            this, SLOT(resetClicked()));
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
            (ui.termsTab->ui.andButton->isChecked()) ? ") AND (" : ") OR (";

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
				sql += Utils::q(field);
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
						sql += (" LIKE " + Utils::like(value));
						break;

					case 1: 	// Doesn't contain
						sql += (" NOT LIKE "
								+ Utils::like(value));
						break;

					case 2: 	// Starts with
						sql += (" LIKE "
								+ Utils::startswith(value));
						break;

					case 3:		// Equals
						sql += (" = " + Utils::q(value, "'"));
						break;

					case 4:		// Not equals
						sql += (" <> " + Utils::q(value, "'"));
						break;

					case 5:		// Bigger than
						sql += (" > " + Utils::q(value, "'"));
						break;

					case 6:		// Smaller than
						sql += (" < " + Utils::q(value, "'"));
						break;

					case 7:		// is null
						sql += (" ISNULL");
						break;

					case 8:		// is not null
						sql += (" NOTNULL");
						break;
				}
			}
		}
		sql += ")";
    }
    return sql;
}

QString QueryEditorWidget::statement()
{
	QString sql = "SELECT\n";

	// columns
	if (selectModel->rowCount() == 0)
		sql += "* ";
	else
	{
		sql += Utils::q(selectModel->stringList(), "\"");
	}

	// Add table name
	sql += ("\nFROM " + Utils::q(m_schema) + "." + Utils::q(m_table));

	// Optionally add terms
    sql +=  whereClause();

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
				if (i > 0) { sql += ", "; }
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

QString QueryEditorWidget::deleteStatement()
{
	QString sql = "DELETE\n";

	// Add table name
    sql += ("\nFROM " + Utils::q(m_schema) + "." + Utils::q(m_table));

	// Add terms
    QString terms(whereClause());
    // Safety precaution: use Empty Table to really delete all rows
    if (terms.isEmpty()) { return NULL; }
	return sql + terms + ";";
}

/* This is called by our parent to set the QueryEditorWidget up
 * for a specified schema and table. */
void QueryEditorWidget::setSchema(QString schema, QString table,
                                  bool schemaMayChange, bool tableMayChange)
{
    ui.schemaList->setEnabled(false); // disable before disconnecting
    disconnect(ui.schemaList, SIGNAL(currentIndexChanged(const QString &)),
               this, SLOT(schemaSelected(const QString &)));
	ui.schemaList->clear();
	ui.schemaList->addItems(Database::getDatabases().keys());
	ui.schemaList->adjustSize();
    int n = ui.schemaList->count();
    int i = ui.schemaList->findText(schema, Qt::MatchExactly);
    if (i < 0) { // "schema" is not a database
        if (n > 0) {
            i = 0;
            schema = ui.schemaList->itemText(0); // default to first entry
            schemaMayChange = true; // but allow user to change it
            tableMayChange = true;
        } else { // no schemas (started with no file?)
            schema.clear();
            table.clear();
        }
    }
    if (schemaMayChange && (n > 1)) { // can't change if only one entry
        ui.schemaList->setCurrentIndex(i);
        connect(ui.schemaList, SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(schemaSelected(const QString &)),
                Qt::UniqueConnection);
        ui.schemaList->setEnabled(true); // enable after connecting
    } else {
        ui.schemaList->setCurrentIndex(i); // i will be -1 if n is 0
    }
    if (m_schema.compare(schema, Qt::CaseInsensitive)) {
        m_schema = schema;
        resetTableList(table, tableMayChange);
    } else {
        tableSelected(findTable(table, tableMayChange));
        resizeWanted = true;
    }
}

void QueryEditorWidget::treeChanged()
{
    setSchema(m_schema, m_table, true, true);
}

/* LiteManWindow calls this if AlterTableDialog alters a table.
 * If it's in our table list, its name may have changed.
 * If it's our currently selected table, its column list may have changed.
 */
void QueryEditorWidget::tableAltered(QString oldName, QTreeWidgetItem * item)
{
	if (m_schema == item->text(1))
	{
		QString newName(item->text(0));
		for (int i = 0; i < ui.tablesList->count(); ++i)
		{
			if (ui.tablesList->itemText(i) == oldName)
			{
				ui.tablesList->setItemText(i, newName);
				if (m_table == oldName) { tableSelected(newName); }
			}
			break;
		}
	}
}

void QueryEditorWidget::addSchema(const QString & schema)
{
    int index = ui.schemaList->findText(schema, Qt::MatchExactly);
    if (index < 0) { // it isn't already there
        ui.schemaList->addItem(schema);
        if (ui.schemaList->count() > 1) {
            connect(ui.schemaList, SIGNAL(currentIndexChanged(const QString &)),
                    this, SLOT(schemaSelected(const QString &)),
                    Qt::UniqueConnection);
            ui.schemaList->setEnabled(true);
        }
    }
}

void QueryEditorWidget::resetClicked()
{
	if (ui.schemaList->isEnabled())
	{
		ui.schemaList->setCurrentIndex(0);
		schemaSelected(ui.schemaList->currentText());
	}
	if (ui.tablesList->isEnabled()) { ui.tablesList->setCurrentIndex(0); }
	tableSelected(ui.tablesList->currentText());
}
