/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QComboBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>

#include "createindexdialog.h"
#include "database.h"
#include "utils.h"


CreateIndexDialog::CreateIndexDialog(const QString & tabName,
									 const QString & schema,
									 LiteManWindow * parent)
	: QDialog(parent),
	m_schema(schema)
{
	creator = parent;
	update = false;
	ui.setupUi(this);
	ui.tableNameLabel->setText(tabName);
	ui.schemaLabel->setText(m_schema);

	FieldList columns = Database::tableFields(tabName, schema);
	ui.tableColumns->setRowCount(columns.size());
	ui.createButton->setDisabled(true);

	for(int i = 0; i < columns.size(); i++)
	{
		QTableWidgetItem * nameItem = new QTableWidgetItem(columns[i].name);
		nameItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		QTableWidgetItem * useItem = new QTableWidgetItem(QString::null);
		useItem->setCheckState(Qt::Unchecked);

		ui.tableColumns->setItem(i, 0, nameItem);
		if (columns[i].pk)
			nameItem->setIcon(Utils::getIcon("key.png"));

		ui.tableColumns->setItem(i, 1, useItem);
		QComboBox *asc = new QComboBox(this);
		asc->addItems(QStringList() << "ASC" << "DESC");
		asc->setEnabled(false);
		ui.tableColumns->setCellWidget(i, 2, asc);
	}
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("createindex/height", QVariant(500)).toInt();
	resize(width(), hh);

	connect(ui.tableColumns, SIGNAL(itemChanged(QTableWidgetItem*)),
			this, SLOT(tableColumns_itemChanged(QTableWidgetItem*)));
	connect(ui.indexNameEdit, SIGNAL(textChanged(const QString&)),
			this, SLOT(indexNameEdit_textChanged(const QString&)));
	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
}

CreateIndexDialog::~CreateIndexDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("createindex/height", QVariant(height()));
}

void CreateIndexDialog::createButton_clicked()
{
	if (creator && creator->checkForPending())
	{
		QStringList cols;
		for (int i = 0; i < ui.tableColumns->rowCount(); ++i)
		{
			if (ui.tableColumns->item(i, 1)->checkState() == Qt::Checked)
			{
				QComboBox* cb;
				cb = qobject_cast<QComboBox*>
					(ui.tableColumns->cellWidget(i, 2));
				cols.append(Utils::quote(ui.tableColumns->item(i, 0)->text())
							+ " "
							+ cb->currentText());
			}
		}
		QString sql = QString("create ")
					  + (ui.uniqueCheckBox->isChecked() ? "unique" : "")
					  + " index "
					  + Utils::quote(m_schema)
					  + "."
					  + Utils::quote(ui.indexNameEdit->text())
					  + " on "
					  + Utils::quote(ui.tableNameLabel->text())
					  + " ("
					  + cols.join(", ")
					  + ");";

		QSqlQuery q(sql, QSqlDatabase::database(SESSION_NAME));
		if(q.lastError().isValid())
		{
			ui.resultEdit->setText(tr("Error while creating index ")
								   + ui.indexNameEdit->text()
								   + ":\n"
								   + q.lastError().text()
								   + tr("\nusing sql statement:\n")
								   + sql);
			return;
		}
		ui.resultEdit->setText(tr("Index created successfully."));
		update = true;
	}
}

void CreateIndexDialog::tableColumns_itemChanged(QTableWidgetItem* item)
{
	int r = item->row();
	Q_ASSERT_X(r != -1 , "CreateIndexDialog", "table item out of the table");
	qobject_cast<QComboBox*>(ui.tableColumns->cellWidget(r, 2))->setEnabled(
							 (item->checkState() == Qt::Checked));
	checkToEnable();
}

void CreateIndexDialog::indexNameEdit_textChanged(const QString &)
{
	checkToEnable();
}

void CreateIndexDialog::checkToEnable()
{
	bool nameTest = !ui.indexNameEdit->text().simplified().isEmpty();
	bool columnTest = false;
	for (int i = 0; i < ui.tableColumns->rowCount(); ++i)
	{
		if (ui.tableColumns->item(i, 1)->checkState() == Qt::Checked)
		{
			columnTest = true;
			break;
		}
	}
	ui.createButton->setEnabled(nameTest && columnTest);
}
