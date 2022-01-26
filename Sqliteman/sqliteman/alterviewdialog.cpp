/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QtCore/QtDebug>

#include "alterviewdialog.h"
#include "database.h"
#include "preferences.h"
#include "utils.h"


AlterViewDialog::AlterViewDialog(const QString & name, const QString & schema,
								 LiteManWindow * parent)
	: DialogCommon(parent)
{
	ui.setupUi(this);
    setResultEdit(ui.resultEdit);
    Preferences * prefs = Preferences::instance();
	resize(prefs->alterviewWidth(), prefs->alterviewHeight());
	ui.databaseCombo->addItem(schema);
	ui.nameEdit->setText(name);
	ui.databaseCombo->setDisabled(true);
	ui.nameEdit->setDisabled(true);

	QString sql = QString("select sql from ")
				  + Utils::q(schema)
				  + ".sqlite_master where name = "
				  + Utils::q(name)
				  + " and type = \"view\" ;";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	while (query.next())
	{
		QString s(query.value(0).toString());
		//FIXME parse general CREATE VIEW statement
		int pos = s.indexOf(QRegExp("\\bAS\\b",  Qt::CaseInsensitive));
		if (pos == -1)
		{
			qDebug() << "regexp parse error. Never should be written out";
			ui.createButton->setEnabled(false);
		}
		else
		{
			ui.sqlEdit->setText(s.right(s.length() - pos - 2).trimmed());
		}
	}

	setWindowTitle(tr("Alter View"));
	ui.createButton->setText("&Alter");

	connect(ui.createButton, SIGNAL(clicked()), this,
			SLOT(createButton_clicked()));
}

AlterViewDialog::~AlterViewDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setalterviewHeight(height());
    prefs->setalterviewWidth(width());
}

void AlterViewDialog::createButton_clicked()
{
	//FIXME this destroys any INSTEAD OF triggers on the view
	//FIXME allow renaming view
	QSqlDatabase db = QSqlDatabase::database(SESSION_NAME);

	ui.resultEdit->setHtml("");
	QString sql = QString("SAVEPOINT ALTER_VIEW ;");
	QSqlQuery q1(sql, db);
	if (q1.lastError().isValid())
	{
		QString errtext = QString(tr("Cannot create savepoint"))
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + q1.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + sql;
		resultAppend(errtext);
		return;
	}
	sql = QString("DROP VIEW ")
		  + Utils::q(ui.databaseCombo->currentText())
		  + "."
		  + Utils::q(ui.nameEdit->text())
		  + ";";
	QSqlQuery q2(sql, db);
	if (q2.lastError().isValid())
	{
		QString errtext = QString(tr("Cannot drop view "))
						  + ui.databaseCombo->currentText()
						  + tr(".")
						  + ui.nameEdit->text()
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + q2.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + sql;
		resultAppend(errtext);
		QSqlQuery q3("ROLLBACK TO ALTER_VIEW;", db);
		if (q3.lastError().isValid())
		{
			resultAppend(tr("Cannot roll back after error"));
		}
		else
		{
			QSqlQuery q4("RELEASE ALTER_VIEW;", db);
			if (!q4.lastError().isValid())
			{
				return;
			}
		}
		resultAppend(tr(
			"Database may be left with a pending savepoint."));
		return;
	}
	m_updated = true;

	sql = QString("CREATE VIEW ")
		  + Utils::q(ui.databaseCombo->currentText())
		  + "."
		  + Utils::q(ui.nameEdit->text())
		  + " AS\n"
		  + ui.sqlEdit->text()
		  + ";";
	QSqlQuery q4(sql, db);
	if(q4.lastError().isValid())
	{
		QString errtext = tr("Cannot create view ")
						  + ui.databaseCombo->currentText()
						  + tr(".")
						  + ui.nameEdit->text()
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + q4.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + sql;
		resultAppend(errtext);
		QSqlQuery q3("ROLLBACK TO ALTER_VIEW;", db);
		return;
	}
	sql = QString("RELEASE ALTER_VIEW ;");
	QSqlQuery q5(sql, db);
	if(q5.lastError().isValid())
	{
		QString errtext = tr("Cannot release savepoint")
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + q5.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + sql;
		resultAppend(errtext);
		QSqlQuery q6("ROLLBACK TO ALTER_VIEW;", db);
		if(q6.lastError().isValid())
		{
			QString errtext = tr("Cannot roll back either")
							  + ":<br/><span style=\" color:#ff0000;\">"
							  + q5.lastError().text()
							  + "<br/></span>" + tr("using sql statement:")
							  + "<br/><tt>" + sql;
			resultAppend(errtext);
		}
		return;
	}
	resultAppend(tr("View altered successfully"));
}
