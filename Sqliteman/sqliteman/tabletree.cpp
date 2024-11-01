/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>

#include "database.h"
#include "tabletree.h"
#include "utils.h"


TableTree::TableTree(QWidget * parent) : QTreeWidget(parent)
{
	trDatabase = tr("Database");
	trTables = tr("Tables");
	trIndexes = tr("Indexes");
	trSysIndexes = tr("System Indexes");
	trViews = tr("Views");
	trTriggers = tr("Triggers");
	trSys = tr("System Catalogue");
	trCols = tr("Columns");
	
	setColumnCount(2);
	setHeaderLabels(QStringList() << trDatabase << "schema");
	hideColumn(1);
	
	setContextMenuPolicy(Qt::CustomContextMenu);
	setDragDropMode(QAbstractItemView::DragOnly);
	setDragEnabled(true);
	setDropIndicatorShown(true);
	setAcceptDrops(false);
    m_pressed = false;
}

void TableTree::buildTree()
{
	QStringList databases(Database::getDatabases().keys());
	clear();
    QStringList::const_iterator i;
    for (i = databases.constBegin(); i != databases.constEnd(); ++i) {
		buildDatabase(*i);
	}
	setSelection(QRect(0, 0, 0, 0), QItemSelectionModel::Clear);
}

void TableTree::buildDatabase(QTreeWidgetItem * dbItem, const QString & schema)
{
	deleteChildren(dbItem);
	buildDatabase(schema);
}

void TableTree::buildDatabase(const QString & schema)
{
	QTreeWidgetItem * dbItem = new QTreeWidgetItem(this, DatabaseItemType);
	dbItem->setIcon(0, Utils::getIcon("database.png"));
	dbItem->setText(0, schema);
	dbItem->setText(1, schema);

	lastTablesItem = new QTreeWidgetItem(dbItem, TablesItemType);
	lastTablesItem->setIcon(0, Utils::getIcon("table.png"));

	lastViewsItem = new QTreeWidgetItem(dbItem, ViewsItemType);
	lastViewsItem->setIcon(0, Utils::getIcon("view.png"));

	QTreeWidgetItem * systemItem = new QTreeWidgetItem(dbItem, SystemItemType);
	systemItem->setIcon(0, Utils::getIcon("system.png"));

	buildTables(lastTablesItem, schema, false);
	buildViews(lastViewsItem, schema);
	buildCatalogue(systemItem, schema);

	dbItem->setExpanded(true);
}

void TableTree::buildTableItem(QTreeWidgetItem * tableItem, bool rebuild)
{
	if (rebuild) { deleteChildren(tableItem); }

	QString schema = tableItem->text(1);
	QString table = tableItem->text(0);
	// columns
	QTreeWidgetItem *columnsItem = new QTreeWidgetItem(tableItem, ColumnItemType);
	buildColumns(columnsItem, schema, table);
	// indexes
	QTreeWidgetItem *indexesItem = new QTreeWidgetItem(tableItem, IndexesItemType);
	buildIndexes(indexesItem, schema, table);
	// system indexes (unique)
	QTreeWidgetItem *sysIndexesItem = new QTreeWidgetItem(tableItem, SysIndexesItemType);
	buildSysIndexes(sysIndexesItem, schema, table);
	// triggers
	QTreeWidgetItem *triggersItem = new QTreeWidgetItem(tableItem, TriggersItemType);
	buildTriggers(triggersItem, schema, table);
}

void TableTree::buildTables(QTreeWidgetItem * tablesItem,
                            const QString & schema, bool expand)
{
    deleteChildren(tablesItem);

    QStringList tables = Database::getObjects("table", schema).keys();
    tablesItem->setText(0, trLabel(trTables).arg(tables.size()));
    tablesItem->setText(1, schema);
    QStringList::const_iterator i;
    for (i = tables.constBegin(); i != tables.constEnd(); ++i) {
        QTreeWidgetItem * tableItem =
            new QTreeWidgetItem(tablesItem, TableType);
        tableItem->setText(0, *i);
        tableItem->setText(1, schema);
        buildTableItem(tableItem, false);
    }
    if (expand) { tablesItem->setExpanded(true); }
}

void TableTree::buildIndexes(QTreeWidgetItem *indexesItem, const QString & schema, const QString & table)
{
	if (indexesItem->type() == TableTree::TableType)
	{
		for (int i = 0; i < indexesItem->childCount(); ++i)
		{
			QTreeWidgetItem * item = indexesItem->child(i);
			if (item->type() == TableTree::IndexesItemType)
			{
				indexesItem = item;
				break;
			}
		}
	}
	if (indexesItem->type() != TableTree::IndexesItemType) { return; }
	deleteChildren(indexesItem);
	QStringList values = Database::getObjects("index", schema).values(table);
	indexesItem->setText(0, trLabel(trIndexes).arg(values.size()));
	indexesItem->setIcon(0, Utils::getIcon("index.png"));
	indexesItem->setText(1, schema);
	for (int i = 0; i < values.size(); ++i)
	{
		QTreeWidgetItem *indexItem = new QTreeWidgetItem(indexesItem, IndexType);
		indexItem->setText(0, values.at(i));
		indexItem->setText(1, schema);
	}
}

void TableTree::buildColumns(QTreeWidgetItem * columnsItem, const QString & schema, const QString & table)
{
	deleteChildren(columnsItem);
	QList<FieldInfo> values = Database::tableFields(table, schema);
	columnsItem->setText(0, trLabel(trCols).arg(values.size()));
	columnsItem->setIcon(0, Utils::getIcon("column.png"));
// 	columnsItem->setText(1, schema);
	for (int i = 0; i < values.size(); ++i)
	{
		QTreeWidgetItem *columnItem = new QTreeWidgetItem(columnsItem, ColumnType);
		columnItem->setText(0, values.at(i).name);
		if (values.at(i).isPartOfPrimaryKey)
			columnItem->setIcon(0, Utils::getIcon("key.png"));
// 		indexItem->setText(1, schema);
	}
}

void TableTree::buildSysIndexes(QTreeWidgetItem *indexesItem, const QString & schema, const QString & table)
{
	deleteChildren(indexesItem);
	QStringList sysIx = Database::getSysIndexes(table, schema);
	indexesItem->setText(0, trLabel(trSysIndexes).arg(sysIx.size()));
	indexesItem->setIcon(0, Utils::getIcon("index.png"));
	indexesItem->setText(1, schema);
	for (int i = 0; i < sysIx.size(); ++i)
	{
		QTreeWidgetItem *indexItem = new QTreeWidgetItem(indexesItem, SysIndexType);
		indexItem->setText(0, sysIx.at(i));
		indexItem->setText(1, schema);
	}
}

void TableTree::buildTriggers(QTreeWidgetItem *triggersItem, const QString & schema, const QString & table)
{
	deleteChildren(triggersItem);
	QStringList values = Database::getObjects("trigger", schema).values(table);
	triggersItem->setText(0, trLabel(trTriggers).arg(values.size()));
	triggersItem->setIcon(0, Utils::getIcon("trigger.png"));
	triggersItem->setText(1, schema);
	for (int i = 0; i < values.size(); ++i)
	{
		QTreeWidgetItem *triggerItem = new QTreeWidgetItem(triggersItem, TriggerType);
		triggerItem->setText(0, values.at(i));
		triggerItem->setText(1, schema);
	}
}

void TableTree::buildViews(QTreeWidgetItem * viewsItem, const QString & schema)
{
	deleteChildren(viewsItem);

	// Build views tree
	QStringList views = Database::getObjects("view", schema).keys();
	viewsItem->setText(0, trLabel(trViews).arg(views.size()));
	viewsItem->setText(1, schema);
    QStringList::const_iterator i;
    for (i = views.constBegin(); i != views.constEnd(); ++i) {
		QTreeWidgetItem * viewItem = new QTreeWidgetItem(viewsItem, ViewType);
		viewItem->setText(0, *i);
		viewItem->setText(1, schema);
		QTreeWidgetItem *triggersItem =
            new QTreeWidgetItem(viewItem, TriggersItemType);
		buildTriggers(triggersItem, schema, *i);
	}
}

void TableTree::buildCatalogue(QTreeWidgetItem * systemItem, const QString & schema)
{
	deleteChildren(systemItem);

	QStringList values = Database::getSysObjects(schema).keys();
	systemItem->setText(0, trLabel(trSys).arg(values.size()));
	systemItem->setText(1, schema);
    QStringList::const_iterator i;
    for (i = values.constBegin(); i != values.constEnd(); ++i) {
		QTreeWidgetItem * sysItem = new QTreeWidgetItem(systemItem, SystemType);
		sysItem->setText(0, *i);
		sysItem->setText(1, schema);
	}
}


void TableTree::deleteChildren(QTreeWidgetItem * item)
{
	QList<QTreeWidgetItem *> items = item->takeChildren();
    QList<QTreeWidgetItem *>::const_iterator i;
    for (i = items.constBegin(); i != items.constEnd(); ++i) {
        // *i is a const pointer, but not a pointer to const
		delete *i;
	}
}

QString TableTree::trLabel(const QString & trStr)
{
	return trStr + " (%1)";
}

QList<QTreeWidgetItem*> TableTree::searchMask(const QString & trStr)
{
	return findItems(trStr + " (", Qt::MatchStartsWith | Qt::MatchRecursive, 0);
}

void TableTree::buildViewTree(QString schema, QString name)
{
    if (schema == "temp") {
        /* Handle the special case where we created a temp view
         * and the temp database was not previously shown. */
        QStringList databases(Database::getDatabases().keys());
        QList<QTreeWidgetItem *> schemas =
            findItems("temp", Qt::MatchFixedString);
        if (   databases.contains("temp", Qt::CaseInsensitive)
            && schemas.isEmpty())
        {
            buildDatabase("temp");
            lastViewsItem->setExpanded(true);
            return;
        }
    }
    QList<QTreeWidgetItem*> l = searchMask(trViews);
    QList<QTreeWidgetItem*>::const_iterator i;
    for (i = l.constBegin(); i != l.constEnd(); ++i) {
        QTreeWidgetItem* item = *i;
		if (item->text(1) == schema && item->type() == ViewsItemType)
        {
			buildViews(item, schema);
            item->setExpanded(true);
            break;
        }
	}
}

void TableTree::buildTableTree(QString schema)
{
    if (schema == "temp") {
        /* Handle the special case where we created a temp table
         * and the temp database was not previously shown. */
        QStringList databases(Database::getDatabases().keys());
        QList<QTreeWidgetItem *> schemas =
            findItems("temp", Qt::MatchFixedString);
        if (   databases.contains("temp", Qt::CaseInsensitive)
            && schemas.isEmpty())
        {
            buildDatabase("temp");
            lastTablesItem->setExpanded(true);
            return;
        }
    }
    QList<QTreeWidgetItem*> l = searchMask(trTables);
    QList<QTreeWidgetItem*>::const_iterator i;
    for (i = l.constBegin(); i != l.constEnd(); ++i) {
        QTreeWidgetItem* item = *i;
		if (item->text(1) == schema && item->type() == TablesItemType) {
			buildTables(item, schema, true);
            item->setExpanded(true);
            break;
        }
	}
}

void TableTree::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		m_dragStartPosition = event->pos();
        m_pressed = true;
    }
	QTreeWidget::mousePressEvent(event);
}

// If we drag an item from the schema browser,
// we can drop its name anywhere that text is acceptable.
void TableTree::mouseMoveEvent(QMouseEvent *event)
{
#if QT_VERSION >= 0x040300
    if (!m_pressed) { ; /*do nothing*/ }
	else if (!(event->buttons() & Qt::LeftButton)) { ; /*do nothing*/ }
	else if ((event->pos() - m_dragStartPosition).manhattanLength()
			< QApplication::startDragDistance())
    { ; /*do nothing*/ }
	else {
        // Even there isn't something we can drag, it's still not a click
        m_pressed = false;
        if (!currentItem()) {  ; /*do nothing*/ }
        else if (   (currentItem()->type() == TableTree::TableType)
                 || (currentItem()->type() == TableTree::ViewType)
                 || (currentItem()->type() == TableTree::DatabaseItemType)
                 || (currentItem()->type() != TableTree::IndexType)
                 || (currentItem()->type() != TableTree::TriggerType)
                 || (currentItem()->type() != TableTree::SystemType)
                 || (currentItem()->type() != TableTree::ColumnType))
        {
            QDrag *drag = new QDrag(this);
            QMimeData *mimeData = new QMimeData;

            mimeData->setText(currentItem()->text(0));
            drag->setMimeData(mimeData);
            drag->exec(Qt::CopyAction);
        }
    }
#endif
	QTreeWidget::mouseMoveEvent(event);
}

void TableTree::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_pressed) {
        m_pressed = false;
        if (event->button() == Qt::LeftButton)
        {
            /* We should check the style here, but our ultimate ancestor
             * QWidget doesn't seem to inherit it. */
            QPoint pos = event->pos();
            if (
                (pos - m_dragStartPosition).manhattanLength()
                < QApplication::startDragDistance())
            {
                QPersistentModelIndex index = indexAt(pos);
                emit activated(index);
                return;
            }
        }
    }
	QTreeWidget::mouseReleaseEvent(event);
}
