/* Copyright © Richard Parkins 2021
 *
 * This code is released under the GNU public Licence.
 * 
 * This is the code which implements a pd macro for gdb.
 * It enables gdb to print Qt classes, in particular QString.
 * The gdb macro is created by including the following code in your ,gdbinit:
 *
define pd
call pd::dump($arg0)
end
set unwindonsignal on # recover from errors in the dumper
 */

#include "pd.h"

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
                return "<QVariant with no type<";
            } else {
                return QString("<QVariant of type %1>").arg(typeName);
            }
        }
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
	(box == NULL) ? dump(*box) : qDebug("<Null QComboBox>");
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
void pd::dump(QItemSelection selection) {
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
    char * text0 = prepareString(item.text(0)).toUtf8().data();
    char * text1 = prepareString(item.text(1)).toUtf8().data();
    QString tip0(item.toolTip(0));
    QString tip1(item.toolTip(1));
    if (tip0.isNull()) {
        if (tip1.isNull()) {
            qDebug("text(0) %s, text(1) %s", text0, text1);
        } else {
            qDebug("text(0) %s, text(1) %s, toolTip %s",
                   text0, text1, tip1.toUtf8().data());
        }
    } else {
        if (tip1.isNull()) {
            qDebug("text(0) %s, toolTip %s, text(1) %s",
                   text0, tip0.toUtf8().data(), text1);
        } else {
            qDebug("text(0) %s, toolTip %s, text(1) %s, toolTip %s",
                   text0, tip0.toUtf8().data(), text1, tip1.toUtf8().data());
        }
    }
}
void pd::dump(QTreeWidgetItem * item) {
	(item != NULL) ?
        dump(*item) : qDebug("Null QTreeWidgetItem");
}

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
// The following are for sqliteman's own application types
void pd::dump(enum tokenType t) { dump(preparetokenType(t)); }
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
void pd::dump(enum exprType t) { dump(prepareexprType(t)); }
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
