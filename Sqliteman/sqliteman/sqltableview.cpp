/*
 * This file written by and copyright © 2015-2025 Richard P. Parkins, M. A.,
 * and released under the GPL.
 *
 * For general Sqliteman copyright and licensing information please refer
 * to the COPYING file provided with the program. Following this notice may exist
 * a copyright and/or license notice that predates the release of Sqliteman
 * for which a new license (GPL+exception) is in place.
 */

#undef DEBUG
#ifdef DEBUG
#include <QtCore/QtDebug> //qDebug
#endif //DEBUG

#include <QApplication>
#include <QHeaderView>
#include <QMargins>
#include <QtCore/qmath.h>
#include <QtCore/QModelIndex>
#include <QSize>
#include <QtCore/QVector>
#include <QTextLayout>
#include <QTextOption>
#include <QStyleOptionViewItem>

#include <math.h>

#include "sqldelegate.h"
#include "sqltableview.h"

/* Compute a suitable width for a text string.
 * This is used to get consistency of widths between headers and data.
 * This must use the same logic as SqlDelegate::sizeHint() and
 * SqlDelegate::drawDisplay(), except that we don't consider breaking the
 * line here since we're looking to see how much width the column would like
 * to have without line breaks.
 * Our caller must have already called SqlDelegate::styleTextLayout().
 */
int SqlTableView::getWidth(QString s) {
    m_textLayout.setText(SqlDelegate::replaceNewLine(s));
    m_textLayout.beginLayout();
    QTextLine line = m_textLayout.createLine();
    line.setLineWidth(INT_MAX);
    return (int)ceil(line.naturalTextWidth());
}

// Compute width of vertical header,
// which is the width of the last row number.
int SqlTableView::vhwidth() {
    // QHeaderView seems to pad with spaces
    // and I haven't discovered how to switch this off.
    return getWidth(" " + QString(verticalHeader()->count() - 1));
}

// Override QTableview::resizeColumnsToContents
// using faster optimised algorithm.
void SqlTableView::resizeColumnsToContents()
{
    if (!model()) { return; }

    ensurePolished();

    QHeaderView * horizHeader = horizontalHeader();
    QHeaderView * vertHeader = verticalHeader();
    horizHeader->setSectionResizeMode(QHeaderView::Interactive);
    horizHeader->setDefaultSectionSize(0);
    horizHeader->setMinimumSectionSize(0);
    horizHeader->setDefaultAlignment(Qt::AlignLeft);
    horizHeader->setStretchLastSection(true);
    int rows = vertHeader->count();
	qreal widthView = viewport()->width() - vhwidth();
	int columns = horizHeader->count();
    int hw; // header width
    int w; // maximum width of any row in column
    int wx; // calcualted width of coulmn
    qreal totalWidth = 0; // total of all columns
	QVector<int> headerwidths(columns); // width wanted by header
	QVector<int> extraWidths(columns); // excess width over header
#ifdef DEBUG
    QVector<QString> columnNames(columns);
    QVector<int> rawWidths(columns);
#endif // DEBUG
    QStyleOptionViewItem option = viewOptions();
    // We want itemDelegate()->sizeHint (in case we have to call it)
    // to return the width that the column wants:
    // We'll decide later whether there is enough room.
    option.rect.setWidth(INT_MAX);
    int gap = (qobject_cast<SqlDelegate*>(itemDelegate()))
                ->styleTextLayout(&m_textLayout, option);
    // Add 1 to margin for border
    int margin = 2 * gap + 1;
    for (int column = 0; column < columns; ++column) {
        int logicalColumn = horizHeader->logicalIndex(column);
        if (horizHeader->isSectionHidden(logicalColumn)) { continue; }
        QVariant value = model()->headerData(column, Qt::Horizontal);
        if (value.canConvert<QString>()) {
#ifdef DEBUG
            columnNames[column] = value.toString();
#endif // DEBUG
            // QHeaderView sesm to pad column names with spaces
            // and I haven't discovered how to stop this.
            hw = getWidth(" " + value.toString() + " ") + margin;
        } else {
            hw = horizHeader->sectionSizeHint(logicalColumn) + margin;
        }
        QVariant decoration = model()->headerData(
            column, Qt::Horizontal, Qt::DecorationRole);
        if (decoration.canConvert<QIcon>()) {
            // We know our icons are 16x16, and there's no
            // straightforward way to extract the size.
            hw += gap + 16;
        }
        if (hw <= 0) {
            // Should be impossible, but maybe the user set a silly style
            // and we want to avoid division by zero later on
            hw = 10;
        }
        w = hw; // Force all columns to be big enough for their headers.
        // Find the maximum width of a field in this column
        for (int row = 0; row < rows; ++row) {
            int logicalRow = vertHeader->logicalIndex(row);
            if (vertHeader->isSectionHidden(logicalRow)) { continue; }
            QModelIndex index = model()->index(row, column);
            QVariant value = index.data(Qt::DisplayRole);
            int width;
            if (!value.canConvert<QString>()) {
                width = itemDelegate()->sizeHint(option, index).width();
            } else {
                width = getWidth(value.toString()) + margin;
            }
            if (w < width) { w = width; }
        }
        totalWidth += w;
        headerwidths[column] = hw;
        extraWidths[column] = w - hw;
    }
#ifdef DEBUG
    qDebug("viewport=%d; vhwidth=%d; margin = %d; "
           "widthView=%.2f; totalWidth=%.2f",
           viewport()->width(), vhwidth(), margin,
           widthView, totalWidth);
#endif // DEBUG
    if (totalWidth <= widthView) {
        // We can fit everything in the window:
        // expand all columns in proportion to their width
        for (int column = 0; column < columns; ++column) {
            int logicalColumn = horizHeader->logicalIndex(column);
            if (horizHeader->isSectionHidden(logicalColumn)) { continue; }
            hw = headerwidths[column];
            w = extraWidths[column];
            wx = (int)((w + hw) * widthView / totalWidth);
            // Avoid accumulating rounding errors
            widthView -= wx;
            totalWidth -= w + hw;
            setColumnWidth(column, wx);
            horizHeader->resizeSection(logicalColumn, wx);
#ifdef DEBUG
            qDebug("%s: hw=%d; w=%d; wx=%d; widthView=%.2f; totalWidth=%.2f",
                columnNames[column].toUtf8().data(),
                hw, w, wx, widthView, totalWidth);
#endif // DEBUG
        }
    } else {
        qreal adjustedWidth = 0;
        for (int column = 0; column < columns; ++column) {
            int logicalColumn = horizHeader->logicalIndex(column);
            if (horizHeader->isSectionHidden(logicalColumn)) { continue; }
            hw = headerwidths[column];
            w = extraWidths[column];
#ifdef DEBUG
            rawWidths[column] = w;
#endif // DEBUG
            double x = w / totalWidth;
            wx = hw + (int)(widthView * x * x * 2);
            // Avoid accumulating rounding errors
            extraWidths[column] = wx;
            adjustedWidth += wx; // re-use for scaled width
        }
        for (int column = 0; column < columns; ++column) {
            int logicalColumn = horizHeader->logicalIndex(column);
            if (horizHeader->isSectionHidden(logicalColumn)) { continue; }
            w = extraWidths[column];
            if (adjustedWidth < widthView) {
                // After scaling, we can fit everything in the window:
                // expand all columns in proportion to their width
                wx = (int)(w * widthView / adjustedWidth);
                // Avoid accumulating rounding errors
                widthView -= wx;
                adjustedWidth -= w;
            } else {
                wx = w;
            }
#ifdef DEBUG
            qDebug("%s: hw=%d; w=%d, adj %d; "
                "wx=%d; widthView=%.2f; totalWidth=%.2f, adj %.2f",
                columnNames[column].toUtf8().data(),
                headerwidths[column], rawWidths[column], extraWidths[column],
                wx, widthView, totalWidth, adjustedWidth);
#endif // DEBUG
            setColumnWidth(column, wx);
            horizHeader->resizeSection(logicalColumn, wx);
        }
    }
    return;
}

// Override QTableview::resizeRowsToContents
// using faster optimised algorithm.
void SqlTableView::resizeRowsToContents()
{
    if (!model()) { return; }

    QHeaderView * horizHeader = horizontalHeader();
    QHeaderView * vertHeader = verticalHeader();
    vertHeader->setSectionResizeMode(QHeaderView::Fixed);
    vertHeader->setMinimumSectionSize(-1);
    int rows = vertHeader->count();
	int columns = horizHeader->count();
    int vhh = vertHeader->defaultSectionSize();
    // Don't give any row more than half the available height
    int maxHeight = (viewport()->height()) - vhh / 2;
    QStyleOptionViewItem option = viewOptions();
    if (!m_prefs->cropColumns()) {
        option.features |= QStyleOptionViewItem::WrapText;
    }
    (qobject_cast<SqlDelegate*>(itemDelegate()))
                    ->styleTextLayout(&m_textLayout, option);
    for (int row = 0; row < rows; ++row) {
        int logicalRow = vertHeader->logicalIndex(row);
        if (vertHeader->isSectionHidden(logicalRow)) { continue; }
        int wantedHeight = vhh;
        for (int column = 0; column < columns; ++column) {
            int logicalColumn = horizHeader->logicalIndex(column);
            if (horizHeader->isSectionHidden(logicalColumn)) { continue; }
            QModelIndex index = model()->index(row, column);
            QVariant value = index.data(Qt::DisplayRole);
            int height;
            int width = columnWidth(column);
            if (value.canConvert<QString>()) {
                m_textLayout.setText(
                    SqlDelegate::replaceNewLine(value.toString()));
                m_textLayout.beginLayout();
                height = 0;
                QTextLine line = m_textLayout.createLine();
                if (m_prefs->cropColumns()) {
                    if (line.isValid()) { height += line.height(); }
                } else while (line.isValid()) {
                    height += line.height();
                    line.setLineWidth(width);
                    line = m_textLayout.createLine();
                }
            } else {
                option.rect.setWidth(width);
                height = itemDelegate()->sizeHint(option, index).height();
            }
            if (height > maxHeight) { height = maxHeight; }
            if (wantedHeight < height) { wantedHeight = height; }
        }
        setRowHeight(row, wantedHeight);
    }
}


// Override constructor
SqlTableView::SqlTableView(QWidget * parent)
    :QTableView(parent)
{
    /* We used to cache the preference values which this class uses,
     * but that used the old value if the user changed a preference
     * after this class has been instantiated, which happens when a
     * DataViewer is created, usually when sqliteman is started.
     * Our singleton preferences class caches all preferences when
     * sqliteman is started, but updates its cache if the user changes
     * a preference, so reading from it gets an up to date value
     * relatively cheaply.
     */
    m_prefs = Preferences::instance();
}
