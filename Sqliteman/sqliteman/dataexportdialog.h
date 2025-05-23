/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef DATAEXPORTDIALOG_H
#define DATAEXPORTDIALOG_H

// QAbstractItemModel is included indirectly by SqlQueryModel
#include <QDialog>
#include <QSqlRecord>
#include <QtCore/QTextStream>
#include <QtCore/QFile>

#include "ui_dataexportdialog.h"

class DataViewer;
class QProgressDialog;
class SqlQueryModel;
class SqlTableModel;


/*! \brief GUI for data export into file or clipboard
\author Petr Vanek <petr@scribus.info>
*/
class DataExportDialog : public QDialog
{
		Q_OBJECT
	public:
		DataExportDialog(DataViewer * parent = 0, const QString & tableName = 0);
		~DataExportDialog();

		bool doExport();

	private:
		const QString m_tableName;
		bool cancelled;
		QAbstractItemModel * m_parentData;
		SqlQueryModel * m_data;
		SqlTableModel * m_table;
		QStringList m_header;
		QProgressDialog * progress;

		QTextStream out;
		QString clipboard;
		QFile file;
		bool exportFile;

		Ui::DataExportDialog ui;
		QMap<QString,QString> formats;

		QSqlRecord getRecord(int i);
		bool exportCSV();
		bool exportHTML();
		bool exportExcelXML();
		bool exportSql();
		bool exportPython();
		bool exportQoreSelect();
		bool exportQoreSelectRows();

		bool openStream();
		bool closeStream();

		bool setProgress(int p);

		/*! \brief Export table header strings too?
		\retval bool true = export, false = do not export header */
		bool header();
		QString endl();

		//! \brief Enable or Disable "OK" button depending on the GUI options
		void checkButtonStatus();

	private slots:
		void fileButton_toggled(bool);
		void clipboardButton_toggled(bool);
		void fileEdit_textChanged(const QString &);
		void searchButton_clicked();
		void cancel();
		void slotAccepted();
};

#endif
