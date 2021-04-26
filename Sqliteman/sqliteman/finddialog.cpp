/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QComboBox>
#include <QLineEdit>
#include <QScrollBar>
#include <QSqlField>
#include <QSqlRecord>
#include <QTreeWidgetItem>

#include "database.h"
#include "dataviewer.h"
#include "finddialog.h"
#include "preferences.h"
#include "sqlparser.h"
#include "utils.h"

bool FindDialog::notSame(QStringList l1, QStringList l2)
{
	QStringList l1s = QStringList(l1);
	QStringList l2s = QStringList(l2);
	l1s.sort();
	l2s.sort();
	return l1s != l2s;
}

bool FindDialog::isNumeric(QVariant::Type t)
{
	// This switch generates compilation warnings because data.type() is
	// declared to return QVariant::Type but actually returns QMetaType::Type,
	// and there is no conversion.
	// How stupid can you get!!!
	// This fixes it for the GNU compiler, for platforms with non-GNU
	// compilers, there might be something similar available.
#ifdef __GNUC__
#pragma GCC diagnostic push "-Wswitch"
#pragma GCC diagnostic ignored "-Wswitch"
#endif
	switch (t)
	{
		case QMetaType::Int:
		case QMetaType::UInt:
		case QMetaType::LongLong:
		case QMetaType::ULongLong:
		case QMetaType::Double:
		case QMetaType::Long:
		case QMetaType::Short:
		case QMetaType::ULong:
		case QMetaType::UShort:
		case QMetaType::Float:
			return true;
		default:
			return false;
	}
#ifdef __GNUC__
#pragma GCC diagnostic pop "-Wswitch"
#endif
}

void FindDialog::closeEvent(QCloseEvent * event)
{
	emit findClosed();
	QMainWindow::closeEvent(event);
}

FindDialog::FindDialog(QWidget * parent)
{
	ui.setupUi(this);
    ui.termsTab->allowNoTerms(false);
    Preferences * prefs = Preferences::instance();
	resize(prefs->finddialogWidth(), prefs->finddialogHeight());
}

FindDialog::~FindDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setfinddialogHeight(height());
    prefs->setfinddialogWidth(width());
}

void FindDialog::updateButtons()
{
	bool enabled = ui.termsTab->ui.termsTable->rowCount() != 0;
	ui.findfirst->setEnabled(enabled);
	ui.findnext->setEnabled(enabled);
	ui.findall->setEnabled(enabled);
}

void FindDialog::doConnections(DataViewer * dataviewer)
{
	connect(ui.findfirst, SIGNAL(clicked()),
			dataviewer, SLOT(findFirst()));
	connect(ui.findnext, SIGNAL(clicked()),
			dataviewer, SLOT(findNext()));
	connect(ui.findall, SIGNAL(clicked()),
			dataviewer, SLOT(findAll()));
	connect(ui.finishedbutton, SIGNAL(clicked()), this, SLOT(close()));
	connect(this, SIGNAL(findClosed()),
			dataviewer, SLOT(findClosing()));
	connect(ui.termsTab, SIGNAL(firstTerm()),
			this, SLOT(updateButtons()));
	connect(ui.termsTab->ui.termMoreButton, SIGNAL(clicked()),
			this, SLOT(updateButtons()));
	connect(ui.termsTab->ui.termLessButton, SIGNAL(clicked()),
			this, SLOT(updateButtons()));
}

void FindDialog::setup(QString schema, QString table)
{
	setWindowTitle(QString("Find in table ") + schema + "." + table);
	QStringList columns;
	SqlParser * parser = Database::parseTable(table, schema);
    QList<FieldInfo> fields = parser->m_fields;
    QList<FieldInfo>::const_iterator i;
	for (i = fields.constBegin(); i != fields.constEnd(); ++i) {
        columns << i->name;
    }
	delete parser;
	if (   (schema != m_schema)
		|| (table != m_table)
		|| (notSame(ui.termsTab->m_columnList, columns)))
	{
		m_schema = schema;
		m_table = table;
		ui.termsTab->m_columnList = columns;
		ui.termsTab->ui.termsTable->clear();
		ui.termsTab->ui.termsTable->setRowCount(0);
		ui.termsTab->ui.caseCheckBox->setChecked(false);
	}
	updateButtons();
}

bool FindDialog::isMatch(QSqlRecord * rec, int i)
{
	QComboBox * field = qobject_cast<QComboBox *>
		(ui.termsTab->ui.termsTable->cellWidget(i, 0));
	QComboBox * relation = qobject_cast<QComboBox *>
		(ui.termsTab->ui.termsTable->cellWidget(i, 1));
	QLineEdit * value = qobject_cast<QLineEdit *>
		(ui.termsTab->ui.termsTable->cellWidget(i, 2));
	if (field && relation)
	{
		QVariant data(rec->value(field->currentText()));
		bool dataOk;
		QString dataString(data.toString());
		if (!(ui.termsTab->ui.caseCheckBox->isChecked()))
		{
			dataString = QLocale().toLower(dataString);
		}
		double dataDouble = data.toDouble(&dataOk);
		bool valOk;
		QString valString;
		double valDouble;
		if (value)
		{
			valString = value->text();
			valDouble = valString.toDouble(&valOk);
			if (!(ui.termsTab->ui.caseCheckBox->isChecked()))
			{
				valString = QLocale().toLower(valString);
			}
		}
		else
		{
			valOk = false;
		}
		switch (relation->currentIndex())
		{
			case 0:	// Contains
				return  dataString.contains(valString);

			case 1:	// Doesn't contain
				return !(dataString.contains(valString));
			
			case 2:	// Starts with
				return dataString.startsWith(valString);

			case 3:	// Equals
				return dataString == valString;

			case 4:	// Not equals
				return dataString != valString;

			case 5:	// Bigger than
				if (isNumeric(rec->field(field->currentText()).type()))
				{
					// Column has NUMERIC affinity
					// so value will be converted to NUMERIC if possible
					if (valOk)
					{
						if (dataOk)
						{
							return dataDouble > valDouble;
						}
						else
						{
							// Value was converted, but not data
							// and TEXT > NUMERIC
							return true;
						}
					}
					else
					{
						if (dataOk)
						{
							// Data was converted, but not value
							// and NUMERIC < TEXT
							return false;
						}
					}
				}
				// No conversions, do string comparison
				return dataString > value->text();

			case 6:	// Smaller than
				if (isNumeric(rec->field(field->currentText()).type()))
				{
					// Column has NUMERIC affinity
					// so value will be converted to NUMERIC if possible
					if (valOk)
					{
						if (dataOk)
						{
							return dataDouble < valDouble;
						}
						else
						{
							// Value was converted, but not data
							// and TEXT > NUMERIC
							return false;
						}
					}
					else
					{
						if (dataOk)
						{
							// Data was converted, but not value
							// and NUMERIC < TEXT
							return true;
						}
					}
				}
				// No conversions, do string comparison
				return dataString < value->text();

			case 7:	// is null
				return data.isNull();

			case 8:	// is not null
				return !(data.isNull());
		}
	}
	return false;
}

bool FindDialog::isMatch(QSqlRecord * rec)
{
	int terms = ui.termsTab->ui.termsTable->rowCount();
	if (terms > 0)
	{
		if (ui.termsTab->ui.andButton->isChecked())
		{
			for (int i = 0; i < terms; ++i)
			{
				if (!isMatch(rec, i)) { return false; }
			}
			return true;
		}
		else // or button must be checked
		{
			for (int i = 0; i < terms; ++i)
			{
				if (isMatch(rec, i)) { return true; }
			}
			return false;
		}
	}
	else
	{
		return true; // empty term list matches anything
	}
}
