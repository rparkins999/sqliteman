/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidgetItem>

#include "createviewdialog.h"
#include "database.h"
#include "litemanwindow.h"
#include "preferences.h"

bool CreateViewDialog::checkColumn(int i, QString cname,
								   QString ctype, QString cextra)
{
	return false;
}

CreateViewDialog::CreateViewDialog(LiteManWindow * parent,
									 QTreeWidgetItem * item)
	: TableEditorDialog(parent)
{
    ui.gridLayout->removeItem(ui.onTableBox);
    delete ui.onTableBox;
    delete ui.onTableLabel;
    delete ui.tableCombo;
	ui.labelTable->setText("View Name:");
	setWindowTitle(tr("Create View"));
	m_tableOrView = "VIEW";

	m_createButton =
		ui.buttonBox->addButton("Create", QDialogButtonBox::ApplyRole);
	m_createButton->setDisabled(true);
	connect(m_createButton, SIGNAL(clicked()), this,
			SLOT(createButton_clicked()));
    setItem(item, true);
    ui.tabWidget->removeTab(0);
    fixTabWidget();
	ui.textEdit->setText("");
    Preferences * prefs = Preferences::instance();
	resize(prefs->createviewWidth(), prefs->createviewHeight());
}

CreateViewDialog::~CreateViewDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setcreateviewHeight(height());
    prefs->setcreateviewWidth(width());
}

void CreateViewDialog::setSql(QString query)
{
	m_dirty = false;
	ui.tabWidget->setCurrentWidget(ui.sqlTab);
	ui.textEdit->setText(query);
	setDirty();
}

void CreateViewDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("");
    if (execSql(getSql(), tr("Cannot create view"))) {
        m_updated = true;
        emit rebuildViewTree(m_databaseName, ui.nameEdit->text());
        resultAppend(tr("View created successfully"));
	}
}

void CreateViewDialog::checkChanges()
{
	m_createButton->setEnabled(checkOk());
}

void CreateViewDialog::databaseChanged(const QString schema)
{
    if (m_databaseName != schema) {
        QString database = ui.queryEditor->ui.schemaList->currentText();
        QString table = ui.queryEditor->ui.tablesList->currentText();
        if (schema != "temp") {
            if (schema != database) { table = QString(); }
            ui.queryEditor->setSchema(schema, table, false, true);
        } else {
            ui.queryEditor->setSchema(database, table, true, true);
        }
        m_databaseName = schema;
    }
}
