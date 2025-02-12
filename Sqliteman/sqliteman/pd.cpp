/* Copyright © Richard Parkins 2021
 *
 * This code is released under the GNU public Licence.
 * 
 * This is the code which implements a pd macro for gdb.
 * It enables gdb to print Qt classes, in particular QString.
 * There is also some code to print some sqlite structures
 * which may be useful when trying to identify issues in the sqlite library.
 * The sqlite code works for version 3.28.0 and might not work
 * for other versions.
 * The gdb macro is created by including the following code in your .gdbinit:
 *
define pd
  if $argc == 0
# show struct Vdbe
    list sqlite3.c:20312,20383
  else
    if $argc == 1
# normal dump argument
      call pd::dump($arg0)
    else
      if $argc == 2
        if $arg1 == "ops"
# special case to dump oplist from a struct Vdbe *
          call pd::dumpOpList((void *)$arg0)
        else
# print a field from a struct Vdbe *
          print ((struct Vdbe *)$arg0)->$arg1
        end
      else
        print "Do not know how to pd with >2 args"
      end
    end
  end
end
# need this to prevent SIGSEGV in pd from killing debuggee
set unwindonsignal on
# need this to fix issues with printing some types
set check type off
 *
 */

#include "pd.h"
#include <cmath>

#ifdef WANT_PD

#include "pd.h"
#include <cmath>
#include <sqlite3.h>

// These variables, private to this file, are used to support
// prettyprinting collections.
namespace {
    static QString line(""); // output line being assembled
    static QString spaces(""); // spaces for indentation
    static bool started;
}

// Start a (possibly nested) prettyprinted list of "type"
void pd::startList(QString type) {
    // We know "type" is a literal string so we don't need to prepare it.
    QString item(type + " { "); // create a list start for "type"
    int size = line.size();
    if (size > 0) { // if there is something already in the line
        // Check if there is room for the item plus ", " or "} "
        if (size + item.size() > 77) {
            qDebug(line.toUtf8().data()); // no, output the line
            line = spaces; // start a new line with the current indent
            line.append(item); // put the list start into it
        } else { // the list start will fit
            if (!started) { line.append(", "); } else { started = false; }
            line.append(item); // add it to the line
        }
    } else { // the line was empty, we are not in a list already
        line = item; // put the start list on a new unindented line
    }
    spaces.append("  "); // increase the indentation within the new list
    started = true;
}
// Add a QString to a prettyprinted list
void pd::addItem(QString s) {
    QString item(prepareString(s)); // catch null or empty QString
    // Check if there is room for the item plus ", " or "} "
    if (line.size() + item.size() > 77) {
        qDebug(line.toUtf8().data()); // no, output the line
        line = spaces; // start a new line with the current indent
        line.append(item); // put the QString into it
    } else { // the QString will fit
        if (!started) { line.append(", "); } else { started = false; }
        line.append(item); // add it to the line
    }
}
// Add a QVariant with a prefix Qstring (which can be empty)
// to a prettyprinted list (with possible nesting).
// Note that QVariant doesn't support general template classes
// because the code cannot know which specialisations exist.
// Instead you have to use QList<Qvariant>, etc., where the value type can be
// determined at runtime. This does allow collections of heterogeneous types.
// There is specific support for QList<QString> because this is so common.
void pd::addItem(QString s, QVariant v) {
    if (v.isNull()) { addItem(s.append("<Null QVariant>")); }
    else { // Check for list-type values
        switch (v.type()) {
            case QVariant::Hash: {
                // add the contents of the QHash
                QHash<QString, QVariant> h = v.toHash();
                if (h.isEmpty()) { addItem(s.append("<Empty QHash>")); }
                else {
                    startList(s.append("QHash")); // start a nested list
                    QHash<QString, QVariant>::const_iterator i;
                    for (i = h.constBegin(); i != h.constEnd(); ++i) {
                        addItem(
                            prepareString(i.key()).append(" -> "), i.value());
                    }
                    endList(); // end the nested list
                }
                break;
            }
            case QVariant::List: {
                // add the contents of the QList
                QList<QVariant> l = v.toList();
                if (l.isEmpty()) { addItem(s.append("<Empty QList>")); }
                else {
                    startList(s.append("QList")); // start a nested list
                    QList<QVariant>::const_iterator i;
                    for (i = l.constBegin(); i != l.constEnd(); ++i) {
                        addItem("", *i);
                    }
                    endList(); // end the nested list
                }
                break;
            }
            case QVariant::Map: {
                // add the contents of the QMap
                QMap<QString, QVariant> m = v.toMap();
                if (m.isEmpty()) { addItem(s.append("<Empty QMap>")); }
                else {
                    startList(s.append("QMap")); // start a nested list
                    QMap<QString, QVariant>::const_iterator i;
                    for (i = m.constBegin(); i != m.constEnd(); ++i) {
                        addItem(
                            prepareString(i.key()).append(" -> "), i.value());
                    }
                    endList(); // end the nested list
                }
                break;
            }
            case QVariant::Polygon: {
                // QPolygon is actually a vescor of QPoint's
                QVector<QPoint> vp = v.value<QPolygon>();
                if (vp.isEmpty()) {
                    addItem(s.append("Empty QPolygon"));
                } else {
                    startList(s.append("QPolygon")); // start a nested list
                    QVector<QPoint>::const_iterator i;
                    for (i = vp.constBegin(); i != vp.constEnd(); ++i) {
                        addItem(preparePoint(*i));
                    }
                    endList(); // end the nested list
                }
                break;
            }
            case QVariant::StringList: {
                // QStringList is actually a list of QString's
                QList<QString> l = v.toStringList();
                if (l.isEmpty()) { addItem(s.append("<Empty QList>")); }
                else {
                    startList(s.append("QList")); // start a nested list
                    QStringList::const_iterator i;
                    for (i = l.constBegin(); i != l.constEnd(); ++i) {
                        addItem(*i);
                    }
                    endList(); // end the nested list
                }
                break;
            }
            default: // add a non-list-type QVariant
                addItem(s.append(prepareVariant(v)));
        }
    }
}
// End a (possibly nested) prettyprinted list
void pd::endList() {
    line.append(" }"); // add a closing brace (we left room for one)
    spaces.chop(2); // reduce the indent outside the list we just closed
    if (spaces.size() == 0) { // if the indent is now zero
        qDebug("%s", line.toUtf8().data()); // output the last line
        line.clear(); // and clear it
    }
}
// convert a QColor to a QString
QString pd::prepareColor(QColor c) {
    return QString( "Red %1, Green %2, Blue %3")
        .arg(c.red()).arg(c.green()).arg(c.blue());
}
// Get the QString name for a QFont
QString pd::prepareFont(QFont f) {
    return QString("Font %1").arg(f.key());
}
// convert a QLine to a QString
QString pd::prepareLine(QLine l) {
    if (l.isNull()) { return "<Null QLine>"; }
    else {
        return QString("Line from (%1, %2) to (%3, %4)")
            .arg(l.x1()).arg(l.y1()).arg(l.x2()).arg(l.y2());
    }
}
// convert a QLineF to a QString (using a different overload of .arg)
QString pd::prepareLineF(QLineF l) {
    if (l.isNull()) { return "<Null QLineF>"; }
    else {
        return QString("LineF from (%1, %2) to (%3, %4)")
            .arg(l.x1()).arg(l.y1()).arg(l.x2()).arg(l.y2());
    }
}
// Get the QString name for a QLocale
QString pd::prepareLocale(QLocale l) { return l.bcp47Name(); }
// convert a QModelIndex to a QString
QString pd::prepareModelIndex(const QModelIndex x) {
    if (x.isValid()) {
        return QString("row %1, column %2").arg(x.row()).arg(x.column());
    } else { return "<Invalid QModelIndex>"; }
}
// Get the class name and object name (if there is one) from a QObject
// We only see pointers to QObject because it has no copy constructor
// ** think about printing parent or child hierarchies
QString pd::prepareObject(const QObject * ob) {
    if (ob == NULL) { return "<Null QObject>"; }
    else {
        QString s(ob->metaObject()->className());
        QString name(ob->objectName());
        if (name.size() > 0) {
            s.append(" (").append(name).append(")");
        }
        return s;
    }
}
// convert a QPoint to a QString
QString pd::preparePoint(QPoint p) {
    return QString("QPoint (%1, %2)").arg(p.x()).arg(p.y());
}
// convert a QPointF to a QString (using a different overload of .arg)
QString pd::preparePointF(QPointF p) {
    return QString("QPointF (%1, %2)").arg(p.x()).arg(p.y());
}
// convert a QRect to a QString
QString pd::prepareRect(QRect r) {
    return QString("QRect from (%1, %2) to (%3, %4)")
        .arg(r.left()).arg(r.top()).arg(r.right()).arg(r.bottom());
}
// convert a QRectF to a QString (using a different overload of .arg)
QString pd::prepareRectF(QRectF r) {
    return QString("QRectF from (%1, %2) to (%3, %4)")
        .arg(r.left()).arg(r.top()).arg(r.right()).arg(r.bottom());
}
// convert a QRegExp to a QString
QString pd::prepareRegExp(QRegExp r) {
    QString p = r.pattern();
    if (p.isNull()) { return "QRegExp with null pattern";
    } else if (p.isEmpty()) {
        return "QRegExp with empty pattern";
    } else if (r.isValid()) {
        return QString("QRegExp(\"%1\")").arg(p);
    } else {
        return QString("Invalid QRegExp(\"%1\")").arg(p);
    }
}
// convert a QSize to a QString
QString pd::prepareSize(QSize s) {
    return QString("QSize (Width %1, height %2)")
        .arg(s.width()).arg(s.height());
}
// convert a QSizeF to a QString (using a different overload of .arg)
QString pd::prepareSizeF(QSizeF s) {
    return QString("QSizeF (Width %1, height %2)")
        .arg(s.width()).arg(s.height());
}
// catch null and empty QString
QString pd::prepareString(QString s) {
    if (s.isNull()) { return "<Null QString>"; }
    else if (s.isEmpty()) { return "<Empty QString>"; }
    else { return s; }
}
QString pd::prepareTreeWidgetItem(const QTreeWidgetItem * const item) {
    if (item == NULL) { return "Null QTreeWidgetItem"; }
    QString s = "text(0) ";
    QString t = item->text(0);
    if (t.isNull()) { s.append("<NULL>"); }
    else if (t.isEmpty()) { s.append("<EMPTY>"); }
    else { s.append(t); }
    t = item->toolTip(0);
    if (!t.isEmpty()) { s.append(", tooltip ").append(t); }
    s.append(", text(1) ");
    t = item->text(1);
    if (t.isNull()) { s.append("<NULL>"); }
    else if (t.isEmpty()) { s.append("<EMPTY>"); }
    else { s.append(t); }
    t = item->toolTip(1);
    if (!t.isEmpty()) { s.append(", tooltip ").append(t); }
    return s;
}
// catch empty or invalid QUrl
QString pd::prepareUrl(QUrl u) {
    if (u.isEmpty()) { return "<Empty QUrl>"; }
    else {
        QString s(u.isValid() ? "%1" : "<Invalid URL %1>");
        return s.arg(u.toString());
    }
}
// only called for non-list-type QVariant's
QString pd::prepareVariant(QVariant v) {
    if (v.isNull()) { return "<Null QVariant>"; }
    switch (v.type()) {
        case QVariant::ByteArray:
        case QVariant::Char:
        case QVariant::Date:
        case QVariant::DateTime:
        case QVariant::Double:
        case QVariant::Int: return v.toString();
        case QVariant::Invalid: return "<Invalid QVariant>";
        case QVariant::LongLong:
        case QVariant::Matrix: return(v.toString());
        case QVariant::Matrix4x4: return "<Matrix4x4>";
        case QVariant::Palette: return "<Palette>";
        case QVariant::Pen: return "<Pen>";
        case QVariant::Pixmap: return "<Pixmap>";
        case QVariant::Point: return preparePoint(v.toPoint());
        case QVariant::PointF: return preparePointF(v.toPointF());
        case QVariant::Quaternion: return "Quaternion";
        case QVariant::Rect: return prepareRect(v.toRect());
        case QVariant::RectF: return prepareRectF(v.toRectF());
        case QVariant::RegExp: return prepareRegExp(v.toRegExp());
        case QVariant::Region: return "<Region>";
        case QVariant::Size: return prepareSize(v.value<QSize>());
        case QVariant::SizeF: return prepareSizeF(v.value<QSizeF>());
        case QVariant::SizePolicy: return "<SizePolicy>";
        case QVariant::String: return(v.toString());
        case QVariant::TextFormat: return "<TextFormat>";
        case QVariant::TextLength: return "<TextLength>";
        case QVariant::Url: return prepareUrl(v.toUrl());
        case QVariant::Vector2D: return "<Vector2D>";
        case QVariant::Vector3D: return "<Vector3D>";
        case QVariant::Vector4D: return "<Vector4D>";
        case QVariant::UserType: return "<UserType>";
        default: {
            QString typeName = v.typeName();
            if (typeName.isNull() || typeName.isEmpty()) {
                return "<QVariant with no type>";
            } else {
                return QString("<QVariant of type %1>").arg(typeName);
            }
        }
    }
}
QString pd::prepareWidget(const QWidget * ob) {
    if (ob == NULL) { return "<Null QWidget>"; }
    else {
        QString s(ob->metaObject()->className());
        QString name(ob->objectName());
        if (name.size() > 0) {
            s.append(" (").append(name).append(")");
        }
        return s;
    }
}

QString pd::prepareFieldInfo(const struct FieldInfo f) {
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
	return s;
}

// All the rest are the real output functions which can be called from gdb
void pd::dump(long long int i) { qDebug("%lld", i); }
void pd::dump(long long unsigned int u) { qDebug("%llu", u);}
void pd::dump(bool b) { qDebug(b ? "true" : "false"); }
void pd::dump(long double d) { qDebug("%Lg", d); }
void pd::dump(const char * s) { qDebug("%s", s); }

void pd::dump(QChar c) { dump(QString(c)); }
void pd::dump(QColor c) { dump(prepareColor(c)); }
void pd::dump(QComboBox & box) { // treat like a list
	int n = box.count();
	if (n == 0)
	{
		qDebug("<Empty QComboBox>");
	}
	else
	{
		startList("QComboBox"); // start the list
		for (int i = 0; i < n; ++i)
		{
			addItem(box.itemText(i)); // add an item
		}
		endList(); // end the list
	}
}
void pd::dump(QComboBox * box) {
	(box == NULL) ? qDebug("<Null QComboBox>") : dump(*box) ;
}
void pd::dump(QFont f) { dump(prepareFont(f)); }
// We can't deal with arbitrary specialisations of QHash
// but we handle this one because it can appear in a QVariant
void pd::dump(QHash<QString, QVariant> &h) {
    if (h.count() == 0) { qDebug ("Empty QHash<QString,QVariant>"); }
    else {
        startList("QHash"); // start the list
        QHash<QString, QVariant>::const_iterator i;
        for (i = h.constBegin(); i != h.constEnd(); ++i) {
            addItem(prepareString(i.key()).append(" -> "), i.value());
        }
        endList(); // end the list
	}
}
void pd::dump(QItemSelection &selection) {
    QList<QModelIndex> l = selection.indexes();
    if (l.isEmpty()) { qDebug ("<Empty QItemSelection>"); }
    else { dump(l); }
}
void pd::dump(QLine l) { dump(prepareLine(l)); }
void pd::dump(QLineEdit & le) {
    qDebug("LineEdit(%s), height %d, width %d",
          le.text().toUtf8().data(), le.size().rheight(), le.size().rwidth());
}
void pd::dump(QLineEdit * le) {
	(le == NULL) ? dump(*le) : qDebug("<Null QLineEdit>");
}
void pd::dump(QLineF l) { dump(prepareLineF(l)); }
// We can't deal with arbitrary specialisations of QList
// but we handle a few common ones.
void pd::dump(QList<bool> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<bool>"); }
    else {
        startList("QList"); // start the list
        QList<bool>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(QString(*i ? "true" : "false"));
        }
        endList(); // end the list
    }
}
void pd::dump(QList<int> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<int>"); }
    else {
        startList("QList"); // start the list
        QList<int>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(QString().setNum(*i));
        }
        endList(); // end the list
    }
}
void pd::dump(QList<float> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<float>"); }
    else {
        startList("QList");
        QList<float>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(QString().setNum(*i));
        }
        endList();
    }
}
void pd::dump(QList<double> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<double>"); }
    else {
        startList("QList");
        QList<double>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(QString().setNum(*i));
        }
        endList();
    }
}
void pd::dump(QList<QModelIndex> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<QModelIndex>"); }
    else {
        startList("QList<QModelIndex>");
        QList<QModelIndex>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(prepareModelIndex(*i));
        }
        endList();
    }
}
void pd::dump(QList<QObject *> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<QObject *>"); }
    else {
        startList("QList");
        QList<QObject *>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(prepareObject(*i));
        }
        endList();
    }
}
void pd::dump(QList<QString> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<QString>"); }
    else {
        startList("QList");
        QList<QString>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(*i);
        }
        endList();
    }
}
void pd::dump(QList<QTreeWidgetItem *> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<QTreeWidgetItem *>"); }
    else {
        startList("QList");
        QList<QTreeWidgetItem *>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(prepareTreeWidgetItem(*i));
        }
        endList();
    }
}
void pd::dump(QList<QVariant> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<QVariant>"); }
    else {
        startList("QList");
        QList<QVariant>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem("", *i);
        }
        endList();
    }
}
void pd::dump(QList<QWidget *> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<QWidget *>"); }
    else {
        startList("QList");
        QList<QWidget *>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(prepareWidget((QWidget *)*i));
        }
        endList();
    }
}
// We can't deal with arbitrary specialisations of QMap
// but we handle a few common ones.
void pd::dump(QMap<QString, QList<QString> > &m)
{
    if (m.isEmpty()) { qDebug ("Empty QMap<QString, QList<QString> >"); }
    else {
        startList("QMap");
        QMap<QString, QList<QString> >::const_iterator i;
        for (i = m.constBegin(); i != m.constEnd(); ++i) {
            // start a nested list
            startList(prepareString(i.key()).append(" -> QList"));
            QList<QString>::const_iterator j;
            for (j = (*i).constBegin(); j != (*i).constEnd(); ++j) {
                addItem(*j);
            }
            endList(); // end the nested lis
        }
        endList();
	}
}
void pd::dump(QMap<QString, QStringList> &m)
{
    if (m.isEmpty()) { qDebug ("Empty QMap<QString, QStringList >"); }
    else {
        startList("QMap");
        QMap<QString, QStringList>::const_iterator i;
        for (i = m.constBegin(); i != m.constEnd(); ++i) {
            // start a nested list
            startList(prepareString(i.key()).append(" -> QList"));
            QStringList::const_iterator j;
            for (j = (*i).constBegin(); j != (*i).constEnd(); ++j) {
                addItem(*j);
            }
            endList(); // end the nested lis
        }
        endList();
	}
}
void pd::dump(QMap<QString, QObject *>&m) {
    if (m.count() == 0) { qDebug ("Empty QMap<QString, QObject *>"); }
    else {
        startList("QMap");
        QMap<QString, QObject *>::const_iterator i;
        for (i = m.constBegin(); i != m.constEnd(); ++i) {
            addItem(
                prepareString(i.key())
                    .append(" -> ").append(prepareObject(i.value())));
        }
        endList();
	}
}
void pd::dump(QMap<QString, QString> &m) {
    if (m.count() == 0) { qDebug ("Empty QMap<QString, QString>"); }
    else {
        startList("QMap");
        QMap<QString, QString>::const_iterator i;
        for (i = m.constBegin(); i != m.constEnd(); ++i) {
            addItem(
                prepareString(i.key())
                    .append(" -> ").append(prepareString(i.value())));
        }
        endList();
	}
}
void pd::dump(QMap<QString, QVariant> &m) {
    if (m.count() == 0) { qDebug("Empty QMap<QString, QVariant>"); }
    else {
        startList("QMap");
        QMap<QString, QVariant>::const_iterator i;
        for (i = m.constBegin(); i != m.constEnd(); ++i) {
            addItem(prepareString(i.key()), i.value());
        }
        endList();
	}
}
void pd::dump(QModelIndex &x) { dump(prepareModelIndex(x)); }
void pd::dump(QObject & ob) { dump(prepareObject(&ob)); }
void pd::dump(QObject * ob) { dump(prepareObject(ob)); }
void pd::dump(QPoint p) { dump(preparePoint(p)); }
void pd::dump(QPointF p) { dump(preparePointF(p)); }
void pd::dump(QRect r) { dump(prepareRect(r)); }
void pd::dump(QRectF r) { dump(prepareRectF(r)); }
void pd::dump(QRegExp r) { dump(prepareRegExp(r)); }
void pd::dump(QSet<QTreeWidgetItem *> &l) {
    if (l.isEmpty()) { qDebug ("Empty QSet<QTreeWidgetItem *>"); }
    else {
        startList("QSet");
        QSet<QTreeWidgetItem *>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(prepareTreeWidgetItem(*i));
        }
        endList();
    }
}
void pd::dump(QSize s) { dump(prepareSize(s)); }
void pd::dump(QSizeF s) { dump(prepareSizeF(s)); }
void pd::dump(QSqlError & e) { dump(e.text()); }
void pd::dump(QSqlError * e) {
	(e != NULL) ? dump(*e) : qDebug("Null QSqlError");
}
// Treat QSqlRecord like a QMap
void pd::dump(QSqlRecord & r) {
	int n = r.count();
    if (n == 0) { qDebug("<Empty QSqlRecord>"); }
    else {
        startList("QSqlRecord"); // start the list
        for (int i = 0; i < n; ++i)
        {
            QVariant v = r.value(i);
            // add a field name, type, and value
            addItem(QString("Field ").append(prepareString(r.fieldName(i)))
                                     .append(", type ")
                                     .append(prepareString(v.typeName()))
                                     .append(": "), v);
        }
        endList(); // end the list
    }
}
void pd::dump(QSqlRecord * r) {
    (r != NULL) ? dump(*r) : qDebug("<Null QSqlRecord>");
}
// This is the bottom-level routine that eventually gets called
// when a non-list-like type has been converted to a QString.
// We still need to call prepareString() because for some types we use
// the type's own string conversion which can return a null or empty QString.
// We could save a call by special-casing the result of one of our
// own prepare(...) calls (which handle null or empty strings), but we're
// interactive here so it isn't worth having the extra code to save time.
void pd::dump(QString s) { qDebug(prepareString(s).toUtf8().data()); }
// gdb thinks QStringList is not the same as QList<Qstring>
void pd::dump(QStringList &l) {
    if (l.isEmpty()) { qDebug ("Empty QStringList"); }
    else {
        startList("QList");
        QStringList::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(*i);
        }
        endList();
    }
}
void pd::dump(QStringRef sr) { dump(sr.toString()); }
void pd::dump(QTableWidgetItem & item) {
    char * text = prepareString(item.text()).toUtf8().data();
    char * tip =  prepareString(item.toolTip()).toUtf8().data();
	qDebug("text %s, toolTip %s", text, tip);
}
void pd::dump(QTableWidgetItem * item) {
	(item != NULL) ? dump(*item) : qDebug("Null QTableWidgetItem");
}
void pd::dump(QTextEdit & te) { dump(te.toPlainText()); }
void pd::dump(QTextEdit * te) {
	(te != NULL) ? dump(*te) : qDebug("Null QTextEdit"); }
void pd::dump(QTreeWidgetItem & item) {
    dump(prepareTreeWidgetItem(&item));
}
void pd::dump(QTreeWidgetItem * item) {
    dump(prepareTreeWidgetItem(item));
}
void pd::dump(QUrl url) { dump(url.toString()); }

// For a list-like QVariant we can call dump() recursively because
// we can get the type at runtime.
void pd::dump(QVariant v) {
    if (v.isNull()) { qDebug("<Null QVariant>"); return; }
    switch (v.type()) {
		case QVariant::Hash:
            dump(v.toHash());
            break;
		case QVariant::List:
            dump(v.toList());
            break;
		case QVariant::Map:
            dump(v.toMap());
            break;
        case QVariant::Polygon: {
                // QPolygon is actually a vectcor of QPoint's
                QVector<QPoint> vp = v.value<QPolygon>();
                if (vp.isEmpty()) { qDebug ("<Empty QPolygon>"); }
                else {
                    startList("QPolygon"); // start the list
                    QVector<QPoint>::const_iterator i;
                    for (i = vp.constBegin(); i != vp.constEnd(); ++i) {
                        addItem(preparePoint(*i));
                    }
                    endList(); // end the list
                }
            break;
        }
        case QVariant::StringList: // special-cased in QVariant
            dump(v.toStringList()); //dump()
            break;
        default: {
            qDebug(prepareVariant(v).toUtf8().data());
        }
    }
}
// We can't deal with arbitrary specialisations of QVector
// but we handle a few common ones.
void pd::dump(QVector<bool> &v) {
    if (v.count() == 0) { qDebug ("Empty QVector<bool>"); }
    else {
        startList ("QVector");
        QVector<bool>::const_iterator i;
        for (i = v.constBegin(); i != v.constEnd(); ++i) {
            addItem(QString(*i ? "true" : "false"));
        }
        endList();
    }
}
void pd::dump(QVector<int> &v) {
    if (v.count() == 0) { qDebug ("Empty QVector<int>"); }
    else {
        startList ("QVector");
        QVector<int>::const_iterator i;
        for (i = v.constBegin(); i != v.constEnd(); ++i) {
            addItem(QString().setNum(*i));
        }
        endList();
    }
}
void pd::dump(QVector<float> &v) {
    if (v.count() == 0) { qDebug ("Empty QVector<float>"); }
    else {
        startList ("QVector");
        QVector<float>::const_iterator i;
        for (i = v.constBegin(); i != v.constEnd(); ++i) {
            addItem(QString().setNum(*i));
        }
        endList();
    }
}
void pd::dump(QVector<double> &v) {
    if (v.count() == 0) { qDebug ("Empty QVector<double>"); }
    else {
        startList ("QVector");
        QVector<double>::const_iterator i;
        for (i = v.constBegin(); i != v.constEnd(); ++i) {
            addItem(QString().setNum(*i));
        }
        endList();
    }
}
void pd::dump(QVector<QObject *> &v) {
    if (v.count() == 0) { qDebug ("Empty QVector<QObject *>"); }
    else {
        startList ("QVector");
        QVector<QObject *>::const_iterator i;
        for (i = v.constBegin(); i != v.constEnd(); ++i) {
            addItem(prepareObject(*i));
        }
        endList();
    }
}
void pd::dump(QVector<QPoint> &v) {
    if (v.count() == 0) { qDebug ("Empty QVector<QPoint>"); }
    else {
        startList ("QVector");
        QVector<QPoint>::const_iterator i;
        for (i = v.constBegin(); i != v.constEnd(); ++i) {
            addItem(preparePoint(*i));
        }
        endList();
    }
}
void pd::dump(QVector<QPointF> &v) {
    if (v.count() == 0) { qDebug ("Empty QVector<QPointF>"); }
    else {
        startList ("QVector");
        QVector<QPointF>::const_iterator i;
        for (i = v.constBegin(); i != v.constEnd(); ++i) {
            addItem(preparePointF(*i));
        }
        endList();
    }
}
void pd::dump(QVector<QVariant> &v) {
    if (v.count() == 0) { qDebug ("Empty QVector<QVariant>"); }
    else {
        startList ("QVector");
        QVector<QVariant>::const_iterator i;
        for (i = v.constBegin(); i != v.constEnd(); ++i) {
            addItem("", *i);
        }
        endList();
    }
}
void pd::dump(QWidget * w) { dump(prepareWidget((QWidget *)w)); }
// The following are for sqliteman's own application types
void pd::dump(const struct Token & t) {
    dump(preparetokenType(t.type) + '(' + t.name + ')');
}
void pd::dump(QList<Token> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<Token>"); }
    else {
        startList("QList");
        QList<Token>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(SqlParser::toString(*i));
        }
        endList();
    }
}
void pd::dump(Expression * e) { dump(SqlParser::toString(e)); }
void pd::dump(QList<Expression *> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<Expression *>"); }
    else {
        startList("QList");
        QList<Expression *>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(SqlParser::toString(*i));
        }
        endList();
    }
}
void pd::dump(const struct FieldInfo &f) { dump(prepareFieldInfo(f)); }
void pd::dump(QList<struct FieldInfo> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<FieldInfo>"); }
    else {
        startList("QList");
        QList<struct FieldInfo>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem(prepareFieldInfo(*i));
        }
        endList();
    }
}
void pd::dump(SqlParser & p) { dump(p.toString()); }
void pd::dump(SqlParser * pp) {
	pp ? dump(*pp) : qDebug("Null SqlParser");
}
void pd::dump(QList<SqlParser *> &l) {
    if (l.isEmpty()) { qDebug ("Empty QList<SqlParser *"); }
    else {
        startList("QList");
        QList<SqlParser *>::const_iterator i;
        for (i = l.constBegin(); i != l.constEnd(); ++i) {
            addItem((*i)->toString());
        }
        endList();
    }
}

#ifdef WANT_VDBE_DECODING
// sqlite opcode list dumper, most of this is copied from sqlite3.c
// this is highly dependent on sqlite version (3.28.0 for this code)
#define OP_Savepoint       0
#define OP_AutoCommit      1
#define OP_Transaction     2
#define OP_SorterNext      3 /* jump                                       */
#define OP_Prev            4 /* jump                                       */
#define OP_Next            5 /* jump                                       */
#define OP_Checkpoint      6
#define OP_JournalMode     7
#define OP_Vacuum          8
#define OP_VFilter         9 /* jump, synopsis: iplan=r[P3] zplan='P4'     */
#define OP_VUpdate        10 /* synopsis: data=r[P3@P2]                    */
#define OP_Goto           11 /* jump                                       */
#define OP_Gosub          12 /* jump                                       */
#define OP_InitCoroutine  13 /* jump                                       */
#define OP_Yield          14 /* jump                                       */
#define OP_MustBeInt      15 /* jump                                       */
#define OP_Jump           16 /* jump                                       */
#define OP_Once           17 /* jump                                       */
#define OP_If             18 /* jump                                       */
#define OP_Not            19 /* same as TK_NOT, synopsis: r[P2]= !r[P1]    */
#define OP_IfNot          20 /* jump                                       */
#define OP_IfNullRow      21 /* jump, synopsis: if P1.nullRow then r[P3]=NULL, goto P2 */
#define OP_SeekLT         22 /* jump, synopsis: key=r[P3@P4]               */
#define OP_SeekLE         23 /* jump, synopsis: key=r[P3@P4]               */
#define OP_SeekGE         24 /* jump, synopsis: key=r[P3@P4]               */
#define OP_SeekGT         25 /* jump, synopsis: key=r[P3@P4]               */
#define OP_IfNoHope       26 /* jump, synopsis: key=r[P3@P4]               */
#define OP_NoConflict     27 /* jump, synopsis: key=r[P3@P4]               */
#define OP_NotFound       28 /* jump, synopsis: key=r[P3@P4]               */
#define OP_Found          29 /* jump, synopsis: key=r[P3@P4]               */
#define OP_SeekRowid      30 /* jump, synopsis: intkey=r[P3]               */
#define OP_NotExists      31 /* jump, synopsis: intkey=r[P3]               */
#define OP_Last           32 /* jump                                       */
#define OP_IfSmaller      33 /* jump                                       */
#define OP_SorterSort     34 /* jump                                       */
#define OP_Sort           35 /* jump                                       */
#define OP_Rewind         36 /* jump                                       */
#define OP_IdxLE          37 /* jump, synopsis: key=r[P3@P4]               */
#define OP_IdxGT          38 /* jump, synopsis: key=r[P3@P4]               */
#define OP_IdxLT          39 /* jump, synopsis: key=r[P3@P4]               */
#define OP_IdxGE          40 /* jump, synopsis: key=r[P3@P4]               */
#define OP_RowSetRead     41 /* jump, synopsis: r[P3]=rowset(P1)           */
#define OP_RowSetTest     42 /* jump, synopsis: if r[P3] in rowset(P1) goto P2 */
#define OP_Or             43 /* same as TK_OR, synopsis: r[P3]=(r[P1] || r[P2]) */
#define OP_And            44 /* same as TK_AND, synopsis: r[P3]=(r[P1] && r[P2]) */
#define OP_Program        45 /* jump                                       */
#define OP_FkIfZero       46 /* jump, synopsis: if fkctr[P1]==0 goto P2    */
#define OP_IfPos          47 /* jump, synopsis: if r[P1]>0 then r[P1]-=P3, goto P2 */
#define OP_IfNotZero      48 /* jump, synopsis: if r[P1]!=0 then r[P1]--, goto P2 */
#define OP_DecrJumpZero   49 /* jump, synopsis: if (--r[P1])==0 goto P2    */
#define OP_IsNull         50 /* jump, same as TK_ISNULL, synopsis: if r[P1]==NULL goto P2 */
#define OP_NotNull        51 /* jump, same as TK_NOTNULL, synopsis: if r[P1]!=NULL goto P2 */
#define OP_Ne             52 /* jump, same as TK_NE, synopsis: IF r[P3]!=r[P1] */
#define OP_Eq             53 /* jump, same as TK_EQ, synopsis: IF r[P3]==r[P1] */
#define OP_Gt             54 /* jump, same as TK_GT, synopsis: IF r[P3]>r[P1] */
#define OP_Le             55 /* jump, same as TK_LE, synopsis: IF r[P3]<=r[P1] */
#define OP_Lt             56 /* jump, same as TK_LT, synopsis: IF r[P3]<r[P1] */
#define OP_Ge             57 /* jump, same as TK_GE, synopsis: IF r[P3]>=r[P1] */
#define OP_ElseNotEq      58 /* jump, same as TK_ESCAPE                    */
#define OP_IncrVacuum     59 /* jump                                       */
#define OP_VNext          60 /* jump                                       */
#define OP_Init           61 /* jump, synopsis: Start at P2                */
#define OP_PureFunc0      62
#define OP_Function0      63 /* synopsis: r[P3]=func(r[P2@P5])             */
#define OP_PureFunc       64
#define OP_Function       65 /* synopsis: r[P3]=func(r[P2@P5])             */
#define OP_Return         66
#define OP_EndCoroutine   67
#define OP_HaltIfNull     68 /* synopsis: if r[P3]=null halt               */
#define OP_Halt           69
#define OP_Integer        70 /* synopsis: r[P2]=P1                         */
#define OP_Int64          71 /* synopsis: r[P2]=P4                         */
#define OP_String         72 /* synopsis: r[P2]='P4' (len=P1)              */
#define OP_Null           73 /* synopsis: r[P2..P3]=NULL                   */
#define OP_SoftNull       74 /* synopsis: r[P1]=NULL                       */
#define OP_Blob           75 /* synopsis: r[P2]=P4 (len=P1)                */
#define OP_Variable       76 /* synopsis: r[P2]=parameter(P1,P4)           */
#define OP_Move           77 /* synopsis: r[P2@P3]=r[P1@P3]                */
#define OP_Copy           78 /* synopsis: r[P2@P3+1]=r[P1@P3+1]            */
#define OP_SCopy          79 /* synopsis: r[P2]=r[P1]                      */
#define OP_IntCopy        80 /* synopsis: r[P2]=r[P1]                      */
#define OP_ResultRow      81 /* synopsis: output=r[P1@P2]                  */
#define OP_CollSeq        82
#define OP_AddImm         83 /* synopsis: r[P1]=r[P1]+P2                   */
#define OP_RealAffinity   84
#define OP_Cast           85 /* synopsis: affinity(r[P1])                  */
#define OP_Permutation    86
#define OP_Compare        87 /* synopsis: r[P1@P3] <-> r[P2@P3]            */
#define OP_IsTrue         88 /* synopsis: r[P2] = coalesce(r[P1]==TRUE,P3) ^ P4 */
#define OP_Offset         89 /* synopsis: r[P3] = sqlite_offset(P1)        */
#define OP_Column         90 /* synopsis: r[P3]=PX                         */
#define OP_Affinity       91 /* synopsis: affinity(r[P1@P2])               */
#define OP_MakeRecord     92 /* synopsis: r[P3]=mkrec(r[P1@P2])            */
#define OP_Count          93 /* synopsis: r[P2]=count()                    */
#define OP_ReadCookie     94
#define OP_SetCookie      95
#define OP_BitAnd         96 /* same as TK_BITAND, synopsis: r[P3]=r[P1]&r[P2] */
#define OP_BitOr          97 /* same as TK_BITOR, synopsis: r[P3]=r[P1]|r[P2] */
#define OP_ShiftLeft      98 /* same as TK_LSHIFT, synopsis: r[P3]=r[P2]<<r[P1] */
#define OP_ShiftRight     99 /* same as TK_RSHIFT, synopsis: r[P3]=r[P2]>>r[P1] */
#define OP_Add           100 /* same as TK_PLUS, synopsis: r[P3]=r[P1]+r[P2] */
#define OP_Subtract      101 /* same as TK_MINUS, synopsis: r[P3]=r[P2]-r[P1] */
#define OP_Multiply      102 /* same as TK_STAR, synopsis: r[P3]=r[P1]*r[P2] */
#define OP_Divide        103 /* same as TK_SLASH, synopsis: r[P3]=r[P2]/r[P1] */
#define OP_Remainder     104 /* same as TK_REM, synopsis: r[P3]=r[P2]%r[P1] */
#define OP_Concat        105 /* same as TK_CONCAT, synopsis: r[P3]=r[P2]+r[P1] */
#define OP_ReopenIdx     106 /* synopsis: root=P2 iDb=P3                   */
#define OP_BitNot        107 /* same as TK_BITNOT, synopsis: r[P2]= ~r[P1] */
#define OP_OpenRead      108 /* synopsis: root=P2 iDb=P3                   */
#define OP_OpenWrite     109 /* synopsis: root=P2 iDb=P3                   */
#define OP_String8       110 /* same as TK_STRING, synopsis: r[P2]='P4'    */
#define OP_OpenDup       111
#define OP_OpenAutoindex 112 /* synopsis: nColumn=P2                       */
#define OP_OpenEphemeral 113 /* synopsis: nColumn=P2                       */
#define OP_SorterOpen    114
#define OP_SequenceTest  115 /* synopsis: if( cursor[P1].ctr++ ) pc = P2   */
#define OP_OpenPseudo    116 /* synopsis: P3 columns in r[P2]              */
#define OP_Close         117
#define OP_ColumnsUsed   118
#define OP_SeekHit       119 /* synopsis: seekHit=P2                       */
#define OP_Sequence      120 /* synopsis: r[P2]=cursor[P1].ctr++           */
#define OP_NewRowid      121 /* synopsis: r[P2]=rowid                      */
#define OP_Insert        122 /* synopsis: intkey=r[P3] data=r[P2]          */
#define OP_Delete        123
#define OP_ResetCount    124
#define OP_SorterCompare 125 /* synopsis: if key(P1)!=trim(r[P3],P4) goto P2 */
#define OP_SorterData    126 /* synopsis: r[P2]=data                       */
#define OP_RowData       127 /* synopsis: r[P2]=data                       */
#define OP_Rowid         128 /* synopsis: r[P2]=rowid                      */
#define OP_NullRow       129
#define OP_SeekEnd       130
#define OP_SorterInsert  131 /* synopsis: key=r[P2]                        */
#define OP_IdxInsert     132 /* synopsis: key=r[P2]                        */
#define OP_IdxDelete     133 /* synopsis: key=r[P2@P3]                     */
#define OP_DeferredSeek  134 /* synopsis: Move P3 to P1.rowid if needed    */
#define OP_IdxRowid      135 /* synopsis: r[P2]=rowid                      */
#define OP_Destroy       136
#define OP_Clear         137
#define OP_ResetSorter   138
#define OP_CreateBtree   139 /* synopsis: r[P2]=root iDb=P1 flags=P3       */
#define OP_SqlExec       140
#define OP_ParseSchema   141
#define OP_LoadAnalysis  142
#define OP_DropTable     143
#define OP_DropIndex     144
#define OP_Real          145 /* same as TK_FLOAT, synopsis: r[P2]=P4       */
#define OP_DropTrigger   146
#define OP_IntegrityCk   147
#define OP_RowSetAdd     148 /* synopsis: rowset(P1)=r[P2]                 */
#define OP_Param         149
#define OP_FkCounter     150 /* synopsis: fkctr[P1]+=P2                    */
#define OP_MemMax        151 /* synopsis: r[P1]=max(r[P1],r[P2])           */
#define OP_OffsetLimit   152 /* synopsis: if r[P1]>0 then r[P2]=r[P1]+max(0,r[P3]) else r[P2]=(-1) */
#define OP_AggInverse    153 /* synopsis: accum=r[P3] inverse(r[P2@P5])    */
#define OP_AggStep       154 /* synopsis: accum=r[P3] step(r[P2@P5])       */
#define OP_AggStep1      155 /* synopsis: accum=r[P3] step(r[P2@P5])       */
#define OP_AggValue      156 /* synopsis: r[P3]=value N=P2                 */
#define OP_AggFinal      157 /* synopsis: accum=r[P1] N=P2                 */
#define OP_Expire        158
#define OP_TableLock     159 /* synopsis: iDb=P1 root=P2 write=P3          */
#define OP_VBegin        160
#define OP_VCreate       161
#define OP_VDestroy      162
#define OP_VOpen         163
#define OP_VColumn       164 /* synopsis: r[P3]=vcolumn(P2)                */
#define OP_VRename       165
#define OP_Pagecount     166
#define OP_MaxPgcnt      167
#define OP_Trace         168
#define OP_CursorHint    169
#define OP_Noop          170
#define OP_Explain       171
#define OP_Abortable     172
#define SQLITE_STOREP2   0x20  /* Store result in reg[P2] rather than jump */
#define READTYPE (v->aOp[i].opcode == OP_ReopenIdx ? \
                    "OP_ReopenIdx" : "OP_OpenRead")
void pd::dumpOpList(void * pVdbe) {
    struct FuncDef {
    signed char nArg;  /* Number of arguments.  -1 means unlimited */
    unsigned int funcFlags;       /* Some combination of SQLITE_FUNC_* */
    void *pUserData;     /* User data parameter */
    struct FuncDef *pNext;      /* Next function with same name */
    void *xSFunc; /* func or agg-step */
    void *xFinalize;                  /* Agg finalizer */
    void *xValue;                     /* Current agg value */
    void *xInverse; /* inverse agg-step */
    const char *zName;   /* SQL name of the function. */
    union {
        struct FuncDef *pHash; /* Next with different name but same hash */
        void *pDestructor;   /* Reference counted destructor function */
        } u;
    };
    struct sqlite3_context {
    void *pOut;              /* The return value is stored here */
    struct FuncDef *pFunc;         /* Pointer to function information */
#if 0 // for the time being we don't use the rest of it
    Mem *pMem;              /* Memory cell used to store aggregate context */
    Vdbe *pVdbe;            /* The VM that owns this context */
    int iOp;                /* Instruction number of OP_Function */
    int isError;            /* Error code returned by the function. */
    u8 skipFlag;            /* Skip accumulator loading if true */
    u8 argc;                /* Number of arguments */
    sqlite3_value *argv[1]; /* Argument set */
#endif
    };
    struct VdbeOp {
     unsigned char opcode;          /* What operation to perform */
     signed char p4type; /* One of the P4_xxx constants for p4 */
     uint16_t p5;/* Fifth parameter is an unsigned 16-bit integer */
     int p1;             /* First operand */
     int p2;             /* Second parameter (often the jump destination) */
     int p3;             /* The third parameter */
     union p4union {     /* fourth parameter */
         int i;          /* Integer value if p4type==P4_INT32 */
         void *p;        /* Generic pointer */
         char *z;        /* Pointer to data for string (char array) types */
         struct FuncDef * pFuncDef;
         struct sqlite3_context * pCtx;
     } p4;
    };
    struct Vdbe {
    void *db;            /* The database connection that owns this statement */
    struct Vdbe *pPrev,*pNext;/* Linked list of VDBEs with the same Vdbe.db */
    void *pParse;          /* Parsing context used to create this Vdbe */
    int16_t nVar;             /* Number of entries in aVar[] */
    uint32_t magic;              /* Magic number for sanity checking */
    int nMem;/* Number of memory locations currently allocated */
    int nCursor;            /* Number of slots in apCsr[] */
    uint32_t cacheCtr;           /* VdbeCursor row cache generation counter */
    int pc;                 /* The program counter */
    int rc;                 /* Value to return */
    int nChange;            /* Number of db changes made since last reset */
    int iStatement;         /* Statement number (or 0 if has no opened stmt) */
    int64_t iCurrentTime;/* Value of julianday('now') for this statement */
    int64_t nFkConstraint;      /* Number of imm. FK constraints this VM */
    int64_t nStmtDefCons;/* Number of def. constraints when stmt started */
    int64_t nStmtDefImmCons;/* Number def. imm constraints when stmt started */
    void *aMem;              /* The memory locations */
    void *apArg;/* Arguments to currently executing user function */
    void *apCsr;/* One element of this array for each open cursor */
    void *aVar;              /* Values for the OP_Variable opcode. */
    struct VdbeOp *aOp;/* Space to hold the virtual machine's program */
    int nOp;                /* Number of instructions in the program */
#if 0 // for the time being we don't use the rest of it
    int nOpAlloc;           /* Slots allocated for aOp[] */
    Mem *aColName;          /* Column names to return */
    Mem *pResultSet;        /* Pointer to an array of results */
    char *zErrMsg;          /* Error message written here */
    VList *pVList;          /* Name of variables */
    u16 nResColumn;/* Number of columns in one row of the result set */
    u8 errorAction;         /* Recovery action to do in case of an error */
    u8 minWriteFileFormat;/* Minimum file format for writable database files */
    u8 prepFlags;           /* SQLITE_PREPARE_* flags */
    bft expired:2;/* 1: recompile VM immediately  2: when convenient */
    bft explain:2;          /* True if EXPLAIN present on SQL command */
    bft doingRerun:1;       /* True if rerunning after an auto-reprepare */
    bft changeCntOn:1;      /* True to update the change-counter */
    bft runOnlyOnce:1;      /* Automatically expire on reset */
    bft usesStmtJournal:1;  /* True if uses a statement journal */
    bft readOnly:1;         /* True for statements that do not write */
    bft bIsReader:1;        /* True for statements that read */
    yDbMask btreeMask;      /* Bitmask of db->aDb[] entries referenced */
    yDbMask lockMask;       /* Subset of btreeMask that requires a lock */
    u32 aCounter[7];        /* Counters used by sqlite3_stmt_status() */
    char *zSql;             /* Text of the SQL statement that generated this */
    void *pFree;            /* Free this when deleting the vdbe */
    VdbeFrame *pFrame;      /* Parent frame */
    VdbeFrame *pDelFrame;   /* List of frame objects to free on VM reset */
    int nFrame;             /* Number of frames in pFrame list */
    u32 expmask;            /* Binding to these vars invalidates VM */
    SubProgram *pProgram;   /* Linked list of all sub-programs used by VM */
    AuxData *pAuxData; /* Linked list of auxdata allocations */
#endif
    };
    struct Vdbe * v = (struct Vdbe *)pVdbe;
    int j;
    for (int i = 0; i < v->nOp; ++i) {
        switch (v->aOp[i].opcode) {
            case OP_Savepoint:
                switch (v->aOp[i].p1) {
                    case 0:
                        qDebug("%d: OP_Savepoint new savepoint %s",
                            i, v->aOp[i].p4.z);
                        continue;
                    case 1:
                        qDebug("%d: OP_Savepoint commit savepoint %s",
                            i, v->aOp[i].p4.z);
                        continue;
                    case 2:
                        qDebug("%d: OP_Savepoint rollback savepoint %s",
                            i, v->aOp[i].p4.z);
                        continue;
                    default:
                        qDebug("%d: OP_Savepoint invalid P1 %d",
                            i, v->aOp[i].p1);
                        continue;
                }
            case OP_AutoCommit:
                switch (v->aOp[i].p1) {
                    case 0: if (v->aOp[i].p2) {
                        break; 
                    } else {
                        qDebug("%d: OP_AutoCommit %s",
                               i, "turn on autocommit, and halt");
                        continue;
                    }
                    case 1: if (v->aOp[i].p2) {
                        qDebug("%d: OP_AutoCommit %s",
                               i, "roll back, turn off autocommit, and halt");
                        continue;
                    } else {
                        qDebug("%d: OP_AutoCommit %s",
                               i, "commit, turn off autocommit, and halt");
                        continue;
                    }
                }
                qDebug("%d: OP_AutoCommit invalid P1 %d P2 %d",
                    i, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_Transaction:
                if (v->aOp[i].p2) {
                    qDebug("%d: OP_Transaction %s",
                        i, "begin or upgrade to write transaction");
                    continue;
                } else {
                    qDebug("%d: OP_Transaction %s",
                        i, "begin read transaction");
                    continue;
                }
            case OP_SorterNext:
                qDebug("%d: OP_SorterNext %s %d %s ->%d if no more",
                       i, "advance sorter", v->aOp[i].p1,
                       "to next sorted record or", v->aOp[i].p2);
                continue;
            case OP_Prev:
                qDebug("%d: OP_Prev %s %d %s ->%d unless no more",
                       i, "backup cursor", v->aOp[i].p1,
                       "to previous key/data pair and", v->aOp[i].p2);
                continue;
            case OP_Next:
                qDebug("%d: OP_Next %s %d %s ->%d unless no more",
                       i, "advance cursor", v->aOp[i].p1,
                       "to next key/data pair and", v->aOp[i].p2);
                continue;
            case OP_Checkpoint:
            {
                int MAX_ATTACHED = 10; // standard default value
                if (v->aOp[i].p1 == MAX_ATTACHED) {
                    qDebug("%d: OP_Checkpoint checkpoint all databases", i);
                } else {
                    qDebug("%d: OP_Checkpoint checkpoint database %i",
                        i, v->aOp[i].p1);
                }
                continue;
            }
            case OP_JournalMode:
                switch (v->aOp[i].p3) {
                    case -1:
                        qDebug("%d: OP_JournalMode P3 = current mode", i);
                        continue;
                    case 0:
                        qDebug("%d: OP_JournalMode %s",
                            i, "commit by deleting journal");
                        continue;
                    case 1:
                        qDebug("%d: OP_JournalMode %s",
                            i, "commit by zeroing journal header");
                        continue;
                    case 2:
                        qDebug("%d: OP_JournalMode disable", i);
                        continue;
                    case 3:
                        qDebug("%d: OP_JournalMode %s",
                            i, "commit by deleting journal");
                        continue;
                    case 4:
                        qDebug("%d: OP_JournalMode %s",
                            i, "commit by truncating journal");
                        continue;
                    case 5:
                        qDebug("%d: OP_JournalMode in-memory journal", i);
                        continue;
                }
                qDebug("%d: OP_JournalMode invalid P3 %d",
                       i, v->aOp[i].p3);
                continue;
            case OP_Vacuum:
                qDebug("%d: OP_Vacuum vacuum database", i); continue;
            case OP_VFilter:
                qDebug("%d: OP_VFilter %s %d, ->%d if no match",
                       i, "invoke xfilter method in virtual table",
                       v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_VUpdate:
                qDebug("%d: OP_VUpdate %s %d",
                       i, "invoke xupdate method in virtual table",
                       v->aOp[i].p4.i);
                continue;
            case OP_Goto:
                qDebug("%d: OP_Goto ->%d", i, v->aOp[i].p2); continue;
            case OP_Gosub:
                qDebug("%d: OP_Gosub r[%d] = %d, ->%d",
                       i, v->aOp[i].p1, i, v->aOp[i].p2);
                continue;
            case OP_InitCoroutine:
                if (v->aOp[i].p2 == 0) {
                    qDebug("%d: OP_InitCoroutine r[%d] = %d",
                        i, v->aOp[i].p1, v->aOp[i].p3 - 1);
                } else {
                    qDebug("%d: OP_InitCoroutine, r[%d] = %d, ->%d",
                        i, v->aOp[i].p1, v->aOp[i].p3 - 1, v->aOp[i].p2);
                }
                continue;
            case OP_Yield:
                qDebug("%d: OP_Yield r[%d} = %d, ->old r[%d}",
                    i, v->aOp[i].p1, i, v->aOp[i].p1);
                continue;
            case OP_MustBeInt:
                qDebug("%d: OP_MustBeInt ->%d if r(%d) not integer",
                    i, v->aOp[i].p2, v->aOp[i].p1);
                continue;
            case OP_Jump:
                qDebug("%d: OP_Jump %s ->%d, %d, %d",
                       i, "if last OP_Compare was <, =, >",
                       v->aOp[i].p1, v->aOp[i].p2, v->aOp[i].p3);
                continue;
            case OP_Once:
                qDebug("%d: OP_Once if not first time ->%d", i, v->aOp[i].p2); continue;
            case OP_If:
                qDebug("%d: OP_If ->%d if r(%d) nonzero%s",
                    i, v->aOp[i].p2, v->aOp[i].p1,
                    (v->aOp[i].p3 ? "or NULL" : ""));
                continue;
            case OP_Not:
                qDebug("%d: OP_Not r(%d) = NULL, 0, 1 if r(%d) %s",
                       i, v->aOp[i].p2, v->aOp[i].p1,
                       "is NULL, not 0, 0");
                continue;
            case OP_IfNot:
                qDebug("%d: OP_If ->%d if r(%d) zero%s",
                    i, v->aOp[i].p2, v->aOp[i].p1,
                    (v->aOp[i].p3 ? "or NULL" : ""));
                continue;
            case OP_IfNullRow:
                qDebug("%d: OP_IfNullRow %s %d %s r[%d] = NULL, and ->%d",
                     i, "if cursor", v->aOp[i].p1, "is a null row",
                     v->aOp[i].p3, v->aOp[i].p2);
                continue;
            case OP_SeekLT:
                if (v->aOp[i].p2) {
                    qDebug("%d: OP_SeekLT %s %d %s ->%d if none",
                        i, "move cursor", v->aOp[i].p1,
                        " to largest record < key, or", v->aOp[i].p2);
                } else {
                    qDebug("%d: OP_SeekLT %s %d %s",
                        i, "move cursor", v->aOp[i].p1,
                        "to largest record < key if there is one");
                }
                continue;
            case OP_SeekLE:
                if (v->aOp[i].p2) {
                    qDebug("%d: OP_SeekLT %s %d %s ->%d if none",
                        i, "move cursor", v->aOp[i].p1,
                        "to largest record <= key or",
                        v->aOp[i].p2);
                } else {
                    qDebug("%d: OP_SeekLT %s %d %s",
                        i, "move cursor", v->aOp[i].p1,
                        "to largest record <= key if there is one");
                }
                continue;
            case OP_SeekGE:
                if (v->aOp[i].p2) {
                    qDebug("%d: OP_SeekLT %s %d %s ->%d if none",
                        i, "move cursor", v->aOp[i].p1,
                        "to smallest record >= key or", v->aOp[i].p2);
                } else {
                    qDebug("%d: OP_SeekLT %s %d %s",
                        i, "move cursor", v->aOp[i].p1,
                        "to smallest record >= key if there is one");
                }
                continue;
            case OP_SeekGT:
                if (v->aOp[i].p2) {
                    qDebug("%d: OP_SeekLT %s %d %s ->%d if none",
                        i, "move cursor", v->aOp[i].p1,
                        "to smallest record > key or", v->aOp[i].p2);
                } else {
                    qDebug("%d: OP_SeekLT %s %d %s",
                        i, "move cursor", v->aOp[i].p1,
                        "to smallest record > if there is one");
                }
                continue;
            case OP_IfNoHope:
                if (v->aOp[i].p4.i == 1) {
                    qDebug("%d: OP_IfNoHope %s r[%d] %s %d ->%d",
                       i, "if not seekHit and", v->aOp[i].p3,
                       "not in index", v->aOp[i].p1, v->aOp[i].p2);
                } else {
                    qDebug("%d: OP_IfNoHope %s r[%d]..r[%d] %s %d ->%d",
                       i, "if not seekHit and",
                       v->aOp[i].p3, v->aOp[i].p3 + v->aOp[i].p4.i - 1,
                       "not in index", v->aOp[i].p1, v->aOp[i].p2);
                }
                continue;
            case OP_NoConflict:
                if (v->aOp[i].p4.i == 0) {
                    qDebug("%d: %s %d %s %d, ->%d, %s %d %s",
                        i, "OP_NoConflict if record", v->aOp[i].p3,
                        "contains a NULL or does not match any row in index",
                        v->aOp[i].p1, v->aOp[i].p2, "else move", v->aOp[i].p1,
                        "to matching record");
                } else if (v->aOp[i].p4.i == 1) {
                    qDebug("%d: %s r[%d] %s %d, ->%d, %s %d %s",
                        i, "OP_NoConflict if", v->aOp[i].p3,
                        "contains a NULL or does not match any row in index",
                        v->aOp[i].p1, v->aOp[i].p2, "else move", v->aOp[i].p1,
                        "to matching record");
                } else {
                    qDebug("%d: %s r[%d]..r[%d] %s %d, ->%d, %s %d %s",
                        i, "OP_NoConflict if",
                        v->aOp[i].p3, v->aOp[i].p3 + v->aOp[i].p4.i - 1,
                        "contains a NULL or does not match any row in index",
                        v->aOp[i].p1, v->aOp[i].p2, "else move", v->aOp[i].p1,
                        "to matching record");
                }
                continue;
            case OP_NotFound:
                qDebug("%d: OP_NotFound %s ->%d",
                       i, "if record not in index", v->aOp[i].p2);
                continue;
            case OP_Found:
                qDebug("%d: OP_Found %s ->%d",
                       i, "if record is in index", v->aOp[i].p2);
                continue;
            case OP_SeekRowid:
                qDebug("%d: OP_SeekRowid ->%d if r[%d] %s r[%d} not found",
                    i, v->aOp[i].p2, v->aOp[i].p3,
                    "not integer or row", v->aOp[i].p3);
                continue;
            case OP_NotExists:
                qDebug("%d: OP_NotExists->%d if row r[%d] not found",
                    i, v->aOp[i].p2, v->aOp[i].p3);
                continue;
            case OP_Last:
                if (v->aOp[i].p2 > 0) {
                    qDebug("%d: OP_Last ->%d %s",
                        i, v->aOp[i].p2,
                        "if table empty or move to last entry");
                } else {
                    qDebug("%d: OP_Last move to last entry", i);
                }
                continue;
            case OP_IfSmaller:
                qDebug("%d: OP_IfSmaller ->%d if fewer rows than %g",
                    i, v->aOp[i].p2, 0.1 * pow(2, (v->aOp[i].p3)));
                continue;
            case OP_SorterSort:
                qDebug("%d: OP_SorterSort sort or ->%d if no records",
                    i, v->aOp[i].p2);
                continue;
            case OP_Sort:
                qDebug("%d: OP_Sort %s ->%d if no records",
                    i, "move to before first or", v->aOp[i].p2);
                continue;
            case OP_Rewind:
                qDebug("%d: OP_Rewind-%s ->%d if no records",
                    i, "move to before first or", v->aOp[i].p2);
                continue;
            case OP_IdxLE:
                qDebug("%d: OP_IdxLE if item at cursor <= r[%d}...r[%d} ->%d",
                    i, v->aOp[i].p3, v->aOp[i].p3 + v->aOp[i].p4.i - 1,
                    v->aOp[i].p2);
                continue;
            case OP_IdxGT:
                qDebug("%d: OP_IdxGT if item at cursor > r[%d}...r[%d} ->%d",
                    i, v->aOp[i].p3, v->aOp[i].p3 + v->aOp[i].p4.i - 1,
                    v->aOp[i].p2);
                continue;
            case OP_IdxLT:
                qDebug("%d: OP_IdxLT if item at cursor < r[%d}...r[%d} ->%d",
                    i, v->aOp[i].p3, v->aOp[i].p3 + v->aOp[i].p4.i - 1,
                    v->aOp[i].p2);
                continue;
            case OP_IdxGE:
                qDebug("%d: OP_IdxGE if item at cursor >= r[%d}...r[%d} ->%d",
                    i, v->aOp[i].p3, v->aOp[i].p3 + v->aOp[i].p4.i - 1,
                    v->aOp[i].p2);
                continue;
            case OP_RowSetRead:
                qDebug("%d: OP_RowSetRead if no rowids ->%d else r[%d]%s",
                    i, v->aOp[i].p2, v->aOp[i].p3, " = least rowid");
                continue;
            case OP_RowSetTest:
                qDebug("%d: OP_RowSetTest if r[%d] in rowset ->%d%s",
                    i, v->aOp[i].p2, v->aOp[i].p2, " else insert it");
                continue;
            case OP_Or:
                qDebug("%d: OP_Or r[%d] = r[%d] logical OR r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_And:
                qDebug("%d: OP_And r(%d) = r(%d) logical AND r(%d",
                    i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_Program:
                qDebug("%d: OP_Program execute trigger program", i); continue;
            case OP_FkIfZero:
                if (v->aOp[i].p1) {
                    qDebug("%d: OP_FkIfZero %s ->%d",
                           i, "if no deferred constraint violations",
                           v->aOp[i].p2);
                } else {
                    qDebug("%d: OP_FkIfZero %s ->%d",
                           i, "if no immediate constraint violations",
                           v->aOp[i].p2);
                }
                continue;
            case OP_IfPos:
                qDebug("%d: OP_IfPos if r[%d] >0 then r[%d]-=%d and ->%d",
                    i, v->aOp[i].p1, v->aOp[i].p1,
                    v->aOp[i].p3, v->aOp[i].p2);
                continue;
            case OP_IfNotZero:
                qDebug("%d%sr[%d]%sr[%d]-- and ->%d%sr[%d] <0 then ->%d",
                    i, ": OP_IfNotZero if ", v->aOp[i].p1, " >0 then ",
                    v->aOp[i].p1, v->aOp[i].p2, ", else if ", v->aOp[i].p1,
                    v->aOp[i].p2);
                continue;
            case OP_DecrJumpZero:
                qDebug("%d: OP_DecrJumpZero if (--r[%d])==0 ->%d",
                    i, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_IsNull:
                qDebug("%d: OP_IsNull if r[%d]==NULL ->%d",
                    i, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_NotNull:
                qDebug("%d: OP_NotNull if r[%d]!=NULL ->%d",
                    i, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_Ne:
                if (v->aOp[i].p5 & SQLITE_STOREP2) {
                    qDebug("%d: OP_Ne r[%d] = (r[%d3]!=r[%d])",
                        i, v->aOp[i].p2, v->aOp[i].p3, v->aOp[i].p1);
                } else {
                    qDebug("%d: OP_Ne if r[%d3]!=r[%d] ->%d",
                        i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                }
                continue;
            case OP_Eq:
                if (v->aOp[i].p5 & SQLITE_STOREP2) {
                    qDebug("%d: OP_Eq r[%d] = (r[%d3]==r[%d])",
                        i, v->aOp[i].p2, v->aOp[i].p3, v->aOp[i].p1);
                } else {
                    qDebug("%d: OP_Eq if r[%d3]==r[%d] ->%d",
                        i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                }
                continue;
            case OP_Gt:
                if (v->aOp[i].p5 & SQLITE_STOREP2) {
                    qDebug("%d: OP_Gt r[%d] = (r[%d3]>r[%d])",
                        i, v->aOp[i].p2, v->aOp[i].p3, v->aOp[i].p1);
                } else {
                    qDebug("%d: OP_Gt if r[%d3]>r[%d] ->%d",
                        i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                }
                continue;
            case OP_Le:
                if (v->aOp[i].p5 & SQLITE_STOREP2) {
                    qDebug("%d: OP_Le r[%d] = (r[%d3]<=r[%d])",
                        i, v->aOp[i].p2, v->aOp[i].p3, v->aOp[i].p1);
                } else {
                    qDebug("%d: OP_Le if r[%d3]<=r[%d] ->%d",
                        i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                }
                continue;
            case OP_Lt:
                if (v->aOp[i].p5 & SQLITE_STOREP2) {
                    qDebug("%d: OP_Lt r[%d] = (r[%d3]<r[%d])",
                        i, v->aOp[i].p2, v->aOp[i].p3, v->aOp[i].p1);
                } else {
                    qDebug("%d: OP_Lt if r[%d3]<r[%d] ->%d",
                        i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                }
                continue;
            case OP_Ge:
                if (v->aOp[i].p5 & SQLITE_STOREP2) {
                    qDebug("%d: OP_Ge r[%d] = (r[%d3]>=r[%d])",
                        i, v->aOp[i].p2, v->aOp[i].p3, v->aOp[i].p1);
                } else {
                    qDebug("%d: OP_Ge if r[%d3]>=r[%d] ->%d",
                        i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                }
                continue;
            case OP_ElseNotEq:
                qDebug("%d: OP_ElseNotEq if r[%d]!=r[%d] ->%d",
                    i, v->aOp[i-1].p3, v->aOp[i-1].p1, v->aOp[i].p2);
                continue;
            case OP_IncrVacuum:
                qDebug("%d: OP_IncrVacuum %s ->%d",
                    i, "next vacuum step and if finished", v->aOp[i].p2);
                continue;
            case OP_VNext:
                qDebug("%d: OP_VNext go to next row and ->%d if there is one",
                    i, v->aOp[i].p2);
            case OP_Init:
                qDebug("%d: OP_Init increment P1 (not r[p1]), ->%d",
                    i, v->aOp[i].p2);
                continue;
            case OP_PureFunc0:
                qDebug("%d: OP_PureFunc0 call function %s",
                    i, v->aOp[i].p4.pFuncDef->zName);
                continue;
            case OP_Function0:
                qDebug("%d: OP_Function0 call function %s",
                    i, v->aOp[i].p4.pFuncDef->zName);
                continue;
            case OP_PureFunc:
                qDebug("%d: OP_PureFunc call function %s",
                    i, v->aOp[i].p4.pCtx->pFunc->zName);
                continue;
            case OP_Function:
                qDebug("%d: OP_Function call function %s",
                    i, v->aOp[i].p4.pCtx->pFunc->zName);
                continue;
            case OP_Return:
                qDebug("%d: OP_Return ->r[%d]", i, v->aOp[i].p1);
                continue;
            case OP_EndCoroutine:
                qDebug("%d: OP_EndCoroutine ->r[%d].P2", i, v->aOp[i].p1);
                continue;
            case OP_HaltIfNull:
                if (v->aOp[i].p1 != 0) {
                    const char * s = v->aOp[i].p4.z ? v->aOp[i].p4.z : "";
                    switch (v->aOp[i].p2) {
                        case 1:
                            qDebug("%d: OP_HaltIfNull if r[%d]%s%s",
                                i, v->aOp[i].p3,
                                " is NULL then rollback and halt", s);
                            continue;
                        case 2:
                            qDebug("%d: OP_HaltIfNull if r[%d]%s%s",
                                i, v->aOp[i].p3,
                                " is NULL then abort and halt", s);
                            continue;
                        case 3:
                            qDebug("%d: OP_HaltIfNull if r[%d]%s%s",
                                i, v->aOp[i].p3,
                                " is NULL then fail and halt", s);
                            continue;
                    }
                }
                qDebug("%d: OP_HaltIfNull if r[%d] is NULL then halt",
                       i, v->aOp[i].p3);
                continue;
            case OP_Halt:
                if (v->aOp[i].p1 != 0) {
                    const char * s = v->aOp[i].p4.z ? v->aOp[i].p4.z : "";
                    switch (v->aOp[i].p2) {
                        case 1:
                            qDebug("%d: OP_Halt rollback and halt%s", i, s);
                            continue;
                        case 2:
                            qDebug("%d: OP_Halt abort and halt%s", i, s);
                            continue;
                        case 3:
                            qDebug("%d: OP_Halt fail and halt%s", i, s);
                            continue;
                    }
                }
                qDebug("%d: OP_Halt", i);
                continue;
            case OP_Integer:
                qDebug("%d: OP_Integer r[%d] = %d",
                    i, v->aOp[i].p2, v->aOp[i].p1);
                continue;
            case OP_Int64:
                qDebug("%d: OP_Int64 r(%d) = %ld",
                    i, v->aOp[i].p2, *(int64_t*)(v->aOp[i].p4.p));
                continue;
            case OP_String:
                if (v->aOp[i].p3 != 0) {
                    qDebug("%d: OP_String r[%d] = %*s",
                        i, v->aOp[i].p2, v->aOp[i].p1, v->aOp[i].p4.z);
                } else {
                    qDebug("%d: OP_String", i);
                }
                continue;
            case OP_Null:
                if (v->aOp[i].p3 <= v->aOp[i].p2) {
                    qDebug("%d: OP_Null r[%d] = NULL",
                        i, v->aOp[i].p2);
                } else {
                    qDebug("%d: OP_Null r[%d], ... r[%d] = NULL",
                        i, v->aOp[i].p2, v->aOp[i].p3);
                }
                continue;
            case OP_SoftNull:
                qDebug("%d: OP_SoftNull r[%d] = NULL", i, v->aOp[i].p1);
                continue;
            case OP_Blob:
                qDebug("%d: OP_Blob r[%d] = blob from P4", i, v->aOp[i].p2);
                continue;
            case OP_Variable:
                qDebug("%d: OP_Variable r[%d] = bound variable %d",
                    i, v->aOp[i].p2, v->aOp[i].p1);
                continue;
            case OP_Move:
                if (v->aOp[i].p1 == 1) {
                    qDebug("%d: OP_Move r[%d] = r[%d], r[%d] = NULL",
                        i, v->aOp[i].p2, v->aOp[i].p1, v->aOp[i].p1);
                } else {
                    qDebug("%d: OP_Move r[%d], ... r[%d] = r[%d], ... r[%d],"
                        "r[%d], ... r[%d] = NULL",
                        i, v->aOp[i].p2, v->aOp[i].p2 + v->aOp[i].p3 - 1,
                        v->aOp[i].p1, v->aOp[i].p1 + v->aOp[i].p3 - 1,
                        v->aOp[i].p1, v->aOp[i].p1 + v->aOp[i].p3 - 1);
                }
                continue;
            case OP_Copy:
                if (v->aOp[i].p1 == 1) {
                    qDebug("%d: OP_Copy r[%d] = r[%d]",
                        i, v->aOp[i].p2, v->aOp[i].p1);
                } else {
                    qDebug("%d: OP_Copy r[%d], ... r[%d] = r[%d], ... r[%d],",
                        i, v->aOp[i].p2, v->aOp[i].p2 + v->aOp[i].p3 - 1,
                        v->aOp[i].p1, v->aOp[i].p1 + v->aOp[i].p3 - 1);
                }
                continue;
            case OP_SCopy:
                qDebug("%d: OP_SCopy r[%d] = r[%d] (shallow copy)",
                    i, v->aOp[i].p2, v->aOp[i].p1);
                continue;
            case OP_IntCopy:
                qDebug("%d: OP_IntCopy r[%d] = r[%d]",
                    i, v->aOp[i].p2, v->aOp[i].p1);
                continue;
            case OP_ResultRow:
                qDebug("%d: OP_ResultRow %s(r[%d], ... r[%d])",
                    i, "sqlite3_step() returns with row",
                    v->aOp[i].p1, v->aOp[i].p1 + v->aOp[i].p2 - 1);
                continue;
            case OP_CollSeq: qDebug("%d: OP_CollSeq", i); continue;
            case OP_AddImm:
                qDebug("%d: OP_AddImm r[%d] += %d",
                    i, v->aOp[i].p2, v->aOp[i].p1);
                continue;
            case OP_RealAffinity:
                qDebug("%d: OP_RealAffinity Apply real affinity to r[%d]",
                    i, v->aOp[i].p1);
                continue;
            case OP_Cast:
                switch (v->aOp[i].p2) {
                    case 'A':
                        qDebug("%d: OP_Cast force r[%d] to be a real",
                            i, v->aOp[i].p1);
                        continue;
                    case 'B':
                        qDebug("%d: OP_Cast force r[%d] to be a text string",
                            i, v->aOp[i].p1);
                        continue;
                    case 'C':
                        qDebug("%d: OP_Cast force r[%d] to be numeric",
                            i, v->aOp[i].p1);
                        continue;
                    case 'D':
                        qDebug("%d: OP_Cast force r[%d] to be an integer",
                            i, v->aOp[i].p1);
                        continue;
                    case 'E':
                        qDebug("%d: OP_Cast force r[%d] to be a real",
                            i, v->aOp[i].p1);
                        continue;
                }
            case OP_Permutation: qDebug("%d: OP_Permutation", i); continue;
            case OP_Compare:
                if (v->aOp[i].p1 == 1) {
                     qDebug("%d: OP_Compare r[%d] with r[%d]",
                        i, v->aOp[i].p1, v->aOp[i].p2);
                } else {
                     qDebug("%d: OP_Compare r[%d] ... r[%d]"
                            "with r[%d] ... r[%d]%s",
                        i, v->aOp[i].p1, v->aOp[i].p1 + v->aOp[i].p3 - 1,
                        v->aOp[i].p1, v->aOp[i].p1 + v->aOp[i].p3 - 1,
                        (v->aOp[i].p5 & 1 ? " (permuted)" : ""));
                }
                continue;
            case OP_IsTrue:
                if (v->aOp[i].p3 == 0) {
                    if (v->aOp[i].p4.i == 0) {
                        qDebug("%d: OP_IsTrue r[%d} = (r[%d] IS TRUE)",
                               i, v->aOp[i].p2, v->aOp[i].p1);
                    } else {
                        qDebug("%d: OP_IsTrue r[%d} = (r[%d] is NOT TRUE)",
                               i, v->aOp[i].p2, v->aOp[i].p1);
                    }
                } else {
                    if (v->aOp[i].p4.i == 0) {
                        qDebug("%d: OP_IsTrue r[%d} = (r[%d] IS NOT FALSE)",
                               i, v->aOp[i].p2, v->aOp[i].p1);
                    } else {
                        qDebug("%d: OP_IsTrue r[%d} = (r[%d] is FALSE)",
                               i, v->aOp[i].p2, v->aOp[i].p1);
                    }
                }
                continue;
            case OP_Offset:
                qDebug("%d: OP_Offset r[%d] byte offset of record in file",
                       i, v->aOp[i].p3);
                continue;
            case OP_Column:
                qDebug("%d: OP_Column r[%d] = data from column %d",
                       i, v->aOp[i].p3, v->aOp[i].p2);
                continue;
            case OP_Affinity:
                for (j = 0; j < v->aOp[i].p2; ++j) {
                    QString s(i);
                    s.append(": OP_Affinity ");
                    if (j != 0) { s.replace(QRegExp("."), " "); }
                    s.append(" Apply ");
                    switch (v->aOp[i].p4.z[j]) {
                        case 'A': s.append("blob"); break;
                        case 'B': s.append("text"); break;
                        case 'C': s.append("numeric"); break;
                        case 'D': s.append("integer"); break;
                        case 'E': s.append("real"); break;
                    }
                    s.append(" affinity to r[")
                     .append(v->aOp[i].p1 + j)
                     .append("]");
                    dump(s);
                }
                continue;
            case OP_MakeRecord:
                qDebug("%d: OP_MakeRecord r[%d] = record from r[%d] ... r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p1,
                    v->aOp[i].p1 + v->aOp[i].p2);
                continue;
            case OP_Count:
                qDebug("%d: OP_Count Store count of rows in r[%d]",
                       i, v->aOp[i].p2);
                continue;
            case OP_ReadCookie:
                switch (v->aOp[i].p3) {
                    case 1:
                        qDebug("%d: OP_ReadCookie r[%d] = schema version",
                            i, v->aOp[i].p2);
                        break;
                     case 2:
                        qDebug("%d: OP_ReadCookie r[%d] = database format",
                            i, v->aOp[i].p2);
                        break;
                     case 3:
                        qDebug("%d: OP_ReadCookie r[%d] = page cache size",
                            i, v->aOp[i].p2);
                        break;
                     case 4:
                        qDebug("%d: OP_ReadCookie r[%d] = largest root page",
                            i, v->aOp[i].p2);
                        break;
                     case 5:
                        qDebug("%d: OP_ReadCookie r[%d] = text format",
                            i, v->aOp[i].p2);
                        break;
                     case 6:
                        qDebug("%d: OP_ReadCookie r[%d] = user version",
                            i, v->aOp[i].p2);
                        break;
                     case 7:
                        qDebug("%d: OP_ReadCookie r[%d] = incremental vaccum",
                            i, v->aOp[i].p2);
                        continue;
                     case 8:
                        qDebug("%d: OP_ReadCookie r[%d] = application ID",
                            i, v->aOp[i].p2);
                        continue;
                     case 15:
                        qDebug("%d: OP_ReadCookie r[%d] = btree data version",
                            i, v->aOp[i].p2);
                        continue;
                     default:
                        qDebug("%d: OP_ReadCookie r[%d] = cookie[%d]",
                            i, v->aOp[i].p2, v->aOp[i].p3);
                        continue;
                }
            case OP_SetCookie:
                switch (v->aOp[i].p2) {
                    case 1:
                        qDebug("%d: OP_SetCookie schema version = %d",
                            i, v->aOp[i].p3);
                        continue;
                     case 2:
                        qDebug("%d: OP_SetCookie database format = %d",
                            i, v->aOp[i].p3);
                        continue;
                     case 3:
                        qDebug("%d: OP_SetCookie page cache size = %d",
                            i, v->aOp[i].p3);
                        continue;
                     case 4:
                        qDebug("%d: OP_SetCookie largest root page = %d",
                            i, v->aOp[i].p3);
                        continue;
                     case 5:
                        qDebug("%d: OP_SetCookie text format = %d",
                            i, v->aOp[i].p3);
                        continue;
                     case 6:
                        qDebug("%d: OP_SetCookie user version = %d",
                            i, v->aOp[i].p3);
                        continue;
                     case 7:
                        qDebug("%d: OP_SetCookie incremental vacuum = %d",
                            i, v->aOp[i].p3);
                        continue;
                     case 8:
                        qDebug("%d: OP_SetCookie application ID = %d",
                            i, v->aOp[i].p3);
                        continue;
                     case 15:
                        qDebug("%d: OP_SetCookie btree data version = %d",
                            i, v->aOp[i].p3);
                        continue;
                     default:
                        qDebug("%d: OP_SetCookie cookie[%d] = %d",
                            i, v->aOp[i].p2, v->aOp[i].p3);
                        continue;
                }
            case OP_BitAnd:
                qDebug("%d: OP_BitAnd r[%d] = r[%d] & r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_BitOr:
                qDebug("%d: OP_BitOr r[%d] = r[%d] | r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_ShiftLeft:
                qDebug("%d: OP_ShiftLeft r[%d] = r[%d] << r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_ShiftRight:
                qDebug("%d: OP_ShiftRight r[%d] = r[%d] >> r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_Add:
                qDebug("%d: OP_Add r[%d] = r[%d] + r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_Subtract:
                qDebug("%d: OP_Subtract r[%d] = r[%d] - r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p2, v->aOp[i].p1);
                continue;
            case OP_Multiply:
                qDebug("%d: OP_Multiply r[%d] = r[%d] * r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_Divide:
                qDebug("%d: OP_Divide r[%d] = r[%d] / r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p2, v->aOp[i].p1);
                continue;
            case OP_Remainder:
                qDebug("%d: OP_Remainder r[%d] = r[%d] %% r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p2, v->aOp[i].p1);
                continue;
            case OP_Concat:
                qDebug("%d: OP_Concat r[%d] = r[%d] || r[%d]",
                    i, v->aOp[i].p3, v->aOp[i].p2, v->aOp[i].p1);
                continue;
            case OP_ReopenIdx:
            case OP_OpenRead:
                switch (v->aOp[i].p3) {
                    case 0:
                        qDebug("%d: %s %s %d on table %d %s",
                            i, READTYPE, "open read-only cursor",
                            v->aOp[i].p1, v->aOp[i].p2,
                            "of main database if not already open");
                        continue;
                    case 1:
                        qDebug("%d: %s %s %d on table %d %s",
                            i, READTYPE, "open read-only cursor",
                            v->aOp[i].p1, v->aOp[i].p2,
                            "of temp database if not already open");
                        continue;
                    default:
                        qDebug("%d: %s %s %d on table %d %s %d %s",
                            i, READTYPE, "open read-only cursor",
                            v->aOp[i].p1, v->aOp[i].p2,
                            "of attached database", v->aOp[i].p3,
                            "if not already open");
                        continue;
                }
            case OP_BitNot:
                qDebug("%d: OP_BitNot r[%d] = ~ r[%d]",
                    i, v->aOp[i].p2, v->aOp[i].p1);
                continue;
            case OP_OpenWrite: qDebug("%d: OP_OpenWrite", i); continue;
            case OP_String8:
                qDebug("%d: OP_String8 r[%d] = %s",
                    i, v->aOp[i].p2, v->aOp[i].p4.z);
                continue;
            case OP_OpenDup:
                qDebug("%d: OP_OpenDup open new cursor %d %s %d",
                    i, v->aOp[i].p1, "as copy of cursor", v->aOp[i].p2);
                continue;
            case OP_OpenAutoindex: qDebug("%d: OP_OpenAutoindex", i); continue;
            case OP_OpenEphemeral: qDebug("%d: OP_OpenEphemeral", i); continue;                       
            case OP_SorterOpen: qDebug("%d: OP_SorterOpen", i); continue;
            case OP_SequenceTest: qDebug("%d: OP_SequenceTest", i); continue;
            case OP_OpenPseudo:
                qDebug("%d: OP_OpenPseudo cursor %d = row from r[%d]",
                    i, v->aOp[i].p1, v->aOp[i].p2);
                continue;
            case OP_Close: qDebug("%d: OP_Close", i); continue;
            case OP_ColumnsUsed: qDebug("%d: OP_ColumnsUsed", i); continue;
            case OP_SeekHit:
                qDebug("%d: OP_SeekHit seekhit flag = %d", i, v->aOp[i].p2);
                continue;
            case OP_Sequence:
                qDebug("%d: OP_Sequence r[%d] = next seqeunce number",
                    i, v->aOp[i].p2);
                continue;
            case OP_NewRowid:
                if (v->aOp[i].p3 == 0) {
                    qDebug("%d: OP_NewRowid r[%d] = next rowid",
                        i, v->aOp[i].p2);
                } else {
                    qDebug("%d: OP_NewRowid r[%d] %s r[%d] = next rowid",
                        i, v->aOp[i].p2, "= root frame,", v->aOp[i].p3);
                }
                continue;
            case OP_Insert:
                qDebug("%d: OP_Insert Insert row from r[%d] at key r[%d]",
                    i, v->aOp[i].p2, v->aOp[i].p3);
                continue;
            case OP_Delete:
                qDebug("%d: OP_Delete delte current record", i);
                continue;
            case OP_ResetCount: qDebug("%d: OP_ResetCount", i); continue;
            case OP_SorterCompare:
                qDebug("%d: OP_SorterCompare if first %d %s r[%d} %s -> %d",
                    i, v->aOp[i].p4.i, "fields of", v->aOp[i].p3,
                    "do not match current record", v->aOp[i].p2);
                continue;
            case OP_SorterData: qDebug("%d: OP_SorterData", i); continue;
            case OP_RowData:
                qDebug("%d: OP_RowData r[%d] = current row", i, v->aOp[i].p2);
                continue;
            case OP_Rowid:
                qDebug("%d: OP_Rowid r[%d] = current rowid", i, v->aOp[i].p2);
                continue;
            case OP_NullRow: qDebug("%d: OP_NullRow", i); continue;
            case OP_SeekEnd:
                qDebug("%d: OP_SeekEnd move to last row of table", i);
                continue;
            case OP_SorterInsert:
                qDebug("%d: OP_SorterInsert write key r[%d] into sorter %d",
                    i, v->aOp[i].p2, v->aOp[i].p1);
                break;
            case OP_IdxInsert:
                qDebug("%d: OP_IdxInsertwrite key r[%d] %s %d",
                    i, v->aOp[i].p2, "with no data into index", v->aOp[i].p1);
                break;
            case OP_IdxDelete: qDebug("%d: OP_IdxDelete", i); break; /* synopsis: key=r[P2@P3]                     */
            case OP_DeferredSeek: qDebug("%d: OP_DeferredSeek", i); break; /* synopsis: Move P3 to P1.rowid if needed    */
            case OP_IdxRowid: qDebug("%d: OP_IdxRowid", i); break; /* synopsis: r[P2]=rowid                      */
            case OP_Destroy: qDebug("%d: OP_Destroy", i); break;
            case OP_Clear: qDebug("%d: OP_Clear", i); break;
            case OP_ResetSorter: qDebug("%d: OP_ResetSorter", i); break;
            case OP_CreateBtree: qDebug("%d: OP_CreateBtree", i); break; /* synopsis: r[P2]=root iDb=P1 flags=P3       */
            case OP_SqlExec: qDebug("%d: OP_SqlExec", i); break;
            case OP_ParseSchema: qDebug("%d: OP_ParseSchema", i); break;
            case OP_LoadAnalysis: qDebug("%d: OP_LoadAnalysis", i); break;
            case OP_DropTable: qDebug("%d: OP_DropTable", i); break;
            case OP_DropIndex: qDebug("%d: OP_DropIndex", i); break;
            case OP_Real: qDebug("%d: OP_Real", i); break; /* same as TK_FLOAT, synopsis: r[P2]=P4       */
            case OP_DropTrigger: qDebug("%d: OP_DropTrigger", i); break;
            case OP_IntegrityCk: qDebug("%d: OP_IntegrityCk", i); break;
            case OP_RowSetAdd: qDebug("%d: OP_RowSetAdd", i); break; /* synopsis: rowset(P1)=r[P2]                 */
            case OP_Param: qDebug("%d: OP_Param", i); break;
            case OP_FkCounter: qDebug("%d: OP_FkCounter", i); break; /* synopsis: fkctr[P1]+=P2                    */
            case OP_MemMax: qDebug("%d: OP_MemMax", i); break; /* synopsis: r[P1]=max(r[P1],r[P2])           */
            case OP_OffsetLimit: qDebug("%d: OP_OffsetLimit", i); break; /* synopsis: if r[P1]>0 then r[P2]=r[P1]+max(0,r[P3]) else r[P2]=(-1) */
            case OP_AggInverse: qDebug("%d: OP_AggInverse", i); break; /* synopsis: accum=r[P3] inverse(r[P2@P5])    */
            case OP_AggStep: qDebug("%d: OP_AggStep", i); break; /* synopsis: accum=r[P3] step(r[P2@P5])       */
            case OP_AggStep1: qDebug("%d: OP_AggStep1", i); break; /* synopsis: accum=r[P3] step(r[P2@P5])       */
            case OP_AggValue: qDebug("%d: OP_AggValue", i); break; /* synopsis: r[P3]=value N=P2                 */
            case OP_AggFinal: qDebug("%d: OP_AggFinal", i); break; /* synopsis: accum=r[P1] N=P2                 */
            case OP_Expire: qDebug("%d: OP_Expire", i); break;
            case OP_TableLock: qDebug("%d: OP_TableLock", i); break; /* synopsis: iDb=P1 root=P2 write=P3          */
            case OP_VBegin: qDebug("%d: OP_VBegin", i); break;
            case OP_VCreate: qDebug("%d: OP_VCreate", i); break;
            case OP_VDestroy: qDebug("%d: OP_VDestroy", i); break;
            case OP_VOpen: qDebug("%d: OP_VOpen", i); break;
            case OP_VColumn: qDebug("%d: OP_VColumn", i); break; /* synopsis: r[P3]=vcolumn(P2)                */
            case OP_VRename: qDebug("%d: OP_VRename", i); break;
            case OP_Pagecount: qDebug("%d: OP_Pagecount", i); break;
            case OP_MaxPgcnt: qDebug("%d: OP_MaxPgcnt", i); break;
            case OP_Trace: qDebug("%d: OP_Trace", i); break;
            case OP_CursorHint: qDebug("%d: OP_CursorHint", i); break;
            case OP_Noop: qDebug("%d: OP_Noop", i); break;
            case OP_Explain: qDebug("%d: OP_Explain", i); break;
            case OP_Abortable: qDebug("%d: OP_Abortable", i); break;
        }
    }
}
#endif // WANT_VDBE_DECODING
#endif // WANT_PD

