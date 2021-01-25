/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QCheckBox>
#include <QMessageBox>
#include <QPalette>
#include <QSettings>
#include <QTableWidget>

#include "mylineedit.h"
#include "sqlparser.h"
#include "tableeditordialog.h"
#include "utils.h"

TableEditorDialog::TableEditorDialog(LiteManWindow * parent)
{
	creator = parent;
	updated = false;
	ui.setupUi(this);
	
	QSettings settings("yarpen.cz", "sqliteman");
	int hh = settings.value("tableeditor/height", QVariant(600)).toInt();
	int ww = settings.value("tableeditor/width", QVariant(800)).toInt();
	resize(ww, hh);
	m_prefs = Preferences::instance();

	ui.databaseCombo->addItems(Database::getDatabases().keys());
	m_dirty = false;

	connect(ui.columnTable, SIGNAL(itemSelectionChanged()),
			this, SLOT(fieldSelected()));
	connect(ui.addButton, SIGNAL(clicked()), this, SLOT(addField()));
	connect(ui.removeButton, SIGNAL(clicked()), this, SLOT(removeField()));
	connect(ui.tabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(tabWidget_currentChanged(int)));
	connect(ui.databaseCombo, SIGNAL(currentIndexChanged(int)),
			this, SLOT(tableNameChanged()));
	connect(ui.nameEdit, SIGNAL(textEdited(const QString&)),
			this, SLOT(tableNameChanged()));
	connect(ui.withoutRowid, SIGNAL(toggled(bool)),
			this, SLOT(checkChanges()));
	resizeWanted = true;
}

TableEditorDialog::~TableEditorDialog()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("tableeditor/height", QVariant(height()));
    settings.setValue("tableeditor/width", QVariant(width()));
}

void TableEditorDialog::addField(QString oldName, QString oldType,
								 int x, QString oldDefault)
{
	bool useNull = m_prefs->nullHighlight();
	QString nullText = m_prefs->nullHighlightText();

	int rc = ui.columnTable->rowCount();
	ui.columnTable->setRowCount(rc + 1);

	MyLineEdit * name = new MyLineEdit(this);
	name->setText(oldName);
	name->setToolTip(oldName);
	name->setFrame(false);
	connect(name, SIGNAL(textEdited(const QString &)),
			this, SLOT(checkChanges()));
	ui.columnTable->setCellWidget(rc, 0, name);

	QComboBox * typeBox = new QComboBox(this);
	QStringList types;
	if (useNull && !nullText.isEmpty()) { types << nullText; }
	else { types << "NULL"; }
	types << "TEXT" << "INTEGER" << "REAL" << "BLOB";
	typeBox->addItems(types);
	typeBox->setEditable(true);
	int n;
	if (oldType.isEmpty())
	{
		n = 0;
	}
	else
	{
		n = typeBox->findText(oldType, Qt::MatchFixedString
									   | Qt::MatchCaseSensitive);
		if (n <= 0)
		{
			n = typeBox->count();
			typeBox->addItem(oldType);
		}
	}
	typeBox->setCurrentIndex(n);
	connect(typeBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(checkChanges()));
	connect(typeBox, SIGNAL(editTextChanged(const QString &)),
			this, SLOT(checkChanges()));
	ui.columnTable->setCellWidget(rc, 1, typeBox);

	QComboBox * extraBox = new QComboBox(this);
	QStringList extras;
	extras << "" << "NOT NULL" << "PRIMARY KEY" << "AUTOINCREMENT";
	extraBox->addItems(extras);
	extraBox->setCurrentIndex(x);
	extraBox->setEditable(false);
	ui.columnTable->setCellWidget(rc, 2, extraBox);
	connect(extraBox, SIGNAL(currentIndexChanged(const QString &)),
			this, SLOT(checkChanges()));

	MyLineEdit * defval = new MyLineEdit(this);
	defval->setText(oldDefault);
	if (useNull && oldDefault.isNull())
	{
		defval->setPlaceholderText(nullText);
	}
	defval->setFrame(false);
	connect(defval, SIGNAL(textEdited(const QString &)),
			this, SLOT(checkChanges()));
	ui.columnTable->setCellWidget(rc, 3, defval);
	ui.columnTable->setCurrentCell(rc, 0);
	resizeWanted = true;
}

QString TableEditorDialog::getFullName()
{
	return QString("CREATE %1 ").arg(m_tableOrView)
		   + Utils::q(ui.databaseCombo->currentText())
		   + "."
		   + Utils::q(ui.nameEdit->text());
}

QString TableEditorDialog::getSQLfromDesign()
{
	QString sql;
	bool first = true;
	QStringList primaryKeys;
	for (int i = 0; i < ui.columnTable->rowCount(); i++)
	{
		if (first)
		{
			first = false;
		}
		else
		{
			sql += ",";
		}
		MyLineEdit * ed =
			qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(i, 0));
		QString name(ed->text());
		sql += Utils::q(name);
		QComboBox * box =
			qobject_cast<QComboBox *>(ui.columnTable->cellWidget(i, 1));
		QString type(box->currentText());
		if (box->currentIndex())
		{
			sql += " " + Utils::q(type);
		}
		box = qobject_cast<QComboBox *>(ui.columnTable->cellWidget(i, 2));
		QString extra(box->currentText());
		if (extra.contains("AUTOINCREMENT"))
		{
			sql += " PRIMARY KEY AUTOINCREMENT";
		}
		else if (extra.contains("PRIMARY KEY"))
		{
			primaryKeys.append(name);
		}
		if (extra.contains("NOT NULL"))
		{
			sql += " NOT NULL";
		}
		ed = qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(i, 3));
		QString defval(ed->text());
		if (!defval.isEmpty())
		{
			sql += " DEFAULT " + defval;
		}
	}
	if (primaryKeys.count() > 0)
	{
		sql += ",\nPRIMARY KEY ( ";
		sql += Utils::q(primaryKeys, "\"");
		sql += " )";
	}
	sql += ")";
	if (ui.withoutRowid->isChecked())
	{
		sql += " WITHOUT ROWID ";
	}
	sql += ";";
	return sql;
}

QString TableEditorDialog::getSQLfromGUI()
{
	QWidget * w = ui.tabWidget->currentWidget();
	if (w == ui.designTab)
	{
		return getSQLfromDesign();
	}
	else if (w == ui.queryTab)
	{
		return ui.queryEditor->statement();
	}
	else
	{
		return ui.textEdit->text();
	}
}

void TableEditorDialog::addField()
{
	addField(QString(), QString(), 0, QString());
	checkChanges();
	resizeWanted = true;
}

void TableEditorDialog::removeField()
{
	ui.columnTable->removeRow(ui.columnTable->currentRow());
	if (ui.columnTable->rowCount() == 0)
	{
		addField();
	}
	else
	{
		checkChanges();
	}
	resizeWanted = true;
}

void TableEditorDialog::fieldSelected()
{
	ui.removeButton->setEnabled(ui.columnTable->selectedRanges().count() != 0);
}

bool TableEditorDialog::checkOk(QString newName)
{
	bool ok = !newName.isEmpty();
	if (ui.tabWidget->currentWidget() == ui.designTab)
	{
		int pkCount = 0;
		int cols = ui.columnTable->rowCount();
		int colsLeft = cols;
		bool autoSeen = false;
        MyLineEdit * edit;
		for (int i = 0; i < cols; i++)
		{
            QString cname = (
                qobject_cast<MyLineEdit*>
                    (ui.columnTable->cellWidget(i, 0)))->text();
			QComboBox * types =
				qobject_cast<QComboBox*>(ui.columnTable->cellWidget(i, 1));
			QString ctype(types->currentText());
			QComboBox * extras =
				qobject_cast<QComboBox*>(ui.columnTable->cellWidget(i, 2));
			QString cextra(extras->currentText());
			if (checkColumn(i, cname, ctype, cextra))
			{
				--colsLeft;
				continue;
			}
			if (cname.isEmpty()) { m_dubious = true; }
			for (int j = 0; j < i; ++j)
			{
				edit =
					qobject_cast<MyLineEdit*>(ui.columnTable->cellWidget(j, 0));
				bool b =
					(edit->text().compare(cname, Qt::CaseInsensitive) != 0);
				ok &= b;
			}
			if (cextra.contains("AUTOINCREMENT"))
			{
				if (ctype.compare("INTEGER", Qt::CaseInsensitive))
				{
					ok = false;
					break;
				}
				autoSeen = true;
			}
			if (cextra.contains("PRIMARY KEY")) { ++pkCount; }
            edit = qobject_cast<MyLineEdit*>
                (ui.columnTable->cellWidget(i, 3));
            QPalette p = edit->palette();
            QString defaultvalue(edit->text());
			if (   (defaultvalue.isEmpty())
                || (SqlParser().isValidDefault(defaultvalue)))
            {
                p.setColor(QPalette::Text, QColor("#000000"));
            } else {
                p.setColor(QPalette::Text, QColor("#FF0000"));
                ok = false;
            }
            edit->setPalette(p);
		}
		if (   (   (ui.withoutRowid->isChecked())
				&& ((pkCount == 0) || autoSeen))
			|| (autoSeen && (pkCount > 0))
			|| (colsLeft == 0))
		{ ok = false; }
	}
	return ok;
}

void TableEditorDialog::resultAppend(QString text)
{
	ui.resultEdit->append(text);
	int lh = QFontMetrics(ui.resultEdit->currentFont()).lineSpacing();
	QTextDocument * doc = ui.resultEdit->document();
	if (doc)
	{
		int h = (int)(doc->size().height());
		if (h < lh * 2) { h = lh * 2 + lh / 2; }
		ui.resultEdit->setFixedHeight(h + lh / 2);
	}
	else
	{
		int lines = text.split("<br/>").count() + 1;
		ui.resultEdit->setFixedHeight(lh * lines);
	}
}

void TableEditorDialog::setFirstLine()
{
	QString fl(getFullName());
	if (m_oldWidget == ui.designTab) { fl += " ( "; }
	else if (m_oldWidget == ui.queryTab) { fl += " AS "; }
	ui.firstLine->setText(fl);
}

void TableEditorDialog::setDirty()
{
	m_dirty = true;
}

void TableEditorDialog::resizeTable()
{
	Utils::setColumnWidths(ui.columnTable);
}

void TableEditorDialog::resizeEvent(QResizeEvent * event)
{
	resizeWanted = true;
	QDialog::resizeEvent(event);
}

void TableEditorDialog::paintEvent(QPaintEvent * event)
{
	if (resizeWanted)
	{
		resizeTable();
		resizeWanted = false;
	}
	QDialog::paintEvent(event);
}

void TableEditorDialog::tableNameChanged()
{
	setFirstLine();
	checkChanges();
}

void TableEditorDialog::tabWidget_currentChanged(int index)
{
	if (ui.tabWidget->widget(index) == ui.sqlTab)
	{
		QWidget * w = ui.tabWidget->widget(m_tabWidgetIndex);
		if (w != ui.sqlTab)
		{
			m_oldWidget = w;
			setFirstLine();
			bool getFromTab = true;
			if (m_dirty)
			{
				int com = QMessageBox::question(this, tr("Sqliteman"),
					tr("Do you want to keep the current content"
					   " of the SQL editor?."
					   "\nYes to keep it,\nNo to create from the %1 tab"
					   "\nCancel to return to the %1 tab")
					.arg(ui.tabWidget->tabText(m_tabWidgetIndex)),
					QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
				if (com == QMessageBox::Yes) { getFromTab = false; }
				else if (com == QMessageBox::Cancel)
				{
					ui.tabWidget->setCurrentIndex(m_tabWidgetIndex);
					return;
				}
			}
			if (getFromTab)
			{
				if (m_oldWidget == ui.designTab)
				{
					ui.textEdit->setText(getSQLfromDesign());
				}
				else if (m_oldWidget == ui.queryTab)
				{
					ui.textEdit->setText(ui.queryEditor->statement());
				}
				setDirty();
			}
		}
		ui.adviceLabel->hide();
	}
	else
	{
		m_oldWidget = ui.tabWidget->widget(index);
		setFirstLine();
		if (ui.tabWidget->indexOf(ui.sqlTab) >= 0)
		{
			ui.adviceLabel->show();
		}
	}
	m_tabWidgetIndex = index;
	checkChanges();
}

QString TableEditorDialog::schema()
{
	return ui.databaseCombo->currentText();
}

QString TableEditorDialog::createdName()
{
	return ui.nameEdit->text();
}

