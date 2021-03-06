/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef UTILS_H
#define UTILS_H

#include <QColor>
#include <QCoreApplication>
#include <QComboBox>
#include <QFont>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelection>
#include <QLine>
#include <QLineEdit>
#include <QLineF>
#include <QList>
#include <QMap>
#include <QModelIndex>
#include <QObject>
#include <QPixmap>
#include <QPixmapCache>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSqlError>
#include <QSqlRecord>
#include <QString>
#include <QTableView>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTime>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVariant>
#include <QVector>

#include "sqlparser.h"


//! Various helper functions
namespace Utils {

    /*! A set of helper functions for simpler code */
    QIcon getIcon(const QString & fileName);
    QPixmap getPixmap(const QString & fileName);

    QString getTranslator(const QString & localeName);

    //! \brief Check if the object tree should be refilled depending on sql statement
    bool updateObjectTree(const QString & sql);

    //! \brief Check if the current table may have changed depending on sql statement
    bool updateTables(const QString & sql);

    //! \brief Quote argument using specified quote character
    QString q(QString s, QString c);

    //! \brief Quote identifier for generated SQL statement
    QString q(QString s);

    //! \brief Quote list of identifiers for generated SQL statement
    QString q(QStringList l, QString c);

    #if 0 // not used, but kept in case needed in the future
    QString unQuote(QString s);
    #endif

    QString like(QString s);
    QString startswith(QString s);

    void setColumnWidths(QTableView * tv);
}
#endif
