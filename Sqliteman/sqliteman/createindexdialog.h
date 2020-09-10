/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef INDEXDIALOG_H
#define INDEXDIALOG_H

#include <qwidget.h>

#include "litemanwindow.h"
#include "ui_createindexdialog.h"


/*! \brief GUI for index creation
\author Petr Vanek <petr@scribus.info>
*/
class CreateIndexDialog : public QDialog
{
	Q_OBJECT

	public:
		/*! \brief Create a dialog
		\param tabName name of the index parent table
		\param schema name of the db schema
		\param parent standard Qt parent
		*/
		CreateIndexDialog(const QString & tabName, const QString & schema,
						  LiteManWindow * parent = 0);
		~CreateIndexDialog();

		bool update;

	private:
		Ui::CreateIndexDialog ui;
		QString m_schema;
		QString m_table;

		void checkToEnable();
		void resultAppend(QString text);
		bool resizeWanted;
		void paintEvent(QPaintEvent * event);
		void resizeEvent(QResizeEvent * event);


		// We ought to be able use use parent() for this, but for some reason
		// qobject_cast<LiteManWindow*>(parent()) doesn't work
		LiteManWindow * creator;

private slots:
		void tableColumns_itemChanged(QTableWidgetItem* item);
		void indexNameEdit_textChanged(const QString & text);
		/*! \brief Parse user's inputs and create a sql statement */
		void createButton_clicked();

};

#endif
