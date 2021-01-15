/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>

#include "constraintsdialog.h"
#include "database.h"
#include "utils.h"

ConstraintsDialog::ConstraintsDialog(const QString & tabName, const QString & schema, QWidget * parent)
	: QDialog(parent),
	m_schema(schema),
	m_table(tabName)
{
	update = false;
	ui.setupUi(this);
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("constraints/height", QVariant(500)).toInt();
	int ww = settings.value("constraints/width", QVariant(600)).toInt();
	resize(ww, hh);

	// trigger name templates
	ui.insertName->setText(QString("tr_cons_%1_ins").arg(tabName));
	ui.updateName->setText(QString("tr_cons_%1_upd").arg(tabName));
	ui.deleteName->setText(QString("tr_cons_%1_del").arg(tabName));

	// not nulls
	QStringList inserts;
	QStringList updates;
	QStringList deletes;
	QStringList nnCols;
	QString stmt;
    QList<FieldInfo> l = Database::tableFields(tabName, schema);
    QList<FieldInfo>::const_iterator i;
    for (i = l.constBegin(); i != l.constEnd(); ++i) {
		if (!i->isNotNull)
			continue;
		nnCols << i->name;
		stmt = QString("SELECT RAISE(ABORT, 'New ")
			   + i->name
			   + " value IS NULL') WHERE new."
			   + Utils::q(i->name)
			   + " IS NULL;\n";
		inserts << "-- NOT NULL check" << stmt;
		updates << "-- NOT NULL check"<< stmt;
	}

	// get FKs
	QString sql = QString("pragma ")
				  + Utils::q(schema)
				  + ".foreign_key_list ("
				  + Utils::q(tabName)
				  + ");";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		QString errtext = QString(tr("Cannot parse constraints:\n"))
						  + query.lastError().text()
						  + tr("\nusing sql statement:\n")
						  + sql;
		ui.resultEdit->setText(errtext);
		return;
	}
	// 2 - table - FK table; 3 - from - column name; 4 - to - fk column name
	QString fkTab;
	QString column;
	QString fkColumn;
	QString nnTemplate;
	QString thenTemplate;
	while (query.next())
	{
		fkTab = query.value(2).toString();
		column = query.value(3).toString();
		fkColumn = query.value(4).toString();
		nnTemplate = "";
		if (nnCols.contains(column, Qt::CaseInsensitive))
		{
			nnTemplate = QString("\n    new.%1 IS NOT NULL AND")
								 .arg(Utils::q(column));
		}
		thenTemplate = QString("\n    RAISE(ABORT, '")
					   + column
					   + " violates foreign key "
					   + fkTab
					   + "("
					   + fkColumn
					   + ")')";
		stmt = QString("SELECT ")
			   + thenTemplate
			   + "\n    where "
			   + nnTemplate
			   + " SELECT "
			   + Utils::q(fkColumn)
			   + " FROM "
			   + Utils::q(fkTab)
			   + " WHERE "
			   + Utils::q(fkColumn)
			   + " = new."
			   + Utils::q(column)
			   + ") IS NULL;\n";
		inserts << "-- FK check" << stmt;
		updates << "-- FK check" << stmt;
		stmt = QString("SELECT ")
			   + thenTemplate
			   + " WHERE (SELECT "
			   + Utils::q(fkColumn)
			   + " FROM "
			   + Utils::q(fkTab)
			   + " WHERE "
			   + Utils::q(fkColumn)
			   + " = old."
			   + Utils::q(column)
			   + ") IS NOT NULL;\n";
		deletes << "-- FK check" << stmt;
	}

	// to the GUI
	ui.insertEdit->setText(inserts.join("\n"));
	ui.updateEdit->setText(updates.join("\n"));
	ui.deleteEdit->setText(deletes.join("\n"));

	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
}

ConstraintsDialog::~ConstraintsDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("constraints/height", QVariant(height()));
    settings.setValue("constraints/width", QVariant(width()));
}

void ConstraintsDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("INSERT trigger<br/>");
	if (!execSql("SAVEPOINT CONSTRAINTS;", tr("Cannot create savepoint")))
	{
		return;
	}
	if (ui.insertEdit->text().length() != 0)
	{
		if (execSql(QString("CREATE TRIGGER ")
					 + Utils::q(ui.insertName->text())
					 + " BEFORE INSERT ON "
					 + Utils::q(m_schema)
					 + "."
					 + Utils::q(m_table)
					 + " FOR EACH ROW\nBEGIN\n"
					 + "-- created by Sqliteman tool\n\n"
					 + ui.insertEdit->text()
					 + "END;",
					 tr("Cannot create trigger:")))
		{
			ui.resultEdit->append("Trigger created successfully");
		}
		else
		{
			doRollback(tr("Cannot roll back after error"));
			return;
		}
	}
	else
	{
		ui.resultEdit->append(tr("No action for INSERT") + "<br/>");
	}

	ui.resultEdit->append("UPDATE trigger<br/>");
	if (ui.updateEdit->text().length() != 0)
	{
		if (execSql(QString("CREATE TRIGGER ")
					 + Utils::q(ui.updateName->text())
					 + " BEFORE UPDATE ON "
					 + Utils::q(m_schema)
					 + "."
					 + Utils::q(m_table)
					 + " FOR EACH ROW\nBEGIN\n"
					 + "-- created by Sqliteman tool\n\n"
					 + ui.updateEdit->text()
					 + "END;",
					 tr("Cannot create trigger")))
		{
			ui.resultEdit->append("Trigger created successfully");
		}
		else
		{
			doRollback(tr("Cannot roll back after error"));
			return;
		}
	}
	else
		ui.resultEdit->append(tr("No action for UPDATE") + "<br/>");

	ui.resultEdit->append("DELETE trigger<br/>");
	if (ui.deleteEdit->text().length() != 0)
	{
		if (execSql(QString("CREATE TRIGGER ")
					 + Utils::q(ui.updateName->text())
					 + " BEFORE DELETE ON "
					 + Utils::q(m_schema)
					 + "."
					 + Utils::q(m_table)
					 + " FOR EACH ROW\nBEGIN\n"
					 + "-- created by Sqliteman tool\n\n"
					 + ui.deleteEdit->text()
					 + "END;",
					 tr("Cannot create trigger")))
		{
			ui.resultEdit->append("Trigger created successfully");
		}
		else
		{
			doRollback(tr("Cannot roll back after error"));
			return;
		}
	}
	else
		ui.resultEdit->append(tr("No action for DELETE") + "<br/>");
	if (!execSql("RELEASE CONSTRAINTS;", tr("Cannot release savepoint")))
	{
		doRollback(tr("Cannot roll back either"));
		return;
	}
}

bool ConstraintsDialog::execSql(const QString & statement,
								   const QString & message)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		QString errtext = message
			   + ":<br/><span style=\" color:#ff0000;\">"
			   + query.lastError().text()
			   + "<br/></span>" + tr("using sql statement:")
			   + "<br/><tt>" + statement
			   + "</tt><br/>";
		ui.resultEdit->append(errtext);
		return false;
	}
	else
	{
		update = true;
		return true;
	}
}

void ConstraintsDialog::doRollback(QString message)
{
	if (execSql("ROLLBACK TO CONSTRAINTS;", message))
	{
		// rollback does not cancel the savepoint
		if (execSql("RELEASE CONSTRAINTS;", QString("")))
		{
			return;
		}
	}
	ui.resultEdit->append(tr("Database may be left with a pending savepoint."));
}
