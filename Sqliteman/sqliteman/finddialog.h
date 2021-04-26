/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef FINDDIALOG_H
#define FINDDIALOG_H

class QCloseEvent;
class QSqlRecord;
class QTreeWidgetItem;

class DataViewer;

#include "termstabwidget.h"
#include "ui_finddialog.h"

/*!
 * @brief A modeless window for doing searches
 * \author Richard Parkins
 */
class FindDialog : public QMainWindow
{
	Q_OBJECT

	private:
		QString m_schema;
		QString m_table;
		bool notSame(QStringList l1, QStringList l2);
		bool isNumeric(QVariant::Type t);

	protected:
		void closeEvent(QCloseEvent * event);

	public slots:
		void updateButtons();

	public:
        Ui::FindDialog ui;
		/*!
		 * @brief Creates the query editor.
		 * @param parent The parent widget for the dialog.
		 */
		FindDialog(QWidget * parent = 0);
		~FindDialog();
		void doConnections(DataViewer * dataviewer);
		void setup(QString schema, QString table);
		bool isMatch(QSqlRecord * rec, int i);
		bool isMatch(QSqlRecord * rec);

	signals:
		void findClosed();
};

#endif //FINDDIALOG_H
