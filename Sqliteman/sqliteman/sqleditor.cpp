/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME handle very long lines better
*/
#include <QMessageBox>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QFileDialog>
#include <QLabel>
#include <QProgressDialog>
#include <QProgressDialog>
#include <QSqlQuery>
#include <QSqlError>
#include <QShortcut>
#include <QtCore/QDateTime>

#include <qscilexer.h>

#include "createviewdialog.h"
#include "database.h"
#include "preferences.h"
#include "queryeditordialog.h"
#include "sqleditor.h"
#include "sqlkeywords.h"
#include "sqlmodels.h"
#include "utils.h"


SqlEditor::SqlEditor(LiteManWindow * parent)
	: QMainWindow(parent),
   	  m_fileWatcher(0)
{
	creator = parent;
    showingMessage = false;
	ui.setupUi(this);

#ifdef Q_WS_MAC
    ui.toolBar->setIconSize(QSize(16, 16));
#endif


	m_fileName = QString();

	ui.sqlTextEdit->prefsChanged();
    // addon run sql shortcut

	changedLabel = new QLabel(this);
	cursorTemplate = tr("Col: %1 Line: %2/%3");
	cursorLabel = new QLabel(this);
	statusBar()->addPermanentWidget(changedLabel);
	statusBar()->addPermanentWidget(cursorLabel);
	sqlTextEdit_cursorPositionChanged(1, 1);

	toSQLParse::settings cur;
	cur.ExpandSpaces = false;
	cur.CommaBefore = false;
	cur.BlockOpenLine = false;
	cur.OperatorSpace = false;
	cur.KeywordUpper = false;
	cur.RightSeparator = false;
	cur.EndBlockNewline = false;
	cur.IndentLevel = true;
	cur.CommentColumn = false;
	toSQLParse::setSetting(cur);

	ui.searchFrame->hide();

	ui.previousToolButton->setIcon(Utils::getIcon("go-previous.png"));
	ui.nextToolButton->setIcon(Utils::getIcon("go-next.png"));
	ui.actionRun_SQL->setIcon(Utils::getIcon("runsql.png"));
	ui.actionRun_Explain->setIcon(Utils::getIcon("explain.png"));
	ui.actionRun_ExplainQueryPlan->setIcon(Utils::getIcon("queryplan.png"));
	ui.actionRun_as_Script->setIcon(Utils::getIcon("runscript.png"));
	ui.action_Open->setIcon(Utils::getIcon("document-open.png"));
	ui.action_Save->setIcon(Utils::getIcon("document-save.png"));
	ui.action_New->setIcon(Utils::getIcon("document-new.png"));
	ui.actionSave_As->setIcon(Utils::getIcon("document-save-as.png"));
	ui.actionCreateView->setIcon(Utils::getIcon("view.png"));
	ui.actionSearch->setIcon(Utils::getIcon("system-search.png"));

    QShortcut * alternativeSQLRun = new QShortcut(this);
    alternativeSQLRun->setKey(Qt::CTRL + Qt::Key_Return);

    Preferences * prefs = Preferences::instance();
    restoreState(prefs->sqleditorState());

    connect(ui.actionShow_History, SIGNAL(triggered()),
            this, SLOT(actionShow_History_triggered()));
    actionShow_History_triggered();

	connect(ui.actionRun_SQL, SIGNAL(triggered()),
			this, SLOT(actionRun_SQL_triggered()));
    // alternative run action for Ctrl+Enter
    connect(alternativeSQLRun, SIGNAL(activated()),
            this, SLOT(actionRun_SQL_triggered()));
	connect(ui.actionRun_ExplainQueryPlan, SIGNAL(triggered()),
			this, SLOT(actionRun_ExplainQueryPlan_triggered()));
	connect(ui.actionRun_Explain, SIGNAL(triggered()),
			this, SLOT(actionRun_Explain_triggered()));
	connect(ui.actionRun_as_Script, SIGNAL(triggered()),
			this, SLOT(actionRun_as_Script_triggered()));
	connect(ui.action_Open, SIGNAL(triggered()),
			this, SLOT(action_Open_triggered()));
	connect(ui.action_Save, SIGNAL(triggered()),
			this, SLOT(action_Save_triggered()));
	connect(ui.action_New, SIGNAL(triggered()),
			this, SLOT(action_New_triggered()));
	connect(ui.actionSave_As, SIGNAL(triggered()),
			this, SLOT(actionSave_As_triggered()));
	connect(ui.actionCreateView, SIGNAL(triggered()),
			this, SLOT(actionCreateView_triggered()));
	connect(ui.sqlTextEdit, SIGNAL(cursorPositionChanged(int,int)),
			this, SLOT(sqlTextEdit_cursorPositionChanged(int,int)));
	connect(ui.sqlTextEdit, SIGNAL(modificationChanged(bool)),
			this, SLOT(documentChanged(bool)));
	connect(parent, SIGNAL(prefsChanged()),
			ui.sqlTextEdit, SLOT(prefsChanged()));

	// search
	connect(ui.actionSearch, SIGNAL(triggered()),
            this, SLOT(actionSearch_triggered()));
	connect(ui.searchEdit, SIGNAL(textChanged(const QString &)),
			this, SLOT(searchEdit_textChanged(const QString &)));
	connect(ui.previousToolButton, SIGNAL(clicked()),
            this, SLOT(findPrevious()));
	connect(ui.nextToolButton, SIGNAL(clicked()), this, SLOT(findNext()));
	connect(ui.searchEdit, SIGNAL(returnPressed()), this, SLOT(findNext()));

    connect(ui.toolBar, SIGNAL(visibilityChanged(bool)),
            this, SLOT(updateVisibility()));
}

SqlEditor::~SqlEditor()
{
    Preferences * prefs = Preferences::instance();
    prefs->setsqleditorState(saveState());
}

void SqlEditor::setStatusMessage(const QString & message)
{
	if (message.isNull() || message.isEmpty())
		ui.statusBar->clearMessage();
	ui.statusBar->showMessage(message);
}

QString SqlEditor::query(bool creatingView)
{
	toSQLParse::editorTokenizer tokens(ui.sqlTextEdit);

	int cpos, cline;
	ui.sqlTextEdit->getCursorPosition(&cline, &cpos);

	int line;
	int pos;
	do
	{
		line = tokens.line();
		pos = tokens.offset();
		toSQLParse::parseStatement(tokens);
	}
	while (   (tokens.line() < cline)
           || ((tokens.line() == cline && tokens.offset() < cpos)));

	QString result(prepareExec(tokens, line, pos));
    if (result.isNull() || result.trimmed().isEmpty()) {
        if (creatingView) {
            QMessageBox::warning(this, tr("Nothing selected"), tr("No text selected to pass to CREATE VIEW"));
        } else {
            QMessageBox::warning(this, tr("No SQL statement"), tr("You are trying to run an empty SQL statement. Hint: select your statement in the editor"));
        }
        return NULL;
    } else if (creator && creator->checkForPending()) {
        return result;
    } else {
        return NULL;
    }
}

QString SqlEditor::prepareExec(toSQLParse::tokenizer &tokens, int line, int pos)
{
	int endLine, endCol;

	if (ui.sqlTextEdit->lines() <= tokens.line())
	{
		endLine = ui.sqlTextEdit->lines() - 1;
		endCol = ui.sqlTextEdit->lineLength(ui.sqlTextEdit->lines() - 1);
	}
	else
	{
		endLine = tokens.line();
		if (ui.sqlTextEdit->lineLength(tokens.line()) <= tokens.offset())
			endCol = ui.sqlTextEdit->lineLength(tokens.line());
		else
		{
			endCol = tokens.offset();
		}
	}
	ui.sqlTextEdit->setSelection(line, pos, endLine, endCol);
	QString t = ui.sqlTextEdit->selectedText();

	bool comment = false;
	bool multiComment = false;
	int oline = line;
	int opos = pos;
	int i;

	for (i = 0;i < t.length() - 1;i++)
	{
		if (comment)
		{
			if (t.at(i) == '\n')
				comment = false;
		}
		else if (multiComment)
		{
			if (t.at(i) == '*' &&
						 t.at(i + 1) == '/')
			{
				multiComment = false;
				i++;
			}
		}
		else if (t.at(i) == '-' &&
					   t.at(i + 1) == '-')
			comment = true;
		else if (t.at(i) == '/' &&
					   t.at(i + 1) == '/')
			comment = true;
		else if (t.at(i) == '/' &&
					   t.at(i + 1) == '*')
			multiComment = true;
		else if (!t.at(i).isSpace() && t.at(i) != '/')
			break;

		if (t.at(i) == '\n')
		{
			line++;
			pos = 0;
		}
		else
			pos++;
	}

	if (line != oline ||
		   pos != opos)
	{
		ui.sqlTextEdit->setSelection(line, pos, endLine, endCol);
		t = t.mid(i);
	}

	return t;
}

void SqlEditor::actionRun_SQL_triggered()
{
    QString sql(query(false));
    if (!sql.isNull()) {
        if (creator->doExecSql(sql, false)) {
            if (Utils::updateObjectTree(sql)) { emit buildTree(); }
            if (Utils::updateTables(sql)) { emit refreshTable(); }
            appendHistory(sql);
            creator->buildPragmasTree();
        }
    }
}

void SqlEditor::actionRun_Explain_triggered()
{
    QString q(query(false));
    if (!q.isNull()) {
        QString s("explain " + q);
        (void)creator->doExecSql(s, false);
        appendHistory(s);
    }
}

void SqlEditor::actionRun_ExplainQueryPlan_triggered()
{
    QString q(query(false));
    if (!q.isNull()) {
        QString s("explain query plan " + q);
        (void)creator->doExecSql(s, false);
        appendHistory(s);
    }
}

void SqlEditor::actionRun_as_Script_triggered()
{
	if ((!creator) || !(creator->checkForPending())) { return; }
	SqlQueryModel * model = 0;
	bool rebuildTree = false;
	bool updateTable = false;
	m_scriptCancelled = false;
	toSQLParse::editorTokenizer tokens(ui.sqlTextEdit);
	int cpos, cline;
	ui.sqlTextEdit->getCursorPosition(&cline, &cpos);
	QString sql;
	int line;
	int pos;
	QProgressDialog * dialog = 0;
	QSqlQuery query(QSqlDatabase::database(SESSION_NAME));
	bool isError = false;
	do
	{
		line = tokens.line();
		pos = tokens.offset();
		qApp->processEvents();
		if (m_scriptCancelled)
			break;
		toSQLParse::parseStatement(tokens);
        if (   tokens.line() < cline
            || (tokens.line() == cline && tokens.offset() < cpos))
        { continue; }
        sql = prepareExec(tokens, line, pos);
        if (sql.trimmed().isEmpty()) { continue; }
        if (dialog == 0) {
            dialog = new QProgressDialog(tr("Executing all statements"),
                         tr("Cancel"), 0, ui.sqlTextEdit->lines(), this);
            dialog->setValue(line);
            connect(dialog, SIGNAL(canceled()), this, SLOT(scriptCancelled()));
            emit sqlScriptStart();
            emit showSqlScriptResult("-- " + tr("Script started"));
        }
        emit showSqlScriptResult(sql);
        SqlQueryModel * mdl = new SqlQueryModel(creator);
        mdl->setQuery(sql, QSqlDatabase::database(SESSION_NAME));
        appendHistory(sql);
        if (mdl->lastError().isValid())
        {
            QString s1(mdl->lastError().driverText());
            QString s2(mdl->lastError().databaseText());
            if (s1.size() > 0) {
                if (s2.size() > 0) {
                    s1.append(" ").append(s2);
                }
            } else { s1 = s2; }
            emit showSqlScriptResult(
                "-- " + tr("Error: %1.").arg(mdl->lastError().text()));
            int com = QMessageBox::question(this, tr("Run as Script"),
                    tr("This script contains the following error:\n")
                    + s1
                    + tr("\nAt line: %1").arg(line),
                    QMessageBox::Ignore, QMessageBox::Abort);
            if (com == QMessageBox::Abort)
            {
                scriptCancelled();
                isError = true;
                break;
            }
        }
        else
        {
            if (Utils::updateObjectTree(sql)) { rebuildTree = true; }
            if (Utils::updateTables(sql)) { updateTable = true; }
            emit showSqlScriptResult("-- " + tr("No error"));
            if (mdl->rowCount() > 0) { model = mdl; }
            else delete mdl;
        }
        emit showSqlScriptResult("--");
	} while (tokens.line() < ui.sqlTextEdit->lines());
	ui.sqlTextEdit->setSelection(cline, cpos, tokens.line(), tokens.offset());
    if (dialog == 0) {
        QMessageBox::warning(this, tr("No SQL statement"),
            tr("There are no SQL statments to execute after the cursor"));
    } else {
        delete dialog;
        if (!isError)
            emit showSqlScriptResult("-- " + tr("Script finished"));
        if (rebuildTree) { emit buildTree(); }
        if (updateTable) { emit refreshTable(); }
        if (model)
        {
            creator->setTableModel(model);
        }
        creator->buildPragmasTree();
    }
}

void SqlEditor::actionCreateView_triggered()
{
	if (ui.sqlTextEdit->hasSelectedText())
	{
		// test the real selection. Qscintilla does not
		// reset selection after file reopening.
		int f1, f2, t1, t2;
		ui.sqlTextEdit->getSelection(&f1, &f2, &t1, &t2);
		if (f1 > 0 || f2 > 0 || t1 > 0 || t2 > 0) {
            emit showSqlScriptResult("");
			creator->createViewFromSql(ui.sqlTextEdit->selectedText());
            return;
        }
	}
	emit showSqlScriptResult("");
	creator->createViewFromSql(query(true));
}

void SqlEditor::action_Open_triggered()
{
	emit showSqlScriptResult("");
	if (!changedConfirm())
		return;

	QString newFile = QFileDialog::getOpenFileName(this, tr("Open SQL Script"),
			QDir::currentPath(), tr("SQL file (*.sql);;All Files (*)"));
	if (newFile.isNull())
		return;
	open(newFile);
}

void SqlEditor::open(const QString &  newFile)
{
	QFile f(newFile);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::warning(this, tr("Open SQL Script"),
							 tr("Cannot open file %1").arg(newFile));
		return;
	}

	canceled = false;
	int prgTmp = 0;
	progress = new QProgressDialog(tr("Opening: %1").arg(newFile),
								   tr("Abort"), 0, f.size(), this);
	connect(progress, SIGNAL(canceled()), this, SLOT(cancel()));
	progress->setWindowModality(Qt::WindowModal);
	progress->setMinimumDuration(1000);
	ui.sqlTextEdit->clear();

	QTextStream in(&f);
	QString line;
	QStringList strList;
	while (!in.atEnd())
	{
		line = in.readLine();
		strList.append(line);
		prgTmp += line.length();
		if (!setProgress(prgTmp))
		{
			strList.clear();
			break;
		}
	}

	f.close();

	m_fileName = newFile;
	setFileWatcher(newFile);

	progress->setLabelText(tr("Formatting the text. Please wait."));
	ui.sqlTextEdit->append(strList.join("\n"));
	ui.sqlTextEdit->setModified(false);

	delete progress;
	progress = 0;
}

void SqlEditor::cancel()
{
	canceled = true;
}

bool SqlEditor::setProgress(int p)
{
	if (canceled)
		return false;
	progress->setValue(p);
	qApp->processEvents();
	return true;
}

void SqlEditor::appendHistory(const QString & sql)
{
    QStringList l;
    l << sql << QDateTime::currentDateTime().toString();
    QTreeWidgetItem * item = new QTreeWidgetItem(ui.historyTreeWidget, l);
    ui.historyTreeWidget->addTopLevelItem(item);
    if (ui.historyTreeWidget->topLevelItemCount() > 30)
        delete ui.historyTreeWidget->takeTopLevelItem(0);
}

void SqlEditor::actionShow_History_triggered()
{
	emit showSqlScriptResult("");
	ui.historyTreeWidget->setVisible(ui.actionShow_History->isChecked());
}

void SqlEditor::action_Save_triggered()
{
	emit showSqlScriptResult("");
	if (m_fileName.isNull())
	{
		actionSave_As_triggered();
		return;
	}
	saveFile();
}

void SqlEditor::action_New_triggered()
{
	emit showSqlScriptResult("");
	if (!changedConfirm())
		return;
	m_fileName = QString();
	ui.sqlTextEdit->clear();
	ui.sqlTextEdit->setModified(false);
	if (m_fileWatcher) { m_fileWatcher->removePaths(m_fileWatcher->files()); }
}

void SqlEditor::actionSave_As_triggered()
{
	emit showSqlScriptResult("");
	QString newFile = QFileDialog::getSaveFileName(this, tr("Save SQL Script"),
			QDir::currentPath(), tr("SQL file (*.sql);;All Files (*)"));
	if (newFile.isNull())
		return;
	m_fileName = newFile;
	setFileWatcher(newFile);
	saveFile();
}

void SqlEditor::saveFile()
{
	QFile f(m_fileName);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		QMessageBox::warning(this, tr("Save SQL Script"),
							 tr("Cannot write into file %1").arg(m_fileName));
	}
	else
	{
		QTextStream out(&f);
		out << ui.sqlTextEdit->text();
		f.close();
   	   	ui.sqlTextEdit->setModified(false);
   	}
}

bool SqlEditor::changedConfirm()
{
	if (ui.sqlTextEdit->isModified())
	{
		int ret = QMessageBox::question(this, tr("New SQL script"),
					tr("Your current SQL script will be lost. Are you sure?"),
					QMessageBox::Yes, QMessageBox::No);
	
		if (ret == QMessageBox::No)
			return false;
	}
	return true;
}

bool SqlEditor::saveOnExit()
{
	if (!ui.sqlTextEdit->isModified()) {
		return true;
	}
	
	const int ret = QMessageBox::question(this, tr("Closing SQL Editor"),
		tr("SQl script has been changed. Do you want to save its content?"),
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,QMessageBox::No);
	
	if (ret == QMessageBox::No) {
			return true;
	}
	
	if (ret == QMessageBox::Cancel) {
		return false;
	}

	if (m_fileName.isNull()) {
		actionSave_As_triggered();
	} else {
		saveFile();
	}
	
	return true;
}

void SqlEditor::sqlTextEdit_cursorPositionChanged(int line, int pos)
{
	cursorLabel->setText(cursorTemplate.arg(pos+1).arg(line+1).arg(ui.sqlTextEdit->lines()));
}

void SqlEditor::documentChanged(bool state)
{
	emit showSqlScriptResult("");
	changedLabel->setText(state ? "*" : " ");
}

void SqlEditor::setFileName(const QString & fname)
{
	open(fname);
}

void SqlEditor::actionSearch_triggered()
{
	emit showSqlScriptResult("");
	ui.searchFrame->setVisible(ui.actionSearch->isChecked());
	if (!ui.searchFrame->isVisible())
	{
		ui.sqlTextEdit->setFocus();
		return;
	}
	ui.searchEdit->selectAll();
	ui.searchEdit->setFocus();
}

void SqlEditor::find(QString ttf, bool forward/*, bool backward*/)
{
	bool found = ui.sqlTextEdit->findFirst(
									ttf,
			 						false,
									ui.caseCheckBox->isChecked(),
									ui.wholeWordsCheckBox->isChecked(),
									true,
									forward);
    ui.sqlTextEdit->highlightAllOccurrences(ttf, ui.caseCheckBox->isChecked(),
                                             ui.wholeWordsCheckBox->isChecked());
	QPalette p = ui.searchEdit->palette();
	p.setColor(QPalette::Active, QPalette::Base,
			   found ? Qt::white : QColor(255, 102, 102));
	ui.searchEdit->setPalette(p);
}

void SqlEditor::searchEdit_textChanged(const QString &)
{
	findNext();
}

void SqlEditor::findNext()
{
	find(ui.searchEdit->text(), true);
}

void SqlEditor::findPrevious()
{
	find(ui.searchEdit->text(), false);
}

void SqlEditor::externalFileChange(const QString & path)
{
    if (showingMessage) { return; }
    showingMessage = true;
   	int b = QMessageBox::information(this, tr("SQL editor"),
		tr("Your currently edited file has been changed outside " \
		   "this application. Do you want to reload it?"),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
	if (b == QMessageBox::Yes) { open(path); }
    showingMessage = false;
	return;
	
}

void SqlEditor::setFileWatcher(const QString & newFileName)
{
	if (m_fileWatcher)
		m_fileWatcher->removePaths(m_fileWatcher->files());
	else
	{
		m_fileWatcher = new QFileSystemWatcher(this);
	   	connect(m_fileWatcher, SIGNAL(fileChanged(const QString &)),
			this, SLOT(externalFileChange(const QString &)));
	}

	m_fileWatcher->addPath(newFileName);
}

void SqlEditor::scriptCancelled()
{
	emit showSqlScriptResult("-- " + tr("Script was cancelled by user"));
	m_scriptCancelled = true;
}

void SqlEditor::updateVisibility()
{
    creator->actToggleSqlEditorToolBar->setChecked(ui.toolBar->isVisible());
}

void SqlEditor::handleToolBar()
{
    ui.toolBar->setVisible(!ui.toolBar->isVisible());
    updateVisibility();
}

void SqlEditor::showEvent(QShowEvent * event)
{
    QMainWindow::showEvent(event);
	ui.sqlTextEdit->setFocus();
    updateVisibility();
}
