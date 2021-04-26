/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef QUERYEDITORWIDGET_H
#define QUERYEDITORWIDGET_H

class QTreeWidgetItem;
class QueryStringModel;

#include "database.h"
#include "termstabwidget.h"
#include "ui_queryeditorwidget.h"

/*!
 * @brief A widget for creating and editing queries
 * \author Igor Khanin
 * \author Petr Vanek <petr@scribus.info>
 * \author Richard Parkins extracted from QueryEditorDialog
 */
class QueryEditorWidget : public QWidget
{
	Q_OBJECT

    private:
		QString m_schema;
		QString m_table;
		QueryStringModel * columnModel;
		QueryStringModel * selectModel;
		QString m_rowid;
		bool resizeWanted; // tables, not window
		QString findTable(QString name, bool allowChange);

    private slots:
		void tableSelected(const QString & table);

        // this isn't actually signaled, but it's here to preserve the order
		void resetTableList(QString name, bool allowChange);
		void schemaSelected(const QString & schema);
		// Fields tab
		void addAllSelect();
		void addSelect();
		void removeAllSelect();
		void removeSelect();
		// Order by tab
		void moreOrders();
		void lessOrders();
		void copySql();

    protected:
		void paintEvent(QPaintEvent * event);
		void resizeEvent(QResizeEvent * event);

	public:
        Ui::QueryEditorWidget ui;
        /*!
		 * @brief Creates the query editor.
		 * @param parent The parent widget.
		 */
		QueryEditorWidget(QWidget * parent = 0);

        QString whereClause();

		//! \brief generates a valid SELECT statemen
        // using the values in the dialog
		QString statement();

		//! \brief generates a valid DELETE statement
        // using the values in the dialog
		QString deleteStatement();

		void setSchema(QString schema, QString table,
                       bool schemaMayChange, bool tableMayChange);
		void treeChanged();
		void tableAltered(QString oldName, QTreeWidgetItem * item);
		void addSchema(const QString & schema);

    public slots:
		void resetClicked(); // called, not signalled, by CreatetableDialog

};

#endif //QUERYEDITORWIDGET_H
