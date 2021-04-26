/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef CREATEINDEXDIALOG_H
#define CREATEINDEXDIALOG_H

#include "database.h"
#include "tableeditordialog.h"

class LiteManWindow;
class QPushButton;

/*! \brief GUI for index creation
\author Petr Vanek <petr@scribus.info>
*/
class CreateIndexDialog : public TableEditorDialog // ->DialogCommon->Qdialog
{
	Q_OBJECT

private:
    QPushButton * m_createButton;

    void collatorIndexChanged(int n, QComboBox * box);
    void addField(FieldInfo f);
    void swap(int i, int j);
    void drop (int i);

private slots:
    void collatorIndexChanged(int n);
    void resetClicked();
    void indexNameEdited(const QString & text);
    void cellClicked(int, int);
    /*! \brief Parse user's inputs and create an sql statement */
    void createButton_clicked();

protected:
    void setFirstLine(QWidget * w);
    void setFirstLine();
    QString getSQLfromDesign();
    QString getSQLfromGUI(QWidget * w);

public:
    /*! \brief Create a dialog
    \param tabName name of the index parent table
    \param schema name of the db schema
    \param parent standard Qt parent
    */
    CreateIndexDialog(const QString & tabName, const QString & schema,
                        LiteManWindow * parent = 0);
    ~CreateIndexDialog();

public slots:
    void checkChanges();
    void tableChanged(const QString table);
    void databaseChanged(const QString schema);
};

#endif // CREATEINDEXDIALOG_H
