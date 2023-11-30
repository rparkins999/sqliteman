/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCompleter>
#include <QtCore/QDir>
#include <QDirModel>
#include <QtCore/QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSqlError>
#include <QSqlQuery>
#include <QtCore/QTextCodec>

#include "database.h"
#include "dataexportdialog.h"
#include "dataviewer.h"
#include "preferences.h"
#include "sqlmodels.h"
#include "utils.h"

#define LF QChar(0x0A)  /* '\n' */
#define CR QChar(0x0D)  /* '\r' */


DataExportDialog::DataExportDialog(DataViewer * parent, const QString & tableName) :
		QDialog(0),
		m_tableName(tableName),
		file(0)
{
	m_parentData = parent->tableData();
	m_data = qobject_cast<SqlQueryModel *>(m_parentData);
	m_table = qobject_cast<SqlTableModel *>(m_parentData);
	m_header = parent->tableHeader();
	cancelled = false;

	ui.setupUi(this);
    Preferences * prefs = Preferences::instance();
	resize(prefs->dataexportWidth(), prefs->dataexportHeight());
	formats[tr("Comma Separated Values (CSV)")] = "csv";
	formats[tr("HTML")] = "html";
	formats[tr("MS Excel XML (XLS)")] = "xls";
	formats[tr("SQL inserts")] = "sql";
	formats[tr("Python List")] = "py";
	formats[tr("Qore \"select\" hash")] = "qore_select";
	formats[tr("Qore \"selectRows\" hash")] = "qore_selectRows";
	ui.formatBox->addItems(formats.keys());
	ui.formatBox->setCurrentIndex(prefs->exportFormat());

	ui.lineEndBox->addItem("UNIX (lf)");
	ui.lineEndBox->addItem("Macintosh (cr)");
	ui.lineEndBox->addItem("MS Windows (crlf)");
	ui.lineEndBox->setCurrentIndex(prefs->exportEol());

	QStringList enc;
    QList<QByteArray> l(QTextCodec::availableCodecs());
    QList<QByteArray>::const_iterator i;
    for (i = l.constBegin(); i != l.constEnd(); ++i) {
        enc << QString(*i);
    }
	enc.sort();
	ui.encodingBox->addItems(enc);
	ui.encodingBox->setCurrentIndex(enc.indexOf(prefs->exportEncoding()));

	ui.fileButton->setChecked(prefs->exportDestination() == 0);
	ui.clipboardButton->setChecked(prefs->exportDestination() == 1);
	ui.headerCheckBox->setChecked(prefs->exportHeaders());

	fileButton_toggled(prefs->exportDestination() == 0);

	QCompleter *completer = new QCompleter(this);
	completer->setModel(new QDirModel(completer));
	ui.fileEdit->setCompleter(completer);

	connect(ui.fileButton, SIGNAL(toggled(bool)),
			this, SLOT(fileButton_toggled(bool)));
	connect(ui.clipboardButton, SIGNAL(toggled(bool)),
			this, SLOT(clipboardButton_toggled(bool)));
	connect(ui.fileEdit, SIGNAL(textChanged(const QString &)),
			this, SLOT(fileEdit_textChanged(const QString &)));
	connect(ui.searchButton, SIGNAL(clicked()),
			this, SLOT(searchButton_clicked()));
	connect(ui.buttonBox, SIGNAL(accepted()),
			this, SLOT(slotAccepted()));
}

DataExportDialog::~DataExportDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setdataexportHeight(height());
    prefs->setdataexportWidth(width());
}

void DataExportDialog::slotAccepted()
{
	Preferences * prefs = Preferences::instance();
	prefs->setExportFormat(ui.formatBox->currentIndex());
	prefs->setExportDestination(ui.fileButton->isChecked() ? 0 : 1);
	prefs->setExportHeaders(ui.headerCheckBox->isChecked());
	prefs->setExportEncoding(ui.encodingBox->currentText());
	prefs->setExportEol(ui.lineEndBox->currentIndex());

	accept();
}

void DataExportDialog::checkButtonStatus()
{
	bool e = false;
	// NULL
	if (ui.fileButton->isChecked() && !ui.fileEdit->text().isEmpty())
		e = true;
	if (ui.clipboardButton->isChecked())
		e = true;
	ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(e);
}

bool DataExportDialog::doExport()
{
	progress = new QProgressDialog("Exporting...", "Abort", 0, 0, this);
	connect(progress, SIGNAL(canceled()), this, SLOT(cancel()));
	progress->setWindowModality(Qt::WindowModal);
	// export everything
	if (m_table) { m_table->fetchAll(); }
	else { m_data->fetchAll(); }

	progress->setMaximum(m_parentData->rowCount());

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	QString curr(formats[ui.formatBox->currentText()]);
	bool res = openStream();
	if (curr == "csv")
		res &= exportCSV();
	else if (curr == "html")
		res &= exportHTML();
	else if (curr == "xls")
		res &= exportExcelXML();
	else if (curr == "sql")
		res &= exportSql();
	else if (curr == "py")
		res &= exportPython();
	else if (curr == "qore_select")
		res &= exportQoreSelect();
	else if (curr == "qore_selectRows")
		res &= exportQoreSelectRows();
	else
		Q_ASSERT_X(0, "unhandled export", "programmer's error. Fix it, man!");

	if (res)
		res &= closeStream();

	progress->setValue(m_parentData->rowCount());
	delete progress;
	progress = 0;

    QApplication::restoreOverrideCursor();
	return res;
}

void DataExportDialog::cancel()
{
	cancelled = true;
}

bool DataExportDialog::setProgress(int p)
{
	if (cancelled)
		return false;
	progress->setValue(p);
	qApp->processEvents();
	return true;
}

bool DataExportDialog::openStream()
{
	// file
	exportFile = ui.fileButton->isChecked();
	if (exportFile)
	{
		file.setFileName(ui.fileEdit->text());
		if (!file.open(QFile::WriteOnly | QFile::Truncate))
		{
			QMessageBox::warning(this, tr("Export Error"),
								 tr("Cannot open file %1 for writting").arg(ui.fileEdit->text()));
			return false;
		}
		out.setDevice(&file);
		out.setCodec(QTextCodec::codecForName(ui.encodingBox->currentText().toLatin1()));
	}
	else
	{
		// clipboard
		clipboard = QString();
		out.setString(&clipboard);
	}
	return true;
}

bool DataExportDialog::closeStream()
{
	out.flush();
	if (exportFile)
		file.close();
	else
	{
		QClipboard *c = QApplication::clipboard();
		c->setText(clipboard);
	}
	return true;
}

QSqlRecord DataExportDialog::getRecord(int i)
{
	if (m_table) { return m_table->record(i); }
	else { return m_data->record(i); }
}


bool DataExportDialog::exportCSV()
{
	if (header())
	{
		for (int i = 0; i < m_header.size(); ++i)
		{
			out << '"' << m_header.at(i) << '"';
			if (i != (m_header.size() - 1))
				out << ", ";
		}
		out << endl();
	}

	for (int i = 0; i < m_parentData->rowCount(); ++i)
	{
		if (!setProgress(i)) { return false; }
		if (m_table && m_table->isDeleted(i)) { continue; }
		QSqlRecord r = getRecord(i);
		for (int j = 0; j < m_header.size(); ++j)
		{
			if (r.value(j).type() == QVariant::ByteArray)
			{
				out << Database::hex(r.value(j).toByteArray());
			}
			else
			{
				out << '"'
					<< r.value(j).toString().replace('"', "\"\"")
					<< '"';
			}
			if (j != (m_header.size() - 1))
				out << ", ";
		}
		out << endl();
	}
	return true;
}

bool DataExportDialog::exportHTML()
{
	out << "<html>" << endl() << "<head>" << endl();
	QString encStr("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=%1\">");
	out << encStr.arg(ui.encodingBox->currentText()) << endl();
	out << "<title>Sqliteman export</title>" << endl() << "</head>" << endl();
	out << "<body>" << endl() << "<table border=\"1\">" << endl();

	if (header())
	{
		out << "<tr>";
		for (int i = 0; i < m_header.size(); ++i)
			out << "<th>"
			<< m_header.at(i).toHtmlEscaped()
			<< "</th>";
		out << "</tr>" << endl();
	}

	for (int i = 0; i < m_parentData->rowCount(); ++i)
	{
		if (!setProgress(i)) { return false; }
		if (m_table && m_table->isDeleted(i)) { continue; }
		out << "<tr>";
		QSqlRecord r = getRecord(i);
		for (int j = 0; j < m_header.size(); ++j)
			out << "<td>"
				<< r.value(j).toString().toHtmlEscaped()
				<< "</td>";
		out << "</tr>" << endl();
	}
	out << "</table>" << endl() << "</body>" << endl()
		<< "</html>";
	return true;
}

bool DataExportDialog::exportExcelXML()
{
	out << "<?xml version=\"1.0\"?>" << endl()
		<< "<ss:Workbook xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">" << endl()
		<< "<ss:Styles><ss:Style ss:ID=\"1\"><ss:Font ss:Bold=\"1\"/></ss:Style></ss:Styles>" << endl()
		<< "<ss:Worksheet ss:Name=\"Sqliteman Export\">" << endl()
		<< "<ss:Table>"<< endl();

	for (int i = 0; i < m_header.size(); ++i)
		out << "<ss:Column ss:Width=\"100\"/>" << endl();

	if (header())
	{
		out << "<ss:Row ss:StyleID=\"1\">" << endl();
		for (int i = 0; i < m_header.size(); ++i)
			out << "<ss:Cell><ss:Data ss:Type=\"String\">"
				<< m_header.at(i).toHtmlEscaped()
				<< "</ss:Data></ss:Cell>" << endl();
		out << "</ss:Row>" << endl();
	}

	for (int i = 0; i < m_parentData->rowCount(); ++i)
	{
		if (!setProgress(i)) { return false; }
		if (m_table && m_table->isDeleted(i)) { continue; }
		out << "<ss:Row>" << endl();
		QSqlRecord r = getRecord(i);
		for (int j = 0; j < m_header.size(); ++j)
			out << "<ss:Cell><ss:Data ss:Type=\"String\">"
			<< r.value(j).toString().toHtmlEscaped()
			<< "</ss:Data></ss:Cell>" << endl();
		out << "</ss:Row>" << endl();
	}

	out << "</ss:Table>" << endl()
		<< "</ss:Worksheet>" << endl()
		<< "</ss:Workbook>" << endl();
	return true;
}

bool DataExportDialog::exportSql()
{
	out << "BEGIN TRANSACTION;" << endl();;
	QString columns(m_header.join("\", \""));

	if (header())
	{
		QString createStatement = "CREATE TABLE "
								  + Utils::q(m_tableName)
								  + " (\""
								  + columns
								  + "\")";
		if (m_table)
		{
			QString createSQL = QString("SELECT sql FROM ")
								+ Database::getMaster(m_table->schema())
								+ " WHERE lower(name) = "
								+ Utils::q(m_tableName.toLower())
								+ ";";
			// Run the query

			QSqlQuery createQuery(createSQL, QSqlDatabase::database(SESSION_NAME));
			// Make sure the query ran successfully
			if (!createQuery.lastError().isValid())
			{
				createQuery.first();
				createStatement = createQuery.value(0).toString();
			}
		}
		out << createStatement << ";" << endl();
	}


	for (int i = 0; i < m_parentData->rowCount(); ++i)
	{
		if (!setProgress(i)) { return false; }
		if (m_table && m_table->isDeleted(i)) { continue; }
		out << "insert into " << Utils::q(m_tableName) << " (\"" << columns << "\") values (";
		QSqlRecord r = getRecord(i);

		for (int j = 0; j < m_header.size(); ++j)
		{
			if (r.value(j).toString().isNull())
				out << "NULL";
			else if (r.value(j).type() == QVariant::ByteArray)
				out << Database::hex(r.value(j).toByteArray());
			else
				out << "'" << r.value(j).toString().replace('\'', "''") << "'";
			if (j != (m_header.size() - 1))
				out << ", ";
		}
		out << ");" << endl();;
	}
	out << "COMMIT;" << endl();;
	return true;
}

bool DataExportDialog::exportPython()
{
	out << "[" << endl();

	for (int i = 0; i < m_parentData->rowCount(); ++i)
	{
		if (!setProgress(i)) { return false; }
		if (m_table && m_table->isDeleted(i)) { continue; }
		out << "	{ ";
		QSqlRecord r = getRecord(i);
		for (int j = 0; j < m_header.size(); ++j)
		{
			// "key" : """value""" python syntax due the potentional EOLs in the strings
			out << "\"" << m_header.at(j) << "\" : \"\"\"" << r.value(j).toString() << "\"\"\"";
			if (j != (m_header.size() - 1))
				out << ", ";
		}
		out << " }," << endl();
	}
	out << "]" << endl();
	return true;
}

bool DataExportDialog::exportQoreSelect()
{
    QString strTempl("\"%1\"");

	out << "my $out = ();" << endl();

    for (int i = 0; i < m_header.count(); ++i)
    {
		if (m_table && m_table->isDeleted(i)) { continue; }
        out << "$out." << m_header.at(i) << " = ";

        for (int j = 0; j < m_parentData->rowCount(); ++j)
        {
    		QSqlRecord r = getRecord(j);
            out << strTempl.arg(r.value(i).toString());
            if (j != m_parentData->rowCount() - 1)
                out << ", ";
        }
        out << ";" << endl();
    }
    return true;
}

bool DataExportDialog::exportQoreSelectRows()
{
	out << "my $out = " << endl();

	for (int i = 0; i < m_parentData->rowCount(); ++i)
	{
		if (!setProgress(i)) { return false; }
		if (m_table && m_table->isDeleted(i)) { continue; }
		out << "	(";
		QSqlRecord r = getRecord(i);
		for (int j = 0; j < m_header.size(); ++j)
		{
			out << "\"" << m_header.at(j) << "\" : \"" << r.value(j).toString() << "\"";
			if (j != (m_header.size() - 1))
				out << ", ";
		}
		out << ") ," << endl();
	}
	out << "" << endl();
	return true;
}

void DataExportDialog::fileButton_toggled(bool state)
{
	ui.fileEdit->setEnabled(state);
	ui.searchButton->setEnabled(state);
	ui.label_2->setEnabled(state);
	checkButtonStatus();
}

void DataExportDialog::clipboardButton_toggled(bool)
{
	checkButtonStatus();
}

void DataExportDialog::fileEdit_textChanged(const QString &)
{
	checkButtonStatus();
}

void DataExportDialog::searchButton_clicked()
{
	QString mask;
	QString curr(formats[ui.formatBox->currentText()]);
	if (curr == "csv")
		mask = tr("Comma Separated Value (*.csv)");
	else if (curr == "html")
		mask = tr("HTML (*.html)");
	else if (curr == "xls")
		mask = tr("MS Excel XML (*.xml)");
	else if (curr == "sql")
		mask = tr("SQL inserts (*.sql)");
	else if (curr == "py")
		mask = tr("Python list (*.py)");
	else if (curr == "qore_select")
		mask = tr("Qore select hash (*.q *.ql *.qc)");
	else if (curr == "qore_selectRows")
		mask = tr("Qore selectRows hash (*.q *.ql *.qc)");
	else
		Q_ASSERT_X(0, "unhandled export", "fix it!");



	QString presetPath(ui.fileEdit->text());
	if (presetPath.isEmpty())
		presetPath = QDir::currentPath();

	QString fileName = QFileDialog::getSaveFileName(this,
			tr("Export to File"),
			presetPath, //QDir::homePath(),
			mask);
	if (!fileName.isNull())
		ui.fileEdit->setText(fileName);
}

bool DataExportDialog::header()
{
	return (ui.headerCheckBox->isChecked());
}

QString DataExportDialog::endl()
{
	int ix = ui.lineEndBox->currentIndex();
	switch (ix)
	{
		case 1: return CR;
		case 2: return QString(CR) + LF;
		case 0:
		default:
			return LF;
	}
}

