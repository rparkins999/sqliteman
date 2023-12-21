/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QInputDialog>
#include <QtCore/QFileInfo>

#include "database.h"
#include "dataviewer.h"
#include "extensionmodel.h"
#include "litemanwindow.h"
#include "preferences.h"
#include "schemabrowser.h"
#include "sqlmodels.h"
#include "utils.h"


SchemaBrowser::SchemaBrowser(QWidget * parent, Qt::WindowFlags f)
	: QWidget(parent, f)
{
	setupUi(this);

	m_extensionModel = new ExtensionModel(this);
	extensionTableView->setModel(m_extensionModel);

#ifndef ENABLE_EXTENSIONS
	schemaTabWidget->setTabEnabled(2, false);
#endif
	connect(schemaTabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(tabWidget_currentChanged(int)));

	connect(setPragmaButton, SIGNAL(clicked()), this, SLOT(setPragmaButton_clicked()));
}

void SchemaBrowser::buildPragmasTree()
{
	int row = pragmaTable->currentRow();
	disconnect(pragmaTable, SIGNAL(currentCellChanged(int, int, int, int)),
			   this, SLOT(pragmaTable_currentCellChanged(int, int, int, int)));
	pragmaTable->clearContents();
	pragmaTable->setRowCount(0);

	addPragma("analysis_limit", "editable", "integer");
	addPragma("application_id", "editable", "integer");
	addPragma("auto_vacuum", "editable", "0, 1, 2");
	addPragma("automatic_index", "editable", "0 or 1");
	addPragma("busy_timeout", "editable", "milliseconds");
	addPragma("cache_size", "editable", "pages or -kbytes");
	addPragma("cache_spill", "editable", "pages");
	addPragma("case_sensitive_like", "editable", "0 or 1");
	addPragma("cell_size_check", "editable", "0 or 1");
	addPragma("checkpoint_fullfsync", "editable", "0 or 1");
	addPragma("collation_list", "table", "");
	addPragma("compile_options", "table", "");
	addPragma("count_changes", "editable", "0 or 1");
	addPragma("data_version", "read-only", "");
	addPragma("database_list", "table", "");
	addPragma("default_cache_size", "editable", "pages");
	addPragma("defer_foreign_keys", "editable", "");

    /* This one is actually editable, but it's dangerous to do so. */
	addPragma("empty_result_callbacks", "read-only", "0 or 1");
	addPragma("encoding", "editable", "UTF-8, UTF-16, UTF16le, UTF16be");
	addPragma("foreign_key_check", "table", "");
	addPragma("foreign_key_list", "table", "");
	addPragma("foreign_keys", "editable", "0 or 1");
	addPragma("freelist_count", "read-only", "");
	addPragma("full_column_names", "editable", "0 or 1");
	addPragma("fullfsync", "editable", "0 or 1");
	addPragma("function_list", "table", "");
	addPragma("hard_heap_limit", "editable", "integer");
	addPragma("ignore_check_constraints", "editable", "0 or 1");
	addPragma("index_info", "table", "");
	addPragma("index_list", "table", "");
	addPragma("index_xinfo", "table", "");
	addPragma("integrity_check", "table", "");
	addPragma("journal_mode", "editable",
              "delete, truncate, persist, memory, wal, off");
	addPragma("journal_size_limit", "editable", "bytes");
	addPragma("legacy_alter_table", "editable", "0 or 1");
	addPragma("locking_mode", "editable", "normal, exclusive");
	addPragma("max_page_count", "editable", "pages");
	addPragma("mmap_size", "editable", "bytes");
	addPragma("module_list", "table", "");
	addPragma("page_count", "read-only", "");
	addPragma("page_size", "read-only", "bytes");
	addPragma("pragma_list", "table", "");
	addPragma("query_only", "editable", "0 or 1");
	addPragma("quick_check", "table", "");
	addPragma("read_uncommitted", "editable", "0 or 1");
	addPragma("recursive_triggers", "editable", "0 or 1");
	addPragma("reverse_unordered_selects", "editable", "0 or 1");
    
    /* This one is actually editable, but it's dangerous to do so. */
	addPragma("schema_version", "read-only", "");
	addPragma("secure_delete", "editable", "0 or 1 or fast");
	addPragma("short_column_names", "editable", "0 or 1");
	addPragma("soft_heap_limit", "editable", "bytes");
	addPragma("synchronous", "editable", "0, 1, 2, 3");
	addPragma("table_info", "table", "");
	addPragma("table_list", "table", "");
	addPragma("table_xinfo", "table", "");
	addPragma("temp_store", "editable", "0, 1, 2");
	addPragma("threads", "editable", "integer");
	addPragma("user_version", "editable", "integer");
	addPragma("wal_autocheckpoint", "editable", "integer");

	if (row < 0) { row = 0; }
	pragmaTable_currentCellChanged(row, 0, 0, 0);
	pragmaTable->setCurrentItem(pragmaTable->item(row, 0));
	connect(pragmaTable, SIGNAL(currentCellChanged(int, int, int, int)),
		    this, SLOT(pragmaTable_currentCellChanged(int, int, int, int)));
}

void SchemaBrowser::addPragma(
    const QString & name, const QString & editable, const QString& valueshint)
{
	int row = pragmaTable->rowCount();
	pragmaTable->setRowCount(row + 1);
	QTableWidgetItem * twi = new QTableWidgetItem(name);
	twi->setToolTip(editable);
	pragmaTable->setItem(row, 0, twi);
	QString value(Database::pragma(name));
	twi = new QTableWidgetItem(value);
	twi->setToolTip(valueshint);
	pragmaTable->setItem(row, 1, twi);
}

void SchemaBrowser::tabWidget_currentChanged(int index)
{
	if (index == 1)
	{
		Utils::setColumnWidths(pragmaTable);
	}
}

void SchemaBrowser::pragmaTable_currentCellChanged(int currentRow, int /*currentColumn*/, int /*previousRow*/, int /*previousColumn*/)
{
    QString name(pragmaTable->item(currentRow, 0)->text());
    QString value(pragmaTable->item(currentRow, 1)->text());
	pragmaName->setText(name);
    if (pragmaTable->item(currentRow, 0)->toolTip().compare("table") == 0)
    {
        pragmaValue->setText("");
        pragmaValue->setReadOnly(true);
        value_label->setText("");
        setPragmaButton->setEnabled(false);
#ifdef WIN32
        // win windows are always top when there is this parent
        DataViewer *w = new DataViewer(0);
#else
        DataViewer *w = new DataViewer(qobject_cast<LiteManWindow *>(parentWidget()));
#endif
        SqlQueryModel *qm = new SqlQueryModel(w);
        w->setAttribute(Qt::WA_DeleteOnClose);
        Preferences * prefs = Preferences::instance();
        resize(prefs->dataviewerWidth(), prefs->dataviewerHeight());

        QString title(QString("pragma") + " " + name);
        w->setWindowTitle(title);
        qm->setQuery(title + ";", QSqlDatabase::database(SESSION_NAME));
        qm->attach();
        qm->fetchAll();

        w->setTableModel(qm);
        w->ui.mainToolBar->hide();
        w->ui.actionRipOut->setEnabled(false);
        w->ui.actionClose->setEnabled(true);
        w->ui.actionClose->setVisible(true);
        w->ui.tabWidget->removeTab(2);
        w->show();
    } else if (pragmaTable->item(currentRow, 0)->toolTip().compare("editable"))
    {
        pragmaValue->setText(pragmaTable->item(currentRow, 1)->text());
        pragmaValue->setReadOnly(true);
        value_label->setText(QString("Value"));
        setPragmaButton->setEnabled(false);
    }
    else // read-only
    {
        pragmaValue->setText(value);
        pragmaValue->setReadOnly(false);
        value_label->setText(QString("Value (")
            + pragmaTable->item(currentRow, 1)->toolTip() + ")");
        setPragmaButton->setEnabled(true);
    }
}

void SchemaBrowser::setPragmaButton_clicked()
{
    Database::execSql(QString("PRAGMA main.")
                    + pragmaName->text()
                    + " = "
                    + pragmaValue->text()
                    + ";");
    buildPragmasTree();
}

void SchemaBrowser::appendExtensions(const QStringList & list, bool switchToTab)
{
	QStringList l(m_extensionModel->extensions());
    QStringList::const_iterator i;
    for (i = list.constBegin(); i != list.constEnd(); ++i) {
		l.removeAll(*i);
		l.append(*i);
	}
	m_extensionModel->setExtensions(l);
	extensionTableView->resizeColumnsToContents();

	if (switchToTab)
		schemaTabWidget->setCurrentIndex(2);
}
