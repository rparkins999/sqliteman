/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include "vacuumdialog.h"
#include "database.h"
#include "preferences.h"
#include "utils.h"

VacuumDialog::VacuumDialog(LiteManWindow * parent)
	: QDialog(parent)
{
	creator = parent;
	ui.setupUi(this);
    Preferences * prefs = Preferences::instance();
	resize(prefs->vacuumWidth(), prefs->vacuumHeight());

	ui.tableList->addItems(Database::getObjects("table").values());
	ui.tableList->addItems(Database::getObjects("index").values());

	connect(ui.allButton, SIGNAL(clicked()), this, SLOT(allButton_clicked()));
	connect(ui.tableButton, SIGNAL(clicked()), this, SLOT(tableButton_clicked()));
}

VacuumDialog::~VacuumDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setvacuumHeight(height());
    prefs->setvacuumWidth(width());
}

void VacuumDialog::allButton_clicked()
{
	if (creator && creator->checkForPending())
	{
		Database::execSql("vacuum;");
	}
}

void VacuumDialog::tableButton_clicked()
{
	QList<QListWidgetItem *> list(ui.tableList->selectedItems());
	if (creator && creator->checkForPending())
	{
		for (int i = 0; i < list.size(); ++i)
		{
			if (!Database::execSql(QString("vacuum %1;")
					.arg(Utils::q(list.at(i)->text()))))
				break;
		}
	}
}

