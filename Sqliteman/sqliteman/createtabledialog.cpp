/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QCheckBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>
#include <QMessageBox>
#include <QLineEdit>

#include "createtabledialog.h"
#include "database.h"
#include "utils.h"

CreateTableDialog::CreateTableDialog(LiteManWindow * parent,
									 QTreeWidgetItem * item)
	: TableEditorDialog(parent)
{
	creator = parent;
	updated = false;
	m_dirty = false;
	ui.removeButton->setEnabled(false); // Disable row removal
	setWindowTitle(tr("Create Table"));

	if (item)
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
	connect(ui.tabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(tabWidget_currentChanged(int)));
	m_createButton =
		ui.buttonBox->addButton("Create", QDialogButtonBox::ApplyRole);
	m_createButton->setDisabled(true);
	connect(m_createButton, SIGNAL(clicked(bool)),
			this, SLOT(createButton_clicked()));
	connect(ui.nameEdit, SIGNAL(textEdited(const QString&)),
			this, SLOT(checkChanges()));

	ui.textEdit->setText("");
	ui.queryEditor->setItem(0);
	addField(); // A table should have at least one field
}

QString CreateTableDialog::getSQLfromGUI()
{
	QString sql;
	switch (m_tabWidgetIndex)
	{
		case 0:
			sql = TableEditorDialog::getSQLfromGUI();
			break;
		case 1:
			sql = getFullName();
			sql += " AS " + ui.queryEditor->statement();
			break;
		case 2:
			sql = ui.textEdit->text();
	}
	return sql;
}

void CreateTableDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("");
	if (creator && creator->checkForPending())
	{
		QString sql(getSQLfromGUI());

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
		resultAppend(tr("Table created successfully"));
	}
}

bool CreateTableDialog::checkRetained(int i)
{
	return true;
}

bool CreateTableDialog::checkColumn(int i, QString cname,
								   QString ctype, QString cextra)
{
	return false;
}

void CreateTableDialog::checkChanges()
{
	QString newName(ui.nameEdit->text().trimmed());
	m_createButton->setEnabled(checkOk(newName));
}

void CreateTableDialog::setDirty()
{
	m_dirty = true;
}

void CreateTableDialog::tabWidget_currentChanged(int index)
{
	if (index == 2)
	{
		if (m_dirty && (m_tabWidgetIndex != 2))
		{
			
			int com = QMessageBox::question(this, tr("Sqliteman"),
				tr("Do you want to keep the current content of the SQL editor?."
				   "\nYes to keep it,\nNo to create from the %1 tab"
				   "\nCancel to return to the %2 tab")
				.arg((m_tabWidgetIndex ? "AS query" : "(columns)"),
					 (m_tabWidgetIndex ? "AS query" : "(columns)")),
				QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
			if (com == QMessageBox::No)
				ui.textEdit->setText(getSQLfromGUI());
			else if (com == QMessageBox::Cancel)
			{
				ui.tabWidget->setCurrentIndex(m_tabWidgetIndex);
				return;
			}
		}
		else
		{
			ui.textEdit->setText(getSQLfromGUI());
			setDirty();
		}
		ui.labelDatabase->hide();
		ui.databaseCombo->hide();
		ui.labelTable->hide();
		ui.nameEdit->hide();
		ui.adviceLabel->hide();
		m_tabWidgetIndex = index;
		m_createButton->setEnabled(true);
	}
	else
	{
		ui.labelDatabase->show();
		ui.databaseCombo->show();
		ui.labelTable->show();
		ui.nameEdit->show();
		ui.adviceLabel->show();
		m_tabWidgetIndex = index;
		checkChanges();
	}
}
