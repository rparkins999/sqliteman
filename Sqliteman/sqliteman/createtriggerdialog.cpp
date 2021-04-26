/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>
#include <QSettings>

#include "createtriggerdialog.h"
#include "database.h"
#include "litemanwindow.h"
#include "preferences.h"
#include "tabletree.h"
#include "utils.h"

CreateTriggerDialog::CreateTriggerDialog(QTreeWidgetItem * item,
										 LiteManWindow * parent)
	: DialogCommon(parent)
{
	ui.setupUi(this);
    setResultEdit(ui.resultEdit);
    Preferences * prefs = Preferences::instance();
	resize(prefs->createtriggerWidth(), prefs->createtriggerHeight());

	if (item->type() == TableTree::TableType)
	{
		ui.textEdit->setText(
			QString("-- sqlite3 simple trigger template\n"
					"CREATE TRIGGER [IF NOT EXISTS] ")
			+ Utils::q(item->text(1))
			+ ".\"<trigger_name>\"\n[ BEFORE | AFTER ]\n"
		    + "DELETE | INSERT | UPDATE | UPDATE OF <column-list>\n ON "
			+ Utils::q(item->text(0))
			+ "\n[ FOR EACH ROW | FOR EACH STATEMENT ] [ WHEN expression ]\n"
			+ "BEGIN\n<statement-list>\nEND;");
        connect(ui.createButton, SIGNAL(clicked()),
			this, SLOT(createButton_clicked()));
	}
	else if (item->type() == TableTree::TableType)
	{
		ui.textEdit->setText(
			QString("-- sqlite3 simple trigger template\n"
					"CREATE TRIGGER [IF NOT EXISTS] ")
			+ Utils::q(item->text(1))
			+ ".\"<trigger_name>\"\nINSTEAD OF "
			+ "[DELETE | INSERT | UPDATE | UPDATE OF <column-list>]\nON "
			+ Utils::q(item->text(0))
			+ "\n[ FOR EACH ROW | FOR EACH STATEMENT ] [ WHEN expression ]\n"
			 + "BEGIN\n<statement-list>\nEND;");
        connect(ui.createButton, SIGNAL(clicked()),
			this, SLOT(createButton_clicked()));
	}
	// otherwise it's an AlterTriggerDialog which initialises itself
}

CreateTriggerDialog::~CreateTriggerDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setcreatetriggerHeight(height());
    prefs->setcreatetriggerWidth(width());
}

void CreateTriggerDialog::createButton_clicked()
{
	ui.resultEdit->setHtml("");
	LiteManWindow * lmw = qobject_cast<LiteManWindow*>(parent());
	if (lmw && lmw->checkForPending())
	{
		QString sql(ui.textEdit->text());
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		
		if(query.lastError().isValid())
		{
			resultAppend(tr("Cannot create trigger")
						 + ":<br/><span style=\" color:#ff0000;\">"
						 + query.lastError().text()
						 + "<br/></span>" + tr("using sql statement:")
						 + "<br/><code>" + sql);
			return;
		}
		resultAppend(tr("Trigger created successfully"));
		m_updated = true;
	}
}
