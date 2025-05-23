// This module implements the QsciMacro class.
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


#include "Qsci/qscimacro.h"

#include <QtCore/qstringlist.h>

#include "Qsci/qsciscintilla.h"


static int fromHex(unsigned char ch);



// The ctor.
QsciMacro::QsciMacro(QsciScintilla *parent)
    : QObject(parent), qsci(parent)
{
}


// The ctor that initialises the macro.
QsciMacro::QsciMacro(const QString &asc, QsciScintilla *parent)
    : QObject(parent), qsci(parent)
{
    load(asc);
}


// The dtor.
QsciMacro::~QsciMacro()
{
}


// Clear the contents of the macro.
void QsciMacro::clear()
{
    macro.clear();
}


// Read a macro from a string.
bool QsciMacro::load(const QString &asc)
{
    bool ok = true;

    macro.clear();

    QStringList fields = asc.split(' ');

    int f = 0;

    while (f < fields.size())
    {
        Macro cmd;
        unsigned len;

        // Extract the 3 fixed fields.
        if (f + 3 > fields.size())
        {
            ok = false;
            break;
        }

        cmd.msg = fields[f++].toUInt(&ok);

        if (!ok)
            break;

        cmd.wParam = fields[f++].toULong(&ok);

        if (!ok)
            break;

        len = fields[f++].toUInt(&ok);

        if (!ok)
            break;

        // Extract any text.
        if (len)
        {
            if (f + 1 > fields.size())
            {
                ok = false;
                break;
            }

            cmd.text.resize(len - 1);

            QByteArray ba = fields[f++].toAscii();
            const char *sp = ba.data();

            char *dp = cmd.text.data();

            if (!sp)
            {
                ok = false;
                break;
            }

            while (len--)
            {
                unsigned char ch;

                ch = *sp++;

                if (ch == '"' || ch <= ' ' || ch >= 0x7f)
                {
                    ok = false;
                    break;
                }

                if (ch == '\\')
                {
                    int b1, b2;

                    if ((b1 = fromHex(*sp++)) < 0 ||
                        (b2 = fromHex(*sp++)) < 0)
                    {
                        ok = false;
                        break;
                    }

                    ch = (b1 << 4) + b2;
                }

                *dp++ = ch;
            }

            if (!ok)
                break;
        }

        macro.append(cmd);
    }
        
    if (!ok)
        macro.clear();

    return ok;
}


// Write a macro to a string.
QString QsciMacro::save() const
{
    QString ms;

    QList<Macro>::const_iterator it;

    for (it = macro.begin(); it != macro.end(); ++it)
    {
        if (!ms.isEmpty())
            ms += ' ';

        unsigned len = (*it).text.size();
        QString m;

        ms += m.sprintf("%u %lu %u", (*it).msg, (*it).wParam, len);

        if (len)
        {
            // In Qt v3, if the length is greater than zero then it also
            // includes the '\0', so we need to make sure that Qt v4 writes the
            // '\0'.  (That the '\0' is written at all is probably a historical
            // bug - using size() instead of length() - which we don't fix so
            // as not to break old macros.)
            ++len;

            ms += ' ';

            const char *cp = (*it).text.data();

            while (len--)
            {
                unsigned char ch = *cp++;

                if (ch == '\\' || ch == '"' || ch <= ' ' || ch >= 0x7f)
                {
                    QString buf;

                    ms += buf.sprintf("\\%02x", ch);
                }
                else
                    ms += ch;
            }
        }
    }

    return ms;
}


// Play the macro.
void QsciMacro::play()
{
    if (!qsci)
        return;

    QList<Macro>::const_iterator it;

    for (it = macro.begin(); it != macro.end(); ++it)
        qsci->SendScintilla((*it).msg, (*it).wParam, (*it).text.data());
}


// Start recording.
void QsciMacro::startRecording()
{
    if (!qsci)
        return;

    macro.clear();

    connect(qsci, SIGNAL(SCN_MACRORECORD(unsigned int, unsigned long, void *)),
            SLOT(record(unsigned int, unsigned long, void *)));

    qsci->SendScintilla(QsciScintillaBase::SCI_STARTRECORD);
}


// End recording.
void QsciMacro::endRecording()
{
    if (!qsci)
        return;

    qsci->SendScintilla(QsciScintillaBase::SCI_STOPRECORD);
    qsci->disconnect(this);
}


// Record a command.
void QsciMacro::record(unsigned int msg, unsigned long wParam, void *lParam)
{
    Macro m;

    m.msg = msg;
    m.wParam = wParam;

    // Determine commands which need special handling of the parameters.
    switch (msg)
    {
    case QsciScintillaBase::SCI_ADDTEXT:
        m.text = QByteArray(reinterpret_cast<const char *>(lParam), wParam);
        break;

    case QsciScintillaBase::SCI_REPLACESEL:
        if (!macro.isEmpty() && macro.last().msg == QsciScintillaBase::SCI_REPLACESEL)
        {
            // This is the command used for ordinary user input so it's a
            // significant space reduction to append it to the previous
            // command.

            macro.last().text.append(reinterpret_cast<const char *>(lParam));
            return;
        }

        /* Drop through. */

    case QsciScintillaBase::SCI_INSERTTEXT:
    case QsciScintillaBase::SCI_APPENDTEXT:
    case QsciScintillaBase::SCI_SEARCHNEXT:
    case QsciScintillaBase::SCI_SEARCHPREV:
        m.text.append(reinterpret_cast<const char *>(lParam));
        break;
    }

    macro.append(m);
}


// Return the given hex character as a binary.
static int fromHex(unsigned char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';

    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;

    return -1;
}
