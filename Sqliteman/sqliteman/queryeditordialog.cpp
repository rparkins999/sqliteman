/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QScrollBar>
#include <QComboBox>
#include <QLineEdit>
#include <QTreeWidgetItem>

#include "preferences.h"
#include "queryeditordialog.h"
#include "queryeditorwidget.h"
#include "tabletree.h"
/*****************************************************************************
 *
 * Note that this is a persistent dialog which still exists even when it
 * isn't being shown. This is in order for it to remember the last built query
 * so that the user can modify it if it isn't quite right.
 *
 *****************************************************************************/

QueryEditorDialog::QueryEditorDialog(QWidget * parent): QDialog(parent)
{
	ui.setupUi(this);
    Preferences * prefs = Preferences::instance();
	resize(prefs->queryeditorWidth(), prefs->queryeditorHeight());
}

QueryEditorDialog::~QueryEditorDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setqueryeditorHeight(height());
    prefs->setqueryeditorWidth(width());
}

void QueryEditorDialog::setItem(QTreeWidgetItem * item)
{
    QString databaseName;
    bool schemaMayChange = true;
    QString tableName;
    bool tableMayChange = true;
    if (item) {
        databaseName = item->text(1);
        schemaMayChange = false;
		if (   (item->type() == TableTree::TableType)
//            building query on view valid but not yet supported
//            || (item->type() == TableTree::ViewType)
        ) {
                tableName = item->text(0);
                tableMayChange = false;
        }
    }
	ui.queryEditor->setSchema(
        databaseName, tableName, schemaMayChange, tableMayChange);
}

QString QueryEditorDialog::statement()
{
	return ui.queryEditor->statement();
}

QString QueryEditorDialog::deleteStatement()
{
	return ui.queryEditor->deleteStatement();
}

void QueryEditorDialog::treeChanged()
{
	ui.queryEditor->treeChanged();
}

void QueryEditorDialog::tableAltered(QString oldName, QTreeWidgetItem * item)
{
	ui.queryEditor->tableAltered(oldName, item);
}
