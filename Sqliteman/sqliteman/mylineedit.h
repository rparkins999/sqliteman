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

#ifndef MYLINEEDIT_H
#define MYLINEEDIT_H

#include <QtCore/QSize>
#include <QtCore/QObject>
#include <QLineEdit>

class MyLineEdit : public QLineEdit
{
    Q_OBJECT
public:
	MyLineEdit(QWidget* parent=0){};
	~MyLineEdit(){};
    QSize sizeHint() const;
};

#endif // MYLINEEDIT_H
