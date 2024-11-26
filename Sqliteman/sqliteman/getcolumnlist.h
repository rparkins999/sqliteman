/* Copyright © 2024 Richard Parkins
 *
 * This is a utility class containing one static member function to get
 * the column list of a table including the rowid if it has one.
 * It is used by QueryEditorWidget and FindDialog, which don't have
 * a conveniently avallable common ancestor.
 */

#ifndef GETCOLUMNLIST_H
#define GETCOLUMNLIST_H

#include <QStringList>

struct ColumnList {
    QStringList columns;
    QString rowid;
    bool queryingTable; // also needed by caller
};

class GetColumnList
{
	public:
        static struct ColumnList getColumnList(QString schema, QString table);
};

#endif //GETCOLUMNLIST_H
