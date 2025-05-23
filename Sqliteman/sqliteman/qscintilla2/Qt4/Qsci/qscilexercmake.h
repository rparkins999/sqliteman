// This defines the interface to the QsciLexerCMake class.
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


#ifndef QSCILEXERCMAKE_H
#define QSCILEXERCMAKE_H

#ifdef __APPLE__
extern "C++" {
#endif

#include <QtCore/qobject.h>

#include <Qsci/qsciglobal.h>
#include <Qsci/qscilexer.h>


//! \brief The QsciLexerCMake class encapsulates the Scintilla CMake lexer.
class QSCINTILLA_EXPORT QsciLexerCMake : public QsciLexer
{
    Q_OBJECT

public:
    //! This enum defines the meanings of the different styles used by the
    //! CMake lexer.
    enum {
        //! The default.
        Default = 0,

        //! A comment.
        Comment = 1,

        //! A string.
        String = 2,

        //! A left quoted string.
        StringLeftQuote = 3,

        //! A right quoted string.
        StringRightQuote = 4,

        //! A function.  (Defined by keyword set number 1.)
        Function = 5,

        //! A variable. (Defined by keyword set number 2.)
        Variable = 6,

        //! A label.
        Label = 7,

        //! A keyword defined in keyword set number 3.  The class must be
        //! sub-classed and re-implement keywords() to make use of this style.
        KeywordSet3 = 8,

        //! A WHILE block.
        BlockWhile = 9,

        //! A FOREACH block.
        BlockForeach = 10,

        //! An IF block.
        BlockIf = 11,

        //! A MACRO block.
        BlockMacro = 12,

        //! A variable within a string.
        StringVariable = 13,

        //! A number.
        Number = 14
    };

    //! Construct a QsciLexerCMake with parent \a parent.  \a parent is
    //! typically the QsciScintilla instance.
    QsciLexerCMake(QObject *parent = 0);

    //! Destroys the QsciLexerCMake instance.
    virtual ~QsciLexerCMake();

    //! Returns the name of the language.
    const char *language() const;

    //! Returns the name of the lexer.  Some lexers support a number of
    //! languages.
    const char *lexer() const;

    //! Returns the foreground colour of the text for style number \a style.
    //!
    //! \sa defaultPaper()
    QColor defaultColor(int style) const;

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

    //! Causes all properties to be refreshed by emitting the propertyChanged()
    //! signal as required.
    void refreshProperties();

    //! Returns true if ELSE blocks can be folded.
    //!
    //! \sa setFoldAtElse()
    bool foldAtElse() const;

public slots:
    //! If \a fold is true then ELSE blocks can be folded.  The default is
    //! false.
    //!
    //! \sa foldAtElse()
    virtual void setFoldAtElse(bool fold);

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
    void setAtElseProp();

    bool fold_atelse;

    QsciLexerCMake(const QsciLexerCMake &);
    QsciLexerCMake &operator=(const QsciLexerCMake &);
};

#ifdef __APPLE__
}
#endif

#endif
