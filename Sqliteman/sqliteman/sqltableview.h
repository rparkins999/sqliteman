/*
 * This file written by and copyright © 2015-2025 Richard P. Parkins, M. A.,
 * and released under the GPL.
 *
 * For general Sqliteman copyright and licensing information please refer
 * to the COPYING file provided with the program. Following this notice may exist
 * a copyright and/or license notice that predates the release of Sqliteman
 * for which a new license (GPL+exception) is in place.
 */
#ifndef SQLTABLEVIEW_H
#define SQLTABLEVIEW_H

#include <QHeaderView>
#include <QTableView>
#include <QTextLayout>

#include "preferences.h"

class SqlTableView : public QTableView
{
	Q_OBJECT

    private:
        QTextLayout m_textLayout;
        int m_textMargin;
        Preferences * m_prefs;

        int getWidth(QString s);
        int vhwidth();

    public:
		void resizeColumnsToContents();
		void resizeRowsToContents();
        SqlTableView(QWidget * parent = 0);
        // QTableView::selectedIndexes() is protected, but our version needs
        // to be callable by DataViewer::updateButtons(), so we overrride
        // with a version that just calls QTableView::selectedIndexes()
		QModelIndexList selectedIndexes() const {
			return QTableView::selectedIndexes();
		}
};
#endif
