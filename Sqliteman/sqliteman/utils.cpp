/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QtCore/qmath.h>

#include "utils.h"

QIcon Utils::getIcon(const QString & fileName)
{
	QPixmap pm;

	if (! QPixmapCache::find(fileName, &pm))
	{
		QPixmap npm(QString(ICON_DIR) + "/" + fileName);
		QPixmapCache::insert(fileName, npm);
		return npm;
	}

	return pm;
}

QPixmap Utils::getPixmap(const QString & fileName)
{
	return QPixmap(QString(ICON_DIR) + "/" + fileName);
}

QString Utils::getTranslator(const QString & localeName)
{
	return QString(TRANSLATION_DIR)
		   + "/sqliteman_"
		   + localeName
		   + ".qm";
}

bool Utils::detaches(const QString & sql)
{
	if (sql.isNull())
		return false;
	QString tmp(sql.trimmed().toUpper());
	return (   tmp.startsWith("DETACH")
		    || tmp.contains("EXEC")); // crude, but will work for now
}

bool Utils::updateObjectTree(const QString & sql)
{
	if (sql.isNull())
		return false;
	QString tmp(sql.trimmed().toUpper());
	return (   tmp.startsWith("ALTER")
			|| tmp.startsWith("ATTACH")
			|| tmp.startsWith("CREATE")
			|| tmp.startsWith("DETACH")
			|| tmp.startsWith("DROP")
		    || tmp.contains("EXEC")); // crude, but will work for now
}

bool Utils::updateTables(const QString & sql)
{
	if (sql.isNull())
		return false;
	QString tmp(sql.trimmed().toUpper());
	return (   tmp.startsWith("ALTER")
			|| tmp.startsWith("DELETE")
			|| tmp.startsWith("DETACH")
			|| tmp.startsWith("DROP")
			|| tmp.startsWith("INSERT")
			|| tmp.startsWith("REPLACE")
			|| tmp.startsWith("UPDATE")
		    || tmp.contains("EXEC")); // crude, but will work for now
}

QString Utils::q(QString s, QString c)
{
	return c + s.replace(c, c + c) + c;
}

QString Utils::q(QString s)
{
	return q(s, "\"");
}

QString Utils::q(QStringList l, QString c)
{
	for (int i = 0; i < l.count(); ++i)
	{
		l.replace(i, l[i].replace(c, c + c));
	}
	return c + l.join(c + ", " + c) + c;
}

#if 0 // not used, but kept in case needed in the future
QString Utils::unQuote(QString s)
{
	if (s.startsWith("'")) {
		s.replace("''", "'");
		return s.mid(1, s.size() - 2);
	}
	else if (s.startsWith("\"")) {
		s.replace("\"\"", "\"");
		return s.mid(1, s.size() - 2);
	}
	else { return s; }
}
#endif

QString Utils::like(QString s)
{
	return "'%"
		   + s.replace("'", "''")
			  .replace("_", "@_")
			  .replace("%", "@%")
			  .replace("@", "@@")
		   + "%' ESCAPE '@'";
}

QString Utils::startswith(QString s)
{
	return "'"
		   + s.replace("'", "''")
			  .replace("_", "@_")
			  .replace("%", "@%")
			  .replace("@", "@@")
		   + "%' ESCAPE '@'";
}

/* Set suitable column widths for a QTableView (or a QTableWidget which inherits
 * from it).
 */
void Utils::setColumnWidths(QTableView * tv)
{
	int widthView = tv->viewport()->width();
	int widthLeft = widthView;
	int widthUsed = 0;
	int columns = tv->horizontalHeader()->count();
	int columnsLeft = columns;
	int half = widthView / (columns * 2);
	QVector<int> wantedWidths(columns);
	QVector<int> gotWidths(columns);
	tv->resizeColumnsToContents();
	int i;
	for (i = 0; i < columns; ++i)
	{
		wantedWidths[i] = tv->columnWidth(i);
		gotWidths[i] = 0;
	}
	/* First give all "small" columns what they want. */
	for (i = 0; i < columns; ++i)
	{
		int w = wantedWidths[i];
		if (w <= half)
		{
			gotWidths[i] = w;
			widthLeft -= w;
			widthUsed += w;
			columnsLeft -= 1;
		}
	}
	/* Now allocate to other columns, giving smaller ones a larger proportion
	 * of what they want;
	 */
	for (i = 0; i < columns; ++i)
	{
		if (gotWidths[i] == 0)
		{
			int w = wantedWidths[i];
			int x = half + (int)qSqrt((qreal)(
				(w - half) * widthLeft / columnsLeft));
			gotWidths[i] = (w < x) ? w : x;
			if (widthLeft > w)
			{
				widthLeft -= w;
			}
			widthUsed += w;
			columnsLeft -= 1;
		}
	}
	/* If there is space left, make all columns proportionately wider to fill
	 * it.
	 */
	if (widthUsed < widthView)
	{
		for (i = 0; i < columns; ++i)
		{
			tv->setColumnWidth(i, gotWidths[i] * widthView / widthUsed);
		}
	}
	else
	{
		for (i = 0; i < columns; ++i)
		{
			tv->setColumnWidth(i, gotWidths[i]);
		}
	}
}
