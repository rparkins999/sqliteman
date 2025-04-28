/* Copyright © 2007-2009 Petr Vanek and 2015-2025 Richard Parkins
 *
 * For general Sqliteman copyright and licensing information please refer
 * to the COPYING file provided with the program. Following this notice may exist
 * a copyright and/or license notice that predates the release of Sqliteman
 * for which a new license (GPL+exception) is in place.
 */

#include <QApplication>
#include <QFocusEvent>
#include <QtCore/QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QtCore/QRect>
#include <QtCore/QSize>
#include <QtCore/QSizeF>
#include <QSqlTableModel>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTextOption>
#include <QToolButton>
#include <QtCore/QVector>

#include "sqldelegate.h"
#include "utils.h"
#include "multieditdialog.h"
#include "database.h"
#include <cmath>

// Apply style option to textLayout.
// Common code for sizeHint(), drawDisplay(),
// and SqlTableView::resizeColumnsToContents() (where it is called only once
// to avoid repeated calls to sizeHint() which would call it every time).
// Returns textMargin.
int SqlDelegate::styleTextLayout(
    QTextLayout * textLayout, const QStyleOptionViewItem &option) const
{
    const QWidget *widget = option.widget;
    const QStyle *style = widget ? widget->style() : QApplication::style();
    QTextOption textOption;
    const int textMargin =
        style->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, widget) + 1;
    textOption.setWrapMode(
        (option.features & QStyleOptionViewItem::WrapText)
        ? QTextOption::WrapAtWordBoundaryOrAnywhere
        : QTextOption::NoWrap);
    textOption.setTextDirection(option.direction);
    textOption.setAlignment(
        QStyle::visualAlignment(option.direction, option.displayAlignment));
    textLayout->setTextOption(textOption);
    textLayout->setFont(option.font);
    return textMargin;
}

// Overrides QItemDelegate::createEditor
QWidget *SqlDelegate::createEditor(QWidget *parent,
								   const QStyleOptionViewItem &/* option */,
								   const QModelIndex &/* index */) const
{
	SqlDelegateUi *editor = new SqlDelegateUi(parent);
	editor->setFocus();
	editor->setFocusPolicy(Qt::StrongFocus);
	connect(editor, SIGNAL(closeEditor()),
			this, SLOT(editor_closeEditor()));
    connect(editor, SIGNAL(textChanged()),
            this, SLOT(editor_textChanged()));
    connect(editor, SIGNAL(nullclicked()),
                this, SLOT(editor_nullClicked()));
	return editor;
}

// Overrides QItemDelegate::setEditorData
void SqlDelegate::setEditorData(QWidget *editor,
								const QModelIndex &index) const
{
    const QSqlTableModel * table =
        qobject_cast<const QSqlTableModel *>(index.model());
    SqlDelegateUi* ed = static_cast<SqlDelegateUi*>(editor);
    ed->m_editable = table != NULL;
	ed->setSqlData(index.model()->data(index, Qt::EditRole));
}

// Overrides QItemDelegate::setModelData
void SqlDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
							   const QModelIndex &index) const
{
	SqlDelegateUi *ed = static_cast<SqlDelegateUi*>(editor);
	if (ed->sqlData() != index.model()->data(index, Qt::EditRole))
		model->setData(index, ed->sqlData(), Qt::EditRole);
// 	else
// 		qDebug("ed->sqlData() == index.model()->data(index, Qt::EditRole)");
}

// Overrides QItemDelegate::updateEditorGeometry
void SqlDelegate::updateEditorGeometry(QWidget *editor,
									   const QStyleOptionViewItem &option,
									   const QModelIndex &/* index */) const
{
	editor->setGeometry(option.rect);
}

// Overrides QItemDelegate::sizeHint
// This must use the same algorithm as drawDisplay()
// except that we don't do elision.
QSize SqlDelegate::sizeHint(const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    QVariant value = index.data(Qt::DisplayRole);
    if (!value.canConvert<QString>()) {
        return QItemDelegate::sizeHint(option, index);
    }
    QTextLayout textLayout;
    styleTextLayout(&textLayout, option);
    qreal height = 0;
    qreal width = 0;
    textLayout.setText(replaceNewLine(value.toString()));
    textLayout.beginLayout();
    do {
        QTextLine line = textLayout.createLine();
        if (!line.isValid()) { break; }
        line.setLineWidth(option.rect.width());
        line.setPosition(QPointF(0, height));
        height += line.height();
        width = qMax(width, line.naturalTextWidth());
    } while (!m_prefs->cropColumns());
    if (height == 0) { // Should never happen, but bail if it does.
        return QItemDelegate::sizeHint(option, index);
    }
    return QSize((int)(ceil(width)), (int)(ceil(height)));
}

SqlDelegate::SqlDelegate(QObject * parent)
	: QItemDelegate(parent)
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

// Overrides QItemDelegate::drawDisplay
void SqlDelegate::drawDisplay(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QRect &rect, const QString &text) const
{
    QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                              ? QPalette::Normal : QPalette::Disabled;
    if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
        cg = QPalette::Inactive;
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(rect, option.palette.brush(cg, QPalette::Highlight));
        painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
    } else {
        painter->setPen(option.palette.color(cg, QPalette::Text));
    }

    if (text.isEmpty())
        return;

    if (option.state & QStyle::State_Editing) {
        painter->save();
        painter->setPen(option.palette.color(cg, QPalette::Text));
        painter->drawRect(rect.adjusted(0, 0, -1, -1));
        painter->restore();
    }
    QTextLayout textLayout;
    int textMargin = styleTextLayout(&textLayout, option);
    // remove width padding
    QRect textRect = option.rect.adjusted(textMargin, 0, -textMargin, 0);
    QString text1 = replaceNewLine(text);
    textLayout.setText(text1);
    qreal height = 0;
    int lineCount = 0;
    bool elide = false;
    textLayout.beginLayout();
    do {
        if (height > textRect.height()) {
            elide = true; // text too big, need to elide
            break;
        }
        QTextLine line = textLayout.createLine();
        if (!line.isValid()) { break; }
        line.setLineWidth(textRect.width());
        line.setPosition(QPointF(0, height));
        height += line.height();
        ++lineCount;
    } while (!m_prefs->cropColumns());
    if (elide) { // redo with elision
        QTextLine lastLine = textLayout.lineAt(lineCount - 1);
        int start = lastLine.textStart();
        QString elided = option.fontMetrics.elidedText(
            text1.mid(start), option.textElideMode, textRect.width());
        textLayout.endLayout();
        textLayout.setText(text1.left(start).append(elided));
        height = 0;
        textLayout.beginLayout();
        do {
            QTextLine line = textLayout.createLine();
            if (!line.isValid()) { break; }
            line.setLineWidth(textRect.width());
            line.setPosition(QPointF(0, height));
            height += line.height();
        } while (true);
    }

    const QSize layoutSize(textRect.width(), textRect.height());
    const QRect layoutRect = QStyle::alignedRect(
        option.direction, option.displayAlignment, layoutSize, textRect);
    textLayout.draw(painter, layoutRect.topLeft(),
        QVector<QTextLayout::FormatRange>(), layoutRect);
}

void SqlDelegate::editor_closeEditor()
{
	SqlDelegateUi *ed = qobject_cast<SqlDelegateUi*>(sender());
	emit commitData(ed);
    emit dataChanged();
	emit closeEditor(ed);
}

void SqlDelegate::editor_textChanged()
{
    SqlDelegateUi *ed = qobject_cast<SqlDelegateUi*>(sender());
    emit commitData(ed);
    emit dataChanged();
}

void SqlDelegate::editor_nullClicked()
{
    emit insertNull();
}

SqlDelegateUi::SqlDelegateUi(QWidget * parent)
	: QWidget(parent)
{
	setupUi(this);

	nullButton->setIcon(Utils::getIcon("setnull.png"));
	editButton->setIcon(Utils::getIcon("edit.png"));

	connect(nullButton, SIGNAL(clicked(bool)),
			this, SLOT(nullButton_clicked(bool)));
	connect(editButton, SIGNAL(clicked(bool)),
			this, SLOT(editButton_clicked(bool)));
	connect(lineEdit, SIGNAL(textEdited(const QString &)),
			this, SLOT(lineEdit_textEdited(const QString &)));
}

void SqlDelegateUi::focusInEvent(QFocusEvent *e)
{
	if (e->gotFocus())
		lineEdit->setFocus();
}

void SqlDelegateUi::setSqlData(const QVariant & data)
{
	m_sqlData = data;
	// blob
	if (data.type() == QVariant::ByteArray)
	{
		lineEdit->setDisabled(true);
		lineEdit->setToolTip(tr(
			"Blobs can be edited with the multiline editor only (Ctrl+Shift+E)"));
		if (m_prefs->blobHighlight())
		{
			lineEdit->setText(m_prefs->blobHighlightText());
		}
		else
		{
			QString hex = Database::hex(data.toByteArray());
			if (m_prefs->cropColumns())
			{
				hex = hex.length() > 20 ? hex.left(20)+"..." : hex;
			}
			lineEdit->setText(hex);
		}
	}
	else if (m_sqlData.toString().contains("\n"))
	{
		lineEdit->setDisabled(true);
		lineEdit->setToolTip(tr(
			"Multiline texts can be edited with the multiline editor only"
			"(Ctrl+Shift+E)"));
		lineEdit->setText(data.toString());
	}
	else
	{
		lineEdit->setText(data.toString());
	}
}

QVariant SqlDelegateUi::sqlData()
{
	return m_sqlData;
}

void SqlDelegateUi::nullButton_clicked(bool state)
{
    emit nullclicked();
	emit closeEditor();
}

void SqlDelegateUi::editButton_clicked(bool state)
{
	MultiEditDialog dia(this);
	qApp->setOverrideCursor(Qt::WaitCursor);
	dia.setData(m_sqlData, m_editable);
	qApp->restoreOverrideCursor();
	if (dia.exec())
	{
		m_sqlData = dia.data();
        emit textChanged();
	}
	emit closeEditor();
}

void SqlDelegateUi::lineEdit_textEdited(const QString & text)
{
	int cursor = lineEdit->cursorPosition();
	m_sqlData = text;
    emit textChanged();
	lineEdit->setCursorPosition(cursor);
}
