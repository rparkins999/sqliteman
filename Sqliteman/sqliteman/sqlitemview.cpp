/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QAction>
#include <QClipboard>
#include <QDataWidgetMapper>
#include <QGridLayout>
#include <QtCore/QMimeData>
#include <QSqlQueryModel>
#include <QSqlRecord>
#include <QTableView>
#include <QTextEdit>

#include "database.h"
#include "multieditdialog.h"
#include "sqlitemview.h"
#include "sqlmodels.h"
#include "utils.h"

SqlItemView::SqlItemView(QWidget * parent)
	: QWidget(parent),
	m_row(0),
	m_changing(false),
	m_model(0)
{
	setupUi(this);

	connect(firstButton, SIGNAL(clicked()),
			this, SLOT(toFirst()));
	connect(previousButton, SIGNAL(clicked()),
			this, SLOT(toPrevious()));

	connect(nextButton, SIGNAL(clicked()),
			this, SLOT(toNext()));
	connect(lastButton, SIGNAL(clicked()),
			this, SLOT(toLast()));
	connect(nullButton, SIGNAL(clicked()),
			this, SLOT(insertNull()));
	nullButton->setIcon(Utils::getIcon("setnull.png"));
	connect(multiButton, SIGNAL(clicked()),
			this, SLOT(openMultiEditor()));
	multiButton->setIcon(Utils::getIcon("edit.png"));
	connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)),
			 this, SLOT(aApp_focusChanged(QWidget*,QWidget*)));
}

void SqlItemView::setModel(QAbstractItemModel * model)
{
	m_model = model;
	QSqlQueryModel * t = qobject_cast<QSqlQueryModel *>(model);
	if (!t)  { return; }
	QSqlTableModel * table = qobject_cast<QSqlTableModel *>(model);
	m_writeable = table != 0;
	QSqlRecord rec(t->record());

	if (scrollWidget->widget())
	{
		delete scrollWidget->takeWidget();
	}

	QWidget * layoutWidget = new QWidget(scrollWidget);
	m_gridLayout = new QGridLayout(layoutWidget);
	QString tmp("%1:");
	actCopy = new QAction(tr("Copy"), layoutWidget);
	actCopy->setShortcut(QKeySequence("Ctrl+C"));
    connect(actCopy, SIGNAL(triggered()), this,
			SLOT(doCopy()));
	actCopyWhole = new QAction(tr("Copy Whole"), layoutWidget);
	actCopyWhole->setShortcut(QKeySequence("Ctrl+W"));
    connect(actCopyWhole, SIGNAL(triggered()), this,
			SLOT(doCopyWhole()));
	if (table)
	{
		actCut = new QAction(tr("Cut"), layoutWidget);
		actCut->setShortcut(QKeySequence("Ctrl+X"));
		actCut->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	    connect(actCut, SIGNAL(triggered()), this,
				SLOT(doCut()));
		actPaste = new QAction(tr("Paste"), layoutWidget);
		actPaste->setShortcut(QKeySequence("Ctrl+V"));
		actPaste->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	    connect(actPaste, SIGNAL(triggered()), this,
				SLOT(doPaste()));
		actPasteOver = new QAction(tr("Paste Over"), layoutWidget);
		actPasteOver->setShortcut(QKeySequence("Ctrl+Alt+V"));
		actPasteOver->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	    connect(actPasteOver, SIGNAL(triggered()), this,
				SLOT(doPasteOver()));
		actSelectAll = new QAction(tr("Select Whole"), layoutWidget);
		actSelectAll->setShortcut(QKeySequence("Ctrl+A"));
		actSelectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	    connect(actSelectAll, SIGNAL(triggered()), this,
				SLOT(doSelectAll()));
		actInsertNull = new QAction(Utils::getIcon("setnull.png"),
									tr("Insert NULL"), layoutWidget);
		actInsertNull->setShortcut(QKeySequence("Ctrl+Alt+N"));
		actInsertNull->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	    connect(actInsertNull, SIGNAL(triggered()), this,
				SLOT(insertNull()));
	}
    actOpenMultiEditor = new QAction(Utils::getIcon("edit.png"),
                                        tr("Open Multiline Editor..."),
                                        this);
    actOpenMultiEditor->setShortcut(QKeySequence("Ctrl+Alt+E"));
    actOpenMultiEditor->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(actOpenMultiEditor, SIGNAL(triggered()),
            this, SLOT(openMultiEditor()));

	for (int i = 0; i < rec.count(); ++i)
	{
		m_gridLayout->addWidget(
			new QLabel(tmp.arg(rec.fieldName(i)), layoutWidget), i, 0);
		QTextEdit * w = new QTextEdit(layoutWidget);
		w->setReadOnly(table ? false : true);
		w->setAcceptRichText(false);
		int mh = QFontMetrics(w->currentFont()).lineSpacing();
		w->setMinimumHeight(mh);
		w->setSizePolicy(QSizePolicy::Expanding,
                         QSizePolicy::MinimumExpanding);
		m_gridLayout->addWidget(w, i, 1);
		m_gridLayout->setRowMinimumHeight(i, mh);
		connect(w, SIGNAL(textChanged()),
				this, SLOT(textChanged()));
		w->addAction(actCopy);
		w->addAction(actCopyWhole);
		if (table)
		{
			w->addAction(actCut);
			w->addAction(actPaste);
			w->addAction(actPasteOver);
			w->addAction(actSelectAll);
			w->addAction(actInsertNull);
		}
        w->addAction(actOpenMultiEditor);
		w->setContextMenuPolicy(Qt::ActionsContextMenu);
	}
	scrollWidget->setWidget(layoutWidget);

	m_count = rec.count();
}

QAbstractItemModel * SqlItemView::model()
{
	return m_model;
}

void SqlItemView::updateButtons(int row)
{
	int rowcount = m_model->rowCount();
	int notDeleted = 0;
	int adjRow = row;
	QVariant data
		= m_model->data(m_model->index(m_row, m_column), Qt::EditRole);
	bool editable = (m_row >= 0) && (m_column >= 0) && m_writeable;
	SqlTableModel * table = qobject_cast<SqlTableModel *>(m_model);
	bool newRow = table ? table->isNewRow(row) : false;
	for (int i = 0; i < rowcount; ++i)
	{
		if (m_table->isRowHidden(i))
		{
			if (i < row) { --adjRow; }
		}
		else { ++notDeleted; }
	}
	positionLabel->setText(
		tr("%1row %2 of %3").arg(newRow ? "new " : "")
							.arg(adjRow + 1).arg(notDeleted));
	previousButton->setEnabled(findDown(row) != row);
	firstButton->setEnabled(findUp(-1) != row);
	nextButton->setEnabled(findUp(row) != row);
	lastButton->setEnabled(findDown(rowcount) != row);
	nullButton->setVisible(editable && !data.isNull());
	multiButton->setVisible(editable);
}

void SqlItemView::setCurrentIndex(int row, int column)
{
	if ((row >= 0) && (column >= 0))
	{
		m_row = row;
		for (m_column = 0; m_column < m_count; ++m_column)
		{
			QWidget * w = m_gridLayout->itemAtPosition(m_column, 1)->widget();
			QTextEdit * te = qobject_cast<QTextEdit *>(w);
			if (te)
			{
				QColor color =
					m_model->data(m_model->index(row, m_column),
								  Qt::BackgroundColorRole).value<QColor>();
				QPalette p(te->palette());
				p.setColor(QPalette::Active, QPalette::Base, color);
				p.setColor(QPalette::Inactive, QPalette::Base, color);
				te->setPalette(p);
				if (!m_changing)
				{
					m_changing = true;
					QVariant rawdata = m_model->data(
						m_model->index(row, m_column), Qt::EditRole);
					if (rawdata.type() == QVariant::ByteArray)
					{
						te->setText(m_model->data(
							m_model->index(row, m_column), Qt::DisplayRole).toString());
						te->setReadOnly(true);
					}
					else
					{
						te->setReadOnly(!m_writeable);
						Qt::ItemDataRole role;
						if (m_writeable && (m_column == column))
						{
							role = Qt::EditRole;
							te->setFocus();
						}
						else
						{
							role = Qt::DisplayRole;
						}
						te->setText(m_model->data(
							m_model->index(row, m_column), role).toString());
					}
					m_changing = false;
				}
			}
		}
	}
	m_column = column;
	updateButtons(row);
}

int SqlItemView::currentRow()
{
	return m_row;
}

int SqlItemView::currentColumn()
{
	return m_column;
}

bool SqlItemView::rowDeleted()
{
	int row = findUp(m_row);
	if (row != m_row) 
	{
		setCurrentIndex(row, m_column);
		emit indexChanged();
		return false;
	}
	row = findDown(m_row);
	if (row != m_row) 
	{
		setCurrentIndex(row, m_column);
		emit indexChanged();
		return false;
	}
	return true;
}

int SqlItemView::findUp(int row)
{
	int r = row;
	while (++r < m_model->rowCount())
	{
		if (m_table->isRowHidden(r)) { continue; }
		else { return r; }
	}
	return row;
}

int SqlItemView::findDown(int row)
{
	int r = row;
	while (--r >= 0)
	{
		if (m_table->isRowHidden(r)) { continue; }
		else { return r; }
	}
	return row;
}

void SqlItemView::toFirst()
{
	int row = findUp(-1);
	if (row != m_row) {
		setCurrentIndex(row, m_column);
		emit indexChanged();
	}
}

void SqlItemView::toPrevious()
{
	int row = findDown(m_row);
	if (row != m_row) {
		setCurrentIndex(row, m_column); 
		emit indexChanged();
	}
}

void SqlItemView::toNext()
{
	int row = findUp(m_row);
	if (row != m_row) {
		setCurrentIndex(row, m_column);
		emit indexChanged();
	}
}

void SqlItemView::toLast()
{
	SqlTableModel * table = qobject_cast<SqlTableModel *>(m_model);
	if (table)
	{
		table->fetchAll();
	}
	else
	{
		SqlQueryModel * q = qobject_cast<SqlQueryModel *>(m_model);
		if (q)
		{
			q->fetchAll();
		}
	}
	int row = findDown(m_model->rowCount());
	if (row != m_row) {
		setCurrentIndex(row, m_column);
		emit indexChanged();
	}
}

void SqlItemView::insertNull()
{
		QModelIndex index = m_model->index(m_row, m_column);
		m_model->setData(index, QVariant());
		setCurrentIndex(m_row, m_column);
		emit dataChanged();
}

void SqlItemView::openMultiEditor()
{
	MultiEditDialog dia(this);
	dia.setData(m_model->data(m_model->index(m_row, m_column), Qt::EditRole),
                m_writeable);
	if (dia.exec() && m_writeable)
	{
		QModelIndex index = m_model->index(m_row, m_column);
		m_model->setData(index, dia.data());
		setCurrentIndex(m_row, m_column);
		emit dataChanged();
	}
}

void SqlItemView::aApp_focusChanged(QWidget* old, QWidget* now)
{
	if (m_model)
	{
		for (int i = 0; i < m_count; ++i)
		{
			QWidget * w = m_gridLayout->itemAtPosition(i, 1)->widget();
			if (w == now)
			{
				m_column = i;
				setCurrentIndex(m_row, m_column);
				emit indexChanged();
				break;
			}
		}
	}
}

void SqlItemView::doCopy()
{
	QWidget * w = m_gridLayout->itemAtPosition(m_column, 1)->widget();
	QTextEdit * te = qobject_cast<QTextEdit *>(w);
	if (te)
	{
		te->copy();
	}
}

void SqlItemView::doCopyWhole()
{
	QWidget * w = m_gridLayout->itemAtPosition(m_column, 1)->widget();
	QTextEdit * te = qobject_cast<QTextEdit *>(w);
	if (te)
	{
		QApplication::clipboard()->setText(te->toPlainText());
	}
}

void SqlItemView::doCut()
{
	QWidget * w = m_gridLayout->itemAtPosition(m_column, 1)->widget();
	QTextEdit * te = qobject_cast<QTextEdit *>(w);
	if (te)
	{
		te->cut();
	}
}

void SqlItemView::doPaste()
{
	QWidget * w = m_gridLayout->itemAtPosition(m_column, 1)->widget();
	QTextEdit * te = qobject_cast<QTextEdit *>(w);
	if (te)
	{
		te->paste();
	}
}

void SqlItemView::doPasteOver()
{
	QWidget * w = m_gridLayout->itemAtPosition(m_column, 1)->widget();
	QTextEdit * te = qobject_cast<QTextEdit *>(w);
	const QMimeData *mimeData = QApplication::clipboard()->mimeData();
	if (te && mimeData->hasText())
	{
		te->setText(mimeData->text());
	}
}

void SqlItemView::doSelectAll()
{
	QWidget * w = m_gridLayout->itemAtPosition(m_column, 1)->widget();
	QTextEdit * te = qobject_cast<QTextEdit *>(w);
	if (te)
	{
		te->selectAll();
	}
}

void SqlItemView::textChanged()
{
	QWidget * w = m_gridLayout->itemAtPosition(m_column, 1)->widget();
	QTextEdit * te = qobject_cast<QTextEdit *>(w);
	if (te && !m_changing)
	{
		m_changing = true;
		QModelIndex index = m_model->index(m_row, m_column);
		m_model->setData(index, QVariant(te->toPlainText()));
		setCurrentIndex(m_row, m_column);
		emit dataChanged();
		m_changing = false;
	}
}
