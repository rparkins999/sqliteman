/* Copyright © Richard Parkins 2021
 *
 * This code is released under the GNU public Licence.
 */

#ifndef PD_H
#define PD_H

// Undefine this if you will never want to use a debugger on the sqlite3
// library and want to make sqliteman smaller.
#define WANT_VDBE_DECODING

// Undefine this if you will never use a debugger at all
// and want to make sqliteman smaller
#define WANT_PD

#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QItemSelection>
#include <QtCore/QLine>
#include <QLineEdit>
#include <QtCore/QLineF>
#include <QtCore/QLocale>
#include <QtCore/QModelIndex>
#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QPointF>
#include <QtCore/QRect>
#include <QtCore/QRectF>
#include <QtCore/QRegExp>
#include <QtCore/QSet>
#include <QtCore/QSize>
#include <QtCore/QSizeF>
#include <QSqlError>
#include <QSqlRecord>
#include <QtCore/QString>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTreeWidgetItem>
#include <QtCore/QUrl>
#include <QtCore/QVariant>

#include "sqlparser.h"

class pd {
// routines are used internally, to prettyprint collection classes
// or to eliminate duplicate code for types that can be in a QVariant.
// They should work recursively with collections inside collections.
private:
    static void startList(QString type);
    static void addItem(QString s);
    static void addItem(QString prefix, const QVariant v);
    static void endList();

    static QString prepareColor(QColor c);
    static QString prepareFont(QFont f);
    static QString prepareLine(QLine l);
    static QString prepareLineF(QLineF l);
    static QString prepareLocale(QLocale l);
    static QString prepareModelIndex(const QModelIndex x);
    static QString prepareObject(const QObject * ob);
    static QString preparePoint(QPoint p);
    static QString preparePointF(QPointF p);
    static QString prepareRect(QRect r);
    static QString prepareRectF(QRectF r);
    static QString prepareRegExp(QRegExp r);
    static QString prepareSize(QSize s);
    static QString prepareSizeF(QSizeF s);
    static QString prepareString(QString s);
    static QString prepareTreeWidgetItem(const QTreeWidgetItem * const item);
    static QString prepareUrl(QUrl u);
    static QString prepareVariant(QVariant v);
    static QString prepareWidget(const QWidget * ob);

    static QString prepareFieldInfo(const struct FieldInfo f);

// All the rest are the class dumping routines which can be called from gdb
public:
    // First a few for simple types: gdb can print these, but we want to be
    // able to use our macro withut having to think whether we really need to.
    static void dump(long long int i);
    static void dump(unsigned long long int u);
    static void dump(bool b);
    static void dump(long double d);
    static void dump(const char * s);
    static void dump(const unsigned char * s) { dump((const char *)s); }

    // Qt types that I use in alphabetical order
    static void dump(QChar c);
    static void dump(QColor c);
    static void dump(QComboBox & box);
    static void dump(QComboBox * box);
    static void dump(QFont f);
    static void dump(QHash<QString, QVariant> &h);
    static void dump(QItemSelection &selection);
    static void dump(QLine l);
    static void dump(QLineEdit & le);
    static void dump(QLineEdit * le);
    static void dump(QLineF l);
    static void dump(QList<bool> &l);
    static void dump(QList<int> &l);
    static void dump(QList<float> &l);
    static void dump(QList<double> &l);
    static void dump(QList<QModelIndex> &l);
    static void dump(QList<QObject *> &l);
    static void dump(QList<QString> &l);
    static void dump(QList<QStringList> &l);
    static void dump(QList<QTreeWidgetItem *> &l);
    static void dump(QList<QVariant> &l);
    static void dump(QList<QWidget *> &l);
    static void dump(QMap<QString, QList<QString> > &m);
    static void dump(QMap<QString, QStringList> &m);
    static void dump(QMap<QString, QObject *> &m);
    static void dump(QMap<QString, QString> &m);
    static void dump(QMap<QString, QVariant> &m);
    static void dump(QModelIndex &x);
    static void dump(QObject & ob);
    static void dump(QObject * ob);
    static void dump(QPoint p);
    static void dump(QPointF p);
    static void dump(QRect r);
    static void dump(QRectF r);
    static void dump(QRegExp r);
    static void dump(QSet<QTreeWidgetItem *> &l);
    static void dump(QSize s);
    static void dump(QSizeF s);
    static void dump(QSqlError & e);
    static void dump(QSqlError * e);
    static void dump(QSqlRecord & r);
    static void dump(QSqlRecord * r);
    static void dump(QString s);
    static void dump(QStringList &l);
    static void dump(QStringRef sr);
    static void dump(QTableWidgetItem & item);
    static void dump(QTableWidgetItem * item);
    static void dump(QTextEdit & te);
    static void dump(QTextEdit * te);
    static void dump(QTreeWidgetItem & item);
    static void dump(QTreeWidgetItem * item);
    static void dump(QUrl url);
    static void dump(QVariant v);
    static void dump(QVector<bool> &v);
    static void dump(QVector<int> &v);
    static void dump(QVector<float> &v);
    static void dump(QVector<double> &v);
    static void dump(QVector<QObject *> &v);
    static void dump(QVector<QPoint> &v);
    static void dump(QVector<QPointF> &v);
    static void dump(QVector<QVariant> &v);
    static void dump(QWidget * w);

    // The following are for sqliteman's own application types
    static void dump(const struct Token & t);
    static void dump(QList<Token> &l);
    static void dump(Expression * e);
    static void dump(QList<Expression *> &l);
    static void dump(const struct FieldInfo &f);
    static void dump(QList<struct FieldInfo> &l);
    static void dump(SqlParser & p);
    static void dump(SqlParser * pp);
    static void dump(QList<SqlParser *> &l);
#ifdef WANT_VDBE_DECODING
    static void dumpOpList(void * pVdbe);
#endif // WANT_VDBE_DECODING

// This bit of fantasy enables me to prettyprint enum values
#undef ENUM
#undef ENUMVALUE
#undef ENUMLAST
#undef ENUMSKIP
#define ENUM(t) \
    static void dump(enum t x) { dump(prepare##t(x)); } \
    static QString prepare##t(enum t x) { switch (x) {
#define ENUMVALUE(x) case x: return #x;
#define ENUMLAST(x) case x: return #x; default: return NULL; } }
#define ENUMSKIP

// This part is repeated for each header file that contains enumerations
// that I want to be able to prettyprint.
#ifdef SQLPARSER_H
#undef SQLPARSER_H
#include "sqlparser.h"
#else
#include "sqlparser.h"
#undef SQLPARSER_H
#endif

#undef ENUM
#undef ENUMVALUE
#undef ENUMLAST
#undef ENUMSKIP
};
#endif
