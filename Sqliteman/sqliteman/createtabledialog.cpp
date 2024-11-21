/*
// For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QCheckBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>

#include "createtabledialog.h"
#include "database.h"
#include "litemanwindow.h"
#include "preferences.h"
#include "utils.h"

CreateTableDialog::CreateTableDialog(LiteManWindow * parent,
									 QTreeWidgetItem * item)
	: TableEditorDialog(parent)
{
    ui.gridLayout->removeItem(ui.onTableBox);
    delete ui.onTableBox;
    delete ui.onTableLabel;
    delete ui.tableCombo;
	ui.withoutRowid->setChecked(false);
	setWindowTitle(tr("Create Table"));
	m_dubious = false;
		m_createButton =
		ui.buttonBox->addButton("Create", QDialogButtonBox::ApplyRole);
        m_createButton->setEnabled(false);
	connect(m_createButton, SIGNAL(clicked(bool)),
			this, SLOT(createButton_clicked()));
    setItem(item, false);
    fixTabWidget();
	ui.textEdit->setText("");
	TableEditorDialog::addField(); // A table should have at least one field
    Preferences * prefs = Preferences::instance();
	resize(prefs->createtableWidth(), prefs->createtableHeight());
}

CreateTableDialog::~CreateTableDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setcreatetableHeight(height());
    prefs->setcreatetableWidth(width());
}

void CreateTableDialog::addField()
{
    TableEditorDialog::addField();
    fudge();
}

void CreateTableDialog::createButton_clicked()
{
	if (m_dubious)
	{
		int ret = QMessageBox::question(this, "Sqliteman",
			tr("A table with an empty column name "
			   "will not display correctly.\n"
			   "Are you sure you want to go ahead?\n\n"
			   "Yes to do it anyway, Cancel to try again."),
			QMessageBox::Yes, QMessageBox::Cancel);
		if (ret == QMessageBox::Cancel) { return; }
	}
	if (ui.nameEdit->text().contains(QRegExp("\\]|\\.")))
	{
		int ret = QMessageBox::question(this, "Sqliteman",
			tr("A table named ")
			+ ui.nameEdit->text()
			+ tr(" will not display correctly.\n"
				 "Are you sure you want to create it?\n\n"
				 "Yes to create, Cancel to try another name."),
			QMessageBox::Yes, QMessageBox::Cancel);
		if (ret == QMessageBox::Cancel) { return; }
	}
	ui.resultEdit->setHtml("");
    if (!execSql(getSql(), tr("Cannot create table"))) {
        return;
    }
	m_updated = true;
    // == OK here because sqlite uses lower case for it
    if (m_noTemp && (m_databaseName == "temp")) {
        ui.queryEditor->resetSchemaList();
    }
	emit rebuildTableTree(ui.databaseCombo->currentText());
	resultAppend(tr("Table created successfully"));
}

bool CreateTableDialog::checkColumn(int i, QString cname,
								   QString ctype, QString cextra)
{
	return false;
}

void CreateTableDialog::checkChanges()
{
	m_dubious = false;
	m_createButton->setEnabled(checkOk());
    ui.removeButton->setEnabled(ui.columnTable->rowCount() > 1);
}

void CreateTableDialog::databaseChanged(const QString schema)
{
    m_databaseName = schema;
    setFirstLine(ui.tabWidget->currentWidget());
    checkChanges();
}
