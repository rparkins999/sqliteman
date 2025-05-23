// This module implements the QsciLexerSQL class.
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


#include "Qsci/qscilexersql.h"

#include <qcolor.h>
#include <qfont.h>
#include <QtCore/qsettings.h>


// The ctor.
QsciLexerSQL::QsciLexerSQL(QObject *parent)
    : QsciLexer(parent),
      fold_comments(false), fold_compact(true), backslash_escapes(false)
{
}


// The dtor.
QsciLexerSQL::~QsciLexerSQL()
{
}


// Returns the language name.
const char *QsciLexerSQL::language() const
{
    return "SQL";
}


// Returns the lexer name.
const char *QsciLexerSQL::lexer() const
{
    return "sql";
}


// Return the style used for braces.
int QsciLexerSQL::braceStyle() const
{
    return Operator;
}


// Returns the foreground colour of the text for a style.
QColor QsciLexerSQL::defaultColor(int style) const
{
    switch (style)
    {
    case Default:
        return QColor(0x80,0x80,0x80);

    case Comment:
    case CommentLine:
    case PlusPrompt:
    case PlusComment:
    case CommentLineHash:
        return QColor(0x00,0x7f,0x00);

    case CommentDoc:
        return QColor(0x7f,0x7f,0x7f);

    case Number:
        return QColor(0x00,0x7f,0x7f);

    case Keyword:
        return QColor(0x00,0x00,0x7f);

    case DoubleQuotedString:
    case SingleQuotedString:
        return QColor(0x7f,0x00,0x7f);

    case PlusKeyword:
        return QColor(0x7f,0x7f,0x00);

    case Operator:
    case Identifier:
        break;

    case CommentDocKeyword:
        return QColor(0x30,0x60,0xa0);

    case CommentDocKeywordError:
        return QColor(0x80,0x40,0x20);

    case KeywordSet5:
        return QColor(0x4b,0x00,0x82);

    case KeywordSet6:
        return QColor(0xb0,0x00,0x40);

    case KeywordSet7:
        return QColor(0x8b,0x00,0x00);

    case KeywordSet8:
        return QColor(0x80,0x00,0x80);
    }

    return QsciLexer::defaultColor(style);
}


// Returns the end-of-line fill for a style.
bool QsciLexerSQL::defaultEolFill(int style) const
{
    if (style == PlusPrompt)
        return true;

    return QsciLexer::defaultEolFill(style);
}


// Returns the font of the text for a style.
QFont QsciLexerSQL::defaultFont(int style) const
{
    QFont f;

    switch (style)
    {
    case Comment:
    case CommentLine:
    case PlusComment:
    case CommentLineHash:
    case CommentDocKeyword:
    case CommentDocKeywordError:
#if defined(Q_OS_WIN)
        f = QFont("Comic Sans MS",9);
#else
        f = QFont("Bitstream Vera Serif",9);
#endif
        break;

    case Keyword:
    case Operator:
        f = QsciLexer::defaultFont(style);
        f.setBold(true);
        break;

    case DoubleQuotedString:
    case SingleQuotedString:
    case PlusPrompt:
#if defined(Q_OS_WIN)
        f = QFont("Courier New",10);
#else
        f = QFont("Bitstream Vera Sans Mono",9);
#endif
        break;

    default:
        f = QsciLexer::defaultFont(style);
    }

    return f;
}


// Returns the set of keywords.
const char *QsciLexerSQL::keywords(int set) const
{
    if (set == 1)
        return
            "absolute action add admin after aggregate alias all "
            "allocate alter and any are array as asc assertion "
            "at authorization before begin binary bit blob "
            "boolean both breadth by call cascade cascaded case "
            "cast catalog char character check class clob close "
            "collate collation column commit completion connect "
            "connection constraint constraints constructor "
            "continue corresponding create cross cube current "
            "current_date current_path current_role current_time "
            "current_timestamp current_user cursor cycle data "
            "date day deallocate dec decimal declare default "
            "deferrable deferred delete depth deref desc "
            "describe descriptor destroy destructor "
            "deterministic dictionary diagnostics disconnect "
            "distinct domain double drop dynamic each else end "
            "end-exec equals escape every except exception exec "
            "execute external false fetch first float for "
            "foreign found from free full function general get "
            "global go goto grant group grouping having host "
            "hour identity if ignore immediate in indicator "
            "initialize initially inner inout input insert int "
            "integer intersect interval into is isolation "
            "iterate join key language large last lateral "
            "leading left less level like limit local localtime "
            "localtimestamp locator map match minute modifies "
            "modify module month names national natural nchar "
            "nclob new next no none not null numeric object of "
            "off old on only open operation option or order "
            "ordinality out outer output pad parameter "
            "parameters partial path postfix precision prefix "
            "preorder prepare preserve primary prior privileges "
            "procedure public read reads real recursive ref "
            "references referencing relative restrict result "
            "return returns revoke right role rollback rollup "
            "routine row rows savepoint schema scroll scope "
            "search second section select sequence session "
            "session_user set sets size smallint some| space "
            "specific specifictype sql sqlexception sqlstate "
            "sqlwarning start state statement static structure "
            "system_user table temporary terminate than then "
            "time timestamp timezone_hour timezone_minute to "
            "trailing transaction translation treat trigger "
            "true under union unique unknown unnest update usage "
            "user using value values varchar variable varying "
            "view when whenever where with without work write "
            "year zone";

    if (set == 4)
        return
            "acc~ept a~ppend archive log attribute bre~ak "
            "bti~tle c~hange cl~ear col~umn comp~ute conn~ect "
            "copy def~ine del desc~ribe disc~onnect e~dit "
            "exec~ute exit get help ho~st i~nput l~ist passw~ord "
            "pau~se pri~nt pro~mpt quit recover rem~ark "
            "repf~ooter reph~eader r~un sav~e set sho~w shutdown "
            "spo~ol sta~rt startup store timi~ng tti~tle "
            "undef~ine var~iable whenever oserror whenever "
            "sqlerror";

    return 0;
}


// Returns the user name of a style.
QString QsciLexerSQL::description(int style) const
{
    switch (style)
    {
    case Default:
        return tr("Default");

    case Comment:
        return tr("Comment");

    case CommentLine:
        return tr("Comment line");

    case CommentDoc:
        return tr("JavaDoc style comment");

    case Number:
        return tr("Number");

    case Keyword:
        return tr("Keyword");

    case DoubleQuotedString:
        return tr("Double-quoted string");

    case SingleQuotedString:
        return tr("Single-quoted string");

    case PlusKeyword:
        return tr("SQL*Plus keyword");

    case PlusPrompt:
        return tr("SQL*Plus prompt");

    case Operator:
        return tr("Operator");

    case Identifier:
        return tr("Identifier");

    case PlusComment:
        return tr("SQL*Plus comment");

    case CommentLineHash:
        return tr("# comment line");

    case CommentDocKeyword:
        return tr("JavaDoc keyword");

    case CommentDocKeywordError:
        return tr("JavaDoc keyword error");

    case KeywordSet5:
        return tr("User defined 1");

    case KeywordSet6:
        return tr("User defined 2");

    case KeywordSet7:
        return tr("User defined 3");

    case KeywordSet8:
        return tr("User defined 4");
    }

    return QString();
}


// Returns the background colour of the text for a style.
QColor QsciLexerSQL::defaultPaper(int style) const
{
    if (style == PlusPrompt)
        return QColor(0xe0,0xff,0xe0);

    return QsciLexer::defaultPaper(style);
}


// Refresh all properties.
void QsciLexerSQL::refreshProperties()
{
    setCommentProp();
    setCompactProp();
    setBackslashEscapesProp();
}


// Read properties from the settings.
bool QsciLexerSQL::readProperties(QSettings &qs,const QString &prefix)
{
    int rc = true;

    fold_comments = qs.value(prefix + "foldcomments", false).toBool();
    fold_compact = qs.value(prefix + "foldcompact", true).toBool();
    backslash_escapes = qs.value(prefix + "backslashescapes", false).toBool();

    return rc;
}


// Write properties to the settings.
bool QsciLexerSQL::writeProperties(QSettings &qs,const QString &prefix) const
{
    int rc = true;

    qs.value(prefix + "foldcomments", fold_comments);
    qs.value(prefix + "foldcompact", fold_compact);
    qs.value(prefix + "backslashescapes", backslash_escapes);

    return rc;
}


// Return true if comments can be folded.
bool QsciLexerSQL::foldComments() const
{
    return fold_comments;
}


// Set if comments can be folded.
void QsciLexerSQL::setFoldComments(bool fold)
{
    fold_comments = fold;

    setCommentProp();
}


// Set the "fold.comment" property.
void QsciLexerSQL::setCommentProp()
{
    emit propertyChanged("fold.comment",(fold_comments ? "1" : "0"));
}


// Return true if folds are compact.
bool QsciLexerSQL::foldCompact() const
{
    return fold_compact;
}


// Set if folds are compact
void QsciLexerSQL::setFoldCompact(bool fold)
{
    fold_compact = fold;

    setCompactProp();
}


// Set the "fold.compact" property.
void QsciLexerSQL::setCompactProp()
{
    emit propertyChanged("fold.compact",(fold_compact ? "1" : "0"));
}


// Return true if backslash escapes are enabled.
bool QsciLexerSQL::backslashEscapes() const
{
    return backslash_escapes;
}


// Enable/disable backslash escapes.
void QsciLexerSQL::setBackslashEscapes(bool enable)
{
    backslash_escapes = enable;

    setBackslashEscapesProp();
}


// Set the "sql.backslash.escapes" property.
void QsciLexerSQL::setBackslashEscapesProp()
{
    emit propertyChanged("sql.backslash.escapes",(backslash_escapes ? "1" : "0"));
}
