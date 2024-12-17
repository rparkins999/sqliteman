/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QtCore/QDateTime>
#include <QtCore/QtDebug> //qDebug
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QtCore/QLocale>
#include <QMessageBox>
#include <QMimeData>
#include <QResizeEvent>
#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>

#include "blobpreviewwidget.h"
#include "database.h"
#include "dataexportdialog.h"
#include "dataviewer.h"
#include "finddialog.h"
#include "multieditdialog.h"
#include "preferences.h"
#include "sqltableview.h"
#include "sqlmodels.h"
#include "sqldelegate.h"
#include "utils.h"

// private methods

void DataViewer::updateButtons()
{
	int row = -1;
	bool haveRows;
	bool rowSelected = false;
	bool singleItem;
	bool editable;
	bool pending;
	bool canPreview;
	int tab = ui.tabWidget->currentIndex();
	QAbstractItemModel * model = ui.tableView->model();
	SqlTableModel * table = qobject_cast<SqlTableModel *>(model);
	QModelIndexList indexList = ui.tableView->selectedIndexes();
    QModelIndexList::const_iterator i;
    for (i = indexList.constBegin(); i != indexList.constEnd(); ++i) {
		if (i->isValid())
		{
			if (row == -1)
			{
				row = i->row();
				rowSelected = (row >= 0);
				singleItem = rowSelected;
			}
			else
			{
				singleItem = false;
				if (row != i->row())
				{
					rowSelected = false;
				}
			}
		}
	}
	activeRow = rowSelected ? row : -1;
	QVariant data;
	if (model)
	{
		if (table)
		{
			editable = true;
			pending = table->pendingTransaction();
		}
		else
		{
			editable = false;
			pending = false;
		}
		haveRows = model->rowCount() > 0;
		data = model->data(ui.tableView->currentIndex(), Qt::EditRole);
	}
	else
	{
		editable = false;
		pending = false;
		haveRows = false;
	}
	if (singleItem && (data.type() == QVariant::ByteArray))
	{
		QPixmap pm;
		pm.loadFromData(data.toByteArray());
		canPreview = !pm.isNull();
	}
	else
	{
		canPreview = false;
	}
	if (table && (m_finder == 0))
	{
		ui.actionFind->setEnabled(true);
		ui.actionFind->setToolTip(tr("Find... ") + "(Ctrl+Alt+F)");
	} else {
		ui.actionFind->setEnabled(false);
		ui.actionFind->setToolTip("(disabled)");
	}
	if (editable)
	{
		ui.actionNew_Row->setEnabled(true);
		ui.actionNew_Row->setToolTip(tr("New Row ") + "(Ctrl+Alt+N)");
		ui.actionShowChanges->setEnabled(true);
        if (showingChanges) {
            ui.actionShowChanges->setIcon(Utils::getIcon("unFindChanged.png"));
            ui.actionShowChanges->setToolTip(tr("Unshow Changes"));
        } else {
            ui.actionShowChanges->setIcon(Utils::getIcon("findChanged.png"));
            ui.actionShowChanges->setToolTip(tr("Show Changes"));
        }
	} else {
		ui.actionNew_Row->setEnabled(false);
		ui.actionNew_Row->setToolTip("(disabled)");
		ui.actionShowChanges->setEnabled(false);
		ui.actionShowChanges->setToolTip("(disabled)");
	}
	if (editable && rowSelected)
	{
		ui.actionCopy_Row->setEnabled(true);
		ui.actionCopy_Row->setToolTip(tr("Duplicate Row ") + "(Ctrl+Alt+=)");
	} else {
		ui.actionCopy_Row->setEnabled(false);
		ui.actionCopy_Row->setToolTip("(disabled)");
	}
	if (haveBuiltQuery && haveRows)
	{
		ui.actionRemove_Row->setEnabled(true);
		ui.actionRemove_Row->setIcon(Utils::getIcon("delete_multiple.png"));
		ui.actionRemove_Row->setToolTip(
			tr("Delete these rows from the table ") + "(Ctrl+Alt+D)");
	} else {
		ui.actionRemove_Row->setIcon(Utils::getIcon("delete_table_row.png"));
		if (editable && rowSelected) {
			ui.actionRemove_Row->setEnabled(true);
			ui.actionRemove_Row->setToolTip(
				tr("Delete selected row ") + "(Ctrl+Alt+D)");
		} else {
			ui.actionRemove_Row->setEnabled(false);
			ui.actionRemove_Row->setToolTip(tr("(disabled)"));
		}
	}
	if (pending)
	{
		ui.actionCommit->setEnabled(true);
		ui.actionCommit->setToolTip("<html><head/><body><p>"
            + tr((Database::isAutoCommit()
			? "Commit unsaved changes in this table to the database "
			: "Write unsaved changes in this table to the pending database transaction "))
			+ "Ctrl+Alt+C)</p></body></html>");
		ui.actionRollback->setEnabled(true);
		ui.actionRollback->setToolTip(
			"<html><head/><body><p>"
			+ tr("Roll back unsaved changes in this table ")
			+ "(Ctrl+Alt+R)</p></body></html>");
	} else {
		ui.actionCommit->setEnabled(false);
		ui.actionCommit->setToolTip("<html><head/><body><p>"
            + tr((Database::isAutoCommit()
			? "(Disabled in Auto Commit mode)"
			: "(Disabled in Transaction Pending mode)"))
			+ "</p></body></html>");
		ui.actionRollback->setEnabled(false);
		ui.actionRollback->setToolTip(tr("(disabled)"));
	}
	if (canPreview || ui.actionBLOB_Preview->isChecked())
	{
		ui.actionBLOB_Preview->setEnabled(true);
		ui.blobPreviewBox->setVisible(
			canPreview && ui.actionBLOB_Preview->isChecked());
		ui.actionBLOB_Preview->setToolTip(tr("Hide BLOB preview  ") + "(Ctrl+Alt+B)");
	}
	else
	{
		ui.actionBLOB_Preview->setEnabled(false);
		ui.blobPreviewBox->setVisible(false);
		if (canPreview)
		{
			ui.actionBLOB_Preview->setToolTip(tr("Show BLOB preview ") + "(Ctrl+Alt+B)");
		} else {
			ui.actionBLOB_Preview->setToolTip(tr("(disabled)"));
		}
	}
	if (haveRows)
	{
		ui.actionExport_Data->setEnabled(true);
		ui.actionExport_Data->setToolTip(tr("Export Data ") + "(Ctrl+Alt+X)");
	} else {
		ui.actionExport_Data->setEnabled(false);
		ui.actionExport_Data->setToolTip(tr("(disabled)"));
	}
	if (haveRows && (tab != 2))
	{
		ui.action_Goto_Line->setEnabled(true);
		ui.action_Goto_Line->setToolTip(tr("Go to record number ") + "(Ctrl+Alt+G)");
	} else {
		ui.action_Goto_Line->setEnabled(false);
		ui.action_Goto_Line->setToolTip(tr("(disabled)"));
	}
	if (haveRows && isTopLevel)
	{
		ui.actionRipOut->setEnabled(true);
		ui.actionRipOut->setToolTip(tr("Table Snapshot ") + "(Ctrl+Alt+T)");
	} else {
		ui.actionRipOut->setEnabled(false);
		ui.actionRipOut->setToolTip(tr("(disabled)"));
	}
	ui.tabWidget->setTabEnabled(1, rowSelected);
	ui.tabWidget->setTabEnabled(2, ui.scriptEdit->lines() > 1);
	
	if (m_finder) { m_finder->updateButtons(); }
}

void DataViewer::unFindAll()
{
	SqlTableModel * model = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (model)
	{
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		model->fetchAll();
		for (int row = 0; row < model->rowCount(); ++row)
		{
			if (!model->isDeleted(row))
			{
				ui.tableView->showRow(row);
			}
		}
		QApplication::restoreOverrideCursor();
	}
	m_doneFindAll = false;
}

void DataViewer::findNext(int row)
{
	SqlTableModel * model = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (model)
	{
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		if (m_doneFindAll) { unFindAll(); }
		model->fetchAll();
		for (; row < model->rowCount(); ++row)
		{
			if (ui.tableView->isRowHidden(row)) { continue; }
			QSqlRecord rec = model->record(row);
			if (m_finder->isMatch(&rec))
			{
				int column = ui.tableView->currentIndex().isValid() ? 
					ui.tableView->currentIndex().column() : 0;
				QModelIndex left = model->createIndex(row, column);
				ui.tableView->selectionModel()->select(
					QItemSelection(left, left),
					QItemSelectionModel::ClearAndSelect);
				ui.tableView->setCurrentIndex(left);
				if (ui.tabWidget->currentIndex() == 1)
				{
					ui.itemView->setCurrentIndex(row, column);
				}
				updateButtons();
				showStatusText(false);
				QApplication::restoreOverrideCursor();
				return;
			}
		}
		QApplication::restoreOverrideCursor();
	}
	setStatusText("Not found");
}

void DataViewer::removeFinder()
{
	if (m_finder)
	{
		m_doneFindAll = false;
		m_finder->close();
		m_finder = 0;
        showStatusText(false);
	}
}

void DataViewer::resizeViewToContents(QAbstractItemModel * model)
{
	if (model->columnCount() <= 0)
		return;

	Utils::setColumnWidths(ui.tableView);
	ui.tableView->resizeRowsToContents();
	dataResized = false;
}

void DataViewer::resizeEvent(QResizeEvent * event)
{
	if (!dataResized && ui.tableView->model())
		resizeViewToContents(ui.tableView->model());
}

// private slots

void DataViewer::findFirst()
{
	findNext(0);
}

void DataViewer::findNext()
{
	findNext(ui.tableView->currentIndex().row() + 1);
}

void DataViewer::findAll()
{
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    int currentRow = ui.itemView->currentRow();
	bool anyFound = false;
    bool currentRowFound = false;
	SqlTableModel * model = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (model)
	{
		model->fetchAll();
		for (int row = 0; row < model->rowCount(); ++row)
		{
			if (!(model->isDeleted(row)))
			{
				QSqlRecord rec = model->record(row);
				if (m_finder->isMatch(&rec))
				{
					anyFound = true;
					ui.tableView->showRow(row);
                    if (row == currentRow)
                    {
                        currentRowFound =  true;
                    }
				}
				else
				{
					ui.tableView->hideRow(row);
				}
			}
		}
	}
	if (!anyFound)
	{
		setStatusText("No match found");
		unFindAll();
	}
	else
	{
		showStatusText(false);
		m_doneFindAll = true;
        if ((ui.tabWidget->currentIndex() == 1) && !currentRowFound)
        {
            ui.tabWidget->setCurrentIndex(0);
        }
        else
        {
            ui.itemView->updateButtons(currentRow);
        }
	}
	QApplication::restoreOverrideCursor();
}

void DataViewer::findClosing()
{
	if (m_doneFindAll)
	{
		unFindAll();
		m_doneFindAll = false;
	}
	m_finder = 0;
	updateButtons();
}

void DataViewer::DataViewer::showChanges()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	SqlTableModel * model = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (model)
	{
		model->fetchAll();
        int rows = model->rowCount();
        if (showingChanges) {
            for (int row = 0; row < rows; ++row)
            {
                if (!(model->isDeleted(row)))
                {
                    ui.tableView->showRow(row);
                }
            }
            setStatusText("");
            showingChanges = false;
        } else {
            int columns = model->columnCount();
            int deletions = 0;
            int changes = 0;
            for (int row = 0; row < rows; ++row)
            {
                if (model->isDeleted(row))
                {
                    ++deletions;
                    break;
                }
                int rowchanges = 0;
                for (int column = 0; column < columns; ++column)
                {
                    if (model->isDirty(model->createIndex(row, column)))
                    {
                        rowchanges = 1;
                        break;
                    }
                }
                if (rowchanges == 0)
                {
                    ui.tableView->hideRow(row);
                }
                else
                {
                    changes += rowchanges;
                }
            }
            if (changes > 0)
            {
                if (deletions > 0)
                {
                    setStatusText(tr("%1 deleted row(s), %2 modified row(s))")
                    .arg(deletions).arg(changes));
                }
                else
                {
                    setStatusText(tr("%1 modified row(s)") .arg(changes));
                }
            }
            else if (deletions > 0)
            {
                setStatusText(tr("%1 deleted row(s)") .arg(deletions));
            } else {
                setStatusText(tr("no changes"));
            }
            showingChanges = true;
        }
    }
	updateButtons();
	QApplication::restoreOverrideCursor();
}

void DataViewer::find()
{
	SqlTableModel *stm = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (stm)
	{
#ifdef WIN32
	    // win windows are always top when there is this parent
	    m_finder = new FindDialog(0);
#else
	    m_finder = new FindDialog(this);
#endif
		m_finder->setAttribute(Qt::WA_DeleteOnClose);
		m_finder->doConnections(this);
		m_finder->setup(stm->schema(), stm->objectName());
		m_doneFindAll = false;
		m_finder->show();
		updateButtons();
	}
}

void DataViewer::columnClicked(int col)
{
	columnSelected = col;
	topRow = 0;
	searchString.clear();
}

void DataViewer::nonColumnClicked()
{
	columnSelected = -1;
}

void DataViewer::rowDoubleClicked(int)
{
	nonColumnClicked();
	ui.tabWidget->setCurrentIndex(1);
}

void DataViewer::addRow()
{
	showStatusText(false);
	nonColumnClicked();
	SqlTableModel * model
		= qobject_cast<SqlTableModel *>(ui.tableView->model());
	if (model)
	{
		model->fetchAll();
		activeRow = model->rowCount();
		if (model->insertRows(activeRow, 1)) {
            ui.tableView->scrollToBottom();
            ui.tableView->selectRow(activeRow);
            if (Preferences::instance()->openNewInItemView())
            {
                rowDoubleClicked(activeRow);
            }
            updateButtons();
            if (ui.tabWidget->currentIndex() == 1)
            {
                ui.itemView->setCurrentIndex(
                    ui.tableView->currentIndex().row(),
                    ui.tableView->currentIndex().column());
            }
        }
	}
}

void DataViewer::copyRow()
{
	showStatusText(false);
	nonColumnClicked();
    SqlTableModel * model =
	    qobject_cast<SqlTableModel *>(ui.tableView->model());
    if (model)
    {
		QModelIndex index = ui.tableView->currentIndex();
        int row = index.row();
        if (row >= 0)
        {
			model->fetchAll();
            activeRow = model->rowCount();
			if (model->copyRow(activeRow, model->record(row)))
			{
                ui.tableView->scrollToBottom();
                ui.tableView->selectRow(activeRow);
				if (Preferences::instance()->openNewInItemView())
				{
					rowDoubleClicked(activeRow);
				}
				updateButtons();
				if (ui.tabWidget->currentIndex() == 1)
				{
					ui.itemView->setCurrentIndex(
						ui.tableView->currentIndex().row(),
						ui.tableView->currentIndex().column());
				}
			}
        }
    }
}

void DataViewer::removeRow()
{
	showStatusText(false);
	if (haveBuiltQuery)
	{
		emit deleteMultiple();
	}
	else
	{
		nonColumnClicked();
		SqlTableModel * model =
			qobject_cast<SqlTableModel *>(ui.tableView->model());
		if (model)
		{
			int row = ui.tableView->currentIndex().row();
			ui.tableView->hideRow(row);
			model->removeRows(row, 1);
			if (ui.tabWidget->currentIndex() == 1)
			{
				if (ui.itemView->rowDeleted())
				{
					// no rows left
					ui.tabWidget->setCurrentIndex(0);
				}
			}
			updateButtons();
		}
	}
}

void DataViewer::deletingRow(int row)
{
	if ((row <= savedActiveRow) && (savedActiveRow > 0)) { --savedActiveRow; }
}

void DataViewer::exportData()
{
	removeErrorMessage();
	nonColumnClicked();
	QString tmpTableName("<any_table>");
	SqlTableModel * m = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (m)
		tmpTableName = m->objectName();

	DataExportDialog *dia = new DataExportDialog(this, tmpTableName);
	if (dia->exec())
		if (!dia->doExport())
			QMessageBox::warning(this, tr("Export Error"), tr("Data export failed"));
	delete dia;
}

void DataViewer::rollback()
{
	removeErrorMessage();
	nonColumnClicked();
	saveSelection();
	// HACK: some Qt4 versions crash on commit/rollback when there
	// is a new - currently edited - row in a transaction. This
	// forces to close the editor/delegate.
	ui.tableView->selectRow(ui.tableView->currentIndex().row());
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	if (model) // paranoia
	{
		m_doneFindAll = false;
        showingChanges = false;
		model->revertAll();
		model->setPendingTransaction(false);
		int n = model->rowCount();
		for (int i = 0; i < n; ++i)
		{
			ui.tableView->showRow(i);
		}
		reSelect();
		resizeViewToContents(model);
		updateButtons();
	}
}

void DataViewer::commit()
{
	removeErrorMessage();
	nonColumnClicked();
	saveSelection();
	// HACK: some Qt4 versions crash on commit/rollback when there
	// is a new - currently edited - row in a transaction. This
	// forces to close the editor/delegate.
	ui.tableView->selectRow(ui.tableView->currentIndex().row());
	SqlTableModel * model
		= qobject_cast<SqlTableModel *>(ui.tableView->model());
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	bool ok = model->submitAll();
	QApplication::restoreOverrideCursor();
	if (!ok)
	{
		int ret = QMessageBox::question(this, tr("Sqliteman"),
				tr("There is a pending transaction in progress."
				   " That cannot be committed now."
				   "\nError: %1\n"
				   "Perform rollback?").arg(model->lastError().text()),
				QMessageBox::Yes, QMessageBox::No);
		if(ret == QMessageBox::Yes)
		{
			rollback();
		}
		return;
	}
    showingChanges = false;
	model->setPendingTransaction(false);
	reSelect();
	resizeViewToContents(model);
	updateButtons();
	emit tableUpdated();
}

void DataViewer::copyHandler()
{
	removeErrorMessage();
	QItemSelectionModel *selectionModel = ui.tableView->selectionModel();
	// This looks very "pythonic" maybe there is better way to do...
	QMap<int,QMap<int,QString> > snapshot;
	QStringList out;

	QModelIndexList l = selectionModel->selectedIndexes();
    QModelIndexList::const_iterator i;
    for (i = l.constBegin(); i != l.constEnd(); ++i) {
		snapshot[i->row()][i->column()] = i->data().toString();
    }
	
	QMapIterator<int,QMap<int,QString> > it(snapshot);
	while (it.hasNext())
	{
		it.next();
		QMapIterator<int,QString> j(it.value());
		while (j.hasNext())
		{
			j.next();
			out << j.value();
			if (j.hasNext())
				out << "\t";
		}
		out << "\n";
	}

	if (out.size() != 0)
		QApplication::clipboard()
			->setText(out.join(QString()));
}

void DataViewer::openStandaloneWindow()
{
	removeErrorMessage();
	nonColumnClicked();
	SqlTableModel *tm = qobject_cast<SqlTableModel*>(ui.tableView->model());

#ifdef WIN32
    // win windows are always top when there is this parent
    DataViewer *w = new DataViewer(0);
#else
    DataViewer *w = new DataViewer(creator);
#endif
	SqlQueryModel *qm = new SqlQueryModel(w);
	w->setAttribute(Qt::WA_DeleteOnClose);
    Preferences * prefs = Preferences::instance();
	resize(prefs->dataviewerWidth(), prefs->dataviewerHeight());	

	//! TODO: change setWindowTitle() to the unified QString().arg() sequence after string unfreezing
	if (tm)
	{
		w->setWindowTitle(tm->tableName() + " - "
				+ QDateTime::currentDateTime().toString() + " - " 
				+ tr("Data Snapshot"));
		qm->setQuery(QString("select * from ")
					 + Utils::q(tm->schema())
					 + "."
					 + Utils::q(tm->objectName())
					 + ";",
					 QSqlDatabase::database(SESSION_NAME));
	}
	else
	{
		w->setWindowTitle("SQL - "
				+ QDateTime::currentDateTime().toString() + " - " 
				+ tr("Data Snapshot"));
		QSqlQueryModel * m = qobject_cast<QSqlQueryModel*>(ui.tableView->model());
		qm->setQuery(m->query());
	}

	qm->attach();
	qm->fetchAll();

	w->setTableModel(qm);
	w->ui.mainToolBar->hide();
	w->ui.actionRipOut->setEnabled(false);
	w->ui.actionClose->setEnabled(true);
	w->ui.actionClose->setVisible(true);
	w->ui.tabWidget->removeTab(2);
	w->show();
	w->setStatusText(tr("%1 snapshot for: %2")
		.arg("<tt>"+QDateTime::currentDateTime().toString()+"</tt><br/>")
		.arg("<br/><tt>" + qm->query().lastQuery())+ "</tt>");
}

void DataViewer::tableView_selectionChanged(const QItemSelection & current,
											const QItemSelection & previous)
{
	removeErrorMessage();
	SqlTableModel *tm = qobject_cast<SqlTableModel*>(ui.tableView->model());
    bool enable = (tm != 0);
    actPasteOver->setEnabled(enable);
    actInsertNull->setEnabled(enable);
    actOpenEditor->setEnabled(enable);
    actOpenMultiEditor->setEnabled(enable);

	updateButtons();
	QModelIndex index = ui.tableView->currentIndex();
	
	if (ui.blobPreviewBox->isVisible())
	{
		if (index.isValid())
		{
			ui.blobPreview->setBlobData(
				ui.tableView->model()->data(index, Qt::EditRole));
		}
		else
		{
			ui.blobPreview->setBlobData(QVariant());
		}
	}
}

void DataViewer::tableView_currentChanged(const QModelIndex & current,
										  const QModelIndex & previous)
{
	// only used for debug output
}

void DataViewer::tableView_dataResized(int column, int oldWidth, int newWidth) 
{
	dataResized = true;
}

void DataViewer::tableView_dataChanged()
{
	removeErrorMessage();
	updateButtons();
	ui.tableView->viewport()->update();
}

void DataViewer::handleBlobPreview(bool state)
{
	nonColumnClicked();
	if (state)
		tableView_selectionChanged(QItemSelection(), QItemSelection());
	updateButtons();
	if (ui.blobPreviewBox->isVisible())
	{
		ui.blobPreview->setBlobData(
			ui.tableView->model()->data(ui.tableView->currentIndex(),
										Qt::EditRole));
	}
}

void DataViewer::tabWidget_currentChanged(int ix)
{
	removeErrorMessage();
	nonColumnClicked();
	QModelIndex ci = ui.tableView->currentIndex();
	if (ix == 0)
	{
		// be careful with this. See itemView_indexChanged() docs.
		disconnect(ui.itemView, SIGNAL(indexChanged()),
				   this, SLOT(itemView_indexChanged()));
		disconnect(ui.itemView, SIGNAL(dataChanged()),
				   this, SLOT(tableView_dataChanged()));
	}
	if (ix == 1)
	{
		setStatusText("");
		ui.itemView->setCurrentIndex(ci.row(), ci.column());
		// be careful with this. See itemView_indexChanged() docs.
		connect(ui.itemView, SIGNAL(indexChanged()),
				this, SLOT(itemView_indexChanged()));
		connect(ui.itemView, SIGNAL(dataChanged()),
				this, SLOT(tableView_dataChanged()));
	}
	
	if (ui.actionBLOB_Preview->isChecked())
		ui.blobPreviewBox->setVisible(ix!=2);
	showStatusText(ix == 0);
	updateButtons();
}

void DataViewer::itemView_indexChanged()
{
	removeErrorMessage();
	ui.tableView->setCurrentIndex(
		ui.tableView->model()->index(ui.itemView->currentRow(),
								     ui.itemView->currentColumn()));
	updateButtons();
}

void DataViewer::gotoLine()
{
	removeErrorMessage();
	nonColumnClicked();
	bool ok;
	int row = QInputDialog::getInt(this, tr("Goto Line"), tr("Goto Line:"),
								   ui.tableView->currentIndex().row(), // value
								   1, // min
								   ui.tableView->model()->rowCount(), // max (no fetchMore loop)
								   1, // step
								   &ok);
	if (!ok)
		return;

	QModelIndex left;
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	int column = ui.tableView->currentIndex().isValid() ? ui.tableView->currentIndex().column() : 0;
	row -= 1;

	if (model)
		left = model->createIndex(row, column);
	else
	{
		SqlQueryModel * model = qobject_cast<SqlQueryModel *>(ui.tableView->model());
		if (model)
			left = model->createIndex(row, column);
	}

	ui.tableView->selectionModel()->select(QItemSelection(left, left),
										   QItemSelectionModel::ClearAndSelect);
	ui.tableView->setCurrentIndex(left);
	if (ui.tabWidget->currentIndex() == 1)
	{
		ui.itemView->setCurrentIndex(row, column);
	}
	updateButtons();
}

void DataViewer::actOpenEditor_triggered()
{
	QModelIndex index(ui.tableView->currentIndex());
	if (index.isValid())
	{
		removeErrorMessage();
	    ui.tableView->edit(index);
	}
}

void DataViewer::actOpenMultiEditor_triggered()
{
	QAbstractItemModel * model = ui.tableView->model();
	QModelIndex index(ui.tableView->currentIndex());
	if (model && index.isValid())
	{
		QVariant data = model->data(index, Qt::EditRole);
		if (data.isValid())
		{
			MultiEditDialog dia(this);
			dia.setData(data);
			if (dia.exec())
			{
				data = dia.data();
				ui.tableView->model()->setData(index, data, Qt::EditRole);
				tableView_dataChanged();
			}
		}
	}
}

void DataViewer::actInsertNull_triggered()
{
	QAbstractItemModel * model = ui.tableView->model();
	QModelIndex index(ui.tableView->currentIndex());
	if (   index.isValid()
		&& model
		&& !(model->data(index, Qt::EditRole).isNull()))
	{
	    model->setData(index, QString(), Qt::EditRole);
		tableView_dataChanged();
	}
}

void DataViewer::doCopyWhole()
{
	QAbstractItemModel * model = ui.tableView->model();
	QModelIndex index(ui.tableView->currentIndex());
	QVariant data = model->data(index, Qt::EditRole);
	QApplication::clipboard()->setText(data.toString());
}

void DataViewer::doPasteOver()
{
	QSqlTableModel * model =
		qobject_cast<QSqlTableModel*>(ui.tableView->model());
	QModelIndex index(ui.tableView->currentIndex());
	const QMimeData *mimeData =
		QApplication::clipboard()->mimeData();
	if (model && index.isValid() && mimeData->hasText())
	{
		model->setData(
			index, mimeData->text(), Qt::EditRole);
	}
}

// public methods

DataViewer::DataViewer(LiteManWindow * parent)
	: QMainWindow(parent),
	  dataResized(true)
{
    creator = parent;
	ui.setupUi(this);
	m_finder = 0;
	canFetchMore = tr("(More rows can be fetched. "
		"Scroll the resultset for more rows and/or read the documentation.)");
	// force the status window to have a document
	ui.statusText->setDocument(new QTextDocument());

#ifdef Q_WS_MAC
    ui.mainToolBar->setIconSize(QSize(16, 16));
    ui.exportToolBar->setIconSize(QSize(16, 16));
#endif

	haveBuiltQuery = false;
    showingChanges = false;
	ui.splitter->setCollapsible(0, false);
	ui.splitter->setCollapsible(1, false);
    ui.actionShowChanges->setIcon(Utils::getIcon("findChanged.png"));
	ui.actionFind->setIcon(Utils::getIcon("system-search.png"));
	ui.actionNew_Row->setIcon(Utils::getIcon("insert_table_row.png"));
    ui.actionCopy_Row->setIcon(Utils::getIcon("duplicate_table_row.png"));
	ui.actionRemove_Row->setIcon(Utils::getIcon("delete_table_row.png"));
	ui.actionCommit->setIcon(Utils::getIcon("database_commit.png"));
	ui.actionRollback->setIcon(Utils::getIcon("database_rollback.png"));
	ui.actionRipOut->setIcon(Utils::getIcon("snapshot.png"));
	ui.actionBLOB_Preview->setIcon(Utils::getIcon("blob.png"));
	ui.actionExport_Data->setIcon(Utils::getIcon("document-export.png"));
	ui.actionClose->setIcon(Utils::getIcon("close.png"));
	ui.action_Goto_Line->setIcon(Utils::getIcon("go-next-use.png"));
	ui.actionClose->setVisible(false);
	ui.actionClose->setEnabled(false);
	isTopLevel = true;

	ui.mainToolBar->show();
	ui.exportToolBar->show();
	
	handleBlobPreview(false);

	actCopyWhole = new QAction(tr("Copy Whole"), ui.tableView);
	actCopyWhole->setShortcut(QKeySequence("Ctrl+W"));
	actCopyWhole->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(actCopyWhole, SIGNAL(triggered()), this,
			SLOT(doCopyWhole()));
	actPasteOver = new QAction(tr("Paste"), ui.tableView);
	actPasteOver->setShortcut(QKeySequence("Ctrl+Alt+V"));
	actPasteOver->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(actPasteOver, SIGNAL(triggered()), this,
			SLOT(doPasteOver()));
	actInsertNull = new QAction(Utils::getIcon("setnull.png"),
								tr("Insert NULL"), ui.tableView);
	actInsertNull->setShortcut(QKeySequence("Ctrl+Alt+N"));
	actInsertNull->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(actInsertNull, SIGNAL(triggered()), this,
			SLOT(actInsertNull_triggered()));
    actOpenEditor = new QAction(tr("Open Data Editor..."), ui.tableView);
	actOpenEditor->setShortcut(QKeySequence("Ctrl+ "));
    connect(actOpenEditor, SIGNAL(triggered()), this,
			SLOT(actOpenEditor_triggered()));
    actOpenMultiEditor = new QAction(Utils::getIcon("edit.png"),
									 tr("Open Multiline Editor..."),
									 ui.tableView);
	actOpenMultiEditor->setShortcut(QKeySequence("Ctrl+Alt+E"));
	actOpenMultiEditor->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(actOpenMultiEditor, SIGNAL(triggered()),
			this, SLOT(actOpenMultiEditor_triggered()));
    ui.tableView->addAction(actCopyWhole);
    ui.tableView->addAction(actPasteOver);
    ui.tableView->addAction(actInsertNull);
    ui.tableView->addAction(actOpenEditor);
    ui.tableView->addAction(actOpenMultiEditor);

	// custom delegate
	SqlDelegate * delegate = new SqlDelegate(this);
	ui.tableView->setItemDelegate(delegate);
	connect(delegate, SIGNAL(dataChanged()),
		this, SLOT(tableView_dataChanged()));
	connect(delegate, SIGNAL(insertNull()),
		this, SLOT(actInsertNull_triggered()));

	// workaround for Ctrl+C
	DataViewerTools::KeyPressEater *keyPressEater =
        new DataViewerTools::KeyPressEater(this);
	ui.tableView->installEventFilter(keyPressEater);

    connect(ui.actionShowChanges, SIGNAL(triggered()),
            this, SLOT(showChanges()));
	connect(ui.actionFind, SIGNAL(triggered()),
			this, SLOT(find()));
	connect(ui.actionNew_Row, SIGNAL(triggered()),
			this, SLOT(addRow()));
    connect(ui.actionCopy_Row, SIGNAL(triggered()),
            this, SLOT(copyRow()));
	connect(ui.actionRemove_Row, SIGNAL(triggered()),
			this, SLOT(removeRow()));
	connect(ui.actionExport_Data, SIGNAL(triggered()),
			this, SLOT(exportData()));
	connect(ui.actionCommit, SIGNAL(triggered()),
			this, SLOT(commit()));
	connect(ui.actionRollback, SIGNAL(triggered()),
			this, SLOT(rollback()));
	connect(ui.actionRipOut, SIGNAL(triggered()),
			this, SLOT(openStandaloneWindow()));
	connect(ui.actionClose, SIGNAL(triggered()),
			this, SLOT(close()));
	connect(ui.action_Goto_Line, SIGNAL(triggered()),
			this, SLOT(gotoLine()));
	connect(keyPressEater, SIGNAL(copyRequest()),
			this, SLOT(copyHandler()));
// 	connect(parent, SIGNAL(prefsChanged()), ui.tableView, SLOT(repaint()));
	connect(ui.actionBLOB_Preview, SIGNAL(toggled(bool)),
			this, SLOT(handleBlobPreview(bool)));
	connect(ui.tabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(tabWidget_currentChanged(int)));
	connect(ui.tableView->horizontalHeader(),
            SIGNAL(sectionResized(int, int, int)),
			this, SLOT(tableView_dataResized(int, int, int)));
	connect(ui.tableView->verticalHeader(),
            SIGNAL(sectionResized(int, int, int)),
			this, SLOT(tableView_dataResized(int, int, int)));
	connect(ui.tableView->verticalHeader(), SIGNAL(sectionDoubleClicked(int)),
			this, SLOT(rowDoubleClicked(int)));
	connect(ui.tableView->verticalHeader(), SIGNAL(sectionClicked(int)),
			this, SLOT(nonColumnClicked()));
	connect(ui.tableView->horizontalHeader(), SIGNAL(sectionClicked(int)),
			this, SLOT(columnClicked(int)));
	connect(ui.tableView, SIGNAL(clicked(const QModelIndex &)),
			this, SLOT(nonColumnClicked()));
    connect(ui.mainToolBar, SIGNAL(visibilityChanged(bool)),
            this, SLOT(updateVisibility()));
    connect(ui.exportToolBar, SIGNAL(visibilityChanged(bool)),
            this, SLOT(updateVisibility()));

	activeRow = -1;
	columnSelected = -1;
	updateButtons();
}

DataViewer::~DataViewer()
{
	removeFinder();
	if (!isTopLevel)
	{
        Preferences * prefs = Preferences::instance();
        prefs->setdataviewerHeight(height());
        prefs->setdataviewerWidth(width());
	}
	freeResources( ui.tableView->model()); // avoid memory leak of model
	showStatusText(false);
}

void DataViewer::setNotPending()
{
	SqlTableModel * old = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (old) { old->setPendingTransaction(false); }
}

bool DataViewer::checkForPending()
{
	SqlTableModel * old = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (old && old->pendingTransaction())
	{
		QString msg (Database::isAutoCommit()
			? tr("There are unsaved changes in table %1.%2.\n"
				 "Do you wish to commit them to the database?\n\n"
				 "Yes = commit changes\nNo = discard changes\n"
				 "Cancel = skip this operation and stay in %1.%2")
			: tr("There are unsaved changes in table %1.%2.\n"
				 "Do you wish to save them to the database?\n"
				 "(This will not commit as you are in pending"
				 " transaction mode)\n"
				 "\nYes = save changes\nNo = discard changes\n"
				 "Cancel = skip this operation and stay in %1.%2"));
		int com = QMessageBox::question(this, tr("Sqliteman"),
			msg.arg(old->schema(), old->objectName()),
			QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
		if (com == QMessageBox::No)
		{
			rollback();
			return true;
		}
		else if (com == QMessageBox::Cancel) { return false; }
		else
		{
			QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
			bool ok = old->submitAll();
			QApplication::restoreOverrideCursor();
			if (!ok)
			{
				/* This should never happen */
				int ret = QMessageBox::question(this, tr("Sqliteman"),
					tr("Failed to write unsaved changes to the database."
					   "\nError: %1\n"
					   "Discard changes?").arg(old->lastError().text()),
												QMessageBox::Yes, QMessageBox::No);
				if (ret == QMessageBox::Yes) { rollback(); }
				else { return false; }
			}
            old->setPendingTransaction(false);
			return true;
		}
	}
	else { return true; }
}

bool DataViewer::setTableModel(QAbstractItemModel * model, bool showButtons)
{
	QAbstractItemModel * old = ui.tableView->model();
    if (old == model) { return true; } // Nothing to do, avoid SIGSEGV
	if (!checkForPending()) { return false; }
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    showingChanges = false;
	ui.tableView->setModel(model); // references old model
	ui.tableView->scrollToTop();
	freeResources(old); // avoid memory leak of model

	connect(ui.tableView->selectionModel(),
			SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
			this,
			SLOT(tableView_selectionChanged(const QItemSelection &, const QItemSelection &)));
	connect(ui.tableView->selectionModel(),
			SIGNAL(currentChanged(const QModelIndex &, const QModelIndex & )),
			this,
			SLOT(tableView_currentChanged(const QModelIndex &, const QModelIndex & )));
	SqlTableModel * stm = qobject_cast<SqlTableModel*>(model);
	if (stm)
	{
		connect(stm, SIGNAL(reallyDeleting(int)), this, SLOT(deletingRow(int)));
		connect(stm, SIGNAL(moreFetched()), this, SLOT(rowCountChanged()));
		if (m_finder)
		{
			m_doneFindAll = false;
			m_finder->setup(stm->schema(), stm->objectName());
		}
		stm->setPalette(ui.tableView->palette());
	}
	else if (m_finder)
	{
		m_doneFindAll = false;
		m_finder->close();
		m_finder = 0;
	}

	ui.itemView->setModel(model);
	ui.itemView->setTable(ui.tableView);
	if (model->columnCount() > 0)
	{
		ui.tabWidget->setCurrentIndex(0);
		resizeViewToContents(model);
	}
	updateButtons();
	
	rowCountChanged();

    QApplication::restoreOverrideCursor();

	return true;
}

void DataViewer::setBuiltQuery(bool value)
{
	haveBuiltQuery = value;
	updateButtons();
}

void DataViewer::setStatusText(const QString & text)
{
	ui.statusText->setHtml(text);
	ui.statusText->show();
    int height = ui.statusText->document()->size().height();
    ui.statusText->setFixedHeight(height);
    ui.splitterBlob->update();
}

void DataViewer::removeErrorMessage()
{
	QString s = ui.statusText->toHtml();
	if (s.contains("<span style=\" color:#ff0000;\">"))
	{
		showStatusText(false);
	}
}

void DataViewer::showStatusText(bool show)
{
	if (show)
	{
		ui.statusText->show();
	}
	else
	{
		ui.statusText->hide();
		ui.statusText->setFixedHeight(0);
	}
	ui.splitterBlob->update();
}

QAbstractItemModel * DataViewer::tableData()
{
	return ui.tableView->model();
}

QStringList DataViewer::tableHeader()
{
	QStringList ret;
	QSqlQueryModel *q = qobject_cast<QSqlQueryModel *>(ui.tableView->model());

	for (int i = 0; i < q->columnCount() ; ++i)
		ret << q->headerData(i, Qt::Horizontal).toString();

	return ret;
}

void DataViewer::freeResources(QAbstractItemModel * old)
{
	SqlTableModel * t = qobject_cast<SqlTableModel*>(old);
	if (t)
	{
		SqlTableModel::detach(t);
	}
	else
	{
		SqlQueryModel * q = qobject_cast<SqlQueryModel*>(old);
		if (q)
		{
			SqlQueryModel::detach(q);
		}
	}
}

void DataViewer::saveSelection()
{
	savedActiveRow = activeRow;
	wasItemView = (ui.tabWidget->currentIndex() == 1);
}

void DataViewer::reSelect()
{
	if (savedActiveRow >= 0)
	{
		ui.tableView->selectRow(savedActiveRow);
		if (wasItemView)
		{
			ui.tabWidget->setCurrentIndex(1);
			ui.itemView->setCurrentIndex(ui.tableView->currentIndex().row(),
										 ui.tableView->currentIndex().column());
		}
	}
}

bool DataViewer::incrementalSearch(QKeyEvent *keyEvent)
{
	QString s (keyEvent->text());
	if (keyEvent->key() == Qt::Key_Backspace)
	{
		if (searchString.isEmpty()) { return false; }
		searchString.chop(1);
		SqlTableModel * model
			= qobject_cast<SqlTableModel*>(ui.tableView->model());
		if (!model) { return false; }
		if (m_doneFindAll) { unFindAll(); }
		while (topRow > 0)
		{
			QModelIndex index(
				model->index(topRow - 1, columnSelected));
			QVariant data = ui.tableView->model()->data(index, Qt::EditRole);
			if (searchString.compare(data.toString(), Qt::CaseInsensitive) >= 0)
			{
				ui.tableView->scrollTo(
					model->index(topRow, columnSelected),
					QAbstractItemView::PositionAtTop);
				break;
			}
			--topRow;
		}
		return true;
	}
	else if (s.isEmpty()) { return false; }
	else
	{
		searchString.append(QLocale().toLower(s));
		SqlTableModel * model
			= qobject_cast<SqlTableModel*>(ui.tableView->model());
		if (!model) { return false; }
		if (m_doneFindAll) { unFindAll(); }
		int rows = model->rowCount();
		for (int i = topRow; i < rows; ++i)
		{
			QModelIndex index(model->index(i, columnSelected));
			QVariant data = ui.tableView->model()->data(index, Qt::EditRole);
			QString d(QLocale().toLower(data.toString()));
			
			if (searchString.localeAwareCompare(d) <= 0)
			{
				topRow = i;
				ui.tableView->scrollTo(index, QAbstractItemView::PositionAtTop);
				break;
			}
		}
		return true;
	}
}

void DataViewer::showSqlScriptResult(QString line)
{
	removeErrorMessage();
	if (line.isEmpty()) { return; }
	ui.scriptEdit->append(line);
	ui.scriptEdit->append("\n");
	ui.scriptEdit->ensureLineVisible(ui.scriptEdit->lines());
	ui.tabWidget->setCurrentIndex(2);
	haveBuiltQuery = false;
	updateButtons();
	emit tableUpdated();
}

void DataViewer::sqlScriptStart()
{
	ui.scriptEdit->clear();
}

void DataViewer::rowCountChanged()
{
	QString cached;
	QSqlQueryModel * model
		= qobject_cast<QSqlQueryModel*>(ui.tableView->model());
	if ((model != 0) && (model->columnCount() > 0))
	{
		if (   (model->rowCount() != 0)
		    && model->canFetchMore())
	    {
			cached = canFetchMore + "<br/>";
	    }
	    else { cached = ""; }

		setStatusText(tr("Query OK<br/>Row(s) returned: %1 %2")
					  .arg(model->rowCount()).arg(cached));
	}
	else { showStatusText(false); }
}

void DataViewer::updateVisibility()
{
    bool visible =    ui.mainToolBar->isVisible()
                   || ui.exportToolBar->isVisible();
    if (creator != NULL)
    {
        creator->actToggleDataViewerToolBar->setChecked(visible);
    }
}

void DataViewer::handleToolBar()
{
    bool visible =    ui.mainToolBar->isVisible()
                   || ui.exportToolBar->isVisible();
    ui.mainToolBar->setHidden(visible);
    ui.exportToolBar->setHidden(visible);
    updateVisibility();
}

void DataViewer::showEvent(QShowEvent * event)
{
    QMainWindow::showEvent(event);
    updateVisibility();
}

/* Tools *************************************************** */

bool DataViewerTools::KeyPressEater::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::KeyPress)
	{
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent == QKeySequence::Copy)
		{
			emit copyRequest();
			return true;
		}
		else if (m_owner->columnSelected >= 0)
		{
			if (m_owner->incrementalSearch(keyEvent)) { return true; }
		}
		return QObject::eventFilter(obj, event);
	}
	else
	{
		// standard event processing
		return QObject::eventFilter(obj, event);
	}
}
