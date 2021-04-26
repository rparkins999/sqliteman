/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME should update m_name and m_schema after successful alter
		  waiting for new statement parser to fix this
*/
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>

#include "altertriggerdialog.h"
#include "database.h"
#include "preferences.h"
#include "utils.h"

AlterTriggerDialog::AlterTriggerDialog(
    QTreeWidgetItem * item, LiteManWindow * parent)
	: CreateTriggerDialog(item, parent)
{
    m_databaseName = item->text(1);;
    m_name = item->text(0);
	ui.setupUi(this);
    setResultEdit(ui.resultEdit);
    Preferences * prefs = Preferences::instance();
	resize(prefs->altertriggerWidth(), prefs->altertriggerHeight());
	ui.createButton->setText(tr("&Alter"));
	setWindowTitle("Alter Trigger");

	QString sql = QString("select sql from ")
	              + Utils::q(m_databaseName)
				  + ".sqlite_master where name = "
				  + Utils::q(m_name)
				  + " and type = \"trigger\" ;";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		QString errtext = tr("Cannot get trigger ")
						  + m_name
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + query.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + sql;
		resultAppend(errtext);
		ui.createButton->setEnabled(false);
	}
	else if (query.next()) {
        ui.textEdit->setText(query.value(0).toString());
		connect(ui.createButton, SIGNAL(clicked()), this, SLOT(alterButton_clicked()));
	}
}

AlterTriggerDialog::~AlterTriggerDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setaltertriggerHeight(height());
    prefs->setaltertriggerWidth(width());
}

void AlterTriggerDialog::alterButton_clicked()
{
	ui.resultEdit->setHtml("");
    QString sql = QString("SAVEPOINT ALTER_TRIGGER ;");
    if (!execSql(sql, tr("Cannot create savepoint"))) { return; }

	sql = QString("DROP TRIGGER ") + Utils::q(m_databaseName) + "."
				  + Utils::q(m_name) + ";";
    if (!execSql(sql, tr("Cannot drop trigger ") + m_name)) {
        execSql("ROLLBACK TO SAVEPOINT ALTER_TRIGGER ;", "");
        execSql("RELEASE SAVEPOINT ALTER_TRIGGER ;", "");
        return;
    }
	
	if (execSql(ui.textEdit->text(), tr("Error while creating trigger "))) {
        resultAppend(tr("Trigger created successfully"));
        m_updated = true;
    } else {
        execSql("ROLLBACK TO SAVEPOINT ALTER_TRIGGER ;", "");
        execSql("RELEASE SAVEPOINT ALTER_TRIGGER ;", "");
        return;
    }

	// for the moment, we don't allow the user to alter the same trigger again
	// because its name may have changed
	accept();
}
