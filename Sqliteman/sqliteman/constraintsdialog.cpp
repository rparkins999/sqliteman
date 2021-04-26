/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>

#include "constraintsdialog.h"
#include "database.h"
#include "preferences.h"
#include "utils.h"

ConstraintsDialog::ConstraintsDialog(
    const QString & tabName, const QString & schema, LiteManWindow * parent)
	: DialogCommon(parent)
{
	m_databaseName = schema;
    m_tableName = tabName;
	ui.setupUi(this);
    setResultEdit(ui.resultEdit);
    Preferences * prefs = Preferences::instance();
	resize(prefs->constraintsWidth(), prefs->constraintsHeight());

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
		resultAppend(errtext);
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
    Preferences * prefs = Preferences::instance();
    prefs->setconstraintsHeight(height());
    prefs->setconstraintsWidth(width());
}

void ConstraintsDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("");
	resultAppend("INSERT trigger<br/>");
	if (!execSql("SAVEPOINT CONSTRAINTS;", tr("Cannot create savepoint")))
	{
		return;
	}
	if (ui.insertEdit->text().length() != 0)
	{
		if (execSql(QString("CREATE TRIGGER ")
					 + Utils::q(ui.insertName->text())
					 + " BEFORE INSERT ON "
					 + Utils::q(m_databaseName)
					 + "."
					 + Utils::q(m_tableName)
					 + " FOR EACH ROW\nBEGIN\n"
					 + "-- created by Sqliteman tool\n\n"
					 + ui.insertEdit->text()
					 + "END;",
					 tr("Cannot create trigger:")))
		{
			resultAppend("Trigger created successfully");
            m_updated = true;
		}
		else
		{
			doRollback(tr("Cannot roll back after error"));
			return;
		}
	}
	else
	{
		resultAppend(tr("No action for INSERT") + "<br/>");
	}

	resultAppend("UPDATE trigger<br/>");
	if (ui.updateEdit->text().length() != 0)
	{
		if (execSql(QString("CREATE TRIGGER ")
					 + Utils::q(ui.updateName->text())
					 + " BEFORE UPDATE ON "
					 + Utils::q(m_databaseName)
					 + "."
					 + Utils::q(m_tableName)
					 + " FOR EACH ROW\nBEGIN\n"
					 + "-- created by Sqliteman tool\n\n"
					 + ui.updateEdit->text()
					 + "END;",
					 tr("Cannot create trigger")))
		{
			resultAppend("Trigger created successfully");
		}
		else
		{
			doRollback(tr("Cannot roll back after error"));
			return;
		}
	}
	else
		resultAppend(tr("No action for UPDATE") + "<br/>");

	resultAppend("DELETE trigger<br/>");
	if (ui.deleteEdit->text().length() != 0)
	{
		if (execSql(QString("CREATE TRIGGER ")
					 + Utils::q(ui.updateName->text())
					 + " BEFORE DELETE ON "
					 + Utils::q(m_databaseName)
					 + "."
					 + Utils::q(m_tableName)
					 + " FOR EACH ROW\nBEGIN\n"
					 + "-- created by Sqliteman tool\n\n"
					 + ui.deleteEdit->text()
					 + "END;",
					 tr("Cannot create trigger")))
		{
			resultAppend("Trigger created successfully");
            m_updated = true;
		}
		else
		{
			doRollback(tr("Cannot roll back after error"));
			return;
		}
	}
	else {
		resultAppend(tr("No action for DELETE") + "<br/>");
    }
	if (!execSql("RELEASE CONSTRAINTS;", tr("Cannot release savepoint")))
	{
		doRollback(tr("Cannot roll back either"));
		return;
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
	resultAppend(tr("Database may be left with a pending savepoint."));
}
