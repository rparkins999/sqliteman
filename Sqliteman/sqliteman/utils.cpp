/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QtCore/qmath.h>
#include <QComboBox>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelection>
#include <QLineEdit>
#include <QModelIndex>
#include <QPixmapCache>
#include <QRect>
#include <QRectF>
#include <QSqlError>
#include <QSqlRecord>
#include <QTableView>
#include <QTextEdit>
#include <QTime>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVariant>

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

// debugging hacks
void Utils::dump(QString s) { qDebug("%s", s.toUtf8().data()); }
void Utils::dump(QModelIndex x)
{
	if (x.isValid())
	{
		dump(QString("row %1, column %2").arg(x.row()).arg(x.column()));
	}
	else
	{
		qDebug("Invalid ModelIndex");
	}
}
void Utils::dump(QList<QModelIndex> l)
{
	if (l.count() == 0)
	{
		qDebug("Empty QModelIndexList");
	}
	else foreach(QModelIndex x, l)
	{
		dump(x);
	}
}
void Utils::dump(QItemSelection selection)
{
	QModelIndexList list = selection.indexes();
	int n = list.count();
	if (n == 0)
	{
		qDebug("selection is empty");
	}
	else
	{
		dump(list);
	}
}
void Utils::dump(QTreeWidgetItem & item)
{
	dump(QString("text(0) %1, text(1) %2").arg(item.text(0), item.text(1)));
}
void Utils::dump(QTreeWidgetItem * item)
{
	item ? dump(*item) : qDebug("Null QTreeWidgetItem");
}
void Utils::dump(QComboBox & box)
{
	int n = box.count();
	if (n == 0)
	{
		qDebug("QComboBox with no items");
	}
	else
	{
		QStringList sl;
		for (int i = 0; i < n; ++i)
		{
			sl.append(box.itemText(i));
		}
		dump(sl);
	}
}
void Utils::dump(QComboBox * box)
{
	box ? dump(*box) : qDebug("Null QComboBox");
}
void Utils::dump(QStringList sl) { dump(sl.join(", ")); }
void Utils::dump(QList<int> il)
{
	if (il.count() == 0)
	{
		qDebug("Empty QList<int>");
	}
	else
	{
		QString s;
		foreach (int i, il)
		{
			if (i != 0) { s.append(", "); }
			s.append(QString("%1").arg(i));
		}
		dump(s);
	}
}
void Utils::dump(QVector<int> iv)
{
	int c = iv.count();
	if (c == 0)
	{
		qDebug("Empty QVector<int>");
	}
	else
	{
		QString s;
		for (int i = 0; i < c; ++i)
		{
			if (i != 0) { s.append(", "); }
			s.append(QString("%1").arg(iv[i]));
		}
		dump(s);
	}
}
void Utils::dump(QTextEdit & te) { dump(te.toPlainText()); }
void Utils::dump(QTextEdit * te)
{
	te ? dump(*te) : qDebug("Null QTextEdit");
}
QString Utils::colorToString(QColor c)
{
	return QString("Red %1, Blue %2, Green %3")
		.arg(c.red()).arg(c.blue()).arg(c.green());
}
void Utils::dump(QColor c)
{
	dump(colorToString(c));
}
QString Utils::rectToString(QRect r)
{
	return QString("Rect from (%1, %2) to (%3, %4)")
		.arg(r.left()).arg(r.top()).arg(r.right()).arg(r.bottom());
}
void Utils::dump(QRect r)
{
	dump(rectToString(r));
}
QString Utils::rectFToString(QRectF r)
{
	return QString("RectF from (%1, %2) to (%3, %4)")
		.arg(r.left()).arg(r.bottom()).arg(r.right()).arg(r.top());
}
void Utils::dump(QRectF r)
{
	dump(rectFToString(r));
}
QString Utils::variantToString(QVariant x)
{
	if (x.isNull()) { return QString("Null QVariant"); }
	if (!x.isValid()) { return QString("Invalid QVariant"); }
	switch(x.type())
	{
		case QVariant::Invalid: return QString("Invalid QVariant");
		case QVariant::BitArray: return QString("BitArray");
		case QVariant::Bitmap: return QString("Bitmap");
		case QVariant::Bool: return QString(x.toBool() ? "true" : "false");
		case QVariant::String:
			if (x.value<QString>().isNull()) return QString("Null String");
			//FALLTHRU
		case QVariant::ByteArray:
		case QVariant::Char:
		case QVariant::Date:
		case QVariant::DateTime:
		case QVariant::Double:
		case QVariant::Int:
		case QVariant::LongLong:
		case QVariant::StringList:
		case QVariant::UInt:
		case QVariant::ULongLong: return x.toString();
		case QVariant::Brush: return QString("Brush");
		case QVariant::Color: return colorToString(x.value<QColor>());
		case QVariant::Cursor: return QString("Cursor");
		case QVariant::EasingCurve: return QString("EasingCurve");
		case QVariant::Font:
		{
			QFont f = x.value<QFont>();
			return QString("Font %1").arg(f.key());
		}
		case QVariant::Hash:return QString("Hash");
		case QVariant::Icon: return QString("Icon");
		case QVariant::Image: return QString("Image");
		case QVariant::KeySequence:
		{
			QKeySequence k = x.value<QKeySequence>();
			return k.toString();
		}
		case QVariant::Line:
		{
			QLine l = x.value<QLine>();
			return QString("Line from (%1, %2) to (%3, %4)")
				   .arg(l.x1()).arg(l.y1()).arg(l.x2()).arg(l.y2());
		}
		case QVariant::LineF:
		{
			QLineF l = x.value<QLineF>();
			return QString("Line from (%1, %2) to (%3, %4)")
				   .arg(l.x1()).arg(l.y1()).arg(l.x2()).arg(l.y2());
		}
		case QVariant::List:
			return QString("List of length %1").arg(x.toList().count());
		case QVariant::Locale:
		{
			QLocale l = x.value<QLocale>();
			return l.bcp47Name();
		}
		case QVariant::Map: return QString("Map");
		case QVariant::Matrix: return QString("Matrix");
		case QVariant::Transform: return QString("Transform");
		case QVariant::Matrix4x4: return QString("Matrix4x4");
		case QVariant::Palette: return QString("Palette");
		case QVariant::Pen: return QString("Pen");
		case QVariant::Pixmap: return QString("Pixmap");
		case QVariant::Point:
		{
			QPoint p = x.value<QPoint>();
			return QString("(%1, %2)").arg(p.x()).arg(p.y());
		}
		case QVariant::PointF:
		{
			QPointF p = x.value<QPointF>();
			return QString("(%1, %2)").arg(p.x()).arg(p.y());
		}
		case QVariant::Polygon:
		{
			QPolygon p = x.value<QPolygon>();
			return QString("Array of %1 points").arg(p.count());
		}
		case QVariant::Quaternion: return QString("Quaternion");
		case QVariant::Rect: return rectToString(x.value<QRect>());
		case QVariant::RectF: return rectFToString(x.value<QRectF>());
		case QVariant::RegExp: return QString(x.toRegExp().pattern());
		case QVariant::Region: return QString("Region");
		case QVariant::Size:
		{
			QSize s = x.value<QSize>();
			return QString("Width %1, height %2").arg(s.width(), s.height());
		}
		case QVariant::SizeF:
		{
			QSizeF s = x.value<QSizeF>();
			return QString("Width %1, height %2")
					.arg(s.width()).arg(s.height());
		}
		case QVariant::SizePolicy: return QString("SizePolicy");
		case QVariant::TextFormat: return QString("TextFormat");
		case QVariant::TextLength: return QString("TextLength");
		case QVariant::Time: return x.toTime().toString();
		case QVariant::Url: return x.toUrl().toString();
		case QVariant::Vector2D: return QString("Vector2D");
		case QVariant::Vector3D: return QString("Vector3D");
		case QVariant::Vector4D: return QString("Vector4D");
		case QVariant::UserType: return QString("UserType");
		default: return QString("Unhandled type QVariant::") + x.typeName();
	}
}
void Utils::dump(QVariant x) { dump(variantToString(x)); }
void Utils::dump(QSqlRecord & rec)
{
	QStringList sl;
	for (int i = 0; i < rec.count(); ++i)
	{
		sl.append(rec.fieldName(i) + " ("
				  + rec.value(i).typeName() + "): "
				  + variantToString(rec.value(i)));
	}
	dump(sl);
}
void Utils::dump(QLineEdit & le)
{
	dump(QString("LineEdit(%1),") .arg(le.text())
		 + QString(" height %1,").arg(le.size().rheight())
		 + QString(" width %1").arg(le.size().rwidth()));
}
void Utils::dump(QLineEdit * le)
{
	le ? dump(*le) : qDebug("Null QLineEdit");
}
void Utils::dump(QSqlError & e)
{
	dump(e.text());
}
void Utils::dump(QSqlError * e)
{
	e ? dump(*e) : qDebug("Null QSqlError");
}
void Utils::dump(Token & t)
{
	dump(SqlParser::toString(t));
}
void Utils::dump(QList<Token> tl)
{
	foreach (Token t, tl) { dump(t); }
}
void Utils::dump(Expression * e)
{
	dump(SqlParser::toString(e));
}
void Utils::dump(FieldInfo f)
{
	QString s("name ");
	s.append(f.name);
	if (!f.type.isEmpty()) { s.append(", type "); s.append(f.type); }
	if (!f.defaultValue.isEmpty())
		{ s.append(", default "); s.append(f.defaultValue); }
	if (f.isPartOfPrimaryKey) { s.append(", PRIMARY KEY"); }
	if (f.isAutoIncrement) { s.append(", AUTOINCREMENT"); }
	if (f.isNotNull) { s.append(", NOT NULL"); }
	if (!f.referencedTable.isEmpty()) {
        s.append(", REFERENCES ");
        s.append(f.referencedTable);
        s.append("(");
        s.append(f.referencedKeys.join(", "));
        s.append(")");
    }
	dump(s);
}
void Utils::dump(QList<FieldInfo> fl)
{
	if (fl.count() == 0) { qDebug("Empty QList<FieldInfo>"); }
	else { foreach (FieldInfo f, fl) { dump(f); } }
}
void Utils::dump(SqlParser & p)
{
	dump(p.toString());
}
void Utils::dump(SqlParser * pp)
{
	pp ? dump(*pp) : qDebug("Null SqlParser");
}
void Utils::dump(QList<SqlParser> pl)
{
	if (pl.count() == 0) { qDebug("Empty QList<SqlParser>"); }
	else { foreach (SqlParser p, pl) { dump(p); } }
}
void Utils::dump(QMap<QString,QString> map)
{
	if (map.count() == 0) { qDebug("Empty QMap"); }
	else
	{
		foreach (QString s, map.keys())
		{
			dump(s + " -> " + map.value(s));
		}
	}
}
