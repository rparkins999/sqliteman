/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	
	FIXME handle column names in first row
	FIXME re-add Psion format
*/
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStandardItemModel>

#if QT_VERSION >= 0x040300
#include <QXmlStreamReader>
#else
#warning "QXmlStreamReader is disabled. Qt 4.3.x required."
#endif

#include <QSqlQuery>
#include <QSqlError>
#include <QTreeWidgetItem>
#include <QtDebug>

#include "importtabledialog.h"
#include "importtablelogdialog.h"
#include "database.h"
#include "sqliteprocess.h"
#include "utils.h"

ImportTableDialog::ImportTableDialog(LiteManWindow * parent,
									 const QString & tableName,
									 const QString & schema)
	: QDialog(parent),
	m_tableName(tableName), m_schema(schema)
{
	update = false;
	creator = parent;
	setupUi(this);
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("importtable/height", QVariant(500)).toInt();
	int ww = settings.value("importtable/width", QVariant(600)).toInt();
	resize(ww, hh);

	QString n;
	int currIx = 0;
    QList<QString> l = Database::getDatabases().keys();
    QList<QString>::const_iterator i;
	int j;
    for (i = l.constBegin(), j = 0; i != l.constEnd(); ++i, ++j) {
		if (*i == m_schema) { currIx = j; }
		schemaComboBox->addItem(*i);
	}
	schemaComboBox->setCurrentIndex(currIx);
	setTablesForSchema(m_schema);
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

#if QT_VERSION < 0x040300
	tabWidget->setTabEnabled(1, false);
#endif

	connect(schemaComboBox, SIGNAL(currentIndexChanged(const QString &)),
			this, SLOT(setTablesForSchema(const QString &)));
	connect(fileButton, SIGNAL(clicked()), this, SLOT(fileButton_clicked()));
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotAccepted()));
	connect(tabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(createPreview()));
	// csv
	colSep->setText(",");
	connect(colSep, SIGNAL(textChanged(QString)),
			this, SLOT(createPreview()));
	quoteChar->setText("\"");
	connect(quoteChar, SIGNAL(textChanged(QString)),
			this, SLOT(createPreview()));
	connect(skipHeaderCheck, SIGNAL(toggled(bool)),
			this, SLOT(skipHeaderCheck_toggled(bool)));

	skipHeaderCheck_toggled(false);
}

ImportTableDialog::~ImportTableDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("importtable/height", QVariant(height()));
    settings.setValue("importtable/width", QVariant(width()));
}

QStringList ImportTableDialog::splitLine(QTextStream * in,
										 QString sep, QString q)
{
	QStringList result;
	QString line = in->readLine();
	int state = 0; // not in quotes
	QString item;
	while (true)
	{
		switch (state)
		{
			case 0: // not in quotes
				if (line.length() == 0)
				{
					result.append(item);
					return result;
				}
				else if (line.startsWith(sep))
				{
					result.append(item);
					item.clear();
					line.remove(0, sep.length());
				}
				else if ((q.length() > 0) && line.startsWith(q))
				{
					line.remove(0, 1);
					state = 1; // in quotes
				}
				else
				{
					item.append(line.at(0));
					line.remove(0, 1);
				}
				break;
			case 1: // in quotes
				if (line.length() == 0)
				{
					if (!in->atEnd())
					{
						item.append('\n');
						line = in->readLine();
					}
					else
					{
						// file ends with unmatched quote
						result.append(item);
						return result;
					}
				}
				else if (line.startsWith(q))
				{
					line.remove(0, 1);
					state = 2; // seen quote in quotes
				}
				else
				{
					item.append(line.at(0));
					line.remove(0, 1);
				}
				break;
			case 2: // seen quote in quotes
				if (line.length() == 0)
				{
					result.append(item);
					return result;
				}
				else if (line.startsWith(sep))
				{
					result.append(item);
					item.clear();
					line.remove(0, sep.length());
					state = 0; // not in quotes
				}
				else if (line.startsWith(q))
				{
					item.append(line.at(0));
					line.remove(0, 1);
					state = 1; // in quotes
				}
				else
				{
					item.append(line.at(0));
					line.remove(0, 1);
					state = 0; // not in quotes
				}
				break;
		}
	}
}

void ImportTableDialog::fileButton_clicked()
{
	QString pth(fileEdit->text());
	pth = pth.isEmpty() ? QDir::currentPath() : pth;
	QString fname = QFileDialog::getOpenFileName(this, tr("File to Import"),
												 pth,
												 tr("CSV Files (*.csv);;MS Excel XML (*.xml);;Text Files (*.txt);;All Files (*)"));
	if (fname.isEmpty())
		return;

	fileEdit->setText(fname);
	updateButton();
	createPreview();
}

void ImportTableDialog::slotAccepted()
{
	QList<QStringList> values;

	if (fileEdit->text().isEmpty())
	{
		return;
	}

	int skipHeader = skipHeaderCheck->isChecked() ? skipHeaderBox->value() : 0;
	QList<FieldInfo> fields
		= Database::tableFields(tableComboBox->currentText(),
								schemaComboBox->currentText());

	switch (tabWidget->currentIndex())
	{
		case 0:
			values = ImportTable::CSVModel(fileEdit->text(), fields,
										   skipHeader, colSep->text(),
										   quoteChar->text(),
										   this, 0).m_values;
			break;
		case 1:
			values = ImportTable::XMLModel(fileEdit->text(), fields,
										   skipHeader, this, 0).m_values;
	}

	// base import
	bool result = true;
	QStringList l;
	QStringList log;
	int cols = Database::tableFields(tableComboBox->currentText(),
									 schemaComboBox->currentText()).count();
	int row = 0;
	int success = 0;
	QStringList binds;
	for (int i = 0; i < cols; ++i) { binds << "?"; }
	QString sql = QString("insert into ")
				  + Utils::q(schemaComboBox->currentText())
				  + "."
				  + Utils::q(tableComboBox->currentText())
				  + " values ("
				  + binds.join(", ")
				  + ");";
		
	QSqlQuery query(QSqlDatabase::database(SESSION_NAME));

	if (   (m_tableName == tableComboBox->currentText())
		&& (m_schema == schemaComboBox->currentText())
		&& ((!creator) || !(creator->checkForPending())))
	{
		return;
	}
	if (!Database::execSql("SAVEPOINT IMPORT_TABLE;"))
	{
		// FIXME emit some failure message here
		return;
	}
	
    QList<QStringList>::const_iterator it;
    for (it = values.constBegin(); it != values.constEnd(); ++it) {
		++row;
		if (it->count() != cols)
		{
			log.append(tr("Row = %1; Imported values = %2; "
						  "Table columns count = %3; Values = (%4)")
					.arg(row).arg(it->count()).arg(cols).arg(it->join(", ")));
			result = false;
			continue;
		}

		query.prepare(sql);
		for (int i = 0; i < cols ; ++i)
		{
			QString s(it->at(i));
			if (s.isEmpty())
			{
				query.addBindValue(QVariant(QVariant::String));
			}
			else
			{
				if (s.startsWith("X'", Qt::CaseInsensitive))
				{
					QByteArray b;
					while (!s.startsWith("'"))
					{
						// blob
						s = s.remove(0, 2);
						b.append((hexValue(s[0]) << 4) + hexValue(s[1]));
					}
					query.addBindValue(QVariant(b));
				}
				else
				{
					query.addBindValue(s);
				}
			}
		}

		query.exec();
		if (query.lastError().isValid())
		{
			log.append(tr("Row = %1; %2")
					   .arg(row).arg(query.lastError().text()));
			result = false;
		}
		else
			++success;
	}

	if (result)
	{
		Database::execSql("RELEASE IMPORT_TABLE;");
		accept();
	}
	else
	{
		ImportTableLogDialog dia(log, this);
		if (dia.exec())
		{
			//FIXME need to override errors here
			if (Database::execSql("RELEASE IMPORT_TABLE;"))
			{
				update = m_alteringActive;
				accept();
				return;
			}
		}
		Database::execSql("ROLLBACK TO IMPORT_TABLE;");
		Database::execSql("RELEASE IMPORT_TABLE;");
	}
}

void ImportTableDialog::updateButton()
{
	bool enabled = !(fileEdit->text().isEmpty());
	if (tabWidget->currentIndex() == 0)
	{
		if (colSep->text().length() == 0) { enabled = false; }
		if (quoteChar->text().length() > 1) { enabled = false; }
		if (   (quoteChar->text().length() == 1)
			&& (colSep->text().startsWith(quoteChar->text())))
		{ enabled = false; }
	}
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
}

char ImportTableDialog::hexValue(QChar c)
{
	char a = c.toAscii();
	if ((a >= '0') && (a <= '9')) { return a - '0'; }
	else if ((a >= 'A') && (a <= 'F')) { return a - 'A' + 10; }
	else if ((a >= 'a') && (a <= 'f')) { return a - 'a' + 10; }
	else { return 0; } // invalid
}

void ImportTableDialog::createPreview(int)
{
	updateButton();
	QItemSelectionModel *m = previewView->selectionModel();
	if (buttonBox->button(QDialogButtonBox::Ok)->isEnabled())
	{
		int skipHeader
			= skipHeaderCheck->isChecked() ? skipHeaderBox->value() : 0;
		QList<FieldInfo> fields
			= Database::tableFields(tableComboBox->currentText(),
									schemaComboBox->currentText());
		switch (tabWidget->currentIndex())
		{
			case 0:
				previewView->setModel(new ImportTable::CSVModel(
					fileEdit->text(), fields, skipHeader,
					colSep->text(), quoteChar->text(), this, 3));
				break;
			case 1:
				previewView->setModel(new ImportTable::XMLModel(
					fileEdit->text(), fields, skipHeader, this, 3));
				break;
		}
	}
	else
	{
		previewView->setModel(new QStandardItemModel());
	}
	delete m;
}

void ImportTableDialog::createPreview(bool)
{
	createPreview();
}

void ImportTableDialog::skipHeaderCheck_toggled(bool checked)
{
	skipHeaderBox->setEnabled(checked);
}

/*
Models
 */
ImportTable::BaseModel::BaseModel(QList<FieldInfo> fields, QObject * parent)
	: QAbstractTableModel(),
	m_columns(0)
{
	m_columnNames.clear();
	m_values.clear();
	m_columns = fields.count();
	for (int i = 0; i < m_columns; ++i)
	{
		m_columnNames.append(fields[i].name);
	}
}

int ImportTable::BaseModel::rowCount(const QModelIndex & /*parent*/) const
{
	return m_values.count();
}

int ImportTable::BaseModel::columnCount(const QModelIndex & /*parent*/) const
{
	return m_columns;
}

QVariant ImportTable::BaseModel::data(const QModelIndex & index, int role) const
{
	if (!index.isValid())
		return QVariant();
	if (role == Qt::DisplayRole || role == Qt::EditRole)
	{
		if (m_values.count() <= index.row())
			return QVariant();
		if (m_values.at(index.row()).count() <= index.column())
			return QVariant();
		return QVariant(m_values.at(index.row()).at(index.column()));
	}
	return QVariant();
}

QVariant ImportTable::BaseModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role != Qt::DisplayRole) { return QVariant(); }

	if (orientation == Qt::Horizontal)
	{
		return m_columnNames[section];
	}

	return QVariant();
}

ImportTable::CSVModel::CSVModel(QString fileName, QList<FieldInfo> fields,
								int skipHeader,
								QString separator, QString quote,
								QObject * parent, int maxRows)
	: BaseModel(fields, parent)
{
	QFile f(fileName);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::warning(qobject_cast<QWidget*>(parent), tr("Data Import"),
							 tr("Cannot open file %1 for reading.")
							 .arg(fileName));
		return;
	}

	QTextStream in(&f);
	int r = 0;
	QStringList row;
	int tmpSkipHeader = 0;
	while (!in.atEnd())
	{
		row = ImportTableDialog::splitLine(&in, separator, quote);
		if (tmpSkipHeader < skipHeader)
		{
			tmpSkipHeader++;
			continue;
		}
		m_values.append(row);
		if (r > maxRows)
			break;
		if (maxRows != 0)
			++r;
	}
	f.close();
}

ImportTable::XMLModel::XMLModel(QString fileName, QList<FieldInfo> fields,
								int skipHeader, QObject * parent, int maxRows)
	: BaseModel(fields, parent)
{
#if QT_VERSION >= 0x040300
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::warning(qobject_cast<QWidget*>(parent), tr("Data Import"),
							 tr("Cannot open file %1 for reading.").arg(fileName));
		return;
	}

	QXmlStreamReader xml(&file);
	QStringList row;
	bool isCell = false;
	int r = 0;
	int tmpSkipHeader = 0;

	while (!xml.atEnd())
	{
		xml.readNext();
		if (xml.isStartElement())
		{
			if (xml.name() == "Row")
			{
				row.clear();
				isCell = false;
			}
			if (xml.name() == "Cell")
				isCell = true;
			if (isCell && xml.name() == "Data")
				row.append(xml.readElementText());
		}
		if (xml.isEndElement())
		{
			if (xml.name() == "Cell")
				isCell = false;
			if (xml.name() == "Row")
			{
				if (tmpSkipHeader < skipHeader)
				{
					tmpSkipHeader++;
					continue;
				}
				m_values.append(row);
				if (row.count() > m_columns)
					m_columns = row.count();
				row.clear();
				isCell = false;
				if (r > maxRows)
					break;
				if (maxRows != 0)
					++r;
			}
		}
	}
	if (xml.error() && xml.error() != QXmlStreamReader::PrematureEndOfDocumentError)
	{
        qDebug() << "XML ERROR:" << xml.lineNumber() << ": " << xml.errorString();
    }

	file.close();
#endif
}

void ImportTableDialog::setTablesForSchema(const QString & schema)
{
	int currIx = 0;
	QString n;

	tableComboBox->clear();
    QList<QString> keys = Database::getObjects("table", schema).keys();
    QList<QString>::const_iterator it;
	int i;
    for (it = keys.constBegin(), i = 0; it != keys.constEnd(); ++it, ++i) {
		if (*it == m_tableName) { currIx = i; }
		tableComboBox->addItem(*it);
	}
	tableComboBox->setCurrentIndex(currIx);
}
