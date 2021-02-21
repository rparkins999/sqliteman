/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QApplication>
#include <QFocusEvent>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QSize>
#include <QSizeF>
#include <QStyle>
#include <QStyleOptionViewItemV4>
#include <QTextOption>
#include <QToolButton>
#include <QVector>

#include "sqldelegate.h"
#include "utils.h"
#include "multieditdialog.h"
#include "preferences.h"
#include "database.h"


SqlDelegate::SqlDelegate(QObject * parent)
	: QItemDelegate(parent)
{
}

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

void SqlDelegate::setEditorData(QWidget *editor,
								const QModelIndex &index) const
{
	static_cast<SqlDelegateUi*>(editor)->setSqlData(
								index.model()->data(index,
								Qt::EditRole));
}

void SqlDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
							   const QModelIndex &index) const
{
	SqlDelegateUi *ed = static_cast<SqlDelegateUi*>(editor);
	if (ed->sqlData() != index.model()->data(index, Qt::EditRole))
		model->setData(index, ed->sqlData(), Qt::EditRole);
// 	else
// 		qDebug("ed->sqlData() == index.model()->data(index, Qt::EditRole)");
}

void SqlDelegate::updateEditorGeometry(QWidget *editor,
									   const QStyleOptionViewItem &option,
									   const QModelIndex &/* index */) const
{
	editor->setGeometry(option.rect);
}

QSizeF SqlDelegate::doTextLayout(int lineWidth) const
{
    qreal height = 0;
    qreal widthUsed = 0;
    textLayout.beginLayout();
    while (true) {
        QTextLine line = textLayout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
        widthUsed = qMax(widthUsed, line.naturalTextWidth());
    }
    textLayout.endLayout();
    return QSizeF(widthUsed, height);
}

void SqlDelegate::drawDisplay(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QRect &rect, const QString &text) const
{
    QTextOption textOption;
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

    const QStyleOptionViewItemV4 opt = option;

    const QWidget *widget;
    QStyle *style;
    if (const QStyleOptionViewItemV3 *v3 =
        qstyleoption_cast<const QStyleOptionViewItemV3 *>(&option))
    {
        widget = v3->widget;
        style = widget->style();
    } else {
        widget = 0;
        style = QApplication::style();
    }

    const int textMargin =
        style->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, widget) + 1;
    QRect textRect = rect.adjusted(textMargin, 0, -textMargin, 0); // remove width padding
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textOption.setTextDirection(option.direction);
    textOption.setAlignment(
        QStyle::visualAlignment(option.direction, option.displayAlignment));
    textLayout.setTextOption(textOption);
    textLayout.setFont(option.font);
    QString text1 = replaceNewLine(text);
    textLayout.setText(text1);

    qreal height = 0;
    int lineCount = 0;
    bool elide = false;
    textLayout.beginLayout();
    while (true) {
        QTextLine line = textLayout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(textRect.width());
        line.setPosition(QPointF(0, height));
        height += line.height();
        if (height > textRect.height()) {
            elide = true;
            break;
        }
        ++lineCount;
    }
    if (lineCount == 0) {
        return; // Should never happen, but bail if it does.
    }
    if (elide) {
        QTextLine lastLine = textLayout.lineAt(lineCount - 1);
        int start = lastLine.textStart();
        QString elided = option.fontMetrics.elidedText(
            text1.mid(start), option.textElideMode, textRect.width());
        textLayout.endLayout();
        textLayout.setText(text1.left(start).append(elided));
        textLayout.beginLayout();
        height = 0;
        while (true) {
            QTextLine line = textLayout.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(textRect.width());
            line.setPosition(QPointF(0, height));
            height += line.height();
        }
    }

    const QSize layoutSize(textRect.width(),textRect.height());
    const QRect layoutRect = QStyle::alignedRect(
        option.direction, option.displayAlignment, layoutSize, textRect);
    textLayout.draw(painter, layoutRect.topLeft(), QVector<QTextLayout::FormatRange>(), layoutRect);
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
	Preferences * prefs = Preferences::instance();
	bool useBlob = prefs->blobHighlight();
	QString blobText = prefs->blobHighlightText();
	bool cropColumns = prefs->cropColumns();

	m_sqlData = data;
	// blob
	if (data.type() == QVariant::ByteArray)
	{
		lineEdit->setDisabled(true);
		lineEdit->setToolTip(tr(
			"Blobs can be edited with the multiline editor only (Ctrl+Shift+E)"));
		Preferences * prefs = Preferences::instance();
		if (useBlob)
		{
			lineEdit->setText(prefs->blobHighlightText());
		}
		else
		{
			QString hex = Database::hex(data.toByteArray());
			if (cropColumns)
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
	dia.setData(m_sqlData);
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
