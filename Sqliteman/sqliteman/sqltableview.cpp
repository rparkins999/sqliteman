/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file written by and copyright © 2018 Richard P. Parkins, M. A., and released under the GPL.
*/

#include <QtCore/QtDebug> //qDebug
#include <QHeaderView>
#include <QMargins>
#include <QtCore/qmath.h>
#include <QtCore/QModelIndex>
#include <QSize>
#include <QtCore/QVector>
#include <QStyleOptionViewItem>

#include "sqltableview.h"

// override QTableview::resizeColumnsToContents
// using faster all-in-one algorithm
void SqlTableView::resizeColumnsToContents()
{
    if (!model()) { return; }

    ensurePolished();

    QHeaderView * horizHeader = horizontalHeader();
    QHeaderView * vertHeader = verticalHeader();
    horizHeader->setSectionResizeMode(QHeaderView::Interactive);
    horizHeader->setMinimumSectionSize(-1);
    int rows = vertHeader->count();
	int widthView = viewport()->width() - vertHeader->sizeHint().width();
	qreal widthLeft = widthView;
	int columns = horizHeader->count();
	int columnsLeft = columns;
	int half = widthView / (columns * 2);
	QVector<int> wantedWidths(columns);
	QVector<int> gotWidths(columns);
	QVector<int> sizeHints(columns);
    QStyleOptionViewItem option = viewOptions();
    QModelIndex index;
    for (int column = 0; column < columns; ++column) {
        int logicalColumn = horizHeader->logicalIndex(column);
        wantedWidths[column] = 0;
        if (horizHeader->isSectionHidden(logicalColumn)) { continue; }
        gotWidths[column] = 0;
        sizeHints[column] = horizHeader->sectionSizeHint(logicalColumn);
        for (int row = 0; row < rows; ++row) {
            int logicalRow = vertHeader->logicalIndex(row);
            if (vertHeader->isSectionHidden(logicalRow)) { continue; }
            QModelIndex index = model()->index(logicalRow, logicalColumn);
            int width = itemDelegate(index)->sizeHint(option, index).width();
            if (wantedWidths[column] < width) {
                wantedWidths[column] = width;
            }
        }
        int w = wantedWidths[column];
        int x = sizeHints[column];
        if (w < x) { // force column to be wide enough for header label
            wantedWidths[column] = x;
            gotWidths[column] = x;
			widthLeft -= x;
			columnsLeft -= 1;
        } else if (w <= half) { // give small column what it wants
            gotWidths[column] = w;
			widthLeft -= w;
			columnsLeft -= 1;
        }
    }
	qreal total = 0;
    for (int column = 0; column < columns; ++column) {
        int logicalColumn = horizHeader->logicalIndex(column);
        if (horizHeader->isSectionHidden(logicalColumn)) {
            continue;
        }
        total += wantedWidths[column];
    }
	qreal ratio = widthView / total;
    qreal adjust = widthLeft / columnsLeft;
    // allocate space to remaining non-small columns
    for (int column = 0; column < columns; ++column) {
        int logicalColumn = horizHeader->logicalIndex(column);
        if (horizHeader->isSectionHidden(logicalColumn)) { continue; }
        int w = wantedWidths[column];
		if (gotWidths[column] == 0)
		{
            // if total width is too big for window, reduce widths in proportion
            w = w * ratio;
            int x = sizeHints[column];
            if (w < x) { w = x; } // keep enough space for header label
            else if (w > half) {
                // otherwise give wider columns less of what they want
                x = half + (int)qSqrt( (w - half) * adjust);
                if (w > x) { w = x; };
            }
            gotWidths[column] = w;
			if (widthLeft > w)
			{
				widthLeft -= w;
			}
			columnsLeft -= 1;
		}
	}
    if (widthLeft > 0) { // still some space left
        ratio = widthView / (widthView - widthLeft);
    } else {
        ratio = 1.0;
    }
    for (int column = 0; column < columns; ++column) {
        int logicalColumn = horizHeader->logicalIndex(column);
        if (horizHeader->isSectionHidden(logicalColumn)) { continue; }
        int w = (int)(gotWidths[column] * ratio);
        setColumnWidth(column, w);
    }
    return;
}
