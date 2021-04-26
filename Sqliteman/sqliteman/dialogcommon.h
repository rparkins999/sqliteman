/*
 * This file written by and copyright © 2021 Richard P. Parkins, M. A.,
 * and released under the GPL.
 *
 * This is some common code abstracted out of several sqliteman dialogs.
 * Inherited by
 * AlterTableDialog via TableEditorDialog
 * AlterTriggerDialog
 * AlterViewDialog
 * CreateIndexDialog via TableEditorDialog
 * CreateTableDialog via TableEditorDialog
 * CreateTriggerDialog
 * CreateViewDialog via TableEditorDialog
 * ConstraintsDialog
 * PopulatorDialog
 * TableEditorDialog
 *
 */

#ifndef DIALOGCOMMON_H
#define DIALOGCOMMON_H

#include <QDialog>
#include <QTextEdit>

class LiteManWindow;

class DialogCommon : public QDialog
{
    Q_OBJECT
public:
		DialogCommon(LiteManWindow * parent);
		~DialogCommon();
        bool m_updated;
protected:
    // We ought to be able use use parent() for this, but for some reason
    // qobject_cast<LiteManWindow*>(parent()) doesn't work
    LiteManWindow * m_creator;
    QString m_databaseName;
    QString m_tableName;
    QTextEdit * m_resultEdit;
    void setResultEdit(QTextEdit * resultEdit);
    void resultAppend(QString text);
    bool execSql(const QString & statement, const QString & message);
};
#endif // DIALOGCOMMON_H
