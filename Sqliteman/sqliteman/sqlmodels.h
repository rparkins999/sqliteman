/* Copyright © 2007-2009 Petr Vanek and 2015-2025 Richard Parkins
 *
 * For general Sqliteman copyright and licensing information please refer
 * to the COPYING file provided with the program. Following this notice may exist
 * a copyright and/or license notice that predates the release of Sqliteman
 * for which a new license (GPL+exception) is in place.
 */

#ifndef SQLMODELS_H
#define SQLMODELS_H

#include <QSqlRecord>
#include <QSqlTableModel>
#include <QItemDelegate>
#include <QPalette>
#include <QSqlRecord>

#include "preferences.h"
#include "sqlparser.h"

class QPushButton;
class QByteArray;


/*! \brief Simple color/behaviour improvements for standard Qt4 Sql Models */
class SqlTableModel : public QSqlTableModel
{
	Q_OBJECT

    private:
		enum IndexType {
			PK,
			Auto,
			Default,
			None
		};

		bool m_pending;
		QString m_schema;
		int m_useCount;
		QList<int> m_deleteCache;
		QMap<int,IndexType> m_header;
        QList<FieldInfo> m_fields;
        QSqlRecord m_copyThis;
		QPalette m_palette;
        Preferences * m_prefs;

		// ****ing broken QSqlTableModel....
		// This map contains an entry for each inserted row:
		// value is true if row has been edited since it was created.
		QMap<int,bool> m_insertCache;

		QVariant data(const QModelIndex & item,
                      int role = Qt::DisplayRole) const;
		bool setData(const QModelIndex & ix,
                     const QVariant & value, int role = Qt::EditRole);
		QVariant headerData(int section,
							Qt::Orientation orientation,
							int role = Qt::DisplayRole) const;

		bool insertRowIntoTable(const QSqlRecord &values);

        // used to calculate a default value
        QVariant evaluate(QString expression);

		void reset(QString tableName, bool isNew);

        void refreshFields();


	private slots:
		/*! \brief Called when a new row is created in the view
        (not in the model).
        */
		void doPrimeInsert(int, QSqlRecord &);

    protected:
        bool deleteRowFromTable(int row);

	public:
		SqlTableModel(QObject * parent = 0, QSqlDatabase db = QSqlDatabase());
		~SqlTableModel() {};

		void setSchema(const QString & schema) { m_schema = schema; };
		QString schema() { return m_schema; };

		void setPalette(QPalette palette) { m_palette = palette; };

		bool pendingTransaction() { return m_pending; };

		/*! \brief Set the pending flag \see m_pending to the transaction state
		and refresh the QTableView vertical header in the case of rollback.
		The cached rows to delete are stored in the \see m_deleteCache.
		\param pending true in the case of active transaction in progress.
		*/
		void setPendingTransaction(bool pending);
        bool copyRow (int row, QSqlRecord record);
        bool insertRows ( int row, int count,
                          const QModelIndex & parent = QModelIndex() );
		bool removeRows ( int row, int count,
                          const QModelIndex & parent = QModelIndex() );
		
		void setTable ( const QString & tableName );
		
		/*! override parent to make public */
		QModelIndex createIndex(int row, int column, void *ptr = 0) const
		{ return QAbstractTableModel::createIndex(row, column, ptr); }

		// release model and delete if m_useCount zero
		static void detach(SqlTableModel * model);
		// add a user
		void attach() { m_useCount++; }
		void fetchAll();

		bool isDeleted(int row);
		bool isNewRow(int row);
		void initRecord(int row);

	signals:
		void reallyDeleting(int row);
		void moreFetched();

	public slots:
		bool select();
		bool submitAll();
		void revertAll();
};

/*! \brief Simple color/behaviour improvements for standard Qt4 Sql Models */
class SqlQueryModel : public QSqlQueryModel
{
	Q_OBJECT

	public:
        Preferences * m_prefs;
		SqlQueryModel( QObject * parent = 0);
		~SqlQueryModel() {};
		void setQuery ( const QSqlQuery & query );
		void setQuery ( const QString & query,
                        const QSqlDatabase & db = QSqlDatabase() );
		bool pendingTransaction() { return false; };

		/*! override parent to make public */
		QModelIndex createIndex(int row, int column, void *ptr = 0) const
		{ return QAbstractTableModel::createIndex(row, column, ptr); }

		// release model and delete if m_useCount zero
		static void detach(SqlQueryModel * model);
		// add a user
		void attach() { m_useCount++; }
		void fetchAll();

signals:
		void rowCountChanged();

	private:
		int m_useCount;
		QSqlRecord info;
		QPalette m_palette;

		QVariant data(const QModelIndex & item,
                      int role = Qt::DisplayRole) const;
        void initialRead();
};

#endif
