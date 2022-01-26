/****************************************************************************
**
** Original version Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** Modifications Copyright (C) 2015 Richard Parkins
** This is exactly the standard QLineEdit except that it computes its
** sizeHint() using the width of the text that it holds rather than a
** fixed number of character widths.
**
** This file may be used under the terms of the GNU General Public License
** version 3.0 as published by the Free Software Foundation.
**
****************************************************************************/

#include <QApplication>
#include <QFontMetrics>
#include <QtCore/QMargins>
#include <QStyle>
#include <QStyleOption>

#include "mylineedit.h"

QSize MyLineEdit::sizeHint() const
{
    ensurePolished();
    QFontMetrics fm(font());
	QMargins tm = textMargins();
	QMargins wm = contentsMargins();
	QString s(text());
	if (s.isEmpty()) { s = placeholderText(); }
    int h = qMax(fm.height(), 14) + 2
            + tm.top() + tm.bottom()
            + wm.top() + wm.bottom();
    int w = fm.boundingRect(s).width() + 4
            + tm.left() + tm.right()
            + wm.left() + wm.right(); // "some"
    QStyleOptionFrameV2 opt;
    initStyleOption(&opt);
    return (style()->sizeFromContents(QStyle::CT_LineEdit, &opt, QSize(w, h).
                                      expandedTo(QApplication::globalStrut()), this));
}
