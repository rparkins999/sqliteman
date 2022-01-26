// This defines the interface to the QsciLexerBash class.
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


#ifndef QSCILEXERBASH_H
#define QSCILEXERBASH_H

#ifdef __APPLE__
extern "C++" {
#endif

#include <QtCore/qobject.h>

#include <Qsci/qsciglobal.h>
#include <Qsci/qscilexer.h>


//! \brief The QsciLexerBash class encapsulates the Scintilla Bash lexer.
class QSCINTILLA_EXPORT QsciLexerBash : public QsciLexer
{
	Q_OBJECT

public:
	//! This enum defines the meanings of the different styles used by the
	//! Bash lexer.
	enum {
		//! The default.
		Default = 0,

		//! An error.
		Error = 1,

		//! A comment.
		Comment = 2,

		//! A number.
		Number = 3,

		//! A keyword.
		Keyword = 4,

		//! A double-quoted string.
		DoubleQuotedString = 5,

		//! A single-quoted string.
		SingleQuotedString = 6,

		//! An operator.
		Operator = 7,

		//! An identifier
		Identifier = 8,

		//! A scalar.
		Scalar = 9,

		//! Parameter expansion.
		ParameterExpansion = 10,

		//! Backticks.
		Backticks = 11,

		//! A here document delimiter.
		HereDocumentDelimiter = 12,

		//! A single quoted here document.
		SingleQuotedHereDocument = 13
	};

	//! Construct a QsciLexerBash with parent \a parent.  \a parent is
    //! typically the QsciScintilla instance.
	QsciLexerBash(QObject *parent = 0);

	//! Destroys the QsciLexerBash instance.
	virtual ~QsciLexerBash();

	//! Returns the name of the language.
	const char *language() const;

	//! Returns the name of the lexer.  Some lexers support a number of
	//! languages.
	const char *lexer() const;

	//! \internal Returns the style used for braces for brace matching.
	int braceStyle() const;

	//! \internal Returns the string of characters that comprise a word.
	const char *wordCharacters() const;

	//! Returns the foreground colour of the text for style number \a style.
	//!
	//! \sa defaultPaper()
	QColor defaultColor(int style) const;

	//! Returns the end-of-line fill for style number \a style.
	bool defaultEolFill(int style) const;

	//! Returns the font for style number \a style.
	QFont defaultFont(int style) const;

	//! Returns the background colour of the text for style number \a style.
	//!
	//! \sa defaultColor()
	QColor defaultPaper(int style) const;

	//! Returns the set of keywords for the keyword set \a set recognised
	//! by the lexer as a space separated string.
	const char *keywords(int set) const;

	//! Returns the descriptive name for style number \a style.  If the
	//! style is invalid for this language then an empty QString is returned.
	//! This is intended to be used in user preference dialogs.
	QString description(int style) const;

	//! Causes all properties to be refreshed by emitting the
	//! propertyChanged() signal as required.
	void refreshProperties();

	//! Returns true if multi-line comment blocks can be folded.
	//!
	//! \sa setFoldComments()
	bool foldComments() const;

	//! Returns true if trailing blank lines are included in a fold block.
	//!
	//! \sa setFoldCompact()
	bool foldCompact() const;

public slots:
	//! If \a fold is true then multi-line comment blocks can be folded.
	//! The default is false.
	//!
	//! \sa foldComments()
	virtual void setFoldComments(bool fold);

	//! If \a fold is true then trailing blank lines are included in a fold
	//! block. The default is true.
	//!
	//! \sa foldCompact()
	virtual void setFoldCompact(bool fold);

protected:
	//! The lexer's properties are read from the settings \a qs.  \a prefix
	//! (which has a trailing '/') should be used as a prefix to the key of
	//! each setting.  true is returned if there is no error.
	//!
	bool readProperties(QSettings &qs,const QString &prefix);

	//! The lexer's properties are written to the settings \a qs.
	//! \a prefix (which has a trailing '/') should be used as a prefix to
	//! the key of each setting.  true is returned if there is no error.
	//!
	bool writeProperties(QSettings &qs,const QString &prefix) const;

private:
	void setCommentProp();
	void setCompactProp();

	bool fold_comments;
	bool fold_compact;

	QsciLexerBash(const QsciLexerBash &);
	QsciLexerBash &operator=(const QsciLexerBash &);
};

#ifdef __APPLE__
}
#endif

#endif
