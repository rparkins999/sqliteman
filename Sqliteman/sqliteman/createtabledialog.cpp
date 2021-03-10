/*
For general Sqliteman copyright and licensing information please refer
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
#include "tabletree.h"
#include "utils.h"

CreateTableDialog::CreateTableDialog(LiteManWindow * parent,
									 QTreeWidgetItem * item)
	: TableEditorDialog(parent)
{
	ui.removeButton->setEnabled(false); // Disable row removal
	ui.withoutRowid->setChecked(false);
	setWindowTitle(tr("Create Table"));
	m_tableOrView = "TABLE";
	m_dubious = false;
	m_oldWidget = ui.designTab;
	
	m_createButton =
		ui.buttonBox->addButton("Create", QDialogButtonBox::ApplyRole);
	m_createButton->setDisabled(true);
	connect(m_createButton, SIGNAL(clicked(bool)),
			this, SLOT(createButton_clicked()));

	if (item && (   (item->type() == TableTree::DatabaseItemType)
				 || (item->type() == TableTree::TablesItemType)))
	{
		int i = ui.databaseCombo->findText(item->text(1),
			Qt::MatchFixedString | Qt::MatchCaseSensitive);
		if (i >= 0)
		{
			ui.databaseCombo->setCurrentIndex(i);
			ui.databaseCombo->setDisabled(true);
		}
	}
	m_tabWidgetIndex = ui.tabWidget->currentIndex();
    m_originalName = QString();

	ui.textEdit->setText("");
	ui.queryEditor->setItem(0);
	addField(); // A table should have at least one field
	Utils::setColumnWidths(ui.columnTable);
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
	if (ui.nameEdit->text().contains(QRegExp
		("\\s|-|\\]|\\[|[`!\"%&*()+={}:;@'~#|\\\\<,>.?/^]")))
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
	QString sql(ui.firstLine->text() + getSQLfromGUI());
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if(query.lastError().isValid())
	{
		resultAppend(tr("Cannot create table")
					 + ":<br/><span style=\" color:#ff0000;\">"
					 + query.lastError().text()
					 + "<br/></span>" + tr("using sql statement:")
					 + "<br/><tt>" + sql);
		return;
	}
	updated = true;
	emit rebuildTableTree(ui.databaseCombo->currentText(),
						 ui.nameEdit->text());
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
	m_createButton->setEnabled(checkOk(ui.nameEdit->text()));
}
