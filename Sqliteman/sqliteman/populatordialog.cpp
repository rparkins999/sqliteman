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
#include <QSettings>
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
	resultEdit->setHtml("");
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("populator/height", QVariant(500)).toInt();
	int ww = settings.value("populator/width", QVariant(600)).toInt();
	resize(ww, hh);
	columnTable->horizontalHeader()->setStretchLastSection(true);

	QList<FieldInfo> fields = Database::tableFields(m_table, m_schema);
	columnTable->clearContents();
	columnTable->setRowCount(fields.size());
	QRegExp sizeExp("\\(\\d+\\)");
	for(int i = 0; i < fields.size(); ++i)
	{
		Populator::PopColumn col;
		col.name = fields[i].name;
		col.type = fields[i].type;
		col.pk = fields[i].isPartOfPrimaryKey;
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
		nameItem->setFlags(Qt::NoItemFlags);
		QTableWidgetItem * typeItem = new QTableWidgetItem(col.type);
		typeItem->setFlags(Qt::NoItemFlags);
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

PopulatorDialog::~PopulatorDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("populator/height", QVariant(height()));
    settings.setValue("populator/width", QVariant(width()));
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
				  + Utils::q(m_schema)
				  + "."
				  + Utils::q(m_table)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if (!execSql(sql, tr("Cannot get statistics for table")))
	{
		return -1;
	}
	while(query.next())
		return query.value(0).toLongLong();
	return -1;
}

QString PopulatorDialog::sqlColumns()
{
	QStringList s;
    QList<Populator::PopColumn>::const_iterator i;
    for (i = m_columnList.constBegin(); i != m_columnList.constEnd(); ++i) {
		if (i->action != Populator::T_IGNORE) { s.append(i->name); }
	}
	return Utils::q(s, "\"");
}

void PopulatorDialog::populateButton_clicked()
{
	// Avoid QVariantList extension because it doesn't work for column names
	// containing special characters
	resultEdit->setHtml("");
	m_columnList.clear();
	for (int i = 0; i < columnTable->rowCount(); ++i)
		m_columnList.append(qobject_cast<PopulatorColumnWidget*>(columnTable->cellWidget(i, 2))->column());

	QList<QVariantList> values;
    QList<Populator::PopColumn>::const_iterator it;
    for (it = m_columnList.constBegin(); it != m_columnList.constEnd(); ++it) {
		switch (it->action)
		{
			case Populator::T_AUTO:
				values.append(autoValues(*it));
				break;
			case Populator::T_AUTO_FROM:
				values.append(autoFromValues(*it));
				break;
			case Populator::T_NUMB:
				values.append(numberValues(*it));
				break;
			case Populator::T_TEXT:
				values.append(textValues(*it));
				break;
			case Populator::T_PREF:
				values.append(textPrefixedValues(*it));
				break;
			case Populator::T_STAT:
				values.append(staticValues(*it));
				break;
			case Populator::T_DT_NOW:
			case Populator::T_DT_NOW_UNIX:
			case Populator::T_DT_NOW_JULIAN:
			case Populator::T_DT_RAND:
			case Populator::T_DT_RAND_UNIX:
			case Populator::T_DT_RAND_JULIAN:
				values.append(dateValues(*it));
				break;
			case Populator::T_IGNORE:
				break;
		};
	}

	if (!execSql("SAVEPOINT POPULATOR;", tr("Cannot create savepoint")))
	{
		execSql("ROLLBACK TO POPULATOR;", tr("Cannot roll back after error"));
		return;
	}

	qlonglong cntPre, cntPost;
	resultEdit->clear();

	cntPre = tableRowCount();
	QSqlQuery query(QSqlDatabase::database(SESSION_NAME));
	for (int i = 0; i < spinBox->value(); ++i)
	{
		QStringList slr;
		for (int j = 0; j < values.count(); ++j)
		{
			slr.append(Utils::q(values.at(j).at(i).toString()));
		}
		QString sql = QString("INSERT ")
					  + (constraintBox->isChecked() ? "OR IGNORE" : "")
					  + " INTO "
					  + Utils::q(m_schema)
					  + "."
					  + Utils::q(m_table)
					  + " ("
					  + sqlColumns()
					  + ") VALUES ("
					  + slr.join(",")
					  + ");";

		query.prepare(sql);
		if (!query.exec())
		{
			QString errtext = tr("Cannot insert values")
							  + ":<br/><span style=\" color:#ff0000;\">"
							  + query.lastError().text()
							  + "<br/></span>" + tr("using sql statement:")
							  + "<br/><tt>" + sql;
			resultAppend(errtext);
			if (!constraintBox->isChecked()) { break; }
		}
		else { updated = true; }
	}

	if (!execSql("RELEASE POPULATOR;", tr("Cannot release savepoint")))
	{
		if (!execSql("ROLLBACK TO POPULATOR;", tr("Cannot roll back either")))
		{
			resultAppend(tr(
				"Database may be left with a pending savepoint."));
		}
		updated = false;
		return;
	}

	cntPost = tableRowCount();

	if (cntPre != -1 && cntPost != -1)
		resultAppend(tr("Row(s) inserted: %1").arg(cntPost-cntPre));
}

QVariantList PopulatorDialog::autoValues(Populator::PopColumn c)
{
	QString sql = QString("select max(")
				  + Utils::q(c.name)
				  + ") from "
				  + Utils::q(m_schema)
				  + "."
				  + Utils::q(m_table)
				  + ";";

	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		QString errtext = tr("Cannot get MAX() for column ")
						  + c.name
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + query.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + sql;
		resultAppend(errtext);
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

bool PopulatorDialog::execSql(const QString & statement, const QString & message)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if(query.lastError().isValid())
	{
		QString errtext = message
						  + ":<br/><span style=\" color:#ff0000;\">"
						  + query.lastError().text()
						  + "<br/></span>" + tr("using sql statement:")
						  + "<br/><tt>" + statement;
		resultAppend(errtext);
		return false;
	}
	return true;
}

void PopulatorDialog::resultAppend(QString text)
{
	resultEdit->append(text);
	int lh = QFontMetrics(resultEdit->currentFont()).lineSpacing();
	QTextDocument * doc = resultEdit->document();
	if (doc)
	{
		int h = (int)(doc->size().height());
		if (h < lh * 2) { h = lh * 2 + lh / 2; }
		resultEdit->setFixedHeight(h + lh / 2);
	}
	else
	{
		int lines = text.split("<br/>").count() + 1;
		resultEdit->setFixedHeight(lh * lines);
	}
}
