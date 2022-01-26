/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QtCore/qmath.h>
#include <QCheckBox>
#include <QColor>
#include <QMessageBox>
#include <QPalette>
#include <QSqlQuery>
#include <QSqlError>
#include <QTableWidget>
#include <QtCore/QTimer>
#include <QTreeWidgetItem>

#include "litemanwindow.h"
#include "mylineedit.h"
#include "preferences.h"
#include "sqlparser.h"
#include "tableeditordialog.h"
#include "tabletree.h"
#include "utils.h"

/* This horrible hack works around an "optimisation" in Qt which causes a
 * QComboBox not to be resized correctly the first time it is drawn after
 * its column has been resized. */
void TableEditorDialog::fudgeSlot()
{
    resize(m_rect.width(), m_rect.height());
    m_fudging = false;
}

void TableEditorDialog::fudge()
{
    resizeTable();
    if (!m_fudging) { // prevent re-entrant use of m_rect
        m_fudging = true;
        m_rect = geometry();
        resize(m_rect.width() + 1, m_rect.height());
        /* The number 20 (milliseconds) here can be operating system dependent:
         * it needs to be big enough for at least one clock tick. */
        QTimer::singleShot(20, this, SLOT(fudgeSlot()));
    }
}

TableEditorDialog::TableEditorDialog(LiteManWindow * parent)
    : DialogCommon(parent)
{
	ui.setupUi(this);
    Preferences * prefs = Preferences::instance();
	m_useNull = prefs->nullHighlight();
	m_nullText = prefs->nullHighlightText();
    m_nullColor = prefs->nullHighlightColor();
	m_dirty = false;
    m_tableOrView = QString();
    m_originalName = QString();
    fixNull(ui.nameEdit);
	connect(ui.nameEdit, SIGNAL(textEdited(const QString&)),
			this, SLOT(tableNameChanged()));
	connect(ui.columnTable, SIGNAL(itemSelectionChanged()),
			this, SLOT(fieldSelected()));
	connect(ui.addButton, SIGNAL(clicked()), this, SLOT(addField()));
	connect(ui.removeButton, SIGNAL(clicked()), this, SLOT(removeField()));
	connect(ui.withoutRowid, SIGNAL(toggled(bool)),
			this, SLOT(checkChanges()));
    setResultEdit(ui.resultEdit);
}

TableEditorDialog::~TableEditorDialog() {}

void TableEditorDialog::fixTabWidget() // ... after possibly deleting some tabs
{
	m_tabWidgetIndex = 0;
    m_oldWidget = (ui.tabWidget->widget(0));
    ui.tabWidget->setCurrentIndex(0);
	connect(ui.tabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(tabWidget_currentChanged(int)));
}

void TableEditorDialog::fixNull(QLineEdit * edit)
{
    if (m_useNull) {
        edit->setPlaceholderText(m_nullText);
        QPalette p = edit->palette();
        if (edit->text().isEmpty()) {
            p.setColor(QPalette::Base, m_nullColor);
        } else {
            p.setColor(QPalette::Base,
                        QApplication::palette().color(QPalette::Base));
        }
        edit->setPalette(p);
    }
}

void TableEditorDialog::addField(FieldInfo field)
{
    bool isNew = field.name.isNull();
	int rc = ui.columnTable->rowCount();
	ui.columnTable->setRowCount(rc + 1);
	MyLineEdit * name = new MyLineEdit(this);
	name->setText(field.name);
    if (!isNew) {
        name->setToolTip(QString("Original name ") + field.name);
    }
	name->setFrame(false);
    fixNull(name);
	connect(name, SIGNAL(textEdited(const QString &)),
			this, SLOT(lineEdited()));
	ui.columnTable->setCellWidget(rc, 0, name);

	QComboBox * typeBox = new QComboBox(this);
    typeBox->setInsertPolicy(QComboBox::InsertAtTop);
    typeBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	QStringList types;
	types << QString();
	types << "BLOB" << "INTEGER" << "REAL" << "TEXT";
	typeBox->addItems(types);
	int n;
	if (isNew || field.type.isEmpty()) { n = 0; }
	else {
		n = typeBox->findText(
            field.type, Qt::MatchFixedString | Qt::MatchCaseSensitive);
		if (n <= 0)
		{
			n = typeBox->count();
			typeBox->addItem(field.type);
		}
	}
    typeBox->setEditable(true);
	typeBox->setCurrentIndex(n);
    typeBox->setToolTip(tr("Type affinity for column"));
    fixNull(typeBox->lineEdit());
	connect(typeBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(typeIndexChanged(int)));
	connect(typeBox, SIGNAL(editTextChanged(const QString &)),
			this, SLOT(typeEdited(const QString &)));
	ui.columnTable->setCellWidget(rc, 1, typeBox);

    //FIXME replace this with something to handle foreign keys
    // maybe a popup instead of a QComboBox?
	QComboBox * extraBox = new QComboBox(this);
    extraBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	QStringList extras;
	extras << QString()            // 0
           << "NOT NULL"           // 1
           << "UNIQUE, NOT NULL"   // 2
           << "PRIMARY KEY"        // 3
           << "AUTOINCREMENT"      // 4
           << "UNIQUE";            // 6
	extraBox->addItems(extras);
    int index = 0;
    if (!isNew) {
        if (field.isNotNull) { index = field.isUnique ? 2 : 1; }
        else if (field.isAutoIncrement) { index = 4; }
        else if (field.isPartOfPrimaryKey) { index = 3; }
        else if (field.isUnique) { index = 5; }
    }
    extraBox->setCurrentIndex(index);
	extraBox->setEditable(false);
    extraBox->setToolTip("Extra features for column");
	ui.columnTable->setCellWidget(rc, 2, extraBox);
	connect(extraBox, SIGNAL(currentIndexChanged(const QString &)),
			this, SLOT(checkChanges()));
	MyLineEdit * defval = new MyLineEdit(this);
    if (!isNew) { defval->setText(field.defaultValue); }
	defval->setFrame(false);
    fixNull(defval);
    defval->setToolTip(tr("Default value for column"));
	ui.columnTable->setCellWidget(rc, 3, defval);
	connect(defval, SIGNAL(textEdited(const QString &)),
			this, SLOT(lineEdited()));
	ui.columnTable->setCurrentCell(rc, 0);
}

QString TableEditorDialog::schema()
{
	return ui.databaseCombo->currentText();
}

void TableEditorDialog::setItem(QTreeWidgetItem * item, bool makeView)
{
	ui.databaseCombo->addItems(m_creator->visibleDatabases());
    m_noTemp = (ui.databaseCombo->findText("temp", Qt::MatchFixedString) < 0);
    if (m_noTemp) { ui.databaseCombo->addItem("temp"); }
    int n = ui.databaseCombo->count();
    int i;
    QString tableName;
    bool databaseMayChange;
    bool tableMayChange;
    if (item) {
        m_databaseName = item->text(1);
		i = ui.databaseCombo->findText(m_databaseName, Qt::MatchFixedString);
        if (i < 0) {
            if (n > 0) {
                i = 0;
                m_databaseName = ui.databaseCombo->itemText(0);
                databaseMayChange = true;
            } else {
                m_databaseName.clear();
                databaseMayChange = false;
            }
            tableMayChange = true;
        } else {
            databaseMayChange = m_noTemp;
            if (item->type() == TableTree::TableType) {
                tableName = item->text(0);
                tableMayChange = false;
            } else { tableMayChange = true; }
        }
    } else {
        i = 0;
        m_databaseName = ui.databaseCombo->itemText(0);
        databaseMayChange = true;
        tableMayChange = true;
    }
    ui.databaseCombo->setCurrentIndex(i);
    databaseMayChange &= (n > 1);
    if (databaseMayChange) {
        connect(ui.databaseCombo, SIGNAL(currentIndexChanged(const QString &)),
                this, SLOT(databaseChanged(const QString &)));
    }
    ui.databaseCombo->setEnabled(databaseMayChange);
    if (makeView) {
        m_tableOrView = "VIEW";
        databaseMayChange = m_databaseName == "temp";
    } else {
        m_tableOrView = "TABLE";
        databaseMayChange = true;
    }
    ui.queryEditor->setSchema(
        m_databaseName, tableName, databaseMayChange, tableMayChange);
}

QString TableEditorDialog::createdName()
{
	return ui.nameEdit->text();
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
    Preferences * prefs = Preferences::instance();
    int width = prefs->textWidthMarkSize();
	QString sql;
    QString line;
	bool first = true;
	QStringList primaryKeys;
	for (int i = 0; i < ui.columnTable->rowCount(); i++)
	{
		QString column;
		MyLineEdit * ed =
			qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(i, 0));
		QString name(ed->text());
		column = Utils::q(name);
		QComboBox * box =
			qobject_cast<QComboBox *>(ui.columnTable->cellWidget(i, 1));
		QString type(box->currentText());
		if (!type.isEmpty())
		{
            if (box->currentIndex() == 0) {
                type = Utils::q(type);
            }
			column += " " + type;
		}
		box = qobject_cast<QComboBox *>(ui.columnTable->cellWidget(i, 2));
		QString extra(box->currentText());
		if (extra.contains("AUTOINCREMENT"))
		{
			column += " PRIMARY KEY AUTOINCREMENT";
		}
		else if (extra.contains("PRIMARY KEY"))
		{
			primaryKeys.append(name);
		}
		if (extra.contains("NOT NULL"))
		{
			column += " NOT NULL";
		}
		ed = qobject_cast<MyLineEdit *>(ui.columnTable->cellWidget(i, 3));
		QString defval(ed->text());
		if (!defval.isEmpty())
		{
			column += " DEFAULT " + defval;
		}
		if (line.size() + column.size() >= width) {
            if (first) { first = false; } else { line += ","; }
            sql += line + "\n";
            line = column;
        } else {
            if (first) { first = false; } else { line += ", "; }
            line += column;
        }
	}
	sql += line;
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

QString TableEditorDialog::getSQLfromGUI(QWidget * w)
{
	if (w == ui.designTab) {
		return getSQLfromDesign();
	} else if (w == ui.queryTab) {
		return ui.queryEditor->statement();
	} else {
		return ui.textEdit->text();
	}
}

QString TableEditorDialog::getSql()
{
    return ui.firstLine->text() + getSQLfromGUI(ui.tabWidget->currentWidget());
}

void TableEditorDialog::addField()
{
    FieldInfo field;
    field.name = QString();
	addField(field);
}

void TableEditorDialog::removeField()
{
    if (ui.columnTable->rowCount() > 1) {
        ui.columnTable->removeRow(ui.columnTable->currentRow());
		checkChanges();
        fudge();
	}
}

void TableEditorDialog::fieldSelected()
{
    if (ui.removeButton) {
        ui.removeButton->setEnabled(
            ui.columnTable->selectedRanges().count() != 0);
    }
}

bool TableEditorDialog::checkOk()
{
    QString newName(ui.nameEdit->text());
    fixNull(ui.nameEdit);
	bool ok = !(newName.isEmpty() || m_databaseName.isEmpty());
	if (   (ui.tabWidget->currentWidget() == ui.designTab)
        && (m_tableOrView != "INDEX"))
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
            QLineEdit * editor = types->lineEdit();
            if (editor) { fixNull(editor); }
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
                p.setColor(QPalette::Text,
                           QApplication::palette().color(QPalette::Text));
            } else {
                p.setColor(QPalette::Text, QColor("#FF0000"));
                ok = false;
            }
            edit->setPalette(p);
            fixNull(edit);
		}
		if (   (   (ui.withoutRowid->isChecked())
				&& ((pkCount == 0) || autoSeen))
			|| (autoSeen && (pkCount > 0))
			|| (colsLeft == 0))
		{ ok = false; }
	}
	if (ok) { // skip costly database queries if we've already failed
        QString databaseName(Utils::q(m_databaseName));
        QString tableName(Utils::q(newName));
        QString fullname = databaseName + "." + tableName;
        if (   m_originalName.isNull()
            || (m_originalName.compare(fullname, Qt::CaseInsensitive) != 0))
        { // creating table or view with new name
            QString sql = "SELECT type FROM " + databaseName + "."
                        + "sqlite_master WHERE "
                        + "(type IS 'table' OR type is 'view' OR type is 'index')"
                        + " AND lower(name) IS lower("
                        + Utils::q(newName, "'") + ");";
            QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
            if (query.lastError().isValid())
            {
                QString errtext = tr("Cannot read schema for ")
                                + ui.databaseCombo->currentText()
                                + ":<br/><span style=\" color:#ff0000;\">"
                                + query.lastError().text()
                                + "<br/></span>" + tr("using sql statement:")
                                + "<br/><code>" + sql;
                resultAppend(errtext);
            } else if (query.first()) {
                ok = false; // new name is already a table or view or index
            }
            query.clear();
        }
    }
	return ok;
}

// base class version is never called, does nothing
bool TableEditorDialog::checkColumn(
        int i, QString cname, QString type, QString cextra)
{
    return true;
}

void TableEditorDialog::setFirstLine(QWidget * w)
{
	QString fl(getFullName());
    if (w == ui.sqlTab) { w = m_oldWidget; }
	if (w == ui.designTab) { fl += " ( "; }
	else if (w == ui.queryTab) { fl += " AS "; }
	ui.firstLine->setText(fl);
}

void TableEditorDialog::setDirty()
{
	m_dirty = true;
}

// special version for column with comboboxes or icons
void TableEditorDialog::resizeTable()
{
	QTableWidget * tv = ui.columnTable;
    if (tv->rowCount() > 0) { // only if table has at least one row
        int widthView = tv->viewport()->width();
        int widthLeft = widthView;
        int widthUsed = 0;
        int columns = tv->columnCount();
        int columnsLeft = columns;
        QVector<int> wantedWidths(columns);
        QVector<int> gotWidths(columns);
        tv->resizeColumnsToContents();
        int i;
        for (i = 0; i < columns; ++i) {
            wantedWidths[i] = tv->columnWidth(i);
            gotWidths[i] = 0;
        }
        i = 0;
        /* First give all "small" columns what they want.
         * If we allocate space for a small column, we go back and look again
         * because widthLeft / columnsLeft may be bigger now.
         */
        while (i < columns) {
            int width = wantedWidths[i];
            if (gotWidths[i] == 0) {
                if ((columnsLeft > 0) && (width <= widthLeft / columnsLeft))
                {
                    gotWidths[i] = width;
                    widthLeft -= width;
                    columnsLeft -= 1;
                    widthUsed = 0;
                    i = 0;
                    continue;
                }
            }
            widthUsed += width;
            ++i;
        }
        /* At this point either all columns are small and have been allocated,
         * or there isn't enough room to give all columns what they want.
         */
        if (widthUsed >= widthView) { // not enough room or exact fit
            float squeeze = ((float)widthView) / (float)widthUsed;
            // Don't squeeze too tight, let TableWidget make a scrollbar instead
            if (squeeze < 0.75) { squeeze = 0.75; }
            for (i = 0; i < columns; ++i) {
                int w = wantedWidths[i] * squeeze;
                tv->setColumnWidth(i, w);
            }
        } else { // share out extra space, but not to comboboxes
            QVector<bool> isCombo(columns);
            int nonCombos = 0;
            widthLeft = widthView;
            for (i = 0; i < columns; ++i) {
                QWidget * w = tv->cellWidget(0, i);
                if (w && w->inherits("QComboBox")) {
                    isCombo[i] = true;
                } else {
                    isCombo[i] = false;
                    ++nonCombos;
                }
                widthLeft -= wantedWidths[i];
            }
            for (i = 0; i < columns; ++i) {
                int width = wantedWidths[i];
                if (!isCombo[i]) {
                    width += widthLeft / nonCombos;
                }
                tv->setColumnWidth(i, width);
            }
        }
    }
}

void TableEditorDialog::resizeEvent(QResizeEvent * event)
{
	QDialog::resizeEvent(event);
    fudge();
}

void TableEditorDialog::tableNameChanged()
{
	setFirstLine(ui.tabWidget->currentWidget());
	checkChanges();
}

void TableEditorDialog::tabWidget_currentChanged(int index)
{
    QWidget * w = ui.tabWidget->widget(index);
	if (w == ui.sqlTab)
	{
		m_oldWidget = ui.tabWidget->widget(m_tabWidgetIndex);
		if (m_oldWidget != ui.sqlTab)
		{
			setFirstLine(m_oldWidget);
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
				ui.textEdit->setText(getSQLfromGUI(m_oldWidget));
				setDirty();
			}
		}
		ui.adviceLabel->hide();
	}
	else
	{
		setFirstLine(w);
        m_tabWidgetIndex = index;
		if (ui.tabWidget->indexOf(ui.sqlTab) >= 0)
		{
			ui.adviceLabel->show();
		}
	}
	checkChanges();
}

void TableEditorDialog::lineEdited() {
    fixNull((QLineEdit *)sender());
    checkChanges();
    resizeTable();
    updateGeometry();
}

void TableEditorDialog::typeEdited(QString text) {
    QComboBox * typeBox = (QComboBox *)sender();
    QLineEdit * editor = typeBox->lineEdit();
    if (typeBox->currentIndex() == 0) {
        typeBox->setItemText(0, editor->text());
    }
    fixNull(editor);
    checkChanges();
    resizeTable();
    updateGeometry();
}

void TableEditorDialog::typeIndexChanged(int index) {
    ((QComboBox *)sender())->setEditable(index == 0);
    checkChanges();
}
