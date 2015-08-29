/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QSqlQuery>
#include <QSqlError>
#include <QHeaderView>
#include <QDateTime>
#include <QMessageBox>
#include <math.h>

#include "populatordialog.h"
#include "populatorcolumnwidget.h"
#include "utils.h"

PopulatorDialog::PopulatorDialog(QWidget * parent, const QString & table, const QString & schema)
	: QDialog(parent),
	  m_schema(schema),
	  m_table(table)
{
	updated = false;
	setupUi(this);
	columnTable->horizontalHeader()->setStretchLastSection(true);

	FieldList fields = Database::tableFields(m_table, m_schema);
	columnTable->clearContents();
	columnTable->setRowCount(fields.size());
	QRegExp sizeExp("\\(\\d+\\)");
	for(int i = 0; i < fields.size(); ++i)
	{
		Populator::PopColumn col;
		col.name = fields[i].name;
		col.type = fields[i].type;
		col.pk = fields[i].pk;
// 		col.action has to be set in PopulatorColumnWidget instance!
		if (sizeExp.indexIn(col.type) != -1)
		{
			QString s = sizeExp.capturedTexts()[0].remove("(").remove(")");
			bool ok;
			col.size = s.toInt(&ok);
			if (!ok)
				col.size = 10;
		}
		else
			col.size = 10;
		col.userValue = "";

		QTableWidgetItem * nameItem = new QTableWidgetItem(col.name);
		QTableWidgetItem * typeItem = new QTableWidgetItem(col.type);
		columnTable->setItem(i, 0, nameItem);
		columnTable->setItem(i, 1, typeItem);
		PopulatorColumnWidget *p = new PopulatorColumnWidget(col, columnTable);
		connect(p, SIGNAL(actionTypeChanged()),
				this, SLOT(checkActionTypes()));
		columnTable->setCellWidget(i, 2, p);
	}

	columnTable->resizeColumnsToContents();
	checkActionTypes();

	connect(populateButton, SIGNAL(clicked()),
			this, SLOT(populateButton_clicked()));
	connect(spinBox, SIGNAL(valueChanged(int)),
			this, SLOT(spinBox_valueChanged(int)));
}

void PopulatorDialog::spinBox_valueChanged(int)
{
	checkActionTypes();
}

void PopulatorDialog::checkActionTypes()
{
	bool enable = false;
	if (spinBox->value() != 0)
	{
		for (int i = 0; i < columnTable->rowCount(); ++i)
		{
			if (qobject_cast<PopulatorColumnWidget*>
					(columnTable->cellWidget(i, 2))->column().action != Populator::T_IGNORE)
			{
				enable = true;
				break;
			}
		}
	}
	populateButton->setEnabled(enable);
}

qlonglong PopulatorDialog::tableRowCount()
{
	QString sql = QString("select count(1) from ")
				  + Utils::quote(m_schema)
				  + "."
				  + Utils::quote(m_table)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	query.exec();
	if (query.lastError().isValid())
	{
		textBrowser->append(tr("Cannot get statistics for table."));
		textBrowser->append(query.lastError().text());
		return -1;
	}
	while(query.next())
		return query.value(0).toLongLong();
	return -1;
}

QString PopulatorDialog::sqlColumns()
{
	QStringList s;
	foreach (Populator::PopColumn i, m_columnList)
	{
		if (i.action != Populator::T_IGNORE)
			s.append(i.name);
	}
	return Utils::quote(s);
}

void PopulatorDialog::populateButton_clicked()
{
	// Avoid QVariantList extension because it doesn't work for column names
	// containing special characters
	m_columnList.clear();
	for (int i = 0; i < columnTable->rowCount(); ++i)
		m_columnList.append(qobject_cast<PopulatorColumnWidget*>(columnTable->cellWidget(i, 2))->column());

	QList<QVariantList> values;
	foreach (Populator::PopColumn i, m_columnList)
	{
		switch (i.action)
		{
			case Populator::T_AUTO:
				values.append(autoValues(i));
				break;
			case Populator::T_AUTO_FROM:
				values.append(autoFromValues(i));
				break;
			case Populator::T_NUMB:
				values.append(numberValues(i));
				break;
			case Populator::T_TEXT:
				values.append(textValues(i));
				break;
			case Populator::T_PREF:
				values.append(textPrefixedValues(i));
				break;
			case Populator::T_STAT:
				values.append(staticValues(i));
				break;
			case Populator::T_DT_NOW:
			case Populator::T_DT_NOW_UNIX:
			case Populator::T_DT_NOW_JULIAN:
			case Populator::T_DT_RAND:
			case Populator::T_DT_RAND_UNIX:
			case Populator::T_DT_RAND_JULIAN:
				values.append(dateValues(i));
				break;
			case Populator::T_IGNORE:
				break;
		};
	}

	if (!Database::execSql("BEGIN TRANSACTION;"))
	{
		textBrowser->append(tr("Begin transaction failed."));
		Database::execSql("ROLLBACK;");
		return;
	}

	qlonglong cntPre, cntPost;
	textBrowser->clear();

	cntPre = tableRowCount();
	QSqlQuery query(QSqlDatabase::database(SESSION_NAME));
	for (int i = 0; i < spinBox->value(); ++i)
	{
		QStringList slr;
		for (int j = 0; j < values.count(); ++j)
		{
			slr.append(Utils::literal(values.at(j).at(i).toString()));
		}
		QString sql = QString("INSERT ")
					  + (constraintBox->isChecked() ? "OR IGNORE" : "")
					  + " INTO "
					  + Utils::quote(m_schema)
					  + "."
					  + Utils::quote(m_table)
					  + " ("
					  + sqlColumns()
					  + ") VALUES ("
					  + slr.join(",")
					  + ");";

		query.prepare(sql);
		if (!query.exec())
		{
			textBrowser->append(query.lastError().text());
			if (!constraintBox->isChecked()) { break; }
		}
		else { updated = true; }
	}

	if (!Database::execSql("COMMIT;"))
	{
		textBrowser->append(tr("Transaction commit failed."));
		Database::execSql("ROLLBACK;");
		updated = false;
		return;
	}

	cntPost = tableRowCount();

	if (cntPre != -1 && cntPost != -1)
		textBrowser->append(tr("Row(s) inserted: %1").arg(cntPost-cntPre));
}

QVariantList PopulatorDialog::autoValues(Populator::PopColumn c)
{
	QString sql = QString("select max(")
				  + Utils::quote(c.name)
				  + ") from "
				  + Utils::quote(m_schema)
				  + "."
				  + Utils::quote(m_table)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	query.exec();

	if (query.lastError().isValid())
	{
		textBrowser->append(tr("Cannot get MAX() for column: %1").arg(c.name));
		textBrowser->append(query.lastError().text());
		return QVariantList();
	}

	int max = 0;
	while(query.next())
		max = query.value(0).toInt();

	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append(i+max+1);

	return ret;
}

QVariantList PopulatorDialog::autoFromValues(Populator::PopColumn c)
{
	// TODO/FIXME: possible string to number conversion error
	// It will need to change PopulatorColumnWidget behavior probably
	int min = c.userValue.toInt();

	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append(i + min + 1);
	return ret;
}

QVariantList PopulatorDialog::numberValues(Populator::PopColumn c)
{
	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append(qrand() % (int)pow(10.0, c.size));
	return ret;
}

QVariantList PopulatorDialog::textValues(Populator::PopColumn c)
{
	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
	{
		QStringList l;
		for (int j = 0; j < c.size; ++j)
			l.append(QChar((qrand() % 58) + 65));
		ret.append(l.join("")
				.replace(QRegExp("(\\[|\\'|\\\\|\\]|\\^|\\_|\\`)"), " ")
				.simplified());
	}
	return ret;
}

QVariantList PopulatorDialog::textPrefixedValues(Populator::PopColumn c)
{
	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append(c.userValue + QString("%1").arg(i+1));
	return ret;
}

QVariantList PopulatorDialog::staticValues(Populator::PopColumn c)
{
	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append(c.userValue);
	return ret;
}

// a helper function used only for PopulatorDialog::dateValues()
float getJulianFromUnix( int unixSecs )
{
	return ( unixSecs / 86400.0 ) + 2440587;
}

QVariantList PopulatorDialog::dateValues(Populator::PopColumn c)
{
	QVariantList ret;

	// prepare some variables to speed up things on the loop
	// current time
	QDateTime now(QDateTime::currentDateTime());
	// timestamp of "now"
	uint now_tstamp = now.toTime_t();
	// pseudo random generator init
	qsrand(now_tstamp);

	for (int i = 0; i < spinBox->value(); ++i)
	{
		switch (c.action)
		{
			case Populator::T_DT_NOW:
				ret.append(now.toString("yyyy-MM-dd hh:mm:ss.z"));
				break;
			case Populator::T_DT_NOW_UNIX:
				ret.append(now_tstamp);
				break;
			case Populator::T_DT_NOW_JULIAN:
				ret.append(getJulianFromUnix(now_tstamp));
				break;
			case Populator::T_DT_RAND:
			{
				QDateTime dt;
				dt.setTime_t( qrand() % now_tstamp );
				ret.append(dt.toString("yyyy-MM-dd hh:mm:ss.z"));
				break;
			}
			case Populator::T_DT_RAND_UNIX:
				ret.append( qrand() % now_tstamp);
				break;
			case Populator::T_DT_RAND_JULIAN:
			{
				QDateTime dt;
				dt.setTime_t( qrand() % now_tstamp );
				ret.append(getJulianFromUnix(dt.toTime_t()));
				break;
			}
			default:
				QMessageBox::critical(this, "Critical error",
								   QString("PopulatorDialog::dateValues unknown type %1").arg(c.userValue));
		} // switch
	} // for

	return ret;
}
