// This module implements the QsciLexerBatch class.
//
// Copyright (c) 2010 Riverbank Computing Limited <info@riverbankcomputing.com>
// 
// This file is part of QScintilla.
// 
// This file may be used under the terms of the GNU General Public
// License versions 2.0 or 3.0 as published by the Free Software
// Foundation and appearing in the files LICENSE.GPL2 and LICENSE.GPL3
// included in the packaging of this file.  Alternatively you may (at
// your option) use any later version of the GNU General Public
// License if such license has been publicly approved by Riverbank
// Computing Limited (or its successors, if any) and the KDE Free Qt
// Foundation. In addition, as a special exception, Riverbank gives you
// certain additional rights. These rights are described in the Riverbank
// GPL Exception version 1.1, which can be found in the file
// GPL_EXCEPTION.txt in this package.
// 
// Please review the following information to ensure GNU General
// Public Licensing requirements will be met:
// http://trolltech.com/products/qt/licenses/licensing/opensource/. If
// you are unsure which license is appropriate for your use, please
// review the following information:
// http://trolltech.com/products/qt/licenses/licensing/licensingoverview
// or contact the sales department at sales@riverbankcomputing.com.
// 
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.


#include "Qsci/qscilexerbatch.h"

#include <qcolor.h>
#include <qfont.h>
#include <QtCore/qsettings.h>


// The ctor.
QsciLexerBatch::QsciLexerBatch(QObject *parent)
    : QsciLexer(parent)
{
}


// The dtor.
QsciLexerBatch::~QsciLexerBatch()
{
}


// Returns the language name.
const char *QsciLexerBatch::language() const
{
    return "Batch";
}


// Returns the lexer name.
const char *QsciLexerBatch::lexer() const
{
    return "batch";
}


// Return the string of characters that comprise a word.
const char *QsciLexerBatch::wordCharacters() const
{
    return "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-";
}


// Returns the foreground colour of the text for a style.
QColor QsciLexerBatch::defaultColor(int style) const
{
    switch (style)
    {
    case Default:
    case Operator:
        return QColor(0x00,0x00,0x00);

    case Comment:
        return QColor(0x00,0x7f,0x00);

    case Keyword:
    case ExternalCommand:
        return QColor(0x00,0x00,0x7f);

    case Label:
        return QColor(0x7f,0x00,0x7f);

    case HideCommandChar:
        return QColor(0x7f,0x7f,0x00);

    case Variable:
        return QColor(0x80,0x00,0x80);
    }

    return QsciLexer::defaultColor(style);
}


// Returns the end-of-line fill for a style.
bool QsciLexerBatch::defaultEolFill(int style) const
{
    switch (style)
    {
    case Label:
        return true;
    }

    return QsciLexer::defaultEolFill(style);
}


// Returns the font of the text for a style.
QFont QsciLexerBatch::defaultFont(int style) const
{
    QFont f;

    switch (style)
    {
    case Comment:
#if defined(Q_OS_WIN)
        f = QFont("Comic Sans MS",9);
#else
        f = QFont("Bitstream Vera Serif",9);
#endif
        break;

    case Keyword:
        f = QsciLexer::defaultFont(style);
        f.setBold(true);
        break;

    case ExternalCommand:
#if defined(Q_OS_WIN)
        f = QFont("Courier New",10);
#else
        f = QFont("Bitstream Vera Sans Mono",9);
#endif
        f.setBold(true);
        break;

    default:
        f = QsciLexer::defaultFont(style);
    }

    return f;
}


// Returns the set of keywords.
const char *QsciLexerBatch::keywords(int set) const
{
    if (set == 1)
        return
            "rem set if exist errorlevel for in do break call "
            "chcp cd chdir choice cls country ctty date del "
            "erase dir echo exit goto loadfix loadhigh mkdir md "
            "move path pause prompt rename ren rmdir rd shift "
            "time type ver verify vol com con lpt nul";

    return 0;
}


// Return the case sensitivity.
bool QsciLexerBatch::caseSensitive() const
{
    return false;
}


// Returns the user name of a style.
QString QsciLexerBatch::description(int style) const
{
    switch (style)
    {
    case Default:
        return tr("Default");

    case Comment:
        return tr("Comment");

    case Keyword:
        return tr("Keyword");

    case Label:
        return tr("Label");

    case HideCommandChar:
        return tr("Hide command character");

    case ExternalCommand:
        return tr("External command");

    case Variable:
        return tr("Variable");

    case Operator:
        return tr("Operator");
    }

    return QString();
}


// Returns the background colour of the text for a style.
QColor QsciLexerBatch::defaultPaper(int style) const
{
    switch (style)
    {
    case Label:
        return QColor(0x60,0x60,0x60);
    }

    return QsciLexer::defaultPaper(style);
}
