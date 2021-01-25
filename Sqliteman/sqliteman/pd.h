/* Copyright © Richard Parkins 2021
 *
 * This code is released under the GNU public Licence.
 */

#ifndef PD_H
#define PD_H

#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QItemSelection>
#include <QLine>
#include <QLineEdit>
#include <QLineF>
#include <QLocale>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QRegExp>
#include <QSize>
#include <QSizeF>
#include <QSqlError>
#include <QSqlRecord>
#include <QString>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVariant>

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
    static QString prepareUrl(QUrl u);
    static QString prepareVariant(QVariant v);

    static QString prepareFieldInfo(const struct FieldInfo f);

// All the rest are the class dumping routines which can be called from gdb
public:
    // First a few for simple types: gdb can print these, but we want to be
    // able to use our macro withut having to think whether we really need to.
    static void dump(long long int i);
    static void dump(unsigned long long int u);
    static void dump(bool b);
    static void dump(long double d);

    // Qt types that I use in alphabetical order
    static void dump(QColor c);
    static void dump(QComboBox & box);
    static void dump(QComboBox * box);
    static void dump(QFont f);
    static void dump(QHash<QString, QVariant> &h);
    static void dump(QItemSelection selection);
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
    static void dump(QList<QVariant> &l);
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
    static void dump(QSize s);
    static void dump(QSizeF s);
    static void dump(QSqlError & e);
    static void dump(QSqlError * e);
    static void dump(QSqlRecord & r);
    static void dump(QSqlRecord * r);
    static void dump(QString s);
    static void dump(QStringList &l);
    static void dump(QTableWidgetItem & item);
    static void dump(QTableWidgetItem * item);
    static void dump(QTextEdit & te);
    static void dump(QTextEdit * te);
    static void dump(QTreeWidgetItem & item);
    static void dump(QTreeWidgetItem * item);
    static void dump(QVariant v);
    static void dump(QVector<bool> &v);
    static void dump(QVector<int> &v);
    static void dump(QVector<float> &v);
    static void dump(QVector<double> &v);
    static void dump(QVector<QObject *> &v);
    static void dump(QVector<QPoint> &v);
    static void dump(QVector<QPointF> &v);
    static void dump(QVector<QVariant> &v);

    // The following are for sqliteman's own application types
    static void dump(enum tokenType t);
    static void dump(const struct Token & t);
    static void dump(QList<Token> &l);
    static void dump(enum exprType t);
    static void dump(Expression * e);
    static void dump(QList<Expression *> &l);
    static void dump(const struct FieldInfo &f);
    static void dump(QList<struct FieldInfo> &l);
    static void dump(SqlParser & p);
    static void dump(SqlParser * pp);
    static void dump(QList<SqlParser *> &l);
    
    // magic for printing enumerations
#define ENUMPRINT
#include "sqlparser.h"
#undef ENUMPRINT
};
#endif
