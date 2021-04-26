/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef TABLEEDITORDIALOG_H
#define TABLEEDITORDIALOG_H

#include <QDialog>

#include "database.h"
#include "dialogcommon.h"
#include "ui_tableeditordialog.h"

/*! \brief A base dialog for creating and editing tables.
This dialog is used as an inheritance parent for
AlterTableDialog
CreateIndexDialog
CreateTableDialog
CreateViewDialog
None of these have their own ui: the layout for TableEditorDialog is
modified instead.

\author Petr Vanek <petr@scribus.info>
\author Igor Khanin
*/
class QTreeWidgetItem;

class TableEditorDialog : public DialogCommon // public QDialog
{
    Q_OBJECT
private slots:
    void fudgeSlot();
private:
    bool m_fudging = false;
    QRect m_rect;
    bool m_useNull;
    QString m_nullText;
    QColor m_nullColor;

public:
    Ui::TableEditorDialog ui;
    void fudge();
    TableEditorDialog(LiteManWindow * parent);
    ~TableEditorDialog();
    void fixTabWidget();
    void fixNull(QLineEdit * edit);
    virtual void addField(FieldInfo field);
    // This returns the current schema selected from ui.databaseCombo
    // (not the schema passed in, if any).
    QString schema();

protected:
    bool m_dirty; // SQL has been edited
    QString m_tableOrView;
    QString m_originalName; // NULL except in Alter Table Dialog
    bool m_dubious; // some column has an empty name
    int m_tabWidgetIndex;
    QWidget * m_oldWidget; // the widget from which we got the SQL
    bool m_noTemp; // temp database does not exist yet

    void setItem(QTreeWidgetItem * item, bool makeView);
    QString createdName();
    bool checkOk();
    virtual bool checkColumn(
        int i, QString cname, QString type, QString cextra);
    virtual QString getSQLfromDesign();
    virtual QString getSQLfromGUI(QWidget * w);
    QString getSql();
    QString getFullName();
    virtual void setFirstLine(QWidget * w);
    void setDirty();
    virtual void resizeTable();
    void resizeEvent(QResizeEvent * event);

public slots:
    virtual void addField();
    virtual void removeField();
    virtual void fieldSelected();
    void tableNameChanged();
    void tabWidget_currentChanged(int index);
    virtual void checkChanges() = 0;
    void lineEdited();
    void typeEdited(QString text);
    void typeIndexChanged(int index);
};

#endif // TABLEEDITORDIALOG_H
