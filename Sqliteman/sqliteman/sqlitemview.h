/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef SQLITEMVIEW_H
#define SQLITEMVIEW_H

#include "ui_sqlitemview.h"

class QDataWidgetMapper;
class QAbstractItemModel;
class QLineEdit;
class QGridLayout;
class QTableView;


/*! \brief Display one record in one form view.
It provides set of generic line edits for current model index.
User can change model indexes by GUI buttons.
\author Petr Vanek <petr@scribus.info>
*/
class SqlItemView : public QWidget, public Ui::SqlItemView
{
	Q_OBJECT

	public:
		SqlItemView(QWidget * parent = 0);

		/*! \brief Set the model and connect the GUI widgets to model's items.
		All previously generated GUI widgets are deleted and recreated
		again depending on the new model QSqlRecord structure. */
		void setModel(QAbstractItemModel * model);
		QAbstractItemModel * model();

		// needed to avoid deleted rows
		void setTable(QTableView * table) { m_table = table; };

		/*! \brief Set the current index for "item view".
		\param row is the row number starting from 0.
		Use model->currentIndex().row() from "real" indexes for it.
		*/
		void setCurrentIndex(int row, int column);
		int currentRow();
		int currentColumn();
		bool rowDeleted();

	signals:
		/*! Emitted when there is a focus change in
		the m_mapper's widgets only. It's used in DataViewer
		for syncing BLOB preview and table view indexes. */
		void indexChanged();
		void dataChanged();

	private:
		//! Current active "column"
		int m_column;
		int m_row;
		int m_count;
		bool m_changing;
		bool m_writeable;
		QAbstractItemModel * m_model;
		QTableView * m_table;

		QGridLayout * m_gridLayout;

		int findUp(int row);
		int findDown(int row);
        QAction * actCopy;
        QAction * actCopyWhole;
        QAction * actCut;
        QAction * actPaste;
        QAction * actPasteOver;
        QAction * actInsertNull;
        QAction * actOpenMultiEditor;

	private slots:
		void toFirst();
		void toPrevious();
		void toNext();
		void toLast();
		void insertNull();
		void openMultiEditor();
		/*! Handle app focus change. It chatches only m_mapper's
		widgets. It emits indexChanged() for DataViewer. */
		void aApp_focusChanged(QWidget* old, QWidget* now);
		void doCopy();
		void doCopyWhole();
		void doCut();
		void doPaste();
		void doPasteOver();

	public slots:
		void textChanged();
		//! \brief Set the navigation buttons state and "X of Y" label.
		void updateButtons(int row);
};

#endif
