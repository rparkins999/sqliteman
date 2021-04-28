/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

New version built on tableeditordialog
*/

#include <QtCore/qmath.h>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QList>
#include <QMessageBox>
#include <QTreeWidgetItem>

#include <sys/types.h>
#include <unistd.h>

#include "createindexdialog.h"
#include "litemanwindow.h"
#include "mylineedit.h"
#include "preferences.h"
#include "sqlparser.h"
#include "utils.h"

// overload slot collatorIndexChanged (different arguments)
void CreateIndexDialog::collatorIndexChanged(int n, QComboBox * box)
{
    box->setToolTip(n ? "" : "default is BINARY");
}

void CreateIndexDialog::addField(FieldInfo f)
{
    int rc = ui.columnTable->rowCount();
    ui.columnTable->setRowCount(rc + 1);
    QTableWidgetItem * it = new QTableWidgetItem(f.name);
    it->setFlags(Qt::ItemIsEnabled);
    ui.columnTable->setItem(rc, 0, it);
    QComboBox * collateBox = new QComboBox(this);
    QStringList collators;
    collators << "" << "BINARY" << "NOCASE" << "RTRIM" // built-in
              << "LOCALIZED" << "LOCALIZED_CASE"; // sqliteman private
    collateBox->addItems(collators);
    int n = collateBox->findText(f.collator, Qt::MatchFixedString);
    if (n < 0) { n = 0; }
    collateBox->setCurrentIndex(n);
    collatorIndexChanged(n, collateBox);
    connect(collateBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(collatorIndexChanged(int)));
    ui.columnTable->setCellWidget(rc, 1, collateBox);
    QComboBox * ascDesc = new QComboBox(this);
    QStringList ascDescStrings;
    ascDescStrings << "ASC" << "DESC";
    ascDesc->addItems(ascDescStrings);
    ascDesc->setCurrentIndex(0);
    ui.columnTable->setCellWidget(rc, 2, ascDesc);
    it = new QTableWidgetItem();
    it->setIcon(Utils::getIcon("move-up.png"));
    it->setToolTip("move up");
    it->setFlags((rc == 0) ? Qt::NoItemFlags : Qt::ItemIsEnabled);
    ui.columnTable->setItem(rc, 3, it);
    it = new QTableWidgetItem();
    it->setIcon(Utils::getIcon("move-down.png"));
    it->setToolTip("move down");
    it->setFlags(Qt::NoItemFlags);
    if (rc > 0) {
        ui.columnTable->item(rc - 1, 4)->setFlags(Qt::ItemIsEnabled);
    }
    ui.columnTable->setItem(rc, 4, it);
    it = new QTableWidgetItem();
    it->setIcon(Utils::getIcon("delete.png"));
    it->setToolTip("remove");
    it->setFlags(Qt::ItemIsEnabled);
    ui.columnTable->setItem(rc, 5, it);
}

void CreateIndexDialog::swap(int i, int j)
{
	// Much of this unpleasantness is caused by the lack of a
	// QTableWidget::takeCellWidget() function.
    QTableWidgetItem * iti = ui.columnTable->takeItem(i, 0);
    QTableWidgetItem * itj = ui.columnTable->takeItem(j, 0);
    ui.columnTable->setItem(i, 0, itj);
    ui.columnTable->setItem(j, 0, iti);
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
	k = iBox->currentIndex();
	jNewBox->setCurrentIndex(k);
    collatorIndexChanged(k, jNewBox);
	jNewBox->setEditable(false);
	connect(jNewBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(collatorIndexChanged()));
    k = jBox->currentIndex();
	iNewBox->setCurrentIndex(k);
    collatorIndexChanged(k, iNewBox);
	iNewBox->setEditable(false);
	connect(iNewBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(collatorIndexChanged()));
	ui.columnTable->setCellWidget(i, 1, iNewBox); // destroys old one
	ui.columnTable->setCellWidget(j, 1, jNewBox); // destroys old one
	iBox = qobject_cast<QComboBox *>(ui.columnTable->cellWidget(i, 2));
	jBox = qobject_cast<QComboBox *>(ui.columnTable->cellWidget(j, 2));
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
	ui.columnTable->setCellWidget(i, 2, iNewBox); // destroys old one
	ui.columnTable->setCellWidget(j, 2, jNewBox); // destroys old one
    iti = ui.columnTable->takeItem(i, 3);
    itj = ui.columnTable->takeItem(j, 3);
    iti->setFlags(i ? Qt::ItemIsEnabled: Qt::NoItemFlags);
    itj->setFlags(j ? Qt::ItemIsEnabled: Qt::NoItemFlags);
    ui.columnTable->setItem(i, 3, itj);
    ui.columnTable->setItem(j, 3, iti);
    k = ui.columnTable->rowCount() - 1;
    iti = ui.columnTable->takeItem(i, 4);
    itj = ui.columnTable->takeItem(j, 4);
    iti->setFlags((i != k) ? Qt::ItemIsEnabled : Qt::NoItemFlags);
    itj->setFlags((j != k) ? Qt::ItemIsEnabled : Qt::NoItemFlags);
    ui.columnTable->setItem(i, 4, itj);
    ui.columnTable->setItem(j, 4, iti);
}

void CreateIndexDialog::drop(int row)
{
    if (ui.columnTable->rowCount() > 1) {
        ui.columnTable->setCurrentCell(row, 0);
        removeField();
        ui.columnTable->item(0, 3)->setFlags(Qt::NoItemFlags);
        ui.columnTable->item(ui.columnTable->rowCount() - 1, 4)
            ->setFlags(Qt::NoItemFlags);
        if (ui.columnTable->rowCount() <= 1) {
            ui.columnTable->item(0, 5)->setFlags(Qt::NoItemFlags);
        }
    }
}

// private slots:
void CreateIndexDialog::collatorIndexChanged(int n)
{
    collatorIndexChanged(n, (QComboBox *)sender());
}

void CreateIndexDialog::resetClicked()
{
    ui.resultEdit->clear();
    ui.queryEditor->ui.termsTab->m_columnList.clear();
	// Initialize fields
	SqlParser * parsed = Database::parseTable(m_tableName, m_databaseName);
	QList<FieldInfo> fields = parsed->m_fields;
	ui.columnTable->clearContents();
	ui.columnTable->setRowCount(0);
	int n = fields.size();
	for (int i = 0; i < n; i++) {
        addField(fields[i]);
        ui.queryEditor->ui.termsTab->m_columnList.append(fields[i].name);
    }
	delete parsed;
    ui.queryEditor->ui.termsTab->ui.orButton->setChecked(true);
    setFirstLine();
	checkChanges();
}

void CreateIndexDialog::indexNameEdited(const QString & text)
{
    setFirstLine();
    checkChanges();
}

void CreateIndexDialog::cellClicked(int row, int column)
{
	int n = ui.columnTable->rowCount();
	switch (column)
	{
		case 3: // move up
			if (row > 0) { swap(row - 1, row);  }
			break;
		case 4: // move down
			if (row < n - 1) { swap(row, row + 1); }
			break;
		case 5: // delete
			if (ui.columnTable->rowCount() > 1) { drop(row); }
			break;
		default: return; // cell handles its editing
	}
}

// User clicked on "Create", go ahead and do it
void CreateIndexDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("");
    if (execSql(getSql(), tr("Cannot create index"))) {
        resultAppend(tr("Index created successfully"));
        emit rebuildTableTree(m_databaseName);
    }
}

// protected:

// implementation of virtual function from TableEditorDialog
void CreateIndexDialog::setFirstLine(QWidget * w) {}

void CreateIndexDialog::setFirstLine()
{
	QString s("CREATE ");
    if (ui.withoutRowid->isChecked()) { s += "UNIQUE "; }
    s = s + "INDEX "
            + Utils::q(m_databaseName) + "." + Utils::q(ui.nameEdit->text())
            + " ON " + Utils::q(m_tableName) + " (";
	ui.firstLine->setText(s);
}

QString CreateIndexDialog::getSQLfromDesign()
{
    QString sql;
    int n = ui.columnTable->rowCount();
    for (int i = 0; i < n; ++i) {
        QComboBox * collatorBox =
            qobject_cast<QComboBox *>(ui.columnTable->cellWidget(i, 1));
        QString collator = collatorBox->currentIndex() ?
            Utils::q(collatorBox->currentText()) : "BINARY";
        QComboBox * ascDesc =
            qobject_cast<QComboBox *>(ui.columnTable->cellWidget(i, 2));
        sql = sql + (i ? ",\n" : "")
                  + Utils::q(ui.columnTable->item(i, 0)->text())
                  + " COLLATE " + collator + " "
                  + ascDesc->currentText();
    }
    return sql + ")" + ui.queryEditor->whereClause() + ";";
}

QString CreateIndexDialog::getSQLfromGUI(QWidget * w)
{
	if (w == ui.sqlTab) {
		return ui.textEdit->text();
	} else {
		return getSQLfromDesign();
	}
}

// public:
CreateIndexDialog::CreateIndexDialog(
    const QString & tabName, const QString & schema, LiteManWindow * parent)
	: TableEditorDialog(parent)
{
	setWindowTitle(tr("Create Index"));
    if (schema.isEmpty()) {
        ui.databaseCombo->addItems(m_creator->visibleDatabases());
        m_noTemp = (ui.databaseCombo->findText(
            "temp", Qt::MatchFixedString) < 0);
        if (m_noTemp) { ui.databaseCombo->addItem("temp"); }
        ui.databaseCombo->setCurrentIndex(0);
        if (ui.databaseCombo->count() > 1) {
            connect(
                ui.databaseCombo, SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(databaseChanged(const QString &)));
            ui.databaseCombo->setEnabled(true);
        } else { ui.databaseCombo->setEnabled(false); }
        m_databaseName = ui.databaseCombo->itemText(0);
    } else {
        ui.databaseCombo->addItem(schema);
        ui.databaseCombo->setEnabled(false);
        m_databaseName = schema;
    }
    ui.labelTable->setText("Index name");
	connect(ui.nameEdit, SIGNAL(textEdited(QString)),
            this, SLOT(indexNameEdited(QString)));
    if (tabName.isEmpty()) {
        ui.tableCombo->addItems(
            Database::getObjects("table", m_databaseName).keys());
        ui.tableCombo->setCurrentIndex(0);
        if (ui.tableCombo->count() > 1) {
            connect(ui.tableCombo, SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(tableChanged(const QString &)));
            ui.tableCombo->setEnabled(true);
        } else { ui.tableCombo->setEnabled(false); }
        m_tableName = ui.tableCombo->itemText(0);
    } else {
        ui.tableCombo->addItem(tabName);
        ui.tableCombo->setEnabled(false);
        m_tableName = tabName;
    }
    ui.tableCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_tableOrView = "INDEX";
    ui.tabWidget->setTabText(0, "Columns");
    ui.tabWidget->setTabToolTip(0, "Choose columns to index");
    ui.tabWidget->setTabText(1, "WHERE");
    ui.tabWidget->setTabToolTip(1, "Add conditions for indexed rows");
	QPushButton * resetButton = new QPushButton("Reset", this);
	ui.tabWidget->setCornerWidget(resetButton, Qt::TopRightCorner);
	connect(resetButton, SIGNAL(clicked(bool)), this, SLOT(resetClicked()));
    fixTabWidget();
    ui.columnTable->horizontalHeaderItem(1)->setText("Collator");
    ui.columnTable->horizontalHeaderItem(1)->setToolTip(
        "Collation order for column");
    ui.columnTable->horizontalHeaderItem(2)->setText("Asc/Desc");
    ui.columnTable->horizontalHeaderItem(3)->setText("");
	ui.columnTable->insertColumn(4);
	ui.columnTable->setHorizontalHeaderItem(4, new QTableWidgetItem());
	ui.columnTable->insertColumn(5);
	ui.columnTable->setHorizontalHeaderItem(5, new QTableWidgetItem());
	connect(ui.columnTable, SIGNAL(cellClicked(int, int)),
			this, SLOT(cellClicked(int,int)));
    ui.withoutRowid->setText("Unique");
    ui.columnsGrid->removeWidget(ui.addButton);
    delete ui.addButton;
    ui.columnsGrid->removeWidget(ui.removeButton);
    delete ui.removeButton;
    ui.removeButton = 0;
    ui.queryEditor->ui.mainLayout->removeItem(ui.queryEditor->ui.hLayout);
    delete ui.queryEditor->ui.DatabaseName;
    delete ui.queryEditor->ui.schemaList;
    delete ui.queryEditor->ui.tableName;
    delete ui.queryEditor->ui.tablesList;
    delete ui.queryEditor->ui.hLayout;
    ui.queryEditor->ui.mainLayout->removeWidget(ui.queryEditor->ui.aTTQlabel);
    delete ui.queryEditor->ui.aTTQlabel;
    ui.queryEditor->ui.tabWidget->removeTab(2);
    ui.queryEditor->ui.tabWidget->removeTab(0);
    ui.queryEditor->ui.tabWidget->setCurrentIndex(0);
    // debugging
    QWidget * innerReset =
        (QWidget *)ui.queryEditor->findChild<QObject*>("Reset");
    if (innerReset) {
        innerReset->setEnabled(false);
        innerReset->hide();
    }
	m_createButton =
		ui.buttonBox->addButton("Create", QDialogButtonBox::ApplyRole);
	m_createButton->setDisabled(true);
	connect(m_createButton, SIGNAL(clicked(bool)),
			this, SLOT(createButton_clicked()));
	resetClicked();
    Preferences * prefs = Preferences::instance();
	resize(prefs->createindexWidth(), prefs->createindexHeight());
}

CreateIndexDialog::~CreateIndexDialog() {
    Preferences * prefs = Preferences::instance();
    prefs->setcreateindexHeight(height());
    prefs->setcreateindexWidth(width());
}

// public slots:

// reimplementation of virtual function from TableEditorDialog
void CreateIndexDialog::checkChanges()
{
	QString newName(ui.nameEdit->text());
	bool ok = checkOk();
	m_createButton->setEnabled(ok);
    ui.resultEdit->clear();
    if (ui.columnTable->rowCount() > 0) {
        ui.columnTable->item(0, 5)->setFlags(
            (ui.columnTable->rowCount() > 1)
                ? Qt::ItemIsEnabled : Qt::NoItemFlags );
    }
}

void CreateIndexDialog::tableChanged(const QString table) {
    m_tableName = table;
    setFirstLine();
    checkChanges();
}

void CreateIndexDialog::databaseChanged(const QString schema) {
    m_databaseName = schema;
    setFirstLine();
    checkChanges();
}
