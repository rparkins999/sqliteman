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
#include "getcolumnlist.h"
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
		/* This is the name of the database that we're doing a query on,
		 * usually "main", but not always: we can have attached databases
		 * or the temp database may exist.
		 */
		QString m_schema;
		/* This is true if we are part of a QueryEditorDialog invoked
		 * from the Database menu, and the user can select a schema
		 * (if there is more than one).
		 */
		bool m_canChangeSchema;
		/* This is the name of the table or view that we're doing a query on.
		 */
		QString m_table;
		/* This is true if we are part of a QueryEditorDialog invoked
		 * from the context menu of a schema, and the user can select
		 * a table or a view (if there is more than one).
		 */
		bool m_canChangeTable;
		QueryStringModel * columnModel;
		QueryStringModel * selectModel;
		bool resizeWanted; // tables, not window
		struct ColumnList m_columnList; // result from getColumnList(...)

    private slots:
		void tableSelected(const QString & table);

	private:
		void findTable(QString name);

	public:
		// True if we're querying a table rather than a view.
		bool m_queryingTable;

		Ui::QueryEditorWidget ui;

		void resetTableList();

    protected:
		void paintEvent(QPaintEvent * event);
		void resizeEvent(QResizeEvent * event);

    private slots:
		void addAllSelect();
		void addSelect();
		void removeAllSelect();
		void removeSelect();
		void moreOrders();
		void lessOrders();
		void schemaSelected(const QString & schema);

	public:
        /*!
		 * @brief Creates the query editor.
		 * @param parent The parent widget.
		 */
		QueryEditorWidget(QWidget * parent = 0);
        QString whereClause();
		QString statement(bool elide);
		QString deleteStatement();
		void tableGone(QString oldName, QString newName);

        // normal call (clicked on TableTree item)
		void setSchema(QString schema, QString table,
					   bool schemaMayChange, bool tableMayChange);
        // Overloaded call to keep previous query
        // (clicked on Database oo Tables or Views)
		void setSchema(QString schema,
					   bool schemaMayChange, bool tableMayChange);

	private:
		void copySql(bool elide);

    public slots:
		void copySql();
		void resetSchemaList();

};

#endif //QUERYEDITORWIDGET_H
