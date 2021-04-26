/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include "importtablelogdialog.h"
#include "preferences.h"

ImportTableLogDialog::ImportTableLogDialog(QStringList log, QWidget * parent)
	: QDialog(parent)
{
	setupUi(this);
	textBrowser->setText(log.join("\n\n"));
    Preferences * prefs = Preferences::instance();
	resize(prefs->importtablelogWidth(), prefs->importtablelogHeight());
}

ImportTableLogDialog::~ImportTableLogDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setimporttablelogHeight(height());
    prefs->setimporttablelogWidth(width());
}
