/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
	FIXME creating empty constraint name is legal
*/
#include <QTreeWidget>
#include <QTableView>
#include <QSplitter>
#include <QMenuBar>
#include <QMenu>
#include <QtCore/QTime>

#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <QtCore/QCoreApplication>
#include <QCloseEvent>
#include <QtCore/QFileInfo>
#include <QAction>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QtDebug>

#include "altertabledialog.h"
#include "altertriggerdialog.h"
#include "alterviewdialog.h"
#include "analyzedialog.h"
#include "buildtime.h"
#include "constraintsdialog.h"
#include "createindexdialog.h"
#include "createtabledialog.h"
#include "createtriggerdialog.h"
#include "createviewdialog.h"
#include "database.h"
#include "dataviewer.h"
#include "helpbrowser.h"
#include "importtabledialog.h"
#include "litemanwindow.h"
#include "populatordialog.h"
#include "preferences.h"
#include "preferencesdialog.h"
#include "queryeditordialog.h"
#include "schemabrowser.h"
#include "sqleditor.h"
#include "sqliteprocess.h"
#include "sqlmodels.h"
#include "utils.h"
#include "vacuumdialog.h"

#ifdef INTERNAL_SQLDRIVER
#include "driver/qsql_sqlite.h"
#endif

LiteManWindow::LiteManWindow(
    const QString & fileToOpen, const QString & scriptToOpen, bool executeScript)
	: QMainWindow(),
	m_lastDB(""),
	m_lastSqlFile(scriptToOpen)
{
    m_currentItem = NULL;
	m_isOpen = false;
	tableTreeTouched = false;
	recentDocs.clear();
	initUI();
	initActions();
	initMenus();

	statusBar();
	m_sqliteVersionLabel = new QLabel(this);
	m_activeItem = 0;
	statusBar()->addPermanentWidget(m_sqliteVersionLabel);
	readSettings();
	queryEditor = new QueryEditorDialog(this);
	// Check command line
	if (!fileToOpen.isEmpty()) { open(fileToOpen); }
	if (executeScript) {
        sqlEditor->actionRun_as_Script_triggered();
    }
}

LiteManWindow::~LiteManWindow()
{
	Preferences::deleteInstance();
}

bool LiteManWindow::checkForPending() {
	return dataViewer->checkForPending();
}

void LiteManWindow::buildPragmasTree() {
	schemaBrowser->buildPragmasTree();
}

void LiteManWindow::closeEvent(QCloseEvent * e)
{
	// check for uncommitted transaction in the DB
	if (!dataViewer->checkForPending())
	{
		e->ignore();
		return;
	}
	
	if (!Database::isAutoCommit())
	{
		int com = QMessageBox::question(this, tr("Sqliteman"),
			tr("There is a database transaction pending.\n"
			   "Do you wish to commit it?\n\n"
			   "Yes = commit\nNo = rollback\n"
			   "Cancel = do not exit sqliteman"),
			QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
		if (com == QMessageBox::Yes)
		{
			QString sql = QString("COMMIT ;");
			QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
			if (query.lastError().isValid())
			{
				dataViewer->setStatusText(
					tr("Cannot commit transaction")
					+ ":<br/><span style=\" color:#ff0000;\">"
					+ query.lastError().text()
					+ "<br/></span>" + tr("using sql statement:")
					+ "<br/><tt>" + sql);
				e->ignore();
				return;
			}
		}
		else if (com == QMessageBox::Cancel)
		{
			e->ignore();
			return;
		}
	}

	if (!sqlEditor->saveOnExit()) {
		e->ignore ();
	}
	
	writeSettings();

	dataViewer->setTableModel(new QSqlQueryModel(), false);
	if (QSqlDatabase::contains(SESSION_NAME))
	{
		QSqlDatabase::database(SESSION_NAME).rollback();
		QSqlDatabase::database(SESSION_NAME).close();
		QSqlDatabase::removeDatabase(SESSION_NAME);
	}

	e->accept();
}


void LiteManWindow::initUI()
{
	setWindowTitle(m_appName);
	splitter = new QSplitter(this);
	splitterSql = new QSplitter(Qt::Vertical, this);

	schemaBrowser = new SchemaBrowser(this);
	dataViewer = new DataViewer(this);
	sqlEditor = new SqlEditor(this);

	splitterSql->addWidget(sqlEditor);
	splitterSql->addWidget(dataViewer);
	splitterSql->setCollapsible(0, false);
	splitterSql->setCollapsible(1, false);

	splitter->addWidget(schemaBrowser);
	splitter->addWidget(splitterSql);
	splitter->setCollapsible(0, false);
	splitter->setCollapsible(1, false);

	setCentralWidget(splitter);

	// Disable the UI, as long as there is no open database
	schemaBrowser->setEnabled(false);
	dataViewer->setEnabled(false);
	sqlEditor->setEnabled(false);
	sqlEditor->hide();

    // schema browser
	connect(schemaBrowser->tableTree,
			SIGNAL(itemActivated(QTreeWidgetItem *, int)),
			this, SLOT(treeItemActivated(QTreeWidgetItem *, int)));
	connect(schemaBrowser->tableTree,
			SIGNAL(customContextMenuRequested(const QPoint &)),
			this, SLOT(treeContextMenuOpened(const QPoint &)));
    connect(schemaBrowser->tableTree,
            SIGNAL(itemSelectionChanged()),
            this, SLOT(tableTreeSelectionChanged()));
	// sql editor
	connect(sqlEditor, SIGNAL(sqlScriptStart()),
			dataViewer, SLOT(sqlScriptStart()));
	connect(sqlEditor, SIGNAL(showSqlScriptResult(QString)),
			dataViewer, SLOT(showSqlScriptResult(QString)));
	connect(sqlEditor, SIGNAL(buildTree()),
			schemaBrowser->tableTree, SLOT(buildTree()));
	connect(sqlEditor, SIGNAL(refreshTable()),
			this, SLOT(refreshTable()));
	connect(dataViewer, SIGNAL(tableUpdated()),
			this, SLOT(updateContextMenu()));
	connect(dataViewer, SIGNAL(deleteMultiple()),
			this, SLOT(doMultipleDeletion()));
}

void LiteManWindow::initActions()
{
	newAct = new QAction(Utils::getIcon("document-new.png"),
						 tr("&New..."), this);
	newAct->setShortcut(tr("Ctrl+N"));
	connect(newAct, SIGNAL(triggered()), this, SLOT(newDB()));

	openAct = new QAction(Utils::getIcon("document-open.png"),
						  tr("&Open..."), this);
	openAct->setShortcut(tr("Ctrl+O"));
	connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

	recentAct = new QAction(tr("&Recent Databases"), this);
	recentFilesMenu = new QMenu(this);
	recentAct->setMenu(recentFilesMenu);

	preferencesAct = new QAction(tr("&Preferences..."), this);
	preferencesAct->setShortcut(tr("Ctrl+P"));
	connect(preferencesAct, SIGNAL(triggered()), this, SLOT(preferences()));

	exitAct = new QAction(Utils::getIcon("close.png"), tr("E&xit"), this);
	exitAct->setShortcut(tr("Ctrl+Q"));
	connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

	aboutAct = new QAction(tr("&About..."), this);
	aboutAct->setShortcut(tr("Ctrl+A"));
	connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

	aboutQtAct = new QAction(tr("About &Qt..."), this);
	connect(aboutQtAct, SIGNAL(triggered()), this, SLOT(aboutQt()));

	helpAct = new QAction(tr("&Help Content..."), this);
	helpAct->setShortcut(tr("F1"));
	connect(helpAct, SIGNAL(triggered()), this, SLOT(help()));

	execSqlAct = new QAction(tr("&SQL Editor"), this);
	execSqlAct->setShortcut(tr("Ctrl+E"));
	execSqlAct->setCheckable(true);
	connect(execSqlAct, SIGNAL(triggered()), this, SLOT(handleSqlEditor()));

    actToggleSqlEditorToolBar = new QAction(tr("SQL Editor toolbar"), this);
    actToggleSqlEditorToolBar->setCheckable(true);
    connect(actToggleSqlEditorToolBar, SIGNAL(triggered()),
            sqlEditor, SLOT(handleToolBar()));

	schemaBrowserAct = new QAction(tr("Schema &Browser"), this);
	schemaBrowserAct->setShortcut(tr("Ctrl+B"));
	schemaBrowserAct->setCheckable(true);
	connect(schemaBrowserAct, SIGNAL(triggered()),
            this, SLOT(handleSchemaBrowser()));

	dataViewerAct = new QAction(tr("Data Vie&wer"), this);
	dataViewerAct->setShortcut(tr("Ctrl+D"));
	dataViewerAct->setCheckable(true);
	connect(dataViewerAct, SIGNAL(triggered()),
            this, SLOT(handleDataViewer()));

    actToggleDataViewerToolBar = new QAction(tr("Data Viewer toolbar"), this);
	actToggleDataViewerToolBar->setCheckable(true);
    connect(actToggleDataViewerToolBar, SIGNAL(triggered()),
            dataViewer, SLOT(handleToolBar()));

	buildQueryAct = new QAction(tr("&Build Query..."), this);
	buildQueryAct->setShortcut(tr("Ctrl+R"));
	connect(buildQueryAct, SIGNAL(triggered()), this, SLOT(buildQuery()));

	buildAnyQueryAct = new QAction(tr("&Build Query..."), this);
	connect(buildAnyQueryAct, SIGNAL(triggered()), this, SLOT(buildAnyQuery()));

	exportSchemaAct = new QAction(tr("&Export Schema..."), this);
	connect(exportSchemaAct, SIGNAL(triggered()), this, SLOT(exportSchema()));

	dumpDatabaseAct = new QAction(tr("&Dump Database..."), this);
	connect(dumpDatabaseAct, SIGNAL(triggered()), this, SLOT(dumpDatabase()));

	createTableAct = new QAction(Utils::getIcon("table.png"),
								 tr("&Create Table..."), this);
	createTableAct->setShortcut(tr("Ctrl+T"));
	connect(createTableAct, SIGNAL(triggered()), this, SLOT(createTable()));

	dropTableAct = new QAction(tr("&Drop Table"), this);
	connect(dropTableAct, SIGNAL(triggered()), this, SLOT(dropTable()));

	alterTableAct = new QAction(tr("&Alter Table..."), this);
	alterTableAct->setShortcut(tr("Ctrl+L"));
	connect(alterTableAct, SIGNAL(triggered()), this, SLOT(alterTable()));

	populateTableAct = new QAction(tr("&Populate Table..."), this);
	connect(populateTableAct, SIGNAL(triggered()), this, SLOT(populateTable()));

	createViewAct = new QAction(Utils::getIcon("view.png"),
								tr("Create &View..."), this);
	createViewAct->setShortcut(tr("Ctrl+G"));
	connect(createViewAct, SIGNAL(triggered()), this, SLOT(createView()));

	dropViewAct = new QAction(tr("&Drop View"), this);
	connect(dropViewAct, SIGNAL(triggered()), this, SLOT(dropView()));

	alterViewAct = new QAction(tr("&Alter View..."), this);
	connect(alterViewAct, SIGNAL(triggered()), this, SLOT(alterView()));

	createIndexAct = new QAction(Utils::getIcon("index.png"),
								 tr("&Create Index..."), this);
	createIndexAct->setShortcut(tr("Ctrl+I"));
	connect(createIndexAct, SIGNAL(triggered()), this, SLOT(createIndex()));

	dropIndexAct = new QAction(tr("&Drop Index"), this);
	connect(dropIndexAct, SIGNAL(triggered()), this, SLOT(dropIndex()));

	describeTableAct = new QAction(tr("D&escribe Table"), this);
	connect(describeTableAct, SIGNAL(triggered()), this, SLOT(describeTable()));

	importTableAct = new QAction(tr("&Import Table Data..."), this);
	connect(importTableAct, SIGNAL(triggered()), this, SLOT(importTable()));

	emptyTableAct = new QAction(tr("&Empty Table"), this);
	connect(emptyTableAct, SIGNAL(triggered()), this, SLOT(emptyTable()));

	createTriggerAct = new QAction(Utils::getIcon("trigger.png"),
								   tr("&Create Trigger..."), this);
	connect(createTriggerAct, SIGNAL(triggered()), this, SLOT(createTrigger()));

	alterTriggerAct = new QAction(tr("&Alter Trigger..."), this);
	connect(alterTriggerAct, SIGNAL(triggered()), this, SLOT(alterTrigger()));

	dropTriggerAct = new QAction(tr("&Drop Trigger"), this);
	connect(dropTriggerAct, SIGNAL(triggered()), this, SLOT(dropTrigger()));

	describeTriggerAct = new QAction(tr("D&escribe Trigger"), this);
	connect(describeTriggerAct, SIGNAL(triggered()), this, SLOT(describeTrigger()));

	describeViewAct = new QAction(tr("D&escribe View"), this);
	connect(describeViewAct, SIGNAL(triggered()), this, SLOT(describeView()));

	describeIndexAct = new QAction(tr("D&escribe Index"), this);
	connect(describeIndexAct, SIGNAL(triggered()), this, SLOT(describeIndex()));

	reindexAct = new QAction(tr("&Reindex"), this);
	connect(reindexAct, SIGNAL(triggered()), this, SLOT(reindex()));

	analyzeAct = new QAction(tr("&Analyze Statistics..."), this);
	connect(analyzeAct, SIGNAL(triggered()), this, SLOT(analyzeDialog()));

	vacuumAct = new QAction(tr("&Vacuum..."), this);
	connect(vacuumAct, SIGNAL(triggered()), this, SLOT(vacuumDialog()));

	attachAct = new QAction(tr("A&ttach Database..."), this);
	connect(attachAct, SIGNAL(triggered()), this, SLOT(attachDatabase()));

	detachAct = new QAction(tr("&Detach Database"), this);
	connect(detachAct, SIGNAL(triggered()), this, SLOT(detachDatabase()));

#ifdef ENABLE_EXTENSIONS
	loadExtensionAct = new QAction(tr("&Load Extensions..."), this);
	connect(loadExtensionAct, SIGNAL(triggered()), this, SLOT(loadExtension()));
#endif

	refreshTreeAct = new QAction(tr("&Refresh Schema Browser"), this);
	connect(refreshTreeAct, SIGNAL(triggered()), schemaBrowser->tableTree, SLOT(buildTree()));

	consTriggAct = new QAction(tr("&Constraint Triggers..."), this);
	connect(consTriggAct, SIGNAL(triggered()), this, SLOT(constraintTriggers()));
}

void LiteManWindow::initMenus()
{
	QMenu * fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(newAct);
	fileMenu->addAction(openAct);
	fileMenu->addAction(recentAct);
	fileMenu->addSeparator();
	fileMenu->addAction(preferencesAct);
	fileMenu->addSeparator();
	fileMenu->addAction(exitAct);

	contextMenu = menuBar()->addMenu(tr("&Context"));
	contextMenu->setEnabled(false);

	databaseMenu = menuBar()->addMenu(tr("&Database"));
	databaseMenu->addAction(createTableAct);
	databaseMenu->addAction(createViewAct);
	databaseMenu->addSeparator();
	databaseMenu->addAction(buildAnyQueryAct);
	databaseMenu->addAction(execSqlAct);
	databaseMenu->addAction(actToggleSqlEditorToolBar);
	databaseMenu->addAction(schemaBrowserAct);
	databaseMenu->addAction(dataViewerAct);
	databaseMenu->addAction(actToggleDataViewerToolBar);
	databaseMenu->addSeparator();
	databaseMenu->addAction(exportSchemaAct);
	databaseMenu->addAction(dumpDatabaseAct);
	databaseMenu->addAction(importTableAct);

	adminMenu = menuBar()->addMenu(tr("&System"));
	adminMenu->addAction(analyzeAct);
	adminMenu->addAction(vacuumAct);
	adminMenu->addSeparator();
	adminMenu->addAction(attachAct);
#ifdef ENABLE_EXTENSIONS
	adminMenu->addSeparator();
	adminMenu->addAction(loadExtensionAct);
#endif

	QMenu * helpMenu = menuBar()->addMenu(tr("&Help"));
	helpMenu->addAction(helpAct);
	helpMenu->addAction(aboutAct);
	helpMenu->addAction(aboutQtAct);

	databaseMenu->setEnabled(false);
	adminMenu->setEnabled(false);
}

void LiteManWindow::updateRecent(QString fn)
{
	if (recentDocs.indexOf(fn) == -1)
		recentDocs.prepend(fn);
	else
		recentDocs.replace(recentDocs.indexOf(fn), fn);
	rebuildRecentFileMenu();
}

void LiteManWindow::removeRecent(QString fn)
{
	if (recentDocs.indexOf(fn) != -1)
		recentDocs.removeAt(recentDocs.indexOf(fn));
	rebuildRecentFileMenu();
}

void LiteManWindow::rebuildRecentFileMenu()
{
	recentFilesMenu->clear();
	uint max = qMin(Preferences::instance()->recentlyUsedCount(), recentDocs.count());
	QFile fi;
	QString accel("&");

	for (uint i = 0; i < max; ++i)
	{
		fi.setFileName(recentDocs.at(i));
		if (!fi.exists())
		{
			removeRecent(recentDocs.at(i));
			break;
		}
		// &10 collides with &1
		if (i > 8)
			accel = "";
		QAction *a = new QAction(accel
								 + QString('1' + i)
								 + " "
								 + recentDocs.at(i),
								 this);
		a->setData(QVariant(recentDocs.at(i)));
		connect(a, SIGNAL(triggered()), this, SLOT(openRecent()));
		recentFilesMenu->addAction(a);
	}
	recentAct->setDisabled(recentDocs.count() == 0);
}

void LiteManWindow::readSettings()
{
    Preferences * prefs = Preferences::instance();
	resize(prefs->litemanwindowWidth(), prefs->litemanwindowHeight());
	splitter->restoreState(prefs->litemansplitter());
	splitterSql->restoreState(prefs->sqlsplitter());

	dataViewer->restoreSplitter(prefs->dataviewersplitter());
    bool shown = prefs->dataviewerShown();
	dataViewer->setVisible(shown);
	dataViewerAct->setChecked(shown);
    shown = prefs->objectbrowserShown();
	schemaBrowser->setVisible(shown);
	schemaBrowserAct->setChecked(shown);
    shown = prefs->sqleditorShown();
	sqlEditor->setVisible(shown);
	execSqlAct->setChecked(shown);

	if (!m_lastSqlFile.isEmpty()) {
        sqlEditor->setFileName(m_lastSqlFile);
    }

	recentDocs = prefs->recentFiles();
	rebuildRecentFileMenu();
}

void LiteManWindow::writeSettings()
{
    Preferences * prefs = Preferences::instance();
    prefs->setlitemanwindowHeight(height());
    prefs->setlitemanwindowWidth(width());
	prefs->setdataviewerShown(dataViewer->isVisible());
	prefs->setobjectbrowserShown(schemaBrowser->isVisible());
	prefs->setsqleditorShown(sqlEditor->isVisible());
	prefs->setdataviewersplitter(dataViewer->saveSplitter());
	prefs->setlitemansplitter(splitter->saveState());
	prefs->setsqlsplitter(splitterSql->saveState());
	prefs->setrecentFIles(recentDocs);
	// last open database
	prefs->setlastDB(QSqlDatabase::database(SESSION_NAME).databaseName());
	prefs->setlastSqlFile(sqlEditor->fileName());
}

void LiteManWindow::newDB()
{
	// Creating a database is virtually the same as opening an existing one.
    // SQLite simply creates a database which doesn't already exist
	QString fileName = QFileDialog::getSaveFileName(this, tr("New Database"), QDir::currentPath(), tr("SQLite database (*)"));

	if(fileName.isNull())
		return;

	if (QFile::exists(fileName))
		QFile::remove(fileName);

	openDatabase(fileName);
}

void LiteManWindow::open(const QString & file)
{
	QString fileName;

	// If no file name was provided, open dialog
	if(!file.isNull()) {
		fileName = file;
    } else {
        QString filetypes = tr(
            "Sqlite database(*.sqlite);;All files(*.*)");
		fileName = QFileDialog::getOpenFileName(this,
								tr("Open Database"),
								QDir::currentPath(),
								filetypes);
    }

	if(fileName.isNull()) { return; }
	if (QFile::exists(fileName))
	{
		openDatabase(fileName);
	}
	else
	{
		dataViewer->setStatusText(
			tr("Cannot open ")
			+ fileName
			+ ":<br/><span style=\" color:#ff0000;\">"
			+ tr ("file does not exist"));
	}
}

void LiteManWindow::openDatabase(const QString & fileName)
{
	if (!checkForPending()) { return; }
	
	/* This messy bit of logic is necessary to ensure that db has gone out of
	 * scope by the time removeDatabase() gets called, otherwise it thinks that
	 * there is an outstanding reference and prints a warning message.
	 */
	{
		QSqlDatabase old = QSqlDatabase::database(SESSION_NAME);
		if (old.isValid())
		{
			removeRef("temp");
			removeRef("main");
			old.close();
			QSqlDatabase::removeDatabase(SESSION_NAME);
			m_isOpen = false;
		}
	}

#ifdef INTERNAL_SQLDRIVER
	QSqlDatabase db =
		QSqlDatabase::addDatabase(new QSQLiteDriver(this), SESSION_NAME);
#else
	QSqlDatabase db =
		QSqlDatabase::addDatabase("QSQLITE", SESSION_NAME);
#endif

	db.setDatabaseName(fileName);

	if (!db.open())
	{
		dataViewer->setStatusText(tr("Cannot open or create ")
								  + QFileInfo(fileName).fileName()
								  + ":<br/><span style=\" color:#ff0000;\">"
								  + db.lastError().text());
		return;
	}
	/* Qt database open() (exactly the sqlite API sqlite3_open16())
	   method does not check if it is really a database. So the dummy
	   select statement should perform a real "is it a database" check
	   for us. */
	QSqlQuery q("select 1 from sqlite_master where 1=2", db);
	if (q.lastError().isValid())
	{
		dataViewer->setStatusText(tr("Cannot access ")
								  + QFileInfo(fileName).fileName()
								  + ":<br/><span style=\" color:#ff0000;\">"
								  + q.lastError().text()
								  + "<br/></span>"
								  + tr("It is probably not a database."));
		db.close();
		return;
	}
	else
	{
		m_isOpen = true;
		dataViewer->removeErrorMessage();

		// check for sqlite library version
		QString ver;
		if (q.exec("select sqlite_version(*);"))
		{
			if(!q.lastError().isValid())
			{
				q.next();
				ver = q.value(0).toString();
			}
			else
				ver = tr("cannot get version");
		}
		else
			ver = tr("cannot get version");
		m_sqliteVersionLabel->setText("Sqlite: " + ver);

#ifdef ENABLE_EXTENSIONS
		// load startup extensions
		bool loadE = Preferences::instance()->allowExtensionLoading();
		handleExtensions(loadE);
		if (loadE && Preferences::instance()->extensionList().count() != 0)
		{
			schemaBrowser->appendExtensions(
					Database::loadExtension(Preferences::instance()->extensionList())
					);
			statusBar()->showMessage(tr("Startup extensions loaded successfully"));
		}
#endif

		QFileInfo fi(fileName);
		QDir::setCurrent(fi.absolutePath());
		m_lastDB = QDir::toNativeSeparators(QDir::currentPath() + "/" + fi.fileName());
	
		updateRecent(fileName);
	
		// Build tree & clean model
		schemaBrowser->tableTree->buildTree();
		schemaBrowser->buildPragmasTree();
		dataViewer->setBuiltQuery(false);
		dataViewer->setTableModel(new QSqlQueryModel(), false);
		m_activeItem = 0;
	
		// Update the title
		setWindowTitle(fi.fileName() + " - " + m_appName);
	}

	// Enable UI
	schemaBrowser->setEnabled(m_isOpen);
	databaseMenu->setEnabled(m_isOpen);
	adminMenu->setEnabled(m_isOpen);
	sqlEditor->setEnabled(m_isOpen);
	dataViewer->setEnabled(m_isOpen);
	if (m_isOpen) {
		int n = Database::makeUserFunctions();
		if (n)
		{
			dataViewer->setStatusText(
				tr("Cannot create user function exec")
				+ ":<br/><span style=\" color:#ff0000;\">"
				+ sqlite3_errstr(n)
				+ "<br/></span>");
		}
	}
}

void LiteManWindow::removeRef(const QString & dbname)
{
	SqlTableModel * m = qobject_cast<SqlTableModel *>(dataViewer->tableData());
	if (m && dbname.compare(m->schema()))
	{
		dataViewer->setBuiltQuery(false);
		dataViewer->setTableModel(new QSqlQueryModel(), false);
		m_activeItem = 0;
	}
}

#ifdef ENABLE_EXTENSIONS
void LiteManWindow::handleExtensions(bool enable)
{
	QString e(enable ? tr("enabled") : tr("disabled"));
	loadExtensionAct->setEnabled(enable);
	if (Database::setEnableExtensions(enable))
		statusBar()->showMessage(tr("Extensions loading is %1").arg(e));
}
#endif

void LiteManWindow::setActiveItem(QTreeWidgetItem * item)
{
	// this sets the active item without any side-effects
	// used when the item was already active but has been recreated
	disconnect(schemaBrowser->tableTree,
			   SIGNAL(itemActivated(QTreeWidgetItem *, int)),
			   this, SLOT(treeItemActivated(QTreeWidgetItem *, int)));
	schemaBrowser->tableTree->setCurrentItem(item);
	connect(schemaBrowser->tableTree,
			SIGNAL(itemActivated(QTreeWidgetItem *, int)),
			this, SLOT(treeItemActivated(QTreeWidgetItem *, int)));
	m_activeItem = item;
    m_currentItem = item;
}

void LiteManWindow::checkForCatalogue()
{
	if (m_activeItem && (m_activeItem->type() == TableTree::SystemType))
	{
		// We are displaying a system item, probably the catalogue,
		// and we've changed something invalidating the catalogue
		dataViewer->saveSelection();
		m_activeItem = 0;
		treeItemActivated(m_activeItem, -1);
		dataViewer->reSelect();
	}
}

void LiteManWindow::openRecent()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (action) { open(action->data().toString()); }
}

QString LiteManWindow::getOSName()
{
    QProcess p;
    QString result;
#if defined Q_OS_LINUX
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("hostnamectl");
    if (p.waitForStarted() && p.waitForFinished()) {
        QString s1(p.readAllStandardOutput());
        QString s2(s1);
        s1.replace(QRegExp("^.*System: ([^\\n]*).*$"), "\\1");
        s2.replace(QRegExp("^.*Kernel: ([^\\n]*).*$"), "\\1");
        result.append(s1).append(" ").append(s2).append("<br/>");
    }
#elif defined Q_OS_WIN32
    p.start("cmd.exe", QStringList() << "ver");
    if (p.waitForStarted() && p.waitForFinished()) {
        QString s1(p.readAllStandardOutput());
        result.append(s1.trimmed()).append("<br/>");
    }
#else
// I don't know what to do here....
#endif
    return result;
}

void LiteManWindow::about()
{
	dataViewer->removeErrorMessage();
    dataViewer->setStatusText(
        tr("sqliteman Version ")
        + SQLITEMAN_VERSION + " " + buildtime + "<br/>"
        + m_sqliteVersionLabel->text() + "<br/>"
        + "Qt " + tr("Version ") + qVersion() + "<br/>"
        + getOSName()
        + tr("Parts")
        + " © 2007 Petr Vanek, "
        + tr("Parts")
        + " © 2021-2023 Richard Parkins<br/>");
}

void LiteManWindow::aboutQt()
{
	dataViewer->removeErrorMessage();
	QMessageBox::aboutQt(this, m_appName);
}

void LiteManWindow::help()
{
	dataViewer->removeErrorMessage();
	if (!helpBrowser) {helpBrowser = new HelpBrowser(m_lang, this); }
	helpBrowser->show();
}

void LiteManWindow::doBuildQuery()
{
	dataViewer->removeErrorMessage();
	int ret = queryEditor->exec();

	if (ret == QDialog::Accepted)
	{
		bool isTable = queryEditor->queryingTable();
		doExecSql(queryEditor->statement(!isTable), isTable);
	}
}

void LiteManWindow::buildQuery()
{
	queryEditor->setItem(m_currentItem);
	doBuildQuery();
}

void LiteManWindow::buildAnyQuery()
{
	queryEditor->setItem(0);
	doBuildQuery();
}

void LiteManWindow::handleSqlEditor()
{
	sqlEditor->setVisible(!sqlEditor->isVisible());
	execSqlAct->setChecked(sqlEditor->isVisible());
}

void LiteManWindow::handleSchemaBrowser()
{
	schemaBrowser->setVisible(!schemaBrowser->isVisible());
	schemaBrowserAct->setChecked(schemaBrowser->isVisible());
}

void LiteManWindow::handleDataViewer()
{
	dataViewer->setVisible(!dataViewer->isVisible());
	dataViewerAct->setChecked(dataViewer->isVisible());
}

void LiteManWindow::exportSchema()
{
	dataViewer->removeErrorMessage();
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export Schema"),
                                                    QDir::currentPath(),
                                                    tr("SQL File (*.sql)"));

	if (fileName.isNull())
		return;

	Database::exportSql(fileName);
}

void LiteManWindow::dumpDatabase()
{
	dataViewer->removeErrorMessage();
	QString fileName = QFileDialog::getSaveFileName(this, tr("Dump Database"),
                                                    QDir::currentPath(),
                                                    tr("SQL File (*.sql)"));

	if (fileName.isNull())
		return;

	if (Database::dumpDatabase(fileName))
	{
		QMessageBox::information(this, m_appName,
								 tr("Dump written into: %1").arg(fileName));
	}
}

void LiteManWindow::createTable()
{
	QTreeWidgetItem old;
	if (m_currentItem != NULL)
	{
		old = QTreeWidgetItem(*m_currentItem);
	}
	dataViewer->removeErrorMessage();
	CreateTableDialog dlg(this, m_currentItem);
	connect(&dlg, SIGNAL(rebuildTableTree(QString)),
			schemaBrowser->tableTree, SLOT(buildTableTree(QString)));
	dlg.exec();
	disconnect(&dlg, SIGNAL(rebuildTableTree(QString)),
			schemaBrowser->tableTree, SLOT(buildTableTree(QString)));
	if (dlg.m_updated)
	{
        QList<QTreeWidgetItem*> l =
            schemaBrowser->tableTree->searchMask(
                schemaBrowser->tableTree->trTables);
        QList<QTreeWidgetItem*>::const_iterator it;
        for (it = l.constBegin(); it != l.constEnd(); ++it) {
            QTreeWidgetItem* p = *it;
			if (   (p->type() == TableTree::TablesItemType)
				&& (p->text(1) == dlg.schema()))
			{
				if (m_activeItem && (m_currentItem != NULL))
				{
					// item recreated but should still be current
					if (dlg.schema() == old.text(1))
					{
						for (int i = 0; i < p->childCount(); ++i)
						{
							if (p->child(i)->text(0) == old.text(0))
							{
								setActiveItem(p->child(i));
							}
						}
					}
				}
			}
		}
		checkForCatalogue();
		queryEditor->tableCreated();
	}
}

void LiteManWindow::alterTable()
{
	dataViewer->removeErrorMessage();
	if (   (!m_currentItem)
        || (m_currentItem->type() != TableTree::TableType))
    {
        return;
    }

	QString oldName = m_currentItem->text(0);
	bool isActive = m_activeItem == m_currentItem;
	dataViewer->saveSelection();
    // checkForPending() is done in the dialog
	AlterTableDialog dlg(this, m_currentItem, isActive);
	connect(&dlg, SIGNAL(rebuildTableTree(QString)),
			schemaBrowser->tableTree, SLOT(buildTableTree(QString)));
	dlg.exec();
	if (dlg.m_updated)
	{
		checkForCatalogue();
		if (isActive)
		{
			m_activeItem = 0; // we've changed it
			treeItemActivated(m_currentItem, -1);
			dataViewer->reSelect();
		}
		dataViewer->setBuiltQuery(false);
		queryEditor->tableAltered(oldName, m_currentItem);
	}
	disconnect(&dlg, SIGNAL(rebuildTableTree(QString)),
			schemaBrowser->tableTree, SLOT(buildTableTree(QString)));
}

void LiteManWindow::populateTable()
{
	dataViewer->removeErrorMessage();
	if (   (!m_currentItem)
        || (m_currentItem->type() != TableTree::TableType))
    {
        return;
    }
	
	bool isActive = m_activeItem == m_currentItem;
	if (isActive && !checkForPending()) { return; }
	PopulatorDialog dlg(this, m_currentItem->text(0), m_currentItem->text(1));
	dlg.exec();
	if (isActive && dlg.m_updated) {
		dataViewer->saveSelection();
		m_activeItem = 0; // we've changed it
		treeItemActivated(m_currentItem, -1);
		dataViewer->reSelect();
	}
}

void LiteManWindow::importTable()
{
	dataViewer->removeErrorMessage();
	QString tableName =
        m_currentItem ? (m_currentItem->type() == TableTree::TableType
                            ? m_currentItem->text(0) : "")
                      : "";

	bool isActive = m_activeItem == m_currentItem;
	if (isActive && !checkForPending()) { return; }
	ImportTableDialog dlg(
        this, tableName, m_currentItem ? m_currentItem->text(1) : "main");
	if (dlg.exec() == QDialog::Accepted)
	{
		dataViewer->saveSelection();
		if (isActive)
		{
			m_activeItem = 0; // we've changed it
			treeItemActivated(m_currentItem, -1);
			dataViewer->reSelect();
		}
		updateContextMenu();
	}
}

void LiteManWindow::dropTable()
{
	dataViewer->removeErrorMessage();
	if (   (!m_currentItem)
        || (m_currentItem->type() != TableTree::TableType))
    {
        return;
    }

	bool isActive = m_activeItem == m_currentItem;
	QString table = m_currentItem->text(0);

	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the table \"%1\"?")
					.arg(table),
					QMessageBox::Yes, QMessageBox::No);

	if (ret == QMessageBox::Yes)
	{
		// don't check for pending, we're dropping it anyway
		QString sql = QString("DROP TABLE ")
					  + Utils::q(m_currentItem->text(1))
					  + "."
					  + Utils::q(table)
					  + ";";
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if (query.lastError().isValid())
		{
			dataViewer->setStatusText(
				tr("Cannot drop table ")
				+ m_currentItem->text(1) + tr(".") + table
				+ ":<br/><span style=\" color:#ff0000;\">"
				+ query.lastError().text()
				+ "<br/></span>" + tr("using sql statement:")
				+ "<br/><tt>" + sql);
		}
		else
		{
			schemaBrowser->tableTree->buildTables(
                m_currentItem->parent(), m_currentItem->text(1), false);
			queryEditor->tableDropped(table);
			checkForCatalogue();
			dataViewer->setBuiltQuery(false);
			if (isActive)
			{
				dataViewer->setNotPending();
				dataViewer->setBuiltQuery(false);
				dataViewer->setTableModel(new QSqlQueryModel(), false);
				m_activeItem = 0;
			}
		}
	}
}

void LiteManWindow::emptyTable()
{
	dataViewer->removeErrorMessage();
	if (   (!m_currentItem)
        || (m_currentItem->type() != TableTree::TableType))
    {
        return;
    }

	bool isActive = m_activeItem == m_currentItem;

	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure you want to remove all records from the"
					   " table  \"%1\"?").arg(m_currentItem->text(0)),
					QMessageBox::Yes, QMessageBox::No);

	if (ret == QMessageBox::Yes)
	{
		// don't check for pending, we're dropping it anyway
		QString sql = QString("DELETE FROM ")
					  + Utils::q(m_currentItem->text(1))
					  + "."
					  + Utils::q(m_currentItem->text(0))
					  + ";";
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if (query.lastError().isValid())
		{
			dataViewer->setStatusText(
				tr("Cannot empty table ")
				+ m_currentItem->text(1) + tr(".") + m_currentItem->text(0)
				+ ":<br/><span style=\" color:#ff0000;\">"
				+ query.lastError().text()
				+ "<br/></span>" + tr("using sql statement:")
				+ "<br/><tt>" + sql);
		}
		else
		{
			dataViewer->setBuiltQuery(false);
			if (isActive)
			{
				dataViewer->setNotPending();
				m_activeItem = 0;
				treeItemActivated(m_currentItem, -1);
			}
			updateContextMenu();
		}
	}
}

void LiteManWindow::createView()
{
	dataViewer->removeErrorMessage();
    // OK if m_currentItem is NULL
	CreateViewDialog dlg(this, m_currentItem);
	connect(&dlg, SIGNAL(rebuildViewTree(QString, QString)),
			schemaBrowser->tableTree, SLOT(buildViewTree(QString, QString)));
	dlg.exec();
	if (dlg.m_updated)
	{
		checkForCatalogue();
		queryEditor->tableCreated();
	}
	disconnect(&dlg, SIGNAL(rebuildViewTree(QString, QString)),
			schemaBrowser->tableTree, SLOT(buildViewTree(QString, QString)));
}

void LiteManWindow::createViewFromSql(QString query)
{
	CreateViewDialog dlg(this, 0);
	connect(&dlg, SIGNAL(rebuildViewTree(QString, QString)),
			schemaBrowser->tableTree, SLOT(buildViewTree(QString, QString)));
	dlg.setSql(query);
	dlg.exec();
	disconnect(&dlg, SIGNAL(rebuildViewTree(QString, QString)),
			schemaBrowser->tableTree, SLOT(buildViewTree(QString, QString)));
	if (dlg.m_updated)
	{
		checkForCatalogue();
		queryEditor->tableCreated();
	}
}

void LiteManWindow::setTableModel(SqlQueryModel * model)
{
	dataViewer->setTableModel(model, false);
}

/* isBuilt is true if we're executing a query on a table from the query builder.
 * Return true if the query was successfuly executed.
 */
bool LiteManWindow::doExecSql(QString query, bool isBuilt)
{
	if (query.isNull() || query.trimmed().isEmpty())
	{
		QMessageBox::warning(this, tr("No SQL statement"), tr("You are trying to run an empty SQL query. Hint: select your query in the editor"));
		return false;
	}
	dataViewer->setStatusText("");
	if (!checkForPending()) { return false; }

	m_activeItem = 0;

	sqlEditor->setStatusMessage();

	QTime time;
	time.start();

	// Run query
	SqlQueryModel * model = new SqlQueryModel(this);
	model->setQuery(query, QSqlDatabase::database(SESSION_NAME));

	sqlEditor->setStatusMessage(
        tr("Duration: %1 seconds").arg(time.elapsed() / 1000.0));
	
	// Check For Error in the SQL
	if(model->lastError().isValid())
	{
        QString s1(model->lastError().driverText());
        QString s2(model->lastError().databaseText());
        if (s1.size() > 0) {
            if (s2.size() > 0) {
                s1.append(" ").append(s2);
            }
        } else { s1 = s2; }
		dataViewer->setBuiltQuery(false);
		dataViewer->setStatusText(
			tr("Query Error: <span style=\" color:#ff0000;\">")
			+ s1
			+ "<br/></span>"
			+ tr("using sql statement:")
			+ "<br/><tt>"
			+ query);
        return false; // invalid query
	} else if (!dataViewer->setTableModel(model, false)) {
        return false; // valid query didn't generate a table
    } else {
		dataViewer->setBuiltQuery(isBuilt && (model->rowCount() != 0));
		dataViewer->rowCountChanged();
		if (Utils::updateObjectTree(query))
		{
			schemaBrowser->tableTree->buildTree();
			queryEditor->setSchema(QString(), QString(), true, true);
		}
		return true;
	}
}

QStringList LiteManWindow::visibleDatabases()
{
    QStringList result;
    QList<QTreeWidgetItem *> schemas =
        schemaBrowser->tableTree->findItems("*", Qt::MatchWildcard);
    QList<QTreeWidgetItem*>::const_iterator i;
    for (i = schemas.constBegin(); i != schemas.constEnd(); ++i) {
        result.append((*i)->text(0));
    }
    return result;
}

QTreeWidgetItem * LiteManWindow::findTreeItem(QString database, QString table)
{
    QList<QTreeWidgetItem*> l =
        schemaBrowser->tableTree->searchMask(
            schemaBrowser->tableTree->trTables);
    QList<QTreeWidgetItem*>::const_iterator it;
    for (it = l.constBegin(); it != l.constEnd(); ++it) {
        QTreeWidgetItem* p = *it;
        if (p->text(1) == database) {
            int count = p->childCount();
            for (int i = 0; i < count; ++i) {
                QTreeWidgetItem* q = p->child(i);
                if (q->text(0) == table) { return q; }
            }
        }
    }
    QMessageBox::critical(
        this, tr("sqliteman internal error"),
        tr("Please report to maintainer\ndatabase %s table %s not found in tree")
            .arg(database).arg(table));
    return NULL;
}

void LiteManWindow::alterView()
{
	//FIXME allow Alter View to change name like Alter Table
	dataViewer->removeErrorMessage();
	if (   (!m_currentItem)
        || (m_currentItem->type() != TableTree::ViewType))
    {
        return;
    }
	bool isActive = m_activeItem == m_currentItem;
	// Can't have pending update on a view, so no checkForPending() here
	// This might change if we allow editing on views with triggers
	AlterViewDialog dia(m_currentItem->text(0), m_currentItem->text(1), this);
	dia.exec();
	if (dia.m_updated)
	{
		QTreeWidgetItem * triggers = m_currentItem->child(0);
		if (triggers)
		{
			schemaBrowser->tableTree->buildTriggers(
                triggers, m_currentItem->text(1), m_currentItem->text(0));
		}
		checkForCatalogue();
		if (isActive)
		{
			dataViewer->saveSelection();
			m_activeItem = 0; // we've changed it
			treeItemActivated(m_currentItem, -1);
			dataViewer->reSelect();
		}
	}
}

void LiteManWindow::dropView()
{
	dataViewer->removeErrorMessage();
	if (   (!m_currentItem)
        || (m_currentItem->type() != TableTree::ViewType))
    {
        return;
    }
	bool isActive = m_activeItem == m_currentItem;
	QString view(m_currentItem->text(0));
	// Can't have pending update on a view, so no checkForPending() here
	// Ask the user for confirmation
	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the view \"%1\"?")
					.arg(view),
					QMessageBox::Yes, QMessageBox::No);

	if(ret == QMessageBox::Yes)
	{
		QString sql = QString("DROP VIEW")
					  + Utils::q(m_currentItem->text(1))
					  + "."
					  + Utils::q(view)
					  + ";";
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if (query.lastError().isValid())
		{
			dataViewer->setStatusText(
				tr("Cannot drop view ")
				+ m_currentItem->text(1) + tr(".") + view
				+ ":<br/><span style=\" color:#ff0000;\">"
				+ query.lastError().text()
				+ "<br/></span>" + tr("using sql statement:")
				+ "<br/><tt>" + sql);
		}
		else
		{
			schemaBrowser->tableTree->buildViews(
                m_currentItem->parent(), m_currentItem->text(1));
			queryEditor->tableDropped(view);
			checkForCatalogue();
			if (isActive)
			{
				dataViewer->setTableModel(new QSqlQueryModel(), false);
				m_activeItem = 0;
			}
		}
	}
}

void LiteManWindow::createIndex()
{
	dataViewer->removeErrorMessage();
	if (!m_currentItem) { return; }
	if (m_currentItem->type() == TableTree::IndexType)
	{
		m_currentItem = m_currentItem->parent();
	}
	if (m_currentItem->type() == TableTree::IndexesItemType)
	{
		m_currentItem = m_currentItem->parent();
	}
	if (m_currentItem->type() != TableTree::TableType)
	{
		return;
	}
	QString table(m_currentItem->text(0));
	QString schema(m_currentItem->text(1));
	CreateIndexDialog dlg(table, schema, this);
	connect(&dlg, SIGNAL(rebuildTableTree(QString)),
			schemaBrowser->tableTree, SLOT(buildTableTree(QString)));
	dlg.exec();
	if (dlg.m_updated)
	{
		schemaBrowser->tableTree->buildIndexes(m_currentItem, schema, table);
		checkForCatalogue();
	}
}

void LiteManWindow::dropIndex()
{
	dataViewer->removeErrorMessage();
	if (   (!m_currentItem)
        || (m_currentItem->type() != TableTree::IndexType))
    {
        return;
    }

	// Ask the user for confirmation
	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the index \"%1\"?")
					.arg(m_currentItem->text(0)),
					QMessageBox::Yes, QMessageBox::No);

	if (ret == QMessageBox::Yes)
	{
		QString sql = QString("DROP INDEX")
					  + Utils::q(m_currentItem->text(1))
					  + "."
					  + Utils::q(m_currentItem->text(0))
					  + ";";
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if (query.lastError().isValid())
		{
			dataViewer->setStatusText(
				tr("Cannot drop index ")
				+ m_currentItem->text(1) + tr(".") + m_currentItem->text(0)
				+ ":<br/><span style=\" color:#ff0000;\">"
				+ query.lastError().text()
				+ "<br/></span>" + tr("using sql statement:")
				+ "<br/><tt>" + sql);
		}
		else
		{
			schemaBrowser->tableTree->buildIndexes(
				m_currentItem->parent(), m_currentItem->text(1),
				m_currentItem->parent()->parent()->text(0));
			checkForCatalogue();
		}
	}
}

void LiteManWindow::treeItemActivated(QTreeWidgetItem * item, int column)
{
	dataViewer->removeErrorMessage();
	if (   (!item)
		|| (m_activeItem == item))
	{
		return;
	}
	
	if (item->type() == TableTree::TableType) {
        if (!checkForPending()) { return; }
		dataViewer->freeResources(dataViewer->tableData());
        SqlTableModel * model = new SqlTableModel(
            0, QSqlDatabase::database(SESSION_NAME));
        model->setSchema(item->text(1));
        model->setTable(item->text(0));
        model->select();
        model->setEditStrategy(SqlTableModel::OnManualSubmit);
        dataViewer->setBuiltQuery(false);
        dataViewer->setTableModel(model, true);
		m_activeItem = item;
    } else if (   (item->type() == TableTree::ViewType)
               || (item->type() == TableTree::SystemType))
	{
        if (!checkForPending()) { return; }
		dataViewer->freeResources(dataViewer->tableData());
        SqlQueryModel * model = new SqlQueryModel(0);
        model->setQuery(QString("select * from ")
                        + Utils::q(item->text(1))
                        + "."
                        + Utils::q(item->text(0)),
                        QSqlDatabase::database(SESSION_NAME));
        dataViewer->setBuiltQuery(false);
        dataViewer->setTableModel(model, false);
		m_activeItem = item;
	}
}

void LiteManWindow::treeContextMenuOpened(const QPoint & pos)
{
	if (contextMenu->actions().count() != 0) {
		contextMenu->exec(
            schemaBrowser->tableTree->viewport()->mapToGlobal(pos));
    }
}

void LiteManWindow::describeObject(QString type)
{
	dataViewer->removeErrorMessage();
    if (!m_currentItem) { return; }
	QString desc(Database::describeObject(
        m_currentItem->text(0), m_currentItem->text(1), type));
	dataViewer->sqlScriptStart();
	dataViewer->showSqlScriptResult(
        "-- " + tr("Describe %1").arg(m_currentItem->text(0).toUpper()));
	dataViewer->showSqlScriptResult(desc);
}

void LiteManWindow::describeTable() {
    if (m_currentItem && (m_currentItem->type() == TableTree::TableType)) {
        describeObject("table");
    }
}

void LiteManWindow::describeTrigger() {
    if (m_currentItem && (m_currentItem->type() == TableTree::TriggerType)) {
        describeObject("trigger");
    }
}

void LiteManWindow::describeView() {
    if (m_currentItem && (m_currentItem->type() == TableTree::ViewType)) {
        describeObject("view");
    }
}

void LiteManWindow::describeIndex() {
    if (m_currentItem && (m_currentItem->type() == TableTree::IndexType)) {
        describeObject("index");
    }
}

void LiteManWindow::updateContextMenu(QTreeWidgetItem * cur)
{
	contextMenu->clear();
	if (!cur) { return; }

	switch(cur->type())
	{
		case TableTree::DatabaseItemType:
			contextMenu->addAction(refreshTreeAct);
			if ((cur->text(0) != "main") && (cur->text(0) != "temp"))
				contextMenu->addAction(detachAct);
			contextMenu->addAction(createTableAct);
			contextMenu->addAction(createViewAct);
			contextMenu->addAction(buildQueryAct);
			break;

		case TableTree::TablesItemType:
			contextMenu->addAction(createTableAct);
			break;

		case TableTree::TableType:
			contextMenu->addAction(createTableAct);
			contextMenu->addAction(createViewAct);
			contextMenu->addAction(createIndexAct);
			contextMenu->addAction(createTriggerAct);
			contextMenu->addAction(describeTableAct);
			contextMenu->addAction(alterTableAct);
			contextMenu->addAction(dropTableAct);
			{
				SqlQueryModel model(0);
				model.setQuery(QString("select * from ")
							   + Utils::q(cur->text(1))
							   + "."
							   + Utils::q(cur->text(0)),
							   QSqlDatabase::database(SESSION_NAME));
				if (model.rowCount() > 0)
					{ contextMenu->addAction(emptyTableAct); }
			}
			contextMenu->addAction(buildQueryAct);
			contextMenu->addAction(reindexAct);
			contextMenu->addSeparator();
			contextMenu->addAction(importTableAct);
			contextMenu->addAction(populateTableAct);
			break;

		case TableTree::ViewsItemType:
			contextMenu->addAction(createViewAct);
			break;

		case TableTree::ViewType:
			contextMenu->addAction(buildQueryAct);
			contextMenu->addAction(createViewAct);
			contextMenu->addAction(describeViewAct);
			contextMenu->addAction(alterViewAct);
			contextMenu->addAction(dropViewAct);
			contextMenu->addAction(createTriggerAct);
			break;

		case TableTree::IndexesItemType:
			contextMenu->addAction(createIndexAct);
			break;

		case TableTree::IndexType:
			contextMenu->addAction(createIndexAct);
			contextMenu->addAction(describeIndexAct);
			contextMenu->addAction(dropIndexAct);
			contextMenu->addAction(reindexAct);
			break;

		case TableTree::TriggersItemType:
			contextMenu->addAction(createTriggerAct);
			if (cur->parent()->type() != TableTree::ViewType)
				contextMenu->addAction(consTriggAct);
			break;

		case TableTree::TriggerType:
			contextMenu->addAction(createTriggerAct);
			contextMenu->addAction(describeTriggerAct);
			contextMenu->addAction(alterTriggerAct);
			contextMenu->addAction(dropTriggerAct);
			break;
	}
	contextMenu->setDisabled(contextMenu->actions().count() == 0);
}

void LiteManWindow::updateContextMenu()
{
	updateContextMenu(m_currentItem);
}

void LiteManWindow::analyzeDialog()
{
	dataViewer->removeErrorMessage();
	AnalyzeDialog *dia = new AnalyzeDialog(this);
	dia->exec();
	delete dia;
    QList<QTreeWidgetItem*> l =
        schemaBrowser->tableTree->searchMask(
            schemaBrowser->tableTree->trSys);
    QList<QTreeWidgetItem*>::const_iterator it;
    for (it = l.constBegin(); it != l.constEnd(); ++it) {
        QTreeWidgetItem* p = *it;
		if (p->type() == TableTree::SystemItemType)
			schemaBrowser->tableTree->buildCatalogue(p, p->text(1));
	}
}

void LiteManWindow::vacuumDialog()
{
	dataViewer->removeErrorMessage();
	VacuumDialog *dia = new VacuumDialog(this);
	dia->exec();
	delete dia;
}

void LiteManWindow::attachDatabase()
{
	dataViewer->removeErrorMessage();
	QString fileName;
	fileName = QFileDialog::getOpenFileName(this, tr("Attach Database"), QDir::currentPath(), tr("SQLite database (*)"));
	if (fileName.isEmpty())
		return;
	if (!QFile::exists(fileName))
	{
		int ret = QMessageBox::question(this, m_appName,
			fileName
			+ tr(" does not exist\n")
			+ tr("Yes to create it, Cancel to skip this operation."),
			QMessageBox::Yes, QMessageBox::Cancel);
		if (ret == QMessageBox::Cancel) { return; }
	}

	QFileInfo f(fileName);
	QString schema;
	bool ok = false;
	while (!ok)
	{
		schema = QInputDialog::getText(this, tr("Attach Database"),
									   tr("Enter a Schema Alias:"),
									   QLineEdit::Normal, f.baseName(), &ok);
		if (!ok) { return; }
		if (   schema.isEmpty()
			|| (schema.compare("temp", Qt::CaseInsensitive) == 0))
		{
			QMessageBox::critical(this, tr("Attach Database"),
				tr("\"temp\" or empty is not a valid schema name"));
			ok = false;
			continue;
		}
		QStringList databases(Database::getDatabases().keys());
        QStringList::const_iterator i;
        for (i = databases.constBegin(); i != databases.constEnd(); ++i) {
			if (i->compare(schema, Qt::CaseInsensitive) == 0)
			{
				QMessageBox::critical(this, tr("Attach Database"),
					tr("%1 is already in use as a schema name")
						.arg(Utils::q(schema)));
				ok = false;
			}
		}
	}
	QString sql = QString("ATTACH DATABASE ")
				  + Utils::q(fileName, "'")
				  + " as "
				  + Utils::q(schema)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		dataViewer->setStatusText(
			tr("Cannot attach database ")
			+ fileName
			+ ":<br/><span style=\" color:#ff0000;\">"
			+ query.lastError().text()
			+ "<br/></span>"
			+ tr("using sql statement:")
			+ "<br/><tt>" + sql);
	}
	else
	{
		QString sql = QString("select 1 from ")
					  + Utils::q(schema)
					  + ".sqlite_master where 1=2 ;";
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if (query.lastError().isValid())
		{
			dataViewer->setStatusText(
				tr("Cannot access ")
				+ QFileInfo(fileName).fileName()
				+ ":<br/><span style=\" color:#ff0000;\">"
				+ query.lastError().text()
				+ "<br/></span>"
				+ tr("It is probably not a database."));
			QSqlQuery qundo(QString("DETACH DATABASE ")
							+ Utils::q(schema)
							+ ";",
							QSqlDatabase::database(SESSION_NAME));
		}
		else
		{
			schemaBrowser->tableTree->buildDatabase(schema);
			queryEditor->schemaAdded(schema);
		}
	}
}

//FIXME detaching a database should close any open table in it
void LiteManWindow::detachDatabase()
{
	dataViewer->removeErrorMessage();
    if (   (!m_currentItem)
        || (m_currentItem->type() != TableTree::DatabaseItemType))
    {
        return;
    }
    if ((m_activeItem == m_currentItem) && !checkForPending()) { return; }
	QString dbname(m_currentItem->text(0));
	removeRef(dbname);
	QString sql = QString("DETACH DATABASE ")
				  + Utils::q(dbname)
				  + ";";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		dataViewer->setStatusText(
			tr("Cannot detach database ")
			+ dbname
			+ ":<br/><span style=\" color:#ff0000;\">"
			+ query.lastError().text()
			+ "<br/></span>"
			+ tr("using sql statement:")
			+ "<br/><tt>" + sql);
	}
	else
	{
		// this removes the item from the tree as well as deleting it
		delete m_currentItem;
        m_currentItem = NULL;
		queryEditor->schemaGone(dbname);
		dataViewer->setBuiltQuery(false);
	}
}

void LiteManWindow::loadExtension()
{
	dataViewer->removeErrorMessage();
	QString mask(tr("Sqlite3 extensions "));
#ifdef Q_WS_WIN
	mask += "(*.dll)";
#else
	mask += "(*.so)";
#endif

	QStringList files = QFileDialog::getOpenFileNames(
						this,
						tr("Select one or more Sqlite3 extensions to load"),
						QDir::currentPath(),
						mask);
	if (files.count() == 0)
		return;

	schemaBrowser->appendExtensions(Database::loadExtension(files), true);
}

void LiteManWindow::createTrigger()
{
    if (!m_currentItem) { return; }
	dataViewer->removeErrorMessage();
	QTreeWidgetItem * triggers = NULL;
	if (m_currentItem->type() == TableTree::TriggerType)
	{
		m_currentItem = m_currentItem->parent();
	}
	if (m_currentItem->type() == TableTree::TriggersItemType)
	{
		triggers = m_currentItem;
		m_currentItem = m_currentItem->parent();
	}
	else
	{
		for (int i = 0; i < m_currentItem->childCount(); ++i)
		{
			if (m_currentItem->child(i)->type() == TableTree::TriggersItemType)
			{
				triggers = m_currentItem->child(i);
				break;
			}
		}
	}
	if (!triggers) { return; }
	CreateTriggerDialog *dia = new CreateTriggerDialog(m_currentItem, this);
	dia->exec();
	if (dia->m_updated)
	{
		schemaBrowser->tableTree->buildTriggers(
            triggers, triggers->text(1), m_currentItem->text(0));
		checkForCatalogue();
	}
}

void LiteManWindow::alterTrigger()
{
	dataViewer->removeErrorMessage();
    if (   (!m_currentItem)
        || (m_currentItem->type() != TableTree::TriggerType))
    {
        return;
    }
	QTreeWidgetItem * triglist = m_currentItem->parent();
	QString trigger(m_currentItem->text(0));
	QString table(triglist->parent()->text(0));
	QString schema(m_currentItem->text(1));
	AlterTriggerDialog *dia =
		new AlterTriggerDialog(m_currentItem, this);
	dia->exec();
	if (dia->m_updated)
	{
		schemaBrowser->tableTree->buildTriggers(triglist, schema, table);
		checkForCatalogue();
	}
}


void LiteManWindow::dropTrigger()
{
	dataViewer->removeErrorMessage();
    if (   (!m_currentItem)
        || (m_currentItem->type() != TableTree::TriggerType))
    {
        return;
    }

	// Ask the user for confirmation
	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the trigger \"%1\"?")
					.arg(m_currentItem->text(0)),
					QMessageBox::Yes, QMessageBox::No);

	if (ret == QMessageBox::Yes)
	{
		QString sql = QString("DROP TRIGGER ")
					  + Utils::q(m_currentItem->text(1))
					  + "."
					  + Utils::q(m_currentItem->text(0))
					  + ";";
		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if (query.lastError().isValid())
		{
			dataViewer->setStatusText(
				tr("Cannot drop trigger ")
				+ m_currentItem->text(1) + tr(".") + m_currentItem->text(0)
				+ ":<br/><span style=\" color:#ff0000;\">"
				+ query.lastError().text()
				+ "<br/></span>" + tr("using sql statement:")
				+ "<br/><tt>" + sql);
		}
		else
		{
			schemaBrowser->tableTree->buildTriggers(
				m_currentItem->parent(), m_currentItem->text(1),
				m_currentItem->parent()->parent()->text(0));
			checkForCatalogue();
		}
	}
}

void LiteManWindow::constraintTriggers()
{
	dataViewer->removeErrorMessage();
    if (   (!m_currentItem)
        || (m_currentItem->type() != TableTree::TriggerType)
        || (m_currentItem->parent()->type() != TableTree::ViewType))
    {
        return;
    }
	QString table(m_currentItem->parent()->text(0));
	QString schema(m_currentItem->parent()->text(1));
	ConstraintsDialog dia(table, schema, this);
	dia.exec();
	if (dia.m_updated)
	{
		schemaBrowser->tableTree->buildTriggers(m_currentItem, schema, table);
		checkForCatalogue();
	}
}

void LiteManWindow::reindex()
{
	dataViewer->removeErrorMessage();
    if (   (!m_currentItem)
        || (   (m_currentItem->type() != TableTree::TableType)
            && (m_currentItem->type() != TableTree::IndexType)))
    {
        return;
    }
    QString sql(QString("REINDEX ")
                + Utils::q(m_currentItem->text(1))
                + "."
                + Utils::q(m_currentItem->text(0))
                + ";");
    QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
    if (query.lastError().isValid())
    {
        dataViewer->setStatusText(
            tr("Cannot reindex ")
            + m_currentItem->text(1) + tr(".") + m_currentItem->text(0)
            + ":<br/><span style=\" color:#ff0000;\">"
            + query.lastError().text()
            + "<br/></span>" + tr("using sql statement:")
            + "<br/><tt>" + sql);
    }
}

/* FIXME We really ought to do better than this.
 * It's called from the SQL editor which does some basic parsing
 * of any SQL code that it executes, so we can make a reasonable
 * guess at what might have changed.
 */
void LiteManWindow::refreshTable()
{
	/* SQL code in the SQL editor may have modified or even removed
     * the current table.
	 */
	dataViewer->setTableModel(new QSqlQueryModel(), false);
	updateContextMenu();
	m_activeItem = 0;
	queryEditor->treeChanged();
}

void LiteManWindow::doMultipleDeletion()
{
	dataViewer->removeErrorMessage();
	QString sql = queryEditor->deleteStatement();
	SqlQueryModel * m =
		qobject_cast<SqlQueryModel *>(dataViewer->tableData());
	if (m && !sql.isNull())
	{
		int com = QMessageBox::question(this, tr("Sqliteman"),
			tr("Do you want to delete these %1 records\n\n"
			   "Yes = delete them\n"
			   "Cancel = do not delete").arg(m->rowCount()),
			QMessageBox::Yes, QMessageBox::No);
		if (com == QMessageBox::Yes)
		{
			doExecSql(sql, false);
			dataViewer->setTableModel(new QSqlQueryModel(), false);
		}
	}
}

void LiteManWindow::preferences()
{
	dataViewer->removeErrorMessage();
	PreferencesDialog prefs(this);
	if (prefs.exec())
		if (prefs.saveSettings())
		{
			emit prefsChanged();
#ifdef ENABLE_EXTENSIONS
			if (m_isOpen) {
				handleExtensions(
					Preferences::instance()->allowExtensionLoading());
			}
#endif
		}
}

void LiteManWindow::tableTreeSelectionChanged() {
    QList<QTreeWidgetItem *> selection(
        schemaBrowser->tableTree->selectedItems());
    if (selection.isEmpty()) {
        m_currentItem = NULL;
    } else {
        m_currentItem = selection.first();
        QTreeWidgetItem * currentitem =
            schemaBrowser->tableTree->currentItem();
        updateContextMenu(currentitem);
    }
}

// Called by sqleditor if it executes a DETACH or an EXEC
// which might contain a DETACH
void LiteManWindow::detaches() {
	queryEditor->schemaGone(QString());
}
