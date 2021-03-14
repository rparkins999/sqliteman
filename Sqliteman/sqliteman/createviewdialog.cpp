/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>

#include "createviewdialog.h"
#include "database.h"
#include "litemanwindow.h"
#include "tabletree.h"

CreateViewDialog::CreateViewDialog(LiteManWindow * parent,
									 QTreeWidgetItem * item)
	: TableEditorDialog(parent)
{
	ui.labelTable->setText("View Name:");
	setWindowTitle(tr("Create View"));
	m_tableOrView = "VIEW";
	m_oldWidget = ui.queryTab;

	m_createButton =
		ui.buttonBox->addButton("Create", QDialogButtonBox::ApplyRole);
	m_createButton->setDisabled(true);
	connect(m_createButton, SIGNAL(clicked()), this,
			SLOT(createButton_clicked()));

	ui.databaseCombo->setCurrentIndex(0);
	if (item && (   (item->type() == TableTree::DatabaseItemType)
				 || (item->type() == TableTree::ViewsItemType)))
	{
		int i = ui.databaseCombo->findText(item->text(1),
			Qt::MatchFixedString | Qt::MatchCaseSensitive);
		if (i >= 0)
		{
			ui.databaseCombo->setCurrentIndex(i);
            if (!addedTemp) {
                ui.databaseCombo->setDisabled(true);
            }
		}
	}
	connect(ui.databaseCombo, SIGNAL(currentIndexChanged(int)),
			this, SLOT(databaseChanged(int)));
    m_originalName = QString();

	ui.tabWidget->setCurrentIndex(1);
	ui.tabWidget->removeTab(0);
	m_tabWidgetIndex = ui.tabWidget->currentIndex();

	ui.textEdit->setText("");
	if (item && (item->type() == TableTree::TableType)) {
        ui.queryEditor->setItem(item);
    } else {
        ui.queryEditor->setItem(0);
    }
}

void CreateViewDialog::setText(QString query)
{
	ui.textEdit->setText(query);
	setDirty();
}

void CreateViewDialog::setSql(QString query)
{
	m_dirty = false;
	ui.tabWidget->setCurrentWidget(ui.sqlTab);
	ui.textEdit->setText(query);
	setDirty();
}

bool CreateViewDialog::event(QEvent * e)
{
	QDialog::event(e);
	if (e->type() == QEvent::Polish)
	{
		databaseChanged(ui.databaseCombo->currentIndex());
	}
	e->accept();
	return true;
}

void CreateViewDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("");
	QString sql(ui.firstLine->text() + getSQLfromGUI());

	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		resultAppend(tr("Cannot create view")
					 + ":<br/><span style=\" color:#ff0000;\">"
					 + query.lastError().text()
					 + "<br/></span>" + tr("using sql statement:")
					 + "<br/><tt>" + sql);
		return;
	}
	updated = true;
	emit rebuildViewTree(ui.databaseCombo->currentText(),
						 ui.nameEdit->text());
	resultAppend(tr("View created successfully"));
}

bool CreateViewDialog::checkColumn(int i, QString cname,
								   QString ctype, QString cextra)
{
	return false;
}

void CreateViewDialog::checkChanges()
{
	m_createButton->setEnabled(checkOk(ui.nameEdit->text()));
}

void CreateViewDialog::databaseChanged(int index)
{
	ui.queryEditor->fixSchema(ui.databaseCombo->itemText(index));
}
