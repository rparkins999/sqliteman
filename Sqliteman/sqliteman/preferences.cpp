/* Copyright © 2007-2009 Petr Vanek and 2015-2024 Richard Parkins
 *
 * For general Sqliteman copyright and licensing information please refer
 * to the COPYING file provided with the program. Following this notice may exist
 * a copyright and/or license notice that predates the release of Sqliteman
 * for which a new license (GPL+exception) is in place.
 */

#include <QtCore/QSettings>
#include <QApplication>
#include <qscilexersql.h>

#include "preferences.h"

/* Rationale:
 * Creating a QSettings object reads in all the settings and creates a map
 * from keys to values: we don't want to do this too often.
 * Reading or writing a value requires a lookup in the map, which is
 * costly if we have many keys but cheaper than reading the whole file
 * (at least if we don't do that every time).
 * So we keep a persistent Preferences instance in Preferences::_instance
 * which contains all the values in member variables. Looking up a value
 * just reads the member variable.
 * Startup is slow anyway because a Settings object must be created the first
 * time we read any preference, but we make it a bit slower by reading all
 * the values from the map at that time.
 * It might be better to keep a persistent QSettings object which would make
 * startup a bit quicker since we only have to read the values that we need
 * to get started. Doing this would be a big change, probably for quite a
 * small gain.
 */

Preferences* Preferences::_instance = 0;

Preferences::Preferences(QObject *parent)
 : QObject(parent)
{
	QFont f(QApplication::font());

	QSettings s("yarpen.cz", "sqliteman");
	m_checkQtVersion = s.value("checkQtVersion", true).toBool();
	// FIXME should we have special label for empty blob
	m_nullHighlight = s.value("prefs/nullCheckBox", true).toBool();
	m_blobHighlight = s.value("prefs/blobCheckBox", true).toBool();
	m_nullHighlightText = s.value("prefs/nullAliasEdit", "{null}").toString();
	m_blobHighlightText = s.value("prefs/blobAliasEdit", "{blob}").toString();
	m_nullHighlightColor = s.value("prefs/nullBgButton", stdLightColor()).value<QColor>();
	m_blobHighlightColor = s.value("prefs/blobBgButton", stdLightColor()).value<QColor>();
	m_recentlyUsedCount = s.value("prefs/recentlyUsedSpinBox", 5).toInt();
	m_openLastDB = s.value("prefs/openLastDB", true).toBool();
	m_openLastSqlFile = s.value("prefs/openLastSqlFile", true).toBool();
	m_readRows = s.value("prefs/readRowsComboBox", 0).toInt();
	m_lastDB = s.value("lastDatabase", QString()).toString();
	m_lastSqlFile = s.value("lastSqlFile", QString()).toString();
    m_extensionDirectory = s.value("extensionDirectory", QString()).toString();
    m_recentFiles = s.value("recentDocs/files").toStringList();
	m_newInItemView = s.value("prefs/openNewInItemView", false).toBool();
    m_prefillNew = s.value("prefs/prefillNew", false).toBool();
	m_GUItranslator = s.value("prefs/languageComboBox", 0).toInt();
	m_GUIstyle = s.value("prefs/styleComboBox", 0).toInt();
	m_GUIfont = s.value("prefs/applicationFont", f).value<QFont>();
	m_cropColumns = s.value("prefs/cropColumnsCheckBox", false).toBool();
	m_sqlFont = s.value("prefs/sqleditor/font", f).value<QFont>();
	m_sqlFontSize = s.value("prefs/sqleditor/fontSize", f.pointSize()).toInt();
	m_activeHighlighting = s.value(
        "prefs/sqleditor/useActiveHighlightCheckBox", true).toBool();
	m_activeHighlightColor = s.value(
        "prefs/sqleditor/activeHighlightButton",
        stdDarkColor()).value<QColor>();
	m_textWidthMark = s.value(
        "prefs/sqleditor/useTextWidthMarkCheckBox", true).toBool();
	m_textWidthMarkSize = s.value(
        "prefs/sqleditor/textWidthMarkSpinBox", 60).toInt();
	m_codeCompletion = s.value(
        "prefs/sqleditor/useCodeCompletion", false).toBool();
	m_codeCompletionLength = s.value(
        "prefs/sqleditor/completionLengthBox", 3).toInt();
	m_useShortcuts = s.value("prefs/sqleditor/useShortcuts", false).toBool();
	m_shortcuts = s.value(
        "prefs/sqleditor/shortcuts", QMap<QString,QVariant>()).toMap();
	// qscintilla
	QsciLexerSQL syntaxLexer;
	m_syDefaultColor = s.value("prefs/qscintilla/syDefaultColor",
							   syntaxLexer.defaultColor(QsciLexerSQL::Default)).value<QColor>();
	m_syKeywordColor = s.value(
        "prefs/qscintilla/syKeywordColor",
        syntaxLexer.defaultColor(QsciLexerSQL::Keyword)).value<QColor>();
	m_syNumberColor = s.value(
        "prefs/qscintilla/syNumberColor",
        syntaxLexer.defaultColor(QsciLexerSQL::Number)).value<QColor>();
	m_syStringColor = s.value(
        "prefs/qscintilla/syStringColor",
        syntaxLexer.defaultColor(
            QsciLexerSQL::SingleQuotedString)).value<QColor>();
	m_syCommentColor = s.value(
        "prefs/qscintilla/syCommentColor",
        syntaxLexer.defaultColor(QsciLexerSQL::Comment)).value<QColor>();
	// data
	m_dateTimeFormat = s.value("data/dateTimeFormat", "MM/dd/yyyy").toString();
	// data export
	m_exportFormat = s.value("dataExport/format", 0).toInt();
	m_exportDestination = s.value("dataExport/destination", 0).toInt();
	m_exportHeaders = s.value("dataExport/headers", true).toBool();
	m_exportEncoding = s.value("dataExport/encoding", "UTF-8").toString();
	m_exportEol = s.value("dataExport/eol", 0).toInt();
    // extensions
    m_allowExtensionLoading = s.value("extensions/allowLoading", true).toBool();
    m_extensionList = s.value("extensions/list", QStringList()).toStringList();
    m_altertableHeight = s.value("altertable/height", 500).toInt();
    m_altertableWidth = s.value("altertable/width", 600).toInt();
    m_altertriggerHeight = s.value("altertrigger/height", 500).toInt();
    m_altertriggerWidth = s.value("altertrigger/width", 600).toInt();
    m_alterviewHeight = s.value("alterview/height", 500).toInt();
    m_alterviewWidth = s.value("alterview/width", 600).toInt();
    m_analyzeHeight = s.value("analyze/height", 500).toInt();
    m_analyzeWidth = s.value("analyze/width", 600).toInt();
    m_constraintsHeight = s.value("constraints/height", 500).toInt();
    m_constraintsWidth = s.value("constraints/width", 600).toInt();
    m_createindexHeight = s.value("createindex/height", 500).toInt();
    m_createindexWidth = s.value("createindex/width", 600).toInt();
    m_createtableHeight = s.value("createtable/height", 500).toInt();
    m_createtableWidth = s.value("createtable/width", 600).toInt();
    m_createtriggerHeight = s.value("createtrigger/height", 500).toInt();
    m_createtriggerWidth = s.value("createtrigger/width", 600).toInt();
    m_createviewHeight = s.value("createview/height", 500).toInt();
    m_createviewWidth = s.value("createview/width", 600).toInt();
    m_dataexportHeight = s.value("dataexport/height", 500).toInt();
    m_dataexportWidth = s.value("dataexport/width", 600).toInt();
    m_dataviewerHeight = s.value("dataviewer/height", 500).toInt();
    m_dataviewerWidth = s.value("dataviewer/width", 600).toInt();
    m_finddialogHeight = s.value("finddialog/height", 500).toInt();
    m_finddialogWidth = s.value("finddialog/width", 600).toInt();
    m_helpHeight = s.value("help/height", 500).toInt();
    m_helpWidth = s.value("help/width", 600).toInt();
    m_importtableHeight = s.value("importtable/height", 500).toInt();
    m_importtableWidth = s.value("importtable/width", 600).toInt();
    m_importtablelogHeight = s.value("importtablelog/height", 500).toInt();
    m_importtablelogWidth = s.value("importtablelog/width", 600).toInt();
    m_litemanwindowHeight = s.value("litemanwindow/height", 500).toInt();
    m_litemanwindowWidth = s.value("litemanwindow/width", 600).toInt();
    m_multieditHeight = s.value("multiedit/height", 500).toInt();
    m_multieditWidth = s.value("multiedit/width", 600).toInt();
    m_populatorHeight = s.value("populator/height", 500).toInt();
    m_populatorWidth = s.value("populator/width", 600).toInt();
    m_preferencesHeight = s.value("preferences/height", 500).toInt();
    m_preferencesWidth = s.value("preferences/width", 600).toInt();
    m_queryeditorHeight = s.value("queryeditor/height", 500).toInt();
    m_queryeditorWidth = s.value("queryeditor/width", 600).toInt();
    m_shortcutHeight = s.value("shortcut/height", 500).toInt();
    m_shortcutWidth = s.value("shortcut/width", 600).toInt();
    m_vacuumHeight = s.value("vacuum/height", 500).toInt();
    m_vacuumWidth = s.value("vacuum/width", 600).toInt();
    m_dataviewerShown = s.value("dataviewer/show", true).toBool();
    m_objectbrowserShown = s.value("objectbrowser/show", true).toBool();
    m_sqleditorShown = s.value("sqleditor/show", true).toBool();
    m_dataviewersplitterState =
        s.value("dataviewer/splitter", QByteArray()).toByteArray();
    m_helpsplitterState = s.value("help/splitter", QByteArray()).toByteArray();
    m_litemansplitterState =
        s.value("window/splitter", QByteArray()).toByteArray();
    m_sqlsplitterState 
        = s.value("sqleditor/splitter", QByteArray()).toByteArray();
    m_sqleditorState = s.value("sqleditorstate", QByteArray()).toByteArray();
}

Preferences::~Preferences()
{
	QSettings settings("yarpen.cz", "sqliteman");
	settings.setValue("checkQtVersion", m_checkQtVersion);
	// lnf
	settings.setValue("prefs/languageComboBox", m_GUItranslator);
	settings.setValue("prefs/styleComboBox", m_GUIstyle);
	settings.setValue("prefs/applicationFont", m_GUIfont);
	settings.setValue("prefs/recentlyUsedSpinBox", m_recentlyUsedCount);
	settings.setValue("prefs/openLastDB", m_openLastDB);
	settings.setValue("prefs/openLastSqlFile", m_openLastSqlFile);
    settings.setValue("lastDatabase", m_lastDB);
    settings.setValue("lastSqlFile", m_lastSqlFile);
    settings.setValue("extensionDirectory", m_extensionDirectory);
	settings.setValue("recentDocs/files", m_recentFiles);
	settings.setValue("prefs/openNewInItemView", m_newInItemView);
    settings.setValue("prefs/prefillNew", m_prefillNew);
	settings.setValue("prefs/readRowsComboBox", m_readRows);
	// data results
	settings.setValue("prefs/nullCheckBox", m_nullHighlight);
	settings.setValue("prefs/nullAliasEdit", m_nullHighlightText);
	settings.setValue("prefs/nullBgButton", m_nullHighlightColor);
	settings.setValue("prefs/blobCheckBox", m_blobHighlight);
	settings.setValue("prefs/blobAliasEdit", m_blobHighlightText);
	settings.setValue("prefs/blobBgButton", m_blobHighlightColor);
	settings.setValue("prefs/cropColumnsCheckBox", m_cropColumns);
	// sql editor
	settings.setValue("prefs/sqleditor/font", m_sqlFont);
    settings.setValue("prefs/sqleditor/fontSize", m_sqlFontSize);
	settings.setValue("prefs/sqleditor/useActiveHighlightCheckBox", m_activeHighlighting);
	settings.setValue("prefs/sqleditor/activeHighlightButton", m_activeHighlightColor);
	settings.setValue("prefs/sqleditor/useTextWidthMarkCheckBox", m_textWidthMark);
	settings.setValue("prefs/sqleditor/textWidthMarkSpinBox", m_textWidthMarkSize);
	settings.setValue("prefs/sqleditor/useCodeCompletion", m_codeCompletion);
	settings.setValue("prefs/sqleditor/completionLengthBox", m_codeCompletionLength);
	settings.setValue("prefs/sqleditor/useShortcuts", m_useShortcuts);
	settings.setValue("prefs/sqleditor/shortcuts", m_shortcuts);
	// qscintilla editor
	settings.setValue("prefs/qscintilla/syDefaultColor", m_syDefaultColor);
	settings.setValue("prefs/qscintilla/syKeywordColor", m_syKeywordColor);
	settings.setValue("prefs/qscintilla/syNumberColor", m_syNumberColor);
	settings.setValue("prefs/qscintilla/syStringColor", m_syStringColor);
	settings.setValue("prefs/qscintilla/syCommentColor", m_syCommentColor);
	//
	settings.setValue("data/dateTimeFormat", m_dateTimeFormat);
	// data export
	settings.setValue("dataExport/format", m_exportFormat);
	settings.setValue("dataExport/destination", m_exportDestination);
	settings.setValue("dataExport/headers", m_exportHeaders);
	settings.setValue("dataExport/encoding", m_exportEncoding);
	settings.setValue("dataExport/eol", m_exportEol);
    // extensions
    settings.setValue("extensions/allowLoading", m_allowExtensionLoading);
    settings.setValue("extensions/list", m_extensionList);
    settings.setValue("altertable/height", m_altertableHeight);
    settings.setValue("altertable/width", m_altertableWidth);
    settings.setValue("altertrigger/height", m_altertriggerHeight);
    settings.setValue("altertrigger/width", m_altertriggerWidth);
    settings.setValue("alterview/height", m_alterviewHeight);
    settings.setValue("alterview/width", m_alterviewWidth);
    settings.setValue("analyze/height", m_analyzeHeight);
    settings.setValue("analyze/width", m_analyzeWidth);
    settings.setValue("constraints/height", m_constraintsHeight);
    settings.setValue("constraints/width", m_constraintsWidth);
    settings.setValue("createindex/height", m_createindexHeight);
    settings.setValue("createindex/width", m_createindexWidth);
    settings.setValue("createtable/height", m_createtableHeight);
    settings.setValue("createtable/width", m_createtableWidth);
    settings.setValue("createtrigger/height", m_createtriggerHeight);
    settings.setValue("createtrigger/width", m_createtriggerWidth);
    settings.setValue("createview/height", m_createviewHeight);
    settings.setValue("createview/width", m_createviewWidth);
    settings.setValue("dataexport/height", m_dataexportHeight);
    settings.setValue("dataexport/width", m_dataexportWidth);
    settings.setValue("dataviewer/height", m_dataviewerHeight);
    settings.setValue("dataviewer/width", m_dataviewerWidth);
    settings.setValue("finddialog/height", m_finddialogHeight);
    settings.setValue("finddialog/width", m_finddialogWidth);
    settings.setValue("help/height", m_helpHeight);
    settings.setValue("help/width", m_helpWidth);
    settings.setValue("importtable/height", m_importtableHeight);
    settings.setValue("importtable/width", m_importtableWidth);
    settings.setValue("importtablelog/height", m_importtablelogHeight);
    settings.setValue("importtablelog/width", m_importtablelogWidth);
    settings.setValue("litemanwindow/height", m_litemanwindowHeight);
    settings.setValue("litemanwindow/width", m_litemanwindowWidth);
    settings.setValue("multiedit/height", m_multieditHeight);
    settings.setValue("multiedit/width", m_multieditWidth);
    settings.setValue("populator/height", m_populatorHeight);
    settings.setValue("populator/width", m_populatorWidth);
    settings.setValue("preferences/height", m_preferencesHeight);
    settings.setValue("preferences/width", m_preferencesWidth);
    settings.setValue("queryeditor/height", m_queryeditorHeight);
    settings.setValue("queryeditor/width", m_queryeditorWidth);
    settings.setValue("shortcut/height", m_shortcutHeight);
    settings.setValue("shortcut/width", m_shortcutWidth);
    settings.setValue("vacuum/height", m_vacuumHeight);
    settings.setValue("vacuum/width", m_vacuumWidth);
    settings.setValue("dataviewer/show", m_dataviewerShown);
    settings.setValue("objectbrowser/show", m_objectbrowserShown);
    settings.setValue("sqleditor/show", m_sqleditorShown);
    settings.setValue("dataviewer/splitter", m_dataviewersplitterState);
    settings.setValue("help/splitter", m_helpsplitterState);
    settings.setValue("window/splitter", m_litemansplitterState);
    settings.setValue("sqleditorstate", m_sqleditorState);
}

Preferences* Preferences::instance()
{
    if (_instance == 0) { _instance = new Preferences(); }
    return _instance;
}

void Preferences::deleteInstance()
{
    if (_instance) { delete _instance; }
    _instance = 0;
}
