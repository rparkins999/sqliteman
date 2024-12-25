/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file written by and copyright © 2015 Richard P. Parkins, M. A., and released
under the GPL.
*/
#ifndef SQLTABLEVIEW_H
#define SQLTABLEVIEW_H

#include <QTableView>
#include <QHeaderView>

class SqlTableView : public QTableView
{
	Q_OBJECT

	public:
		SqlTableView(QWidget * parent = 0) {
		}
		QModelIndexList selectedIndexes() const {
			return QTableView::selectedIndexes();
		}
		void resizeColumnsToContents();

};
#endif
