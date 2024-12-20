// This module implements the "official" high-level API of the Qt port of
// Scintilla.  It is modelled on QTextEdit - a method of the same name should
// behave in the same way.
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


#include "Qsci/qsciscintilla.h"

#include <string.h>
#include <qapplication.h>
#include <qcolor.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qpoint.h>

#include "Qsci/qsciabstractapis.h"
#include "Qsci/qscicommandset.h"
#include "Qsci/qscilexer.h"
#include "Qsci/qscistyle.h"
#include "Qsci/qscistyledtext.h"


// Make sure these match the values in Scintilla.h.  We don't #include that
// file because it just causes more clashes.
#define KEYWORDSET_MAX  8
#define MARKER_MAX      31

#define ScintillaStringData(s)      (s).constData()
#define ScintillaStringLength(s)    (s).size()

// The list separators for auto-completion and user lists.
const char acSeparator = '\x03';
const char userSeparator = '\x04';

// The default fold margin width.
static const int defaultFoldMarginWidth = 14;

// The default set of characters that make up a word.
static const char *defaultWordChars = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";


// The ctor.
QsciScintilla::QsciScintilla(QWidget *parent)
    : QsciScintillaBase(parent),
      allocatedMarkers(0), oldPos(-1), selText(false), fold(NoFoldStyle),
      foldmargin(2), autoInd(false), braceMode(NoBraceMatch),
      acSource(AcsNone), acThresh(-1), wchars(defaultWordChars),
      call_tips_style(CallTipsNoContext), maxCallTips(-1), showSingle(false),
      explicit_fillups(""), fillups_enabled(false)
{
    connect(this,SIGNAL(SCN_MODIFYATTEMPTRO()),
             SIGNAL(modificationAttempted()));

    connect(this,SIGNAL(SCN_MODIFIED(int,int,const char *,int,int,int,int,int,int,int)),
             SLOT(handleModified(int,int,const char *,int,int,int,int,int,int,int)));
    connect(this,SIGNAL(SCN_CALLTIPCLICK(int)),
             SLOT(handleCallTipClick(int)));
    connect(this,SIGNAL(SCN_CHARADDED(int)),
             SLOT(handleCharAdded(int)));
    connect(this,SIGNAL(SCN_MARGINCLICK(int,int,int)),
             SLOT(handleMarginClick(int,int,int)));
    connect(this,SIGNAL(SCN_SAVEPOINTREACHED()),
             SLOT(handleSavePointReached()));
    connect(this,SIGNAL(SCN_SAVEPOINTLEFT()),
             SLOT(handleSavePointLeft()));
    connect(this,SIGNAL(SCN_UPDATEUI()),
             SLOT(handleUpdateUI()));
    connect(this,SIGNAL(QSCN_SELCHANGED(bool)),
             SLOT(handleSelectionChanged(bool)));
    connect(this,SIGNAL(SCN_AUTOCSELECTION(const char *,int)),
             SLOT(handleAutoCompletionSelection()));
    connect(this,SIGNAL(SCN_USERLISTSELECTION(const char *,int)),
             SLOT(handleUserListSelection(const char *,int)));

    // Set the default font.
    setFont(QApplication::font());

    // Set the default fore and background colours.
    QPalette pal = QApplication::palette();
    setColor(pal.text().color());
    setPaper(pal.base().color());

#if defined(Q_OS_WIN)
    setEolMode(EolWindows);
#elif defined(Q_OS_MAC)
    setEolMode(EolMac);
#else
    setEolMode(EolUnix);
#endif

    // Capturing the mouse seems to cause problems on multi-head systems. Qt
    // should do the right thing anyway.
    SendScintilla(SCI_SETMOUSEDOWNCAPTURES, 0UL);

    SendScintilla(SCI_SETPROPERTY, "fold", "1");
    SendScintilla(SCI_SETPROPERTY, "fold.html", "1");

    setMatchedBraceForegroundColor(Qt::blue);
    setUnmatchedBraceForegroundColor(Qt::red);

    setAnnotationDisplay(AnnotationStandard);
    setLexer();

    // Set the visible policy.  These are the same as SciTE's defaults
    // which, presumably, are sensible.
    SendScintilla(SCI_SETVISIBLEPOLICY, VISIBLE_STRICT | VISIBLE_SLOP, 4);

    // Create the standard command set.
    stdCmds = new QsciCommandSet(this);

    doc.display(this,0);
}


// The dtor.
QsciScintilla::~QsciScintilla()
{
    // Detach any current lexer.
    detachLexer();

    doc.undisplay(this);
    delete stdCmds;
}


// Return the current text colour.
QColor QsciScintilla::color() const
{
    return nl_text_colour;
}


// Set the text colour.
void QsciScintilla::setColor(const QColor &c)
{
    if (lex.isNull())
    {
        // Assume style 0 applies to everything so that we don't need to use
        // SCI_STYLECLEARALL which clears everything.
        SendScintilla(SCI_STYLESETFORE, 0, c);
        nl_text_colour = c;
    }
}


// Return the current paper colour.
QColor QsciScintilla::paper() const
{
    return nl_paper_colour;
}


// Set the paper colour.
void QsciScintilla::setPaper(const QColor &c)
{
    if (lex.isNull())
    {
        // Assume style 0 applies to everything so that we don't need to use
        // SCI_STYLECLEARALL which clears everything.  We still have to set the
        // default style as well for the background without any text.
        SendScintilla(SCI_STYLESETBACK, 0, c);
        SendScintilla(SCI_STYLESETBACK, STYLE_DEFAULT, c);
        nl_paper_colour = c;
    }
}


// Set the default font.
void QsciScintilla::setFont(const QFont &f)
{
    if (lex.isNull())
    {
        // Assume style 0 applies to everything so that we don't need to use
        // SCI_STYLECLEARALL which clears everything.
        setStylesFont(f, 0);
        QWidget::setFont(f);
    }
}


// Enable/disable auto-indent.
void QsciScintilla::setAutoIndent(bool autoindent)
{
    autoInd = autoindent;
}


// Set the brace matching mode.
void QsciScintilla::setBraceMatching(BraceMatch bm)
{
    braceMode = bm;
}


// Handle the addition of a character.
void QsciScintilla::handleCharAdded(int ch)
{
    // Ignore if there is a selection.
    long pos = SendScintilla(SCI_GETSELECTIONSTART);

    if (pos != SendScintilla(SCI_GETSELECTIONEND) || pos == 0)
        return;

    // If auto-completion is already active then see if this character is a
    // start character.  If it is then create a new list which will be a subset
    // of the current one.  The case where it isn't a start character seems to
    // be handled correctly elsewhere.
    if (isListActive())
    {
        if (isStartChar(ch))
        {
            cancelList();
            startAutoCompletion(acSource, false, false);
        }

        return;
    }

    // Handle call tips.
    if (call_tips_style != CallTipsNone && !lex.isNull() && strchr("(),", ch) != NULL)
        callTip();

    // Handle auto-indentation.
    if (autoInd)
        if (lex.isNull() || (lex->autoIndentStyle() & AiMaintain))
            maintainIndentation(ch, pos);
        else
            autoIndentation(ch, pos);

    // See if we might want to start auto-completion.
    if (!isCallTipActive() && acSource != AcsNone)
        if (isStartChar(ch))
            startAutoCompletion(acSource, false, false);
        else if (acThresh >= 1 && isWordCharacter(ch))
            startAutoCompletion(acSource, true, false);
}


// See if a call tip is active.
bool QsciScintilla::isCallTipActive() const
{
    return SendScintilla(SCI_CALLTIPACTIVE);
}


// Handle a possible change to any current call tip.
void QsciScintilla::callTip()
{
    QsciAbstractAPIs *apis = lex->apis();

    if (!apis)
        return;

    int pos, commas = 0;
    bool found = false;
    char ch;

    pos = SendScintilla(SCI_GETCURRENTPOS);

    // Move backwards through the line looking for the start of the current
    // call tip and working out which argument it is.
    while ((ch = getCharacter(pos)) != '\0')
    {
        if (ch == ',')
            ++commas;
        else if (ch == ')')
        {
            int depth = 1;

            // Ignore everything back to the start of the corresponding
            // parenthesis.
            while ((ch = getCharacter(pos)) != '\0')
            {
                if (ch == ')')
                    ++depth;
                else if (ch == '(' && --depth == 0)
                    break;
            }
        }
        else if (ch == '(')
        {
            found = true;
            break;
        }
    }

    // Cancel any existing call tip.
    SendScintilla(SCI_CALLTIPCANCEL);

    // Done if there is no new call tip to set.
    if (!found)
        return;

    QStringList context = apiContext(pos, pos, ctPos);

    if (context.isEmpty())
        return;

    // The last word is complete, not partial.
    context << QString();

    ct_cursor = 0;
    ct_shifts.clear();
    ct_entries = apis->callTips(context, commas, call_tips_style, ct_shifts);

    int nr_entries = ct_entries.count();

    if (nr_entries == 0)
        return;

    if (maxCallTips > 0 && maxCallTips < nr_entries)
    {
        ct_entries = ct_entries.mid(0, maxCallTips);
        nr_entries = maxCallTips;
    }

    int shift;
    QString ct;

    int nr_shifts = ct_shifts.count();

    if (maxCallTips < 0 && nr_entries > 1)
    {
        shift = (nr_shifts > 0 ? ct_shifts.first() : 0);
        ct = ct_entries[0];
        ct.prepend('\002');
    }
    else
    {
        if (nr_shifts > nr_entries)
            nr_shifts = nr_entries;

        // Find the biggest shift.
        shift = 0;

        for (int i = 0; i < nr_shifts; ++i)
        {
            int sh = ct_shifts[i];

            if (shift < sh)
                shift = sh;
        }

        ct = ct_entries.join("\n");
    }

    QByteArray ct_ba = ct.toLatin1();
    const char *cts = ct_ba.data();

    SendScintilla(SCI_CALLTIPSHOW, adjustedCallTipPosition(shift), cts);

    // Done if there is more than one call tip.
    if (nr_entries > 1)
        return;

    // Highlight the current argument.
    const char *astart;

    if (commas == 0)
        astart = strchr(cts, '(');
    else
        for (astart = strchr(cts, ','); astart && --commas > 0; astart = strchr(astart + 1, ','))
            ;

    if (!astart || !*++astart)
        return;

    // The end is at the next comma or unmatched closing parenthesis.
    const char *aend;
    int depth = 0;

    for (aend = astart; *aend; ++aend)
    {
        char ch = *aend;

        if (ch == ',' && depth == 0)
            break;
        else if (ch == '(')
            ++depth;
        else if (ch == ')')
        {
            if (depth == 0)
                break;

            --depth;
        }
    }

    if (astart != aend)
        SendScintilla(SCI_CALLTIPSETHLT, astart - cts, aend - cts);
}


// Handle a call tip click.
void QsciScintilla::handleCallTipClick(int dir)
{
    int nr_entries = ct_entries.count();

    // Move the cursor while bounds checking.
    if (dir == 1)
    {
        if (ct_cursor - 1 < 0)
            return;

        --ct_cursor;
    }
    else if (dir == 2)
    {
        if (ct_cursor + 1 >= nr_entries)
            return;

        ++ct_cursor;
    }
    else
        return;

    int shift = (ct_shifts.count() > ct_cursor ? ct_shifts[ct_cursor] : 0);
    QString ct = ct_entries[ct_cursor];

    // Add the arrows.
    if (ct_cursor < nr_entries - 1)
        ct.prepend('\002');

    if (ct_cursor > 0)
        ct.prepend('\001');

    SendScintilla(SCI_CALLTIPSHOW, adjustedCallTipPosition(shift), ct.toLatin1().data());
}


// Shift the position of the call tip (to take any context into account) but
// don't go before the start of the line.
int QsciScintilla::adjustedCallTipPosition(int ctshift) const
{
    int ct = ctPos;

    if (ctshift)
    {
        int ctmin = SendScintilla(SCI_POSITIONFROMLINE, SendScintilla(SCI_LINEFROMPOSITION, ct));

        if (ct - ctshift < ctmin)
            ct = ctmin;
    }

    return ct;
}


// Return the list of words that make up the context preceding the given
// position.  The list will only have more than one element if there is a lexer
// set and it defines start strings.  If so, then the last element might be
// empty if a start string has just been typed.  On return pos is at the start
// of the context.
QStringList QsciScintilla::apiContext(int pos, int &context_start,
        int &last_word_start)
{
    enum {
        Either,
        Separator,
        Word
    };

    QStringList words;
    int good_pos = pos, expecting = Either;

    last_word_start = -1;

    while (pos > 0)
    {
        if (getSeparator(pos))
        {
            if (expecting == Either)
                words.prepend(QString());
            else if (expecting == Word)
                break;

            good_pos = pos;
            expecting = Word;
        }
        else
        {
            QString word = getWord(pos);

            if (word.isEmpty() || expecting == Separator)
                break;

            words.prepend(word);
            good_pos = pos;
            expecting = Separator;

            // Return the position of the start of the last word if required.
            if (last_word_start < 0)
                last_word_start = pos;
        }

        // Strip any preceding spaces (mainly around operators).
        char ch;

        while ((ch = getCharacter(pos)) != '\0')
        {
            // This is the same definition of space that Scintilla uses.
            if (ch != ' ' && (ch < 0x09 || ch > 0x0d))
            {
                ++pos;
                break;
            }
        }
    }

    // A valid sequence always starts with a word and so should be expecting a
    // separator.
    if (expecting != Separator)
        words.clear();

    context_start = good_pos;

    return words;
}


// Try and get a lexer's word separator from the text before the current
// position.  Return true if one was found and adjust the position accordingly.
bool QsciScintilla::getSeparator(int &pos) const
{
    int opos = pos;

    // Go through each separator.
    for (int i = 0; i < wseps.count(); ++i)
    {
        const QString &ws = wseps[i];

        // Work backwards.
        uint l;

        for (l = ws.length(); l; --l)
        {
            char ch = getCharacter(pos);

            if (ch == '\0' || ws.at(l - 1) != ch)
                break;
        }

        if (!l)
            return true;

        // Reset for the next separator.
        pos = opos;
    }

    return false;
}


// Try and get a word from the text before the current position.  Return the
// word if one was found and adjust the position accordingly.
QString QsciScintilla::getWord(int &pos) const
{
    QString word;
    bool numeric = true;
    char ch;

    while ((ch = getCharacter(pos)) != '\0')
    {
        if (!isWordCharacter(ch))
        {
            ++pos;
            break;
        }

        if (ch < '0' || ch > '9')
            numeric = false;

        word.prepend(ch);
    }

    // We don't auto-complete numbers.
    if (numeric)
        word.truncate(0);

    return word;
}


// Get the "next" character (ie. the one before the current position) in the
// current line.  The character will be '\0' if there are no more.
char QsciScintilla::getCharacter(int &pos) const
{
    if (pos <= 0)
        return '\0';

    char ch = SendScintilla(SCI_GETCHARAT, --pos);

    // Don't go past the end of the previous line.
    if (ch == '\n' || ch == '\r')
    {
        ++pos;
        return '\0';
    }

    return ch;
}


// See if a character is an auto-completion start character, ie. the last
// character of a word separator.
bool QsciScintilla::isStartChar(char ch) const
{
    QString s = QChar(ch);

    for (int i = 0; i < wseps.count(); ++i)
        if (wseps[i].endsWith(s))
            return true;

    return false;
}


// Possibly start auto-completion.
void QsciScintilla::startAutoCompletion(AutoCompletionSource acs,
        bool checkThresh, bool single)
{
    int start, ignore;
    QStringList context = apiContext(SendScintilla(SCI_GETCURRENTPOS), start,
            ignore);

    if (context.isEmpty())
        return;

    // Get the last word's raw data and length.
    ScintillaString s = convertTextQ2S(context.last());
    const char *last_data = ScintillaStringData(s);
    int last_len = ScintillaStringLength(s);

    if (checkThresh && last_len < acThresh)
        return;

    // Generate the string representing the valid words to select from.
    QStringList wlist;

    if ((acs == AcsAll || acs == AcsAPIs) && !lex.isNull())
    {
        QsciAbstractAPIs *apis = lex->apis();

        if (apis)
            apis->updateAutoCompletionList(context, wlist);
    }

    if (acs == AcsAll || acs == AcsDocument)
    {
        int sflags = SCFIND_WORDSTART;

        if (!SendScintilla(SCI_AUTOCGETIGNORECASE))
            sflags |= SCFIND_MATCHCASE;

        SendScintilla(SCI_SETSEARCHFLAGS, sflags);

        int pos = 0;
        int dlen = SendScintilla(SCI_GETLENGTH);
        int caret = SendScintilla(SCI_GETCURRENTPOS);
        int clen = caret - start;
        char *orig_context = new char[clen + 1];

        SendScintilla(SCI_GETTEXTRANGE, start, caret, orig_context);

        for (;;)
        {
            int fstart;

            SendScintilla(SCI_SETTARGETSTART, pos);
            SendScintilla(SCI_SETTARGETEND, dlen);

            if ((fstart = SendScintilla(SCI_SEARCHINTARGET, clen, orig_context)) < 0)
                break;

            // Move past the root part.
            pos = fstart + clen;

            // Skip if this is the context we are auto-completing.
            if (pos == caret)
                continue;

            // Get the rest of this word.
            QString w = last_data;

            while (pos < dlen)
            {
                char ch = SendScintilla(SCI_GETCHARAT, pos);

                if (!isWordCharacter(ch))
                    break;

                w += ch;
                ++pos;
            }

            // Add the word if it isn't already there.
            if (!w.isEmpty() && !wlist.contains(w))
                wlist.append(w);
        }

        delete []orig_context;
    }

    if (wlist.isEmpty())
        return;

    wlist.sort();

    SendScintilla(SCI_AUTOCSETCHOOSESINGLE, single);
    SendScintilla(SCI_AUTOCSETSEPARATOR, acSeparator);

    QByteArray chlist = wlist.join(QChar(acSeparator)).toLatin1();
    SendScintilla(SCI_AUTOCSHOW, last_len, chlist.constData());
}


// Maintain the indentation of the previous line.
void QsciScintilla::maintainIndentation(char ch, long pos)
{
    if (ch != '\r' && ch != '\n')
        return;

    int curr_line = SendScintilla(SCI_LINEFROMPOSITION, pos);

    // Get the indentation of the preceding non-zero length line.
    int ind = 0;

    for (int line = curr_line - 1; line >= 0; --line)
    {
        if (SendScintilla(SCI_GETLINEENDPOSITION, line) >
            SendScintilla(SCI_POSITIONFROMLINE, line))
        {
            ind = indentation(line);
            break;
        }
    }

    if (ind > 0)
        autoIndentLine(pos, curr_line, ind);
}


// Implement auto-indentation.
void QsciScintilla::autoIndentation(char ch, long pos)
{
    int curr_line = SendScintilla(SCI_LINEFROMPOSITION, pos);
    int ind_width = indentWidth();
    long curr_line_start = SendScintilla(SCI_POSITIONFROMLINE, curr_line);

    const char *block_start = lex->blockStart();
    bool start_single = (block_start && qstrlen(block_start) == 1);

    const char *block_end = lex->blockEnd();
    bool end_single = (block_end && qstrlen(block_end) == 1);

    if (end_single && block_end[0] == ch)
    {
        if (!(lex->autoIndentStyle() & AiClosing) && rangeIsWhitespace(curr_line_start, pos - 1))
            autoIndentLine(pos, curr_line, blockIndent(curr_line - 1) - ind_width);
    }
    else if (start_single && block_start[0] == ch)
    {
        // De-indent if we have already indented because the previous line was
        // a start of block keyword.
        if (!(lex->autoIndentStyle() & AiOpening) && curr_line > 0 && getIndentState(curr_line - 1) == isKeywordStart && rangeIsWhitespace(curr_line_start, pos - 1))
            autoIndentLine(pos, curr_line, blockIndent(curr_line - 1) - ind_width);
    }
    else if (ch == '\r' || ch == '\n')
        autoIndentLine(pos, curr_line, blockIndent(curr_line - 1));
}


// Set the indentation for a line.
void QsciScintilla::autoIndentLine(long pos, int line, int indent)
{
    if (indent < 0)
        return;

    long pos_before = SendScintilla(SCI_GETLINEINDENTPOSITION, line);
    SendScintilla(SCI_SETLINEINDENTATION, line, indent);
    long pos_after = SendScintilla(SCI_GETLINEINDENTPOSITION, line);
    long new_pos = -1;

    if (pos_after > pos_before)
        new_pos = pos + (pos_after - pos_before);
    else if (pos_after < pos_before && pos >= pos_after)
        if (pos >= pos_before)
            new_pos = pos + (pos_after - pos_before);
        else
            new_pos = pos_after;

    if (new_pos >= 0)
        SendScintilla(SCI_SETSEL, new_pos, new_pos);
}


// Return the indentation of the block defined by the given line (or something
// significant before).
int QsciScintilla::blockIndent(int line)
{
    if (line < 0)
        return 0;

    // Handle the trvial case.
    if (!lex->blockStartKeyword() && !lex->blockStart() && !lex->blockEnd())
        return indentation(line);

    int line_limit = line - lex->blockLookback();

    if (line_limit < 0)
        line_limit = 0;

    for (int l = line; l >= line_limit; --l)
    {
        IndentState istate = getIndentState(l);

        if (istate != isNone)
        {
            int ind_width = indentWidth();
            int ind = indentation(l);

            if (istate == isBlockStart)
            {
                if (!(lex -> autoIndentStyle() & AiOpening))
                    ind += ind_width;
            }
            else if (istate == isBlockEnd)
            {
                if (lex -> autoIndentStyle() & AiClosing)
                    ind -= ind_width;

                if (ind < 0)
                    ind = 0;
            }
            else if (line == l)
                ind += ind_width;

            return ind;
        }
    }

    return indentation(line);
}


// Return true if all characters starting at spos up to, but not including
// epos, are spaces or tabs.
bool QsciScintilla::rangeIsWhitespace(long spos, long epos)
{
    while (spos < epos)
    {
        char ch = SendScintilla(SCI_GETCHARAT, spos);

        if (ch != ' ' && ch != '\t')
            return false;

        ++spos;
    }

    return true;
}


// Returns the indentation state of a line.
QsciScintilla::IndentState QsciScintilla::getIndentState(int line)
{
    IndentState istate;

    // Get the styled text.
    long spos = SendScintilla(SCI_POSITIONFROMLINE, line);
    long epos = SendScintilla(SCI_POSITIONFROMLINE, line + 1);

    char *text = new char[(epos - spos + 1) * 2];

    SendScintilla(SCI_GETSTYLEDTEXT, spos, epos, text);

    int style, bstart_off, bend_off;

    // Block start/end takes precedence over keywords.
    const char *bstart_words = lex->blockStart(&style);
    bstart_off = findStyledWord(text, style, bstart_words);

    const char *bend_words = lex->blockEnd(&style);
    bend_off = findStyledWord(text, style, bend_words);

    // If there is a block start but no block end characters then ignore it
    // unless the block start is the last significant thing on the line, ie.
    // assume Python-like blocking.
    if (bstart_off >= 0 && !bend_words)
        for (int i = bstart_off * 2; text[i] != '\0'; i += 2)
            if (!QChar(text[i]).isSpace())
                return isNone;

    if (bstart_off > bend_off)
        istate = isBlockStart;
    else if (bend_off > bstart_off)
        istate = isBlockEnd;
    else
    {
        const char *words = lex->blockStartKeyword(&style);

        istate = (findStyledWord(text,style,words) >= 0) ? isKeywordStart : isNone;
    }

    delete[] text;

    return istate;
}


// text is a pointer to some styled text (ie. a character byte followed by a
// style byte).  style is a style number.  words is a space separated list of
// words.  Returns the position in the text immediately after the last one of
// the words with the style.  The reason we are after the last, and not the
// first, occurance is that we are looking for words that start and end a block
// where the latest one is the most significant.
int QsciScintilla::findStyledWord(const char *text, int style,
        const char *words)
{
    if (!words)
        return -1;

    // Find the range of text with the style we are looking for.
    const char *stext;

    for (stext = text; stext[1] != style; stext += 2)
        if (stext[0] == '\0')
            return -1;

    // Move to the last character.
    const char *etext = stext;

    while (etext[2] != '\0')
        etext += 2;

    // Backtrack until we find the style.  There will be one.
    while (etext[1] != style)
        etext -= 2;

    // Look for each word in turn.
    while (words[0] != '\0')
    {
        // Find the end of the word.
        const char *eword = words;

        while (eword[1] != ' ' && eword[1] != '\0')
            ++eword;

        // Now search the text backwards.
        const char *wp = eword;

        for (const char *tp = etext; tp >= stext; tp -= 2)
        {
            if (tp[0] != wp[0] || tp[1] != style)
            {
                // Reset the search.
                wp = eword;
                continue;
            }

            // See if all the word has matched.
            if (wp-- == words)
                return ((tp - text) / 2) + (eword - words) + 1;
        }

        // Move to the start of the next word if there is one.
        words = eword + 1;

        if (words[0] == ' ')
            ++words;
    }

    return -1;
}


// Return true if the code page is UTF8.
bool QsciScintilla::isUtf8() const
{
    return (SendScintilla(SCI_GETCODEPAGE) == SC_CP_UTF8);
}


// Set the code page.
void QsciScintilla::setUtf8(bool cp)
{
    setAttribute(Qt::WA_InputMethodEnabled, cp);
    SendScintilla(SCI_SETCODEPAGE, (cp ? SC_CP_UTF8 : 0));
}


// Return the end-of-line mode.
QsciScintilla::EolMode QsciScintilla::eolMode() const
{
    return (EolMode)SendScintilla(SCI_GETEOLMODE);
}


// Set the end-of-line mode.
void QsciScintilla::setEolMode(EolMode mode)
{
    SendScintilla(SCI_SETEOLMODE, mode);
}


// Convert the end-of-lines to a particular mode.
void QsciScintilla::convertEols(EolMode mode)
{
    SendScintilla(SCI_CONVERTEOLS, mode);
}


// Return the edge colour.
QColor QsciScintilla::edgeColor() const
{
    long res = SendScintilla(SCI_GETEDGECOLOUR);

    return QColor((int)res, ((int)(res >> 8)) & 0x00ff, ((int)(res >> 16)) & 0x00ff);
}


// Set the edge colour.
void QsciScintilla::setEdgeColor(const QColor &col)
{
    SendScintilla(SCI_SETEDGECOLOUR, col);
}


// Return the edge column.
int QsciScintilla::edgeColumn() const
{
    return SendScintilla(SCI_GETEDGECOLUMN);
}


// Set the edge column.
void QsciScintilla::setEdgeColumn(int colnr)
{
    SendScintilla(SCI_SETEDGECOLUMN, colnr);
}


// Return the edge mode.
QsciScintilla::EdgeMode QsciScintilla::edgeMode() const
{
    return (EdgeMode)SendScintilla(SCI_GETEDGEMODE);
}


// Set the edge mode.
void QsciScintilla::setEdgeMode(EdgeMode mode)
{
    SendScintilla(SCI_SETEDGEMODE, mode);
}


// Return the end-of-line visibility.
bool QsciScintilla::eolVisibility() const
{
    return SendScintilla(SCI_GETVIEWEOL);
}


// Set the end-of-line visibility.
void QsciScintilla::setEolVisibility(bool visible)
{
    SendScintilla(SCI_SETVIEWEOL, visible);
}


// Return the whitespace visibility.
QsciScintilla::WhitespaceVisibility QsciScintilla::whitespaceVisibility() const
{
    return (WhitespaceVisibility)SendScintilla(SCI_GETVIEWWS);
}


// Set the whitespace visibility.
void QsciScintilla::setWhitespaceVisibility(WhitespaceVisibility mode)
{
    SendScintilla(SCI_SETVIEWWS, mode);
}


// Return the line wrap mode.
QsciScintilla::WrapMode QsciScintilla::wrapMode() const
{
    return (WrapMode)SendScintilla(SCI_GETWRAPMODE);
}


// Set the line wrap mode.
void QsciScintilla::setWrapMode(WrapMode mode)
{
    SendScintilla(SCI_SETLAYOUTCACHE,
            (mode == WrapNone ? SC_CACHE_CARET : SC_CACHE_DOCUMENT));
    SendScintilla(SCI_SETWRAPMODE, mode);
}


// Set the line wrap visual flags.
void QsciScintilla::setWrapVisualFlags(WrapVisualFlag eflag,
        WrapVisualFlag sflag, int sindent)
{
    int flags = SC_WRAPVISUALFLAG_NONE;
    int loc = SC_WRAPVISUALFLAGLOC_DEFAULT;

    if (eflag == WrapFlagByText)
    {
        flags |= SC_WRAPVISUALFLAG_END;
        loc |= SC_WRAPVISUALFLAGLOC_END_BY_TEXT;
    }
    else if (eflag == WrapFlagByBorder)
        flags |= SC_WRAPVISUALFLAG_END;

    if (sflag == WrapFlagByText)
    {
        flags |= SC_WRAPVISUALFLAG_START;
        loc |= SC_WRAPVISUALFLAGLOC_START_BY_TEXT;
    }
    else if (sflag == WrapFlagByBorder)
        flags |= SC_WRAPVISUALFLAG_START;

    SendScintilla(SCI_SETWRAPVISUALFLAGS, flags);
    SendScintilla(SCI_SETWRAPVISUALFLAGSLOCATION, loc);
    SendScintilla(SCI_SETWRAPSTARTINDENT, sindent);
}


// Set the folding style.
void QsciScintilla::setFolding(FoldStyle folding, int margin)
{
    fold = folding;
    foldmargin = margin;

    if (folding == NoFoldStyle)
    {
        SendScintilla(SCI_SETMARGINWIDTHN, margin, 0L);
        return;
    }

    int mask = SendScintilla(SCI_GETMODEVENTMASK);
    SendScintilla(SCI_SETMODEVENTMASK, mask | SC_MOD_CHANGEFOLD);

    SendScintilla(SCI_SETFOLDFLAGS, SC_FOLDFLAG_LINEAFTER_CONTRACTED);

    SendScintilla(SCI_SETMARGINTYPEN, margin, (long)SC_MARGIN_SYMBOL);
    SendScintilla(SCI_SETMARGINMASKN, margin, SC_MASK_FOLDERS);
    SendScintilla(SCI_SETMARGINSENSITIVEN, margin, 1);

    // Set the marker symbols to use.
    switch (folding)
    {
    case PlainFoldStyle:
        setFoldMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_MINUS);
        setFoldMarker(SC_MARKNUM_FOLDER, SC_MARK_PLUS);
        setFoldMarker(SC_MARKNUM_FOLDERSUB);
        setFoldMarker(SC_MARKNUM_FOLDERTAIL);
        setFoldMarker(SC_MARKNUM_FOLDEREND);
        setFoldMarker(SC_MARKNUM_FOLDEROPENMID);
        setFoldMarker(SC_MARKNUM_FOLDERMIDTAIL);
        break;

    case CircledFoldStyle:
        setFoldMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_CIRCLEMINUS);
        setFoldMarker(SC_MARKNUM_FOLDER, SC_MARK_CIRCLEPLUS);
        setFoldMarker(SC_MARKNUM_FOLDERSUB);
        setFoldMarker(SC_MARKNUM_FOLDERTAIL);
        setFoldMarker(SC_MARKNUM_FOLDEREND);
        setFoldMarker(SC_MARKNUM_FOLDEROPENMID);
        setFoldMarker(SC_MARKNUM_FOLDERMIDTAIL);
        break;

    case BoxedFoldStyle:
        setFoldMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
        setFoldMarker(SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
        setFoldMarker(SC_MARKNUM_FOLDERSUB);
        setFoldMarker(SC_MARKNUM_FOLDERTAIL);
        setFoldMarker(SC_MARKNUM_FOLDEREND);
        setFoldMarker(SC_MARKNUM_FOLDEROPENMID);
        setFoldMarker(SC_MARKNUM_FOLDERMIDTAIL);
        break;

    case CircledTreeFoldStyle:
        setFoldMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_CIRCLEMINUS);
        setFoldMarker(SC_MARKNUM_FOLDER, SC_MARK_CIRCLEPLUS);
        setFoldMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
        setFoldMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNERCURVE);
        setFoldMarker(SC_MARKNUM_FOLDEREND, SC_MARK_CIRCLEPLUSCONNECTED);
        setFoldMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_CIRCLEMINUSCONNECTED);
        setFoldMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNERCURVE);
        break;

    case BoxedTreeFoldStyle:
        setFoldMarker(SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
        setFoldMarker(SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
        setFoldMarker(SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
        setFoldMarker(SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
        setFoldMarker(SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
        setFoldMarker(SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
        setFoldMarker(SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);
        break;
    }

    SendScintilla(SCI_SETMARGINWIDTHN, margin, defaultFoldMarginWidth);
}


// Clear all current folds.
void QsciScintilla::clearFolds()
{
    recolor();

    int maxLine = SendScintilla(SCI_GETLINECOUNT);

    for (int line = 0; line < maxLine; line++)
    {
        int level = SendScintilla(SCI_GETFOLDLEVEL, line);

        if (level & SC_FOLDLEVELHEADERFLAG)
        {
            SendScintilla(SCI_SETFOLDEXPANDED, line, 1);
            foldExpand(line, true, false, 0, level);
            line--;
        }
    }
}


// Set up a folder marker.
void QsciScintilla::setFoldMarker(int marknr, int mark)
{
    SendScintilla(SCI_MARKERDEFINE, marknr, mark);

    if (mark != SC_MARK_EMPTY)
    {
        SendScintilla(SCI_MARKERSETFORE, marknr, QColor(Qt::white));
        SendScintilla(SCI_MARKERSETBACK, marknr, QColor(Qt::black));
    }
}


// Handle a click in the fold margin.  This is mostly taken from SciTE.
void QsciScintilla::foldClick(int lineClick, int bstate)
{
    bool shift = bstate & Qt::ShiftModifier;
    bool ctrl = bstate & Qt::ControlModifier;

    if (shift && ctrl)
    {
        foldAll();
        return;
    }

    int levelClick = SendScintilla(SCI_GETFOLDLEVEL, lineClick);

    if (levelClick & SC_FOLDLEVELHEADERFLAG)
    {
        if (shift)
        {
            // Ensure all children are visible.
            SendScintilla(SCI_SETFOLDEXPANDED, lineClick, 1);
            foldExpand(lineClick, true, true, 100, levelClick);
        }
        else if (ctrl)
        {
            if (SendScintilla(SCI_GETFOLDEXPANDED, lineClick))
            {
                // Contract this line and all its children.
                SendScintilla(SCI_SETFOLDEXPANDED, lineClick, 0L);
                foldExpand(lineClick, false, true, 0, levelClick);
            }
            else
            {
                // Expand this line and all its children.
                SendScintilla(SCI_SETFOLDEXPANDED, lineClick, 1);
                foldExpand(lineClick, true, true, 100, levelClick);
            }
        }
        else
        {
            // Toggle this line.
            SendScintilla(SCI_TOGGLEFOLD, lineClick);
        }
    }
}


// Do the hard work of hiding and showing lines.  This is mostly taken from
// SciTE.
void QsciScintilla::foldExpand(int &line, bool doExpand, bool force,
        int visLevels, int level)
{
    int lineMaxSubord = SendScintilla(SCI_GETLASTCHILD, line,
            level & SC_FOLDLEVELNUMBERMASK);

    line++;

    while (line <= lineMaxSubord)
    {
        if (force)
        {
            if (visLevels > 0)
                SendScintilla(SCI_SHOWLINES, line, line);
            else
                SendScintilla(SCI_HIDELINES, line, line);
        }
        else if (doExpand)
            SendScintilla(SCI_SHOWLINES, line, line);

        int levelLine = level;

        if (levelLine == -1)
            levelLine = SendScintilla(SCI_GETFOLDLEVEL, line);

        if (levelLine & SC_FOLDLEVELHEADERFLAG)
        {
            if (force)
            {
                if (visLevels > 1)
                    SendScintilla(SCI_SETFOLDEXPANDED, line, 1);
                else
                    SendScintilla(SCI_SETFOLDEXPANDED, line, 0L);

                foldExpand(line, doExpand, force, visLevels - 1);
            }
            else if (doExpand)
            {
                if (!SendScintilla(SCI_GETFOLDEXPANDED, line))
                    SendScintilla(SCI_SETFOLDEXPANDED, line, 1);

                foldExpand(line, true, force, visLevels - 1);
            }
            else
                foldExpand(line, false, force, visLevels - 1);
        }
        else
            line++;
    }
}


// Fully expand (if there is any line currently folded) all text.  Otherwise,
// fold all text.  This is mostly taken from SciTE.
void QsciScintilla::foldAll(bool children)
{
    recolor();

    int maxLine = SendScintilla(SCI_GETLINECOUNT);
    bool expanding = true;

    for (int lineSeek = 0; lineSeek < maxLine; lineSeek++)
    {
        if (SendScintilla(SCI_GETFOLDLEVEL,lineSeek) & SC_FOLDLEVELHEADERFLAG)
        {
            expanding = !SendScintilla(SCI_GETFOLDEXPANDED, lineSeek);
            break;
        }
    }

    for (int line = 0; line < maxLine; line++)
    {
        int level = SendScintilla(SCI_GETFOLDLEVEL, line);

        if (!(level & SC_FOLDLEVELHEADERFLAG))
            continue;

        if (children ||
            (SC_FOLDLEVELBASE == (level & SC_FOLDLEVELNUMBERMASK)))
        {
            if (expanding)
            {
                SendScintilla(SCI_SETFOLDEXPANDED, line, 1);
                foldExpand(line, true, false, 0, level);
                line--;
            }
            else
            {
                int lineMaxSubord = SendScintilla(SCI_GETLASTCHILD, line, -1);

                SendScintilla(SCI_SETFOLDEXPANDED, line, 0L);

                if (lineMaxSubord > line)
                    SendScintilla(SCI_HIDELINES, line + 1, lineMaxSubord);
            }
        }
    }
}


// Handle a fold change.  This is mostly taken from SciTE.
void QsciScintilla::foldChanged(int line,int levelNow,int levelPrev)
{
    if (levelNow & SC_FOLDLEVELHEADERFLAG)
    {
        if (!(levelPrev & SC_FOLDLEVELHEADERFLAG))
            SendScintilla(SCI_SETFOLDEXPANDED, line, 1);
    }
    else if (levelPrev & SC_FOLDLEVELHEADERFLAG)
    {
        if (!SendScintilla(SCI_GETFOLDEXPANDED, line))
        {
            // Removing the fold from one that has been contracted so should
            // expand.  Otherwise lines are left invisible with no way to make
            // them visible.
            foldExpand(line, true, false, 0, levelPrev);
        }
    }
}


// Toggle the fold for a line if it contains a fold marker.
void QsciScintilla::foldLine(int line)
{
    SendScintilla(SCI_TOGGLEFOLD, line);
}


// Handle the SCN_MODIFIED notification.
void QsciScintilla::handleModified(int pos, int mtype, const char *text,
        int len, int added, int line, int foldNow, int foldPrev, int token,
        int annotationLinesAdded)
{
    if (mtype & SC_MOD_CHANGEFOLD)
    {
        if (fold)
            foldChanged(line, foldNow, foldPrev);
    }

    if (mtype & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
    {
        emit textChanged();

        if (added != 0)
            emit linesChanged();
    }
}


// Zoom in a number of points.
void QsciScintilla::zoomIn(int range)
{
    zoomTo(SendScintilla(SCI_GETZOOM) + range);
}


// Zoom in a single point.
void QsciScintilla::zoomIn()
{
    SendScintilla(SCI_ZOOMIN);
}


// Zoom out a number of points.
void QsciScintilla::zoomOut(int range)
{
    zoomTo(SendScintilla(SCI_GETZOOM) - range);
}


// Zoom out a single point.
void QsciScintilla::zoomOut()
{
    SendScintilla(SCI_ZOOMOUT);
}


// Set the zoom to a number of points.
void QsciScintilla::zoomTo(int size)
{
    if (size < -10)
        size = -10;
    else if (size > 20)
        size = 20;

    SendScintilla(SCI_SETZOOM, size);
}


// Find the first occurrence of a string.
bool QsciScintilla::findFirst(const QString &expr, bool re, bool cs, bool wo,
        bool wrap, bool forward, int line, int index, bool show)
{
    findState.inProgress = false;

    if (expr.isEmpty())
        return false;

    findState.expr = expr;
    findState.wrap = wrap;
    findState.forward = forward;

    findState.flags =
        (cs ? SCFIND_MATCHCASE : 0) |
        (wo ? SCFIND_WHOLEWORD : 0) |
        (re ? SCFIND_REGEXP : 0);

    if (line < 0 || index < 0)
        findState.startpos = SendScintilla(SCI_GETCURRENTPOS);
    else
        findState.startpos = positionFromLineIndex(line, index);

    if (forward)
        findState.endpos = SendScintilla(SCI_GETLENGTH);
    else
        findState.endpos = 0;

    findState.show = show;

    return doFind();
}


// Find the next occurrence of a string.
bool QsciScintilla::findNext()
{
    if (!findState.inProgress)
        return false;

    return doFind();
}


// Do the hard work of findFirst() and findNext().
bool QsciScintilla::doFind()
{
    SendScintilla(SCI_SETSEARCHFLAGS, findState.flags);

    int pos = simpleFind();

    // See if it was found.  If not and wraparound is wanted, try again.
    if (pos == -1 && findState.wrap)
    {
        if (findState.forward)
        {
            findState.startpos = 0;
            findState.endpos = SendScintilla(SCI_GETLENGTH);
        }
        else
        {
            findState.startpos = SendScintilla(SCI_GETLENGTH);
            findState.endpos = 0;
        }

        pos = simpleFind();
    }

    if (pos == -1)
    {
        findState.inProgress = false;
        return false;
    }

    // It was found.
    long targstart = SendScintilla(SCI_GETTARGETSTART);
    long targend = SendScintilla(SCI_GETTARGETEND);

    // Ensure the text found is visible if required.
    if (findState.show)
    {
        int startLine = SendScintilla(SCI_LINEFROMPOSITION, targstart);
        int endLine = SendScintilla(SCI_LINEFROMPOSITION, targend);

        for (int i = startLine; i <= endLine; ++i)
            SendScintilla(SCI_ENSUREVISIBLEENFORCEPOLICY, i);
    }

    // Now set the selection.
    SendScintilla(SCI_SETSEL, targstart, targend);

    // Finally adjust the start position so that we don't find the same one
    // again.
    if (findState.forward)
        findState.startpos = targend;
    else if ((findState.startpos = targstart - 1) < 0)
        findState.startpos = 0;

    findState.inProgress = true;
    return true;
}


// Do a simple find between the start and end positions.
int QsciScintilla::simpleFind()
{
    if (findState.startpos == findState.endpos)
        return -1;

    SendScintilla(SCI_SETTARGETSTART, findState.startpos);
    SendScintilla(SCI_SETTARGETEND, findState.endpos);

    ScintillaString s = convertTextQ2S(findState.expr);

    return SendScintilla(SCI_SEARCHINTARGET, ScintillaStringLength(s),
            ScintillaStringData(s));
}


// Replace the text found with the previous findFirst() or findNext().
void QsciScintilla::replace(const QString &replaceStr)
{
    if (!findState.inProgress)
        return;

    long start = SendScintilla(SCI_GETSELECTIONSTART);

    SendScintilla(SCI_TARGETFROMSELECTION);

    int cmd = (findState.flags & SCFIND_REGEXP) ? SCI_REPLACETARGETRE : SCI_REPLACETARGET;

    ScintillaString s = convertTextQ2S(replaceStr);
    long len = SendScintilla(cmd, -1, ScintillaStringData(s));

    // Reset the selection.
    SendScintilla(SCI_SETSELECTIONSTART, start);
    SendScintilla(SCI_SETSELECTIONEND, start + len);

    if (findState.forward)
        findState.startpos = start + len;
}


// Query the modified state.
bool QsciScintilla::isModified() const
{
    return doc.isModified();
}


// Set the modified state.
void QsciScintilla::setModified(bool m)
{
    if (!m)
        SendScintilla(SCI_SETSAVEPOINT);
}


// Handle the SCN_MARGINCLICK notification.
void QsciScintilla::handleMarginClick(int pos, int modifiers, int margin)
{
    int state = 0;

    if (modifiers & SCMOD_SHIFT)
        state |= Qt::ShiftModifier;

    if (modifiers & SCMOD_CTRL)
        state |= Qt::ControlModifier;

    if (modifiers & SCMOD_ALT)
        state |= Qt::AltModifier;

    int line = SendScintilla(SCI_LINEFROMPOSITION, pos);

    if (fold && margin == foldmargin)
        foldClick(line, state);
    else
        emit marginClicked(margin, line, Qt::KeyboardModifiers(state));
}


// Handle the SCN_SAVEPOINTREACHED notification.
void QsciScintilla::handleSavePointReached()
{
    doc.setModified(false);
    emit modificationChanged(false);
}


// Handle the SCN_SAVEPOINTLEFT notification.
void QsciScintilla::handleSavePointLeft()
{
    doc.setModified(true);
    emit modificationChanged(true);
}


// Handle the QSCN_SELCHANGED signal.
void QsciScintilla::handleSelectionChanged(bool yes)
{
    selText = yes;

    emit copyAvailable(yes);
    emit selectionChanged();
}


// Get the current selection.
void QsciScintilla::getSelection(int *lineFrom, int *indexFrom, int *lineTo,
        int *indexTo) const
{
    if (selText)
    {
        lineIndexFromPosition(SendScintilla(SCI_GETSELECTIONSTART), lineFrom,
                indexFrom);
        lineIndexFromPosition(SendScintilla(SCI_GETSELECTIONEND), lineTo,
                indexTo);
    }
    else
        *lineFrom = *indexFrom = *lineTo = *indexTo = -1;
}


// Sets the current selection.
void QsciScintilla::setSelection(int lineFrom, int indexFrom, int lineTo,
        int indexTo)
{
    SendScintilla(SCI_SETSEL, positionFromLineIndex(lineFrom, indexFrom),
            positionFromLineIndex(lineTo, indexTo));
}


// Set the background colour of selected text.
void QsciScintilla::setSelectionBackgroundColor(const QColor &col)
{
    int alpha = col.alpha();
    
    if (alpha == 255)
        alpha = SC_ALPHA_NOALPHA;

    SendScintilla(SCI_SETSELBACK, 1, col);
    SendScintilla(SCI_SETSELALPHA, alpha);
}


// Set the foreground colour of selected text.
void QsciScintilla::setSelectionForegroundColor(const QColor &col)
{
    SendScintilla(SCI_SETSELFORE, 1, col);
}


// Reset the background colour of selected text to the default.
void QsciScintilla::resetSelectionBackgroundColor()
{
    SendScintilla(SCI_SETSELALPHA, SC_ALPHA_NOALPHA);
    SendScintilla(SCI_SETSELBACK, 0UL);
}


// Reset the foreground colour of selected text to the default.
void QsciScintilla::resetSelectionForegroundColor()
{
    SendScintilla(SCI_SETSELFORE, 0UL);
}


// Set the fill to the end-of-line for the selection.
void QsciScintilla::setSelectionToEol(bool filled)
{
    SendScintilla(SCI_SETSELEOLFILLED, filled);
}


// Return the fill to the end-of-line for the selection.
bool QsciScintilla::selectionToEol() const
{
    return SendScintilla(SCI_GETSELEOLFILLED);
}


// Set the width of the caret.
void QsciScintilla::setCaretWidth(int width)
{
    SendScintilla(SCI_SETCARETWIDTH, width);
}


// Set the foreground colour of the caret.
void QsciScintilla::setCaretForegroundColor(const QColor &col)
{
    SendScintilla(SCI_SETCARETFORE, col);
}


// Set the background colour of the line containing the caret.
void QsciScintilla::setCaretLineBackgroundColor(const QColor &col)
{
    int alpha = col.alpha();

    if (alpha == 255)
        alpha = SC_ALPHA_NOALPHA;

    SendScintilla(SCI_SETCARETLINEBACK, col);
    SendScintilla(SCI_SETCARETLINEBACKALPHA, alpha);
}


// Set the state of the background colour of the line containing the caret.
void QsciScintilla::setCaretLineVisible(bool enable)
{
    SendScintilla(SCI_SETCARETLINEVISIBLE, enable);
}


// Query the read-only state.
bool QsciScintilla::isReadOnly() const
{
    return SendScintilla(SCI_GETREADONLY);
}


// Set the read-only state.
void QsciScintilla::setReadOnly(bool ro)
{
    SendScintilla(SCI_SETREADONLY, ro);
}


// Append the given text.
void QsciScintilla::append(const QString &text)
{
    bool ro = ensureRW();

    ScintillaString s = convertTextQ2S(text);
    SendScintilla(SCI_APPENDTEXT, ScintillaStringLength(s),
            ScintillaStringData(s));

    SendScintilla(SCI_EMPTYUNDOBUFFER);

    setReadOnly(ro);
}


// Insert the given text at the current position.
void QsciScintilla::insert(const QString &text)
{
    insertAtPos(text, -1);
}


// Insert the given text at the given line and offset.
void QsciScintilla::insertAt(const QString &text, int line, int index)
{
    insertAtPos(text, positionFromLineIndex(line, index));
}


// Insert the given text at the given position.
void QsciScintilla::insertAtPos(const QString &text, int pos)
{
    bool ro = ensureRW();

    SendScintilla(SCI_BEGINUNDOACTION);
    SendScintilla(SCI_INSERTTEXT, pos,
            ScintillaStringData(convertTextQ2S(text)));
    SendScintilla(SCI_ENDUNDOACTION);

    setReadOnly(ro);
}


// Begin a sequence of undoable actions.
void QsciScintilla::beginUndoAction()
{
    SendScintilla(SCI_BEGINUNDOACTION);
}


// End a sequence of undoable actions.
void QsciScintilla::endUndoAction()
{
    SendScintilla(SCI_ENDUNDOACTION);
}


// Redo a sequence of actions.
void QsciScintilla::redo()
{
    SendScintilla(SCI_REDO);
}


// Undo a sequence of actions.
void QsciScintilla::undo()
{
    SendScintilla(SCI_UNDO);
}


// See if there is something to redo.
bool QsciScintilla::isRedoAvailable() const
{
    return SendScintilla(SCI_CANREDO);
}


// See if there is something to undo.
bool QsciScintilla::isUndoAvailable() const
{
    return SendScintilla(SCI_CANUNDO);
}


// Return the number of lines.
int QsciScintilla::lines() const
{
    return SendScintilla(SCI_GETLINECOUNT);
}


// Return the line at a position.
int QsciScintilla::lineAt(const QPoint &pos) const
{
    long chpos = SendScintilla(SCI_POSITIONFROMPOINTCLOSE, pos.x(), pos.y());

    if (chpos < 0)
        return -1;

    return SendScintilla(SCI_LINEFROMPOSITION, chpos);
}


// Return the length of a line.
int QsciScintilla::lineLength(int line) const
{
    if (line < 0 || line >= SendScintilla(SCI_GETLINECOUNT))
        return -1;

    return SendScintilla(SCI_LINELENGTH, line);
}


// Return the length of the current text.
int QsciScintilla::length() const
{
    return SendScintilla(SCI_GETTEXTLENGTH);
}


// Remove any selected text.
void QsciScintilla::removeSelectedText()
{
    SendScintilla(SCI_REPLACESEL, "");
}


// Return the current selected text.
QString QsciScintilla::selectedText() const
{
    if (!selText)
        return QString();

    char *buf = new char[SendScintilla(SCI_GETSELECTIONEND) - SendScintilla(SCI_GETSELECTIONSTART) + 1];

    SendScintilla(SCI_GETSELTEXT, buf);

    QString qs = convertTextS2Q(buf);
    delete[] buf;

    return qs;
}


// Return the current text.
QString QsciScintilla::text() const
{
    int buflen = length() + 1;
    char *buf = new char[buflen];

    SendScintilla(SCI_GETTEXT, buflen, buf);

    QString qs = convertTextS2Q(buf);
    delete[] buf;

    return qs;
}


// Return the text of a line.
QString QsciScintilla::text(int line) const
{
    int line_len = lineLength(line);

    if (line_len < 1)
        return QString();

    char *buf = new char[line_len + 1];

    SendScintilla(SCI_GETLINE, line, buf);
    buf[line_len] = '\0';

    QString qs = convertTextS2Q(buf);
    delete[] buf;

    return qs;
}


// Set the given text.
void QsciScintilla::setText(const QString &text)
{
    bool ro = ensureRW();

    SendScintilla(SCI_SETTEXT, ScintillaStringData(convertTextQ2S(text)));
    SendScintilla(SCI_EMPTYUNDOBUFFER);

    setReadOnly(ro);
}


// Get the cursor position
void QsciScintilla::getCursorPosition(int *line, int *index) const
{
    lineIndexFromPosition(SendScintilla(SCI_GETCURRENTPOS), line, index);
}


// Set the cursor position
void QsciScintilla::setCursorPosition(int line, int index)
{
    SendScintilla(SCI_GOTOPOS, positionFromLineIndex(line, index));
}


// Ensure the cursor is visible.
void QsciScintilla::ensureCursorVisible()
{
    SendScintilla(SCI_SCROLLCARET);
}


// Ensure a line is visible.
void QsciScintilla::ensureLineVisible(int line)
{
    SendScintilla(SCI_ENSUREVISIBLEENFORCEPOLICY, line);
}


// Copy text to the clipboard.
void QsciScintilla::copy()
{
    SendScintilla(SCI_COPY);
}


// Cut text to the clipboard.
void QsciScintilla::cut()
{
    SendScintilla(SCI_CUT);
}


// Paste text from the clipboard.
void QsciScintilla::paste()
{
    SendScintilla(SCI_PASTE);
}


// Select all text, or deselect any selected text.
void QsciScintilla::selectAll(bool select)
{
    if (select)
        SendScintilla(SCI_SELECTALL);
    else
        SendScintilla(SCI_SETANCHOR, SendScintilla(SCI_GETCURRENTPOS));
}


// Delete all text.
void QsciScintilla::clear()
{
    bool ro = ensureRW();

    SendScintilla(SCI_BEGINUNDOACTION);
    SendScintilla(SCI_CLEARALL);
    SendScintilla(SCI_ENDUNDOACTION);

    setReadOnly(ro);
}


// Return the indentation of a line.
int QsciScintilla::indentation(int line) const
{
    return SendScintilla(SCI_GETLINEINDENTATION, line);
}


// Set the indentation of a line.
void QsciScintilla::setIndentation(int line, int indentation)
{
    SendScintilla(SCI_BEGINUNDOACTION);
    SendScintilla(SCI_SETLINEINDENTATION, line, indentation);
    SendScintilla(SCI_ENDUNDOACTION);
}


// Indent a line.
void QsciScintilla::indent(int line)
{
    setIndentation(line, indentation(line) + indentWidth());
}


// Unindent a line.
void QsciScintilla::unindent(int line)
{
    int newIndent = indentation(line) - indentWidth();

    if (newIndent < 0)
        newIndent = 0;

    setIndentation(line, newIndent);
}


// Return the indentation of the current line.
int QsciScintilla::currentIndent() const
{
    return indentation(SendScintilla(SCI_LINEFROMPOSITION,
                SendScintilla(SCI_GETCURRENTPOS)));
}


// Return the current indentation width.
int QsciScintilla::indentWidth() const
{
    int w = indentationWidth();

    if (w == 0)
        w = tabWidth();

    return w;
}


// Return the state of indentation guides.
bool QsciScintilla::indentationGuides() const
{
    return (SendScintilla(SCI_GETINDENTATIONGUIDES) != SC_IV_NONE);
}


// Enable and disable indentation guides.
void QsciScintilla::setIndentationGuides(bool enable)
{
    int iv;

    if (!enable)
        iv = SC_IV_NONE;
    else if (lex.isNull())
        iv = SC_IV_REAL;
    else
        iv = lex->indentationGuideView();

    SendScintilla(SCI_SETINDENTATIONGUIDES, iv);
}


// Set the background colour of indentation guides.
void QsciScintilla::setIndentationGuidesBackgroundColor(const QColor &col)
{
    SendScintilla(SCI_STYLESETBACK, STYLE_INDENTGUIDE, col);
}


// Set the foreground colour of indentation guides.
void QsciScintilla::setIndentationGuidesForegroundColor(const QColor &col)
{
    SendScintilla(SCI_STYLESETFORE, STYLE_INDENTGUIDE, col);
}


// Return the indentation width.
int QsciScintilla::indentationWidth() const
{
    return SendScintilla(SCI_GETINDENT);
}


// Set the indentation width.
void QsciScintilla::setIndentationWidth(int width)
{
    SendScintilla(SCI_SETINDENT, width);
}


// Return the tab width.
int QsciScintilla::tabWidth() const
{
    return SendScintilla(SCI_GETTABWIDTH);
}


// Set the tab width.
void QsciScintilla::setTabWidth(int width)
{
    SendScintilla(SCI_SETTABWIDTH, width);
}


// Return the effect of the backspace key.
bool QsciScintilla::backspaceUnindents() const
{
    return SendScintilla(SCI_GETBACKSPACEUNINDENTS);
}


// Set the effect of the backspace key.
void QsciScintilla::setBackspaceUnindents(bool unindents)
{
    SendScintilla(SCI_SETBACKSPACEUNINDENTS, unindents);
}


// Return the effect of the tab key.
bool QsciScintilla::tabIndents() const
{
    return SendScintilla(SCI_GETTABINDENTS);
}


// Set the effect of the tab key.
void QsciScintilla::setTabIndents(bool indents)
{
    SendScintilla(SCI_SETTABINDENTS, indents);
}


// Return the indentation use of tabs.
bool QsciScintilla::indentationsUseTabs() const
{
    return SendScintilla(SCI_GETUSETABS);
}


// Set the indentation use of tabs.
void QsciScintilla::setIndentationsUseTabs(bool tabs)
{
    SendScintilla(SCI_SETUSETABS, tabs);
}


// Return the margin type.
QsciScintilla::MarginType QsciScintilla::marginType(int margin) const
{
    return (MarginType)SendScintilla(SCI_GETMARGINTYPEN, margin);
}


// Set the margin type.
void QsciScintilla::setMarginType(int margin, QsciScintilla::MarginType type)
{
    SendScintilla(SCI_SETMARGINTYPEN, margin, type);
}


// Clear margin text.
void QsciScintilla::clearMarginText(int line)
{
    if (line < 0)
        SendScintilla(SCI_MARGINSETTEXT, line, (const char *)0);
    else
        SendScintilla(SCI_MARGINTEXTCLEARALL);
}


// Annotate a line.
void QsciScintilla::setMarginText(int line, const QString &text, int style)
{
    int style_offset = SendScintilla(SCI_MARGINGETSTYLEOFFSET);

    SendScintilla(SCI_MARGINSETTEXT, line,
            ScintillaStringData(convertTextQ2S(text)));

    SendScintilla(SCI_MARGINSETSTYLE, line, style - style_offset);
}


// Annotate a line.
void QsciScintilla::setMarginText(int line, const QString &text, const QsciStyle &style)
{
    setMarginText(line, text, style.style());
}


// Annotate a line.
void QsciScintilla::setMarginText(int line, const QsciStyledText &text)
{
    setMarginText(line, text.text(), text.style());
}


// Annotate a line.
void QsciScintilla::setMarginText(int line, const QList<QsciStyledText> &text)
{
    char *styles;
    ScintillaString styled_text = styleText(text, &styles,
            SendScintilla(SCI_MARGINGETSTYLEOFFSET));

    SendScintilla(SCI_MARGINSETTEXT, line, ScintillaStringData(styled_text));
    SendScintilla(SCI_MARGINSETSTYLES, line, styles);

    delete[] styles;
}


// Return the state of line numbers in a margin.
bool QsciScintilla::marginLineNumbers(int margin) const
{
    return SendScintilla(SCI_GETMARGINTYPEN, margin);
}


// Enable and disable line numbers in a margin.
void QsciScintilla::setMarginLineNumbers(int margin, bool lnrs)
{
    SendScintilla(SCI_SETMARGINTYPEN, margin,
            lnrs ? SC_MARGIN_NUMBER : SC_MARGIN_SYMBOL);
}


// Return the marker mask of a margin.
int QsciScintilla::marginMarkerMask(int margin) const
{
    return SendScintilla(SCI_GETMARGINMASKN, margin);
}


// Set the marker mask of a margin.
void QsciScintilla::setMarginMarkerMask(int margin,int mask)
{
    SendScintilla(SCI_SETMARGINMASKN, margin, mask);
}


// Return the state of a margin's sensitivity.
bool QsciScintilla::marginSensitivity(int margin) const
{
    return SendScintilla(SCI_GETMARGINSENSITIVEN, margin);
}


// Enable and disable a margin's sensitivity.
void QsciScintilla::setMarginSensitivity(int margin,bool sens)
{
    SendScintilla(SCI_SETMARGINSENSITIVEN, margin, sens);
}


// Return the width of a margin.
int QsciScintilla::marginWidth(int margin) const
{
    return SendScintilla(SCI_GETMARGINWIDTHN, margin);
}


// Set the width of a margin.
void QsciScintilla::setMarginWidth(int margin, int width)
{
    SendScintilla(SCI_SETMARGINWIDTHN, margin, width);
}


// Set the width of a margin to the width of some text.
void QsciScintilla::setMarginWidth(int margin, const QString &s)
{
    int width = SendScintilla(SCI_TEXTWIDTH, STYLE_LINENUMBER,
            ScintillaStringData(convertTextQ2S(s)));

    setMarginWidth(margin, width);
}


// Set the background colour of all margins.
void QsciScintilla::setMarginsBackgroundColor(const QColor &col)
{
    handleStylePaperChange(col, STYLE_LINENUMBER);
}


// Set the foreground colour of all margins.
void QsciScintilla::setMarginsForegroundColor(const QColor &col)
{
    handleStyleColorChange(col, STYLE_LINENUMBER);
}


// Set the font of all margins.
void QsciScintilla::setMarginsFont(const QFont &f)
{
    setStylesFont(f, STYLE_LINENUMBER);
}


// Define a marker based on a symbol.
int QsciScintilla::markerDefine(MarkerSymbol sym, int mnr)
{
    checkMarker(mnr);

    if (mnr >= 0)
        SendScintilla(SCI_MARKERDEFINE, mnr, static_cast<long>(sym));

    return mnr;
}


// Define a marker based on a character.
int QsciScintilla::markerDefine(char ch, int mnr)
{
    checkMarker(mnr);

    if (mnr >= 0)
        SendScintilla(SCI_MARKERDEFINE, mnr,
                static_cast<long>(SC_MARK_CHARACTER) + ch);

    return mnr;
}


// Define a marker based on a QPixmap.
int QsciScintilla::markerDefine(const QPixmap &pm, int mnr)
{
    checkMarker(mnr);

    if (mnr >= 0)
        SendScintilla(SCI_MARKERDEFINEPIXMAP, mnr, pm);

    return mnr;
}


// Add a marker to a line.
int QsciScintilla::markerAdd(int linenr, int mnr)
{
    if (mnr < 0 || mnr > MARKER_MAX || (allocatedMarkers & (1 << mnr)) == 0)
        return -1;

    return SendScintilla(SCI_MARKERADD, linenr, mnr);
}


// Get the marker mask for a line.
unsigned QsciScintilla::markersAtLine(int linenr) const
{
    return SendScintilla(SCI_MARKERGET, linenr);
}


// Delete a marker from a line.
void QsciScintilla::markerDelete(int linenr, int mnr)
{
    if (mnr <= MARKER_MAX)
    {
        if (mnr < 0)
        {
            unsigned am = allocatedMarkers;

            for (int m = 0; m <= MARKER_MAX; ++m)
            {
                if (am & 1)
                    SendScintilla(SCI_MARKERDELETE, linenr, m);

                am >>= 1;
            }
        }
        else if (allocatedMarkers & (1 << mnr))
            SendScintilla(SCI_MARKERDELETE, linenr, mnr);
    }
}


// Delete a marker from the text.
void QsciScintilla::markerDeleteAll(int mnr)
{
    if (mnr <= MARKER_MAX)
    {
        if (mnr < 0)
            SendScintilla(SCI_MARKERDELETEALL, -1);
        else if (allocatedMarkers & (1 << mnr))
            SendScintilla(SCI_MARKERDELETEALL, mnr);
    }
}


// Delete a marker handle from the text.
void QsciScintilla::markerDeleteHandle(int mhandle)
{
    SendScintilla(SCI_MARKERDELETEHANDLE, mhandle);
}


// Return the line containing a marker instance.
int QsciScintilla::markerLine(int mhandle) const
{
    return SendScintilla(SCI_MARKERLINEFROMHANDLE, mhandle);
}


// Search forwards for a marker.
int QsciScintilla::markerFindNext(int linenr, unsigned mask) const
{
    return SendScintilla(SCI_MARKERNEXT, linenr, mask);
}


// Search backwards for a marker.
int QsciScintilla::markerFindPrevious(int linenr, unsigned mask) const
{
    return SendScintilla(SCI_MARKERPREVIOUS, linenr, mask);
}


// Set the marker background colour.
void QsciScintilla::setMarkerBackgroundColor(const QColor &col, int mnr)
{
    if (mnr <= MARKER_MAX)
    {
        int alpha = col.alpha();

        // An opaque background would make the text invisible.
        if (alpha == 255)
            alpha = SC_ALPHA_NOALPHA;

        if (mnr < 0)
        {
            unsigned am = allocatedMarkers;

            for (int m = 0; m <= MARKER_MAX; ++m)
            {
                if (am & 1)
                {
                    SendScintilla(SCI_MARKERSETBACK, m, col);
                    SendScintilla(SCI_MARKERSETALPHA, m, alpha);
                }

                am >>= 1;
            }
        }
        else if (allocatedMarkers & (1 << mnr))
        {
            SendScintilla(SCI_MARKERSETBACK, mnr, col);
            SendScintilla(SCI_MARKERSETALPHA, mnr, alpha);
        }
    }
}


// Set the marker foreground colour.
void QsciScintilla::setMarkerForegroundColor(const QColor &col, int mnr)
{
    if (mnr <= MARKER_MAX)
    {
        if (mnr < 0)
        {
            unsigned am = allocatedMarkers;

            for (int m = 0; m <= MARKER_MAX; ++m)
            {
                if (am & 1)
                    SendScintilla(SCI_MARKERSETFORE, m, col);

                am >>= 1;
            }
        }
        else if (allocatedMarkers & (1 << mnr))
        {
            SendScintilla(SCI_MARKERSETFORE, mnr, col);
        }
    }
}


// Check a marker, allocating a marker number if necessary.
void QsciScintilla::checkMarker(int &mnr)
{
    if (mnr >= 0)
    {
        // Note that we allow existing markers to be explicitly redefined.
        if (mnr > MARKER_MAX)
            mnr = -1;
    }
    else
    {
        unsigned am = allocatedMarkers;

        // Find the smallest unallocated marker number.
        for (mnr = 0; mnr <= MARKER_MAX; ++mnr)
        {
            if ((am & 1) == 0)
                break;

            am >>= 1;
        }
    }

    // Define the marker if it is valid.
    if (mnr >= 0)
        allocatedMarkers |= (1 << mnr);
}


// Reset the fold margin colours.
void QsciScintilla::resetFoldMarginColors()
{
    SendScintilla(SCI_SETFOLDMARGINHICOLOUR, 0, 0L);
    SendScintilla(SCI_SETFOLDMARGINCOLOUR, 0, 0L);
}


// Set the fold margin colours.
void QsciScintilla::setFoldMarginColors(const QColor &fore, const QColor &back)
{
    SendScintilla(SCI_SETFOLDMARGINHICOLOUR, 1, fore);
    SendScintilla(SCI_SETFOLDMARGINCOLOUR, 1, back);
}


// Set the call tips background colour.
void QsciScintilla::setCallTipsBackgroundColor(const QColor &col)
{
    SendScintilla(SCI_CALLTIPSETBACK, col);
}


// Set the call tips foreground colour.
void QsciScintilla::setCallTipsForegroundColor(const QColor &col)
{
    SendScintilla(SCI_CALLTIPSETFORE, col);
}


// Set the call tips highlight colour.
void QsciScintilla::setCallTipsHighlightColor(const QColor &col)
{
    SendScintilla(SCI_CALLTIPSETFOREHLT, col);
}


// Set the matched brace background colour.
void QsciScintilla::setMatchedBraceBackgroundColor(const QColor &col)
{
    SendScintilla(SCI_STYLESETBACK, STYLE_BRACELIGHT, col);
}


// Set the matched brace foreground colour.
void QsciScintilla::setMatchedBraceForegroundColor(const QColor &col)
{
    SendScintilla(SCI_STYLESETFORE, STYLE_BRACELIGHT, col);
}


// Set the unmatched brace background colour.
void QsciScintilla::setUnmatchedBraceBackgroundColor(const QColor &col)
{
    SendScintilla(SCI_STYLESETBACK, STYLE_BRACEBAD, col);
}


// Set the unmatched brace foreground colour.
void QsciScintilla::setUnmatchedBraceForegroundColor(const QColor &col)
{
    SendScintilla(SCI_STYLESETFORE, STYLE_BRACEBAD, col);
}


// Detach any lexer.
void QsciScintilla::detachLexer()
{
    if (!lex.isNull())
    {
        lex->setEditor(0);
        lex->disconnect(this);

        SendScintilla(SCI_STYLERESETDEFAULT);
        SendScintilla(SCI_STYLECLEARALL);
        SendScintilla(SCI_CLEARDOCUMENTSTYLE);
    }
}


// Set the lexer.
void QsciScintilla::setLexer(QsciLexer *lexer)
{
    // Detach any current lexer.
    detachLexer();

    // Connect up the new lexer.
    lex = lexer;

    if (lex)
    {
        if (lex->lexer())
            SendScintilla(SCI_SETLEXERLANGUAGE, lex->lexer());
        else
            SendScintilla(SCI_SETLEXER, lex->lexerId());

        lex->setEditor(this);

        connect(lex,SIGNAL(colorChanged(const QColor &, int)),
                SLOT(handleStyleColorChange(const QColor &, int)));
        connect(lex,SIGNAL(eolFillChanged(bool, int)),
                SLOT(handleStyleEolFillChange(bool, int)));
        connect(lex,SIGNAL(fontChanged(const QFont &, int)),
                SLOT(handleStyleFontChange(const QFont &, int)));
        connect(lex,SIGNAL(paperChanged(const QColor &, int)),
                SLOT(handleStylePaperChange(const QColor &, int)));
        connect(lex,SIGNAL(propertyChanged(const char *, const char *)),
                SLOT(handlePropertyChange(const char *, const char *)));

        // Set the keywords.  Scintilla allows for sets numbered 0 to
        // KEYWORDSET_MAX (although the lexers only seem to exploit 0 to
        // KEYWORDSET_MAX - 1).  We number from 1 in line with SciTE's property
        // files.
        for (int k = 0; k <= KEYWORDSET_MAX; ++k)
        {
            const char *kw = lex -> keywords(k + 1);

            if (!kw)
                kw = "";

            SendScintilla(SCI_SETKEYWORDS, k, kw);
        }

        // Initialise each style.  Do the default first so its (possibly
        // incorrect) font setting gets reset when style 0 is set.
        setLexerStyle(STYLE_DEFAULT);

        int nrStyles = 1 << SendScintilla(SCI_GETSTYLEBITS);

        for (int s = 0; s < nrStyles; ++s)
            if (!lex->description(s).isNull())
                setLexerStyle(s);

        // Initialise the properties.
        lex->refreshProperties();

        // Set the auto-completion fillups and word separators.
        setAutoCompletionFillupsEnabled(fillups_enabled);
        wseps = lex->autoCompletionWordSeparators();

        wchars = lex->wordCharacters();

        if (!wchars)
            wchars = defaultWordChars;

        SendScintilla(SCI_AUTOCSETIGNORECASE, !lex->caseSensitive());

        recolor();
    }
    else
    {
        SendScintilla(SCI_SETLEXER, SCLEX_CONTAINER);

        setColor(nl_text_colour);
        setPaper(nl_paper_colour);

        SendScintilla(SCI_AUTOCSETFILLUPS, "");
        SendScintilla(SCI_AUTOCSETIGNORECASE, false);
        wseps.clear();
        wchars = defaultWordChars;
    }
}


// Set a particular style of the current lexer.
void QsciScintilla::setLexerStyle(int style)
{
    handleStyleColorChange(lex->color(style), style);
    handleStyleEolFillChange(lex->eolFill(style), style);
    handleStyleFontChange(lex->font(style), style);
    handleStylePaperChange(lex->paper(style), style);
}


// Get the current lexer.
QsciLexer *QsciScintilla::lexer() const
{
    return lex;
}


// Handle a change in lexer style foreground colour.
void QsciScintilla::handleStyleColorChange(const QColor &c, int style)
{
    SendScintilla(SCI_STYLESETFORE, style, c);
}


// Handle a change in lexer style end-of-line fill.
void QsciScintilla::handleStyleEolFillChange(bool eolfill, int style)
{
    SendScintilla(SCI_STYLESETEOLFILLED, style, eolfill);
}


// Handle a change in lexer style font.
void QsciScintilla::handleStyleFontChange(const QFont &f, int style)
{
    setStylesFont(f, style);

    if (style == lex->braceStyle())
    {
        setStylesFont(f, STYLE_BRACELIGHT);
        setStylesFont(f, STYLE_BRACEBAD);
    }
}


// Set the font for a style.
void QsciScintilla::setStylesFont(const QFont &f, int style)
{
    SendScintilla(SCI_STYLESETFONT, style, f.family().toAscii().data());
    SendScintilla(SCI_STYLESETSIZE, style, f.pointSize());
    SendScintilla(SCI_STYLESETBOLD, style, f.bold());
    SendScintilla(SCI_STYLESETITALIC, style, f.italic());
    SendScintilla(SCI_STYLESETUNDERLINE, style, f.underline());

    // Tie the font settings of the default style to that of style 0 (the style
    // conventionally used for whitespace by lexers).  This is needed so that
    // fold marks, indentations, edge columns etc are set properly.
    if (style == 0)
        setStylesFont(f, STYLE_DEFAULT);
}


// Handle a change in lexer style background colour.
void QsciScintilla::handleStylePaperChange(const QColor &c, int style)
{
    SendScintilla(SCI_STYLESETBACK, style, c);
}


// Handle a change in lexer property.
void QsciScintilla::handlePropertyChange(const char *prop, const char *val)
{
    SendScintilla(SCI_SETPROPERTY, prop, val);
}


// Handle a change to the user visible user interface.
void QsciScintilla::handleUpdateUI()
{
    int newPos = SendScintilla(SCI_GETCURRENTPOS);

    if (newPos != oldPos)
    {
        oldPos = newPos;

        int line = SendScintilla(SCI_LINEFROMPOSITION, newPos);
        int col = SendScintilla(SCI_GETCOLUMN, newPos);

        emit cursorPositionChanged(line, col);
    }

    if (braceMode != NoBraceMatch)
        braceMatch();
}


// Handle brace matching.
void QsciScintilla::braceMatch()
{
    long braceAtCaret, braceOpposite;

    findMatchingBrace(braceAtCaret, braceOpposite, braceMode);

    if (braceAtCaret >= 0 && braceOpposite < 0)
    {
        SendScintilla(SCI_BRACEBADLIGHT, braceAtCaret);
        SendScintilla(SCI_SETHIGHLIGHTGUIDE, 0UL);
    }
    else
    {
        char chBrace = SendScintilla(SCI_GETCHARAT, braceAtCaret);

        SendScintilla(SCI_BRACEHIGHLIGHT, braceAtCaret, braceOpposite);

        long columnAtCaret = SendScintilla(SCI_GETCOLUMN, braceAtCaret);
        long columnOpposite = SendScintilla(SCI_GETCOLUMN, braceOpposite);

        if (chBrace == ':')
        {
            long lineStart = SendScintilla(SCI_LINEFROMPOSITION, braceAtCaret);
            long indentPos = SendScintilla(SCI_GETLINEINDENTPOSITION,
                    lineStart);
            long indentPosNext = SendScintilla(SCI_GETLINEINDENTPOSITION,
                    lineStart + 1);

            columnAtCaret = SendScintilla(SCI_GETCOLUMN, indentPos);

            long columnAtCaretNext = SendScintilla(SCI_GETCOLUMN,
                    indentPosNext);
            long indentSize = SendScintilla(SCI_GETINDENT);

            if (columnAtCaretNext - indentSize > 1)
                columnAtCaret = columnAtCaretNext - indentSize;

            if (columnOpposite == 0)
                columnOpposite = columnAtCaret;
        }

        long column = columnAtCaret;

        if (column > columnOpposite)
            column = columnOpposite;

        SendScintilla(SCI_SETHIGHLIGHTGUIDE, column);
    }
}


// Check if the character at a position is a brace.
long QsciScintilla::checkBrace(long pos, int brace_style, bool &colonMode)
{
    long brace_pos = -1;
    char ch = SendScintilla(SCI_GETCHARAT, pos);

    if (ch == ':')
    {
        // A bit of a hack, we should really use a virtual.
        if (!lex.isNull() && qstrcmp(lex->lexer(), "python") == 0)
        {
            brace_pos = pos;
            colonMode = true;
        }
    }
    else if (ch && strchr("[](){}<>", ch))
    {
        if (brace_style < 0)
            brace_pos = pos;
        else
        {
            int style = SendScintilla(SCI_GETSTYLEAT, pos) & 0x1f;

            if (style == brace_style)
                brace_pos = pos;
        }
    }

    return brace_pos;
}


// Find a brace and it's match.  Return true if the current position is inside
// a pair of braces.
bool QsciScintilla::findMatchingBrace(long &brace, long &other,BraceMatch mode)
{
    bool colonMode = false;
    int brace_style = (lex.isNull() ? -1 : lex->braceStyle());

    brace = -1;
    other = -1;

    long caretPos = SendScintilla(SCI_GETCURRENTPOS);

    if (caretPos > 0)
        brace = checkBrace(caretPos - 1, brace_style, colonMode);

    bool isInside = false;

    if (brace < 0 && mode == SloppyBraceMatch)
    {
        brace = checkBrace(caretPos, brace_style, colonMode);

        if (brace >= 0 && !colonMode)
            isInside = true;
    }

    if (brace >= 0)
    {
        if (colonMode)
        {
            // Find the end of the Python indented block.
            long lineStart = SendScintilla(SCI_LINEFROMPOSITION, brace);
            long lineMaxSubord = SendScintilla(SCI_GETLASTCHILD, lineStart, -1);

            other = SendScintilla(SCI_GETLINEENDPOSITION, lineMaxSubord);
        }
        else
            other = SendScintilla(SCI_BRACEMATCH, brace);

        if (other > brace)
            isInside = !isInside;
    }

    return isInside;
}


// Move to the matching brace.
void QsciScintilla::moveToMatchingBrace()
{
    gotoMatchingBrace(false);
}


// Select to the matching brace.
void QsciScintilla::selectToMatchingBrace()
{
    gotoMatchingBrace(true);
}


// Move to the matching brace and optionally select the text.
void QsciScintilla::gotoMatchingBrace(bool select)
{
    long braceAtCaret;
    long braceOpposite;

    bool isInside = findMatchingBrace(braceAtCaret, braceOpposite,
            SloppyBraceMatch);

    if (braceOpposite >= 0)
    {
        // Convert the character positions into caret positions based on
        // whether the caret position was inside or outside the braces.
        if (isInside)
        {
            if (braceOpposite > braceAtCaret)
                braceAtCaret++;
            else
                braceOpposite++;
        }
        else
        {
            if (braceOpposite > braceAtCaret)
                braceOpposite++;
            else
                braceAtCaret++;
        }

        ensureLineVisible(SendScintilla(SCI_LINEFROMPOSITION, braceOpposite));

        if (select)
            SendScintilla(SCI_SETSEL, braceAtCaret, braceOpposite);
        else
            SendScintilla(SCI_SETSEL, braceOpposite, braceOpposite);
    }
}


// Return a position from a line number and an index within the line.
int QsciScintilla::positionFromLineIndex(int line, int index) const
{
    int pos = SendScintilla(SCI_POSITIONFROMLINE, line);

    // Allow for multi-byte characters.
    for(int i = 0; i < index; i++)
        pos = SendScintilla(SCI_POSITIONAFTER, pos);

    return pos;
}


// Return a line number and an index within the line from a position.
void QsciScintilla::lineIndexFromPosition(int position, int *line, int *index) const
{
    int lin = SendScintilla(SCI_LINEFROMPOSITION, position);
    int linpos = SendScintilla(SCI_POSITIONFROMLINE, lin);
    int indx = 0;

    // Allow for multi-byte characters.
    while (linpos < position)
    {
        int new_linpos = SendScintilla(SCI_POSITIONAFTER, linpos);

        // If the position hasn't moved then we must be at the end of the text
        // (which implies that the position passed was beyond the end of the
        // text).
        if (new_linpos == linpos)
            break;

        linpos = new_linpos;
        ++indx;
    }

    *line = lin;
    *index = indx;
}


// Convert a Scintilla string to a Qt Unicode string.
QString QsciScintilla::convertTextS2Q(const char *s) const
{
    if (isUtf8())
        return QString::fromUtf8(s);

    return QString::fromLatin1(s);
}


// Convert a Qt Unicode string to a Scintilla string.
QsciScintilla::ScintillaString QsciScintilla::convertTextQ2S(const QString &q) const
{
    if (isUtf8())
        return q.toUtf8();

    return q.toLatin1();
}


// Set the source of the automatic auto-completion list.
void QsciScintilla::setAutoCompletionSource(AutoCompletionSource source)
{
    acSource = source;
}


// Set the threshold for automatic auto-completion.
void QsciScintilla::setAutoCompletionThreshold(int thresh)
{
    acThresh = thresh;
}


// Set the auto-completion word separators if there is no current lexer.
void QsciScintilla::setAutoCompletionWordSeparators(const QStringList &separators)
{
    if (lex.isNull())
        wseps = separators;
}


// Explicitly auto-complete from all sources.
void QsciScintilla::autoCompleteFromAll()
{
    startAutoCompletion(AcsAll, false, showSingle);
}


// Explicitly auto-complete from the APIs.
void QsciScintilla::autoCompleteFromAPIs()
{
    startAutoCompletion(AcsAPIs, false, showSingle);
}


// Explicitly auto-complete from the document.
void QsciScintilla::autoCompleteFromDocument()
{
    startAutoCompletion(AcsDocument, false, showSingle);
}


// Check if a character can be in a word.
bool QsciScintilla::isWordCharacter(char ch) const
{
    return (strchr(wchars, ch) != NULL);
}


// Return the set of valid word characters.
const char *QsciScintilla::wordCharacters() const
{
    return wchars;
}


// Recolour the document.
void QsciScintilla::recolor(int start, int end)
{
    SendScintilla(SCI_COLOURISE, start, end);
}


// Registered an image.
void QsciScintilla::registerImage(int id, const QPixmap &pm)
{
    SendScintilla(SCI_REGISTERIMAGE, id, pm);
}


// Clear all registered images.
void QsciScintilla::clearRegisteredImages()
{
    SendScintilla(SCI_CLEARREGISTEREDIMAGES);
}


// Enable auto-completion fill-ups.
void QsciScintilla::setAutoCompletionFillupsEnabled(bool enable)
{
    const char *fillups;

    if (!enable)
        fillups = "";
    else if (!lex.isNull())
        fillups = lex->autoCompletionFillups();
    else
        fillups = explicit_fillups.data();

    SendScintilla(SCI_AUTOCSETFILLUPS, fillups);

    fillups_enabled = enable;
}


// See if auto-completion fill-ups are enabled.
bool QsciScintilla::autoCompletionFillupsEnabled() const
{
    return fillups_enabled;
}


// Set the fill-up characters for auto-completion if there is no current lexer.
void QsciScintilla::setAutoCompletionFillups(const char *fillups)
{
    explicit_fillups = fillups;
    setAutoCompletionFillupsEnabled(fillups_enabled);
}


// Set the case sensitivity for auto-completion if there is no current lexer.
void QsciScintilla::setAutoCompletionCaseSensitivity(bool cs)
{
    if (lex.isNull())
        SendScintilla(SCI_AUTOCSETIGNORECASE, !cs);
}


// Return the case sensitivity for auto-completion.
bool QsciScintilla::autoCompletionCaseSensitivity() const
{
    return !SendScintilla(SCI_AUTOCGETIGNORECASE);
}


// Set the replace word mode for auto-completion.
void QsciScintilla::setAutoCompletionReplaceWord(bool replace)
{
    SendScintilla(SCI_AUTOCSETDROPRESTOFWORD, replace);
}


// Return the replace word mode for auto-completion.
bool QsciScintilla::autoCompletionReplaceWord() const
{
    return SendScintilla(SCI_AUTOCGETDROPRESTOFWORD);
}


// Set the single item mode for auto-completion.
void QsciScintilla::setAutoCompletionShowSingle(bool single)
{
    showSingle = single;
}


// Return the single item mode for auto-completion.
bool QsciScintilla::autoCompletionShowSingle() const
{
    return showSingle;
}


// Set current call tip style.
void QsciScintilla::setCallTipsStyle(CallTipsStyle style)
{
    call_tips_style = style;
}


// Set maximum number of call tips displayed.
void QsciScintilla::setCallTipsVisible(int nr)
{
    maxCallTips = nr;
}


// Set the document to display.
void QsciScintilla::setDocument(const QsciDocument &document)
{
    if (doc.pdoc != document.pdoc)
    {
        doc.undisplay(this);
        doc.attach(document);
        doc.display(this,&document);
    }
}


// Ensure the document is read-write and return true if was was read-only.
bool QsciScintilla::ensureRW()
{
    bool ro = isReadOnly();

    if (ro)
        setReadOnly(false);

    return ro;
}


// Return the number of the first visible line.
int QsciScintilla::firstVisibleLine() const
{
    return SendScintilla(SCI_GETFIRSTVISIBLELINE);
}


// Return the height in pixels of the text in a particular line.
int QsciScintilla::textHeight(int linenr) const
{
    return SendScintilla(SCI_TEXTHEIGHT, linenr);
}


// See if auto-completion or user list is active.
bool QsciScintilla::isListActive() const
{
    return SendScintilla(SCI_AUTOCACTIVE);
}


// Cancel any current auto-completion or user list.
void QsciScintilla::cancelList()
{
    SendScintilla(SCI_AUTOCCANCEL);
}


// Handle a selection from the auto-completion list.
void QsciScintilla::handleAutoCompletionSelection()
{
    if (!lex.isNull())
    {
        QsciAbstractAPIs *apis = lex->apis();

        if (apis)
            apis->autoCompletionSelected(acSelection);
    }
}


// Display a user list.
void QsciScintilla::showUserList(int id, const QStringList &list)
{
    // Sanity check to make sure auto-completion doesn't get confused.
    if (id <= 0)
        return;

    SendScintilla(SCI_AUTOCSETSEPARATOR, userSeparator);
    SendScintilla(SCI_USERLISTSHOW, id,
            list.join(QChar(userSeparator)).toLatin1().data());
}


// Translate the SCN_USERLISTSELECTION notification into something more useful.
void QsciScintilla::handleUserListSelection(const char *text, int id)
{
    emit userListActivated(id, QString(text));
}


// Return the case sensitivity of any lexer.
bool QsciScintilla::caseSensitive() const
{
    return lex.isNull() ? true : lex->caseSensitive();
}


// Return true if the current list is an auto-completion list rather than a
// user list.
bool QsciScintilla::isAutoCompletionList() const
{
    return (SendScintilla(SCI_AUTOCGETSEPARATOR) == acSeparator);
}


// Read the text from a QIODevice.
bool QsciScintilla::read(QIODevice *io)
{
    const int min_size = 1024 * 8;

    int buf_size = min_size;
    char *buf = new char[buf_size];

    int data_len = 0;
    bool ok = true;

    qint64 part;

    // Read the whole lot in so we don't have to worry about character
    // boundaries.
    do
    {
        // Make sure there is a minimum amount of room.
        if (buf_size - data_len < min_size)
        {
            buf_size *= 2;
            char *new_buf = new char[buf_size * 2];

            memcpy(new_buf, buf, data_len);
            delete[] buf;
            buf = new_buf;
        }

        part = io->read(buf + data_len, buf_size - data_len - 1);

        data_len += part;
    }
    while (part > 0);

    if (part < 0)
        ok = false;
    else
    {
        buf[data_len] = '\0';

        bool ro = ensureRW();

        SendScintilla(SCI_SETTEXT, buf);
        SendScintilla(SCI_EMPTYUNDOBUFFER);

        setReadOnly(ro);
    }

    delete[] buf;

    return ok;
}


// Write the text to a QIODevice.
bool QsciScintilla::write(QIODevice *io) const
{
    const char *buf = reinterpret_cast<const char *>(SendScintillaPtrResult(SCI_GETCHARACTERPOINTER));

    const char *bp = buf;
    uint buflen = qstrlen(buf);

    while (buflen > 0)
    {
        qint64 part = io->write(bp, buflen);

        if (part < 0)
            return false;

        bp += part;
        buflen -= part;
    }

    return true;
}


// Return the word at the given cooridinates.
QString QsciScintilla::wordAtPoint(const QPoint &point) const
{
    long close_pos = SendScintilla(SCI_POSITIONFROMPOINTCLOSE, point.x(),
            point.y());

    if (close_pos < 0)
        return QString();

    long start_pos = SendScintilla(SCI_WORDSTARTPOSITION, close_pos, true);
    long end_pos = SendScintilla(SCI_WORDENDPOSITION, close_pos, true);
    int word_len = end_pos - start_pos;

    if (word_len <= 0)
        return QString();

    char *buf = new char[word_len + 1];
    SendScintilla(SCI_GETTEXTRANGE, start_pos, end_pos, buf);
    QString word = convertTextS2Q(buf);
    delete[] buf;

    return word;
}


// Return the display style for annotations.
QsciScintilla::AnnotationDisplay QsciScintilla::annotationDisplay() const
{
    return (AnnotationDisplay)SendScintilla(SCI_ANNOTATIONGETVISIBLE);
}


// Set the display style for annotations.
void QsciScintilla::setAnnotationDisplay(QsciScintilla::AnnotationDisplay display)
{
    SendScintilla(SCI_ANNOTATIONSETVISIBLE, display);
}


// Clear annotations.
void QsciScintilla::clearAnnotations(int line)
{
    if (line < 0)
        SendScintilla(SCI_ANNOTATIONSETTEXT, line, (const char *)0);
    else
        SendScintilla(SCI_ANNOTATIONCLEARALL);
}


// Annotate a line.
void QsciScintilla::annotate(int line, const QString &text, int style)
{
    int style_offset = SendScintilla(SCI_ANNOTATIONGETSTYLEOFFSET);

    ScintillaString s = convertTextQ2S(text);

    SendScintilla(SCI_ANNOTATIONSETTEXT, line, ScintillaStringData(s));

    // SCI_ANNOTATIONSETSTYLE is broken in Scintilla v1.78 when the text
    // contains newlines.
#if 0
    SendScintilla(SCI_ANNOTATIONSETSTYLE, line, style - style_offset);
#else
    int nr_bytes = ScintillaStringLength(s);
    char *styles = new char[nr_bytes];

    memset(styles, style - style_offset, nr_bytes);
    SendScintilla(SCI_ANNOTATIONSETSTYLES, line, styles);

    delete[] styles;
#endif
}


// Annotate a line.
void QsciScintilla::annotate(int line, const QString &text, const QsciStyle &style)
{
    annotate(line, text, style.style());
}


// Annotate a line.
void QsciScintilla::annotate(int line, const QsciStyledText &text)
{
    annotate(line, text.text(), text.style());
}


// Annotate a line.
void QsciScintilla::annotate(int line, const QList<QsciStyledText> &text)
{
    char *styles;
    ScintillaString styled_text = styleText(text, &styles,
            SendScintilla(SCI_ANNOTATIONGETSTYLEOFFSET));

    SendScintilla(SCI_ANNOTATIONSETTEXT, line,
            ScintillaStringData(styled_text));
    SendScintilla(SCI_ANNOTATIONSETSTYLES, line, styles);

    delete[] styles;
}


// Get the annotation for a line, if any.
QString QsciScintilla::annotation(int line) const
{
    char *buf = new char[SendScintilla(SCI_ANNOTATIONGETTEXT, line, (const char *)0) + 1];

    buf[SendScintilla(SCI_ANNOTATIONGETTEXT, line, buf)] = '\0';

    QString qs = convertTextS2Q(buf);
    delete[] buf;

    return qs;
}


// Convert a list of styled text to the low-level arrays.
QsciScintilla::ScintillaString QsciScintilla::styleText(const QList<QsciStyledText> &styled_text, char **styles, int style_offset)
{
    QString text;
    int i;

    // Build the full text.
    for (i = 0; i < styled_text.count(); ++i)
    {
        const QsciStyledText &st = styled_text[i];

        text.append(st.text());
    }

    ScintillaString s = convertTextQ2S(text);

    // There is a style byte for every byte.
    char *sp = *styles = new char[ScintillaStringLength(s)];

    for (i = 0; i < styled_text.count(); ++i)
    {
        const QsciStyledText &st = styled_text[i];
        ScintillaString part = convertTextQ2S(st.text());
        int part_length = ScintillaStringLength(part);

        for (int c = 0; c < part_length; ++c)
            *sp++ = (char)(st.style() - style_offset);
    }

    return s;
}
