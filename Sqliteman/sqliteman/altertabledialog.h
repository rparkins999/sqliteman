/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef ALTERTABLEDIALOG_H
#define ALTERTABLEDIALOG_H

#include "database.h"
#include "tableeditordialog.h"

class QTreeWidgetItem;
class QPushButton;

/*! \brief Handle alter table features.
Sqlite3 has very limited ALTER TABLE feature set, so
there is only support for "table rename" and "add column".
Table Renaming is handled directly in the LiteManWindow
method renameTable().
Adding columns - see addColumns(). It's a wrapper around
plain ALTER TABLE ADD COLUMN statement.
Drop Column is using a workaround with tmp table, insert-select
statement and renaming. See createButton_clicked()
\author Petr Vanek <petr@scribus.info>
*/

class AlterTableDialog : public TableEditorDialog // ->DialogCommon->QDialog
{
	Q_OBJECT

	private:
		void addField(FieldInfo field, bool isNew);

	private slots:
		void addField();

		//! \brief Fill the GUI with table structure.
		void resetClicked();

	public:
		AlterTableDialog(LiteManWindow * parent = 0,
						 QTreeWidgetItem * item = 0,
						 const bool isActive = false
						);
		~AlterTableDialog();

	private:
		QTreeWidgetItem * m_item;
		QPushButton * m_alterButton;
		QList<FieldInfo> m_fields;
		QVector<bool> m_isIndexed;
		QVector<int> m_oldColumn; // -1 if no old column
		bool m_hadRowid;
		bool m_alteringActive; // true if altering currently active table
		bool m_altered; // something changed other than the name
		bool m_dropped; // a column has been dropped
		int m_oldPragmaAlterTable; // really boolean but sqlite uses an int
        int m_oldPragmaForeignKeys; // really boolean but sqlite uses an int

        void updateButtons(); // fix up enabling and disabling of buttons

		//! \brief Roll back whole transaction after failure
		bool doRollback(QString message);

		//! \brief Returns a list of SQL statements to recreate triggers
		QStringList originalSource(QString tableName);

		//! \brief Returns a list of parsed create index statments
		QList<SqlParser *> originalIndexes(QString tableName);

		/*! \brief Perform a rename table action if it's required.
		\retval true on success.
		*/
		bool renameTable(QString oldTableName, QString newTableName);

		/*! \brief Create a new table with a previously unused name.
		\param databaseName database in which to create it:
                            if null or empty, use "main".
		\param createBody body of create statement
                          starting with "AS" or a "(".
		\retval name of created table
        This tries to create a new table with a generated name.
        If it fails because the generated name is already in use, it tries
        with different names for another 19 attempts. If it still fails,
        it shows an error message and returns null. If it succeeds, it returns
        the generated name. Note we don't save time by checking whether the
        error is something other than the name already existing: this would be
        a programming error and it isn't worth having the extra code in the
        released version. It should work correctly if multiple tasks are using
        it simultaneously.
		*/
        QString createNew (QString databaseName, QString createBody);

        // Tries to rename oldTableName to a generated temporary name
        // and returns the temporary name if it succeeds.
        // If after several tries it fails, it returns null.
        QString renameTemp (QString oldTableName);

        bool checkColumn(int i, QString cname,
						 QString ctype, QString cextra);
		void swap(int i, int j);
		void drop (int i);
        void restorePragmas();
        // does the internals of alterButton_clicked()
        bool doit(QString newTableName);

    signals:
		void rebuildTableTree(QString schema);

	private slots:
		void cellClicked(int, int);
		void alterButton_clicked();

		//! \brief Setup the Alter button if there is something changed
		void checkChanges();
};

#endif
