/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef CREATEVIEWDIALOG_H
#define CREATEVIEWDIALOG_H

#include <qwidget.h>

#include "litemanwindow.h"
#include "tableeditordialog.h"

class QTreeWidgetItem;
class QPushButton;

/*! \brief GUI for view creation
\author Petr Vanek <petr@scribus.info>
*/
class CreateViewDialog : public TableEditorDialog // ->DialogCommon->QDialog
{
	Q_OBJECT

	private:
		QPushButton * m_createButton;

		bool checkColumn(int i, QString cname,
						 QString ctype, QString cextra);

	public:
		CreateViewDialog(LiteManWindow * parent = 0,
						  QTreeWidgetItem * item = 0);
		~CreateViewDialog();
		void setSql(QString query);

	signals:
		/*! \brief Rebuild part of the tree */
		void rebuildViewTree(QString schema, QString name);

	private slots:
		void createButton_clicked();
		void checkChanges();
		void databaseChanged(const QString schema);
};

#endif
