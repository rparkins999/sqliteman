/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef CREATETABLEDIALOG_H
#define CREATETABLEDIALOG_H

#include "tableeditordialog.h"

class QTreeWidgetItem;
class QPushButton;

/*! \brief A GUI for CREATE TABLE procedure.
\author Petr Vanek <petr@scribus.info>
*/
class CreateTableDialog : public TableEditorDialog
{
	Q_OBJECT

	public:
		CreateTableDialog(LiteManWindow * parent = 0,
						  QTreeWidgetItem * item = 0);
		~CreateTableDialog(){};

	private:
		/*! \brief Analyze user changes and performs the CREATE TABLE sql
		*/
		bool checkColumn(int i, QString cname,
						 QString ctype, QString cextra);


		QPushButton * m_createButton;

    signals:
		void rebuildTableTree(QString schema, QString name);

	private slots:
		void createButton_clicked();
		void checkChanges();
};

#endif
