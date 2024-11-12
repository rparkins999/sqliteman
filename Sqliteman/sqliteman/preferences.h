/* Copyright © 2007-2009 Petr Vanek and 2015-2024 Richard Parkins
 *
 * For general Sqliteman copyright and licensing information please refer
 * to the COPYING file provided with the program. Following this notice may exist
 * a copyright and/or license notice that predates the release of Sqliteman
 * for which a new license (GPL+exception) is in place.
 */

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QtCore/QByteArray>
#include <QColor>
#include <QFont>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QVariant>

class Preferences : public QObject
{
	Q_OBJECT

	public:
		Preferences(QObject *parent = 0);
		~Preferences();

		static Preferences* instance();
		static void deleteInstance();

		static QColor stdLightColor() { return QColor(255, 254, 205); }
		static QColor stdDarkColor() { return QColor(225, 237, 255); }

		bool checkQtVersion() { return m_checkQtVersion; }

		bool nullHighlight() { return m_nullHighlight; }
		void setNullHighlight(bool v) { m_nullHighlight = v; }

		bool blobHighlight() { return m_blobHighlight; }
		void setBlobHighlight(bool v) { m_blobHighlight = v; }

		QString nullHighlightText() { return m_nullHighlightText; }
		void setNullHighlightText(const QString & v) {
            m_nullHighlightText = v; 
        }

		QString blobHighlightText() { return m_blobHighlightText; }
		void setBlobHighlightText(const QString & v) {
            m_blobHighlightText = v;
        }

		QColor nullHighlightColor() { return m_nullHighlightColor; };
		void setNullHighlightColor(const QColor & v) {
            m_nullHighlightColor = v;
        }

		QColor blobHighlightColor() { return m_blobHighlightColor; }
		void setBlobHighlightColor(const QColor & v) {
            m_blobHighlightColor = v;
        }

		int recentlyUsedCount() { return m_recentlyUsedCount; }
		void setRecentlyUsedCount(int v) { m_recentlyUsedCount = v; }

		bool openLastDB() { return m_openLastDB; }
		bool openLastSqlFile() { return m_openLastSqlFile; }
		void setOpenLastDB(bool v) { m_openLastDB = v; }
		void setOpenLastSqlFile(bool v) { m_openLastSqlFile = v; }
		QString lastDB() { return m_lastDB; }
        void setlastDB(QString v) { m_lastDB = v; }
		QString lastSqlFile() { return m_lastSqlFile; }
        void setlastSqlFile(QString v) { m_lastSqlFile = v; }
        QString extensionDirectory() { return m_extensionDirectory; }
        void setextensionDirectory(QString v) { m_extensionDirectory = v; }
        QStringList recentFiles() { return m_recentFiles; }
        void setrecentFIles(QStringList v) { m_recentFiles = v; }
		
		int rowsToRead() { return m_readRows; }
		void setRowsToRead(int index) { m_readRows = index; }
		
		bool openNewInItemView() { return m_newInItemView; }
		void setOpenNewInItemView(bool v) { m_newInItemView = v; }
		
		bool prefillNew() { return m_prefillNew; }
		void setPrefillNew(bool v) { m_prefillNew = v; }

		int GUItranslator() { return m_GUItranslator; }
		void setGUItranslator(int v) { m_GUItranslator = v; }

		int GUIstyle() { return m_GUIstyle; }
		void setGUIstyle(int v) { m_GUIstyle = v; }

		QFont GUIfont() { return m_GUIfont; }
		void setGUIfont(const QFont & v) { m_GUIfont = v; }

		bool cropColumns() { return m_cropColumns; }
		void setCropColumns(bool v) { m_cropColumns = v; }

		QFont sqlFont() { return m_sqlFont; }
		void setSqlFont(QFont v) { m_sqlFont = v; }

		int sqlFontSize() { return m_sqlFontSize; }
		void setSqlFontSize(int v) { m_sqlFontSize = v; }

		bool activeHighlighting() { return m_activeHighlighting; }
		void setActiveHighlighting(bool v) { m_activeHighlighting = v; }

		QColor activeHighlightColor() { return m_activeHighlightColor; }
		void setActiveHighlightColor(const QColor & v) {
            m_activeHighlightColor = v;
        }

		bool textWidthMark() { return m_textWidthMark; }
		void setTextWidthMark(bool v) { m_textWidthMark = v; }

		int textWidthMarkSize() { return m_textWidthMarkSize; }
		void setTextWidthMarkSize(int v) { m_textWidthMarkSize = v; }

		bool codeCompletion() { return m_codeCompletion; }
		void setCodeCompletion(bool v) { m_codeCompletion = v; }

		int codeCompletionLength() { return m_codeCompletionLength; }
		void setCodeCompletionLength(int v) { m_codeCompletionLength = v; }

		bool useShortcuts() { return m_useShortcuts; }
		void setUseShortcuts(bool v) { m_useShortcuts = v; }

		QMap<QString,QVariant> shortcuts() { return m_shortcuts; }
		void setShortcuts(QMap<QString,QVariant>  v) { m_shortcuts = v; }

		QString dateTimeFormat() { return m_dateTimeFormat; }
		void setDateTimeFormat(const QString & v) { m_dateTimeFormat = v; }

		// data export
		int exportFormat() { return m_exportFormat; }
		void setExportFormat(int v) { m_exportFormat = v; }

		int exportDestination() { return m_exportDestination; }
		void setExportDestination(int v) { m_exportDestination = v; }

		bool exportHeaders() { return m_exportHeaders; }
		void setExportHeaders(bool v) { m_exportHeaders = v; }

		QString exportEncoding() { return m_exportEncoding; }
		void setExportEncoding(const QString & v) { m_exportEncoding = v; }

		int exportEol() { return m_exportEol; }
		void setExportEol(int v) { m_exportEol = v; }

		// qscintilla syntax
		QColor syDefaultColor() { return m_syDefaultColor; }
		void setSyDefaultColor(const QColor & v ) { m_syDefaultColor = v; }

		QColor syKeywordColor() { return m_syKeywordColor; }
		void setSyKeywordColor(const QColor & v ) { m_syKeywordColor = v; }

		QColor syNumberColor() { return m_syNumberColor; }
		void setSyNumberColor(const QColor & v ) { m_syNumberColor = v; }

		QColor syStringColor() { return m_syStringColor; }
		void setSyStringColor(const QColor & v ) { m_syStringColor = v; }

		QColor syCommentColor() { return m_syCommentColor; }
		void setSyCommentColor(const QColor & v ) { m_syCommentColor = v; }

        // extensions
        bool allowExtensionLoading() { return m_allowExtensionLoading; }
        void setAllowExtensionLoading(bool v) { m_allowExtensionLoading = v; }

        QStringList extensionList() { return m_extensionList; }
        void setExtensionList(const QStringList & v) { m_extensionList = v; }

        // window sizes
        int altertableHeight() { return m_altertableHeight; }
        void setaltertableHeight(int v) { m_altertableHeight = v; }
        int altertableWidth() { return m_altertableWidth; }
        void setaltertableWidth(int v) { m_altertableWidth = v; }
        int altertriggerHeight() { return m_altertriggerHeight; }
        void setaltertriggerHeight(int v) { m_altertriggerHeight = v; }
        int altertriggerWidth() { return m_altertriggerWidth; }
        void setaltertriggerWidth(int v) { m_altertriggerWidth = v; }
        int alterviewHeight() { return m_alterviewHeight; }
        void setalterviewHeight(int v) { m_alterviewHeight = v; }
        int alterviewWidth() { return m_alterviewWidth; }
        void setalterviewWidth(int v) { m_alterviewWidth = v; }
        int analyzeHeight() { return m_analyzeHeight; }
        void setanalyzeHeight(int v) { m_analyzeHeight = v; }
        int analyzeWidth() { return m_analyzeWidth; }
        void setanalyzeWidth(int v) { m_analyzeWidth = v; }
        int constraintsHeight() { return m_constraintsHeight; }
        void setconstraintsHeight(int v) { m_constraintsHeight = v; }
        int constraintsWidth() { return m_constraintsWidth; }
        void setconstraintsWidth(int v) { m_constraintsWidth = v; }
        int createindexHeight() { return m_createindexHeight; }
        void setcreateindexHeight(int v) { m_createindexHeight = v; }
        int createindexWidth() { return m_createindexWidth; }
        void setcreateindexWidth(int v) { m_createindexWidth = v; }
        int createtableHeight() { return m_createtableHeight; }
        void setcreatetableHeight(int v) { m_createtableHeight = v; }
        int createtableWidth() { return m_createtableWidth; }
        void setcreatetableWidth(int v) { m_createtableWidth = v; }
        int createtriggerHeight() { return m_createtriggerHeight; }
        void setcreatetriggerHeight(int v) { m_createtriggerHeight = v; }
        int createtriggerWidth() { return m_createtriggerWidth; }
        void setcreatetriggerWidth(int v) { m_createtriggerWidth = v; }
        int createviewHeight() { return m_createviewHeight; }
        void setcreateviewHeight(int v) { m_createviewHeight = v; }
        int createviewWidth() { return m_createviewWidth; }
        void setcreateviewWidth(int v) { m_createviewWidth = v; }
        int dataexportHeight() { return m_dataexportHeight; }
        void setdataexportHeight(int v) { m_dataexportHeight = v; }
        int dataexportWidth() { return m_dataexportWidth; }
        void setdataexportWidth(int v) { m_dataexportWidth = v; }
        int dataviewerHeight() { return m_dataviewerHeight; }
        void setdataviewerHeight(int v) { m_dataviewerHeight = v; }
        int dataviewerWidth() { return m_dataviewerWidth; }
        void setdataviewerWidth(int v) { m_dataviewerWidth = v; }
        int finddialogHeight() { return m_finddialogHeight; }
        void setfinddialogHeight(int v) { m_finddialogHeight = v; }
        int finddialogWidth() { return m_finddialogWidth; }
        void setfinddialogWidth(int v) { m_finddialogWidth = v; }
        int helpHeight() { return m_helpHeight; }
        void sethelpHeight(int v) { m_helpHeight = v; }
        int helpWidth() { return m_helpWidth; }
        void sethelpWidth(int v) { m_helpWidth = v; }
        int importtableHeight() { return m_importtableHeight; }
        void setimporttableHeight(int v) { m_importtableHeight = v; }
        int importtableWidth() { return m_importtableWidth; }
        void setimporttableWidth(int v) { m_importtableWidth = v; }
        int importtablelogHeight() { return m_importtablelogHeight; }
        void setimporttablelogHeight(int v) { m_importtablelogHeight = v; }
        int importtablelogWidth() { return m_importtablelogWidth; }
        void setimporttablelogWidth(int v) { m_importtablelogWidth = v; }
        int litemanwindowHeight() { return m_litemanwindowHeight; }
        void setlitemanwindowHeight(int v) { m_litemanwindowHeight = v; }
        int litemanwindowWidth() { return m_litemanwindowWidth; }
        void setlitemanwindowWidth(int v) { m_litemanwindowWidth = v; }
        int multieditHeight() { return m_multieditHeight; }
        void setmultieditHeight(int v) { m_multieditHeight = v; }
        int multieditWidth() { return m_multieditWidth; }
        void setmultieditWidth(int v) { m_multieditWidth = v; }
        int populatorHeight() { return m_populatorHeight; }
        void setpopulatorHeight(int v) { m_populatorHeight = v; }
        int populatorWidth() { return m_populatorWidth; }
        void setpopulatorWidth(int v) { m_populatorWidth = v; }
        int preferencesHeight() { return m_preferencesHeight; }
        void setpreferencesHeight(int v) { m_preferencesHeight = v; }
        int preferencesWidth() { return m_preferencesWidth; }
        void setpreferencesWidth(int v) { m_preferencesWidth = v; }
        int queryeditorHeight() { return m_queryeditorHeight; }
        void setqueryeditorHeight(int v) { m_queryeditorHeight = v; }
        int queryeditorWidth() { return m_queryeditorWidth; }
        void setqueryeditorWidth(int v) { m_queryeditorWidth = v; }
        int shortcutHeight() { return m_shortcutHeight; }
        void setshortcutHeight(int v) { m_shortcutHeight = v; }
        int shortcutWidth() { return m_shortcutWidth; }
        void setshortcutWidth(int v) { m_shortcutWidth = v; }
        int vacuumHeight() { return m_vacuumHeight; }
        void setvacuumHeight(int v) { m_vacuumHeight = v; }
        int vacuumWidth() { return m_vacuumWidth; }
        void setvacuumWidth(int v) { m_vacuumWidth = v; }
        bool dataviewerShown() { return m_dataviewerShown; }
        void setdataviewerShown(bool v) { m_dataviewerShown = v; }
        bool objectbrowserShown() { return m_objectbrowserShown; }
        void setobjectbrowserShown(bool v) { m_objectbrowserShown = v; }
        bool sqleditorShown() { return m_sqleditorShown; }
        void setsqleditorShown(bool v) { m_sqleditorShown = v; }
        QByteArray dataviewersplitter() { return m_dataviewersplitterState; }
        void setdataviewersplitter(QByteArray v) {
            m_dataviewersplitterState = v;
        }
        QByteArray helpsplitter() { return m_helpsplitterState; }
        void sethelpsplitter(QByteArray v) { m_helpsplitterState = v; }
        QByteArray litemansplitter() { return m_litemansplitterState; }
        void setlitemansplitter(QByteArray v) { m_litemansplitterState = v; }
        QByteArray sqlsplitter() { return m_sqlsplitterState; }
        void setsqlsplitter(QByteArray v) { m_sqlsplitterState = v; }
        QByteArray sqleditorState() { return m_sqleditorState; }
        void setsqleditorState(QByteArray v) { m_sqleditorState = v; }

	signals:
		void prefsChanged();

	private:
		/*! \brief The only instance of PrefsManager available.
		Preferences is singleton and the instance can be queried with the method
		instance().
		*/
		static Preferences* _instance;

		bool m_checkQtVersion;
		bool m_nullHighlight;
		bool m_blobHighlight;
		QString m_nullHighlightText;
		QString m_blobHighlightText;
		QColor m_nullHighlightColor;
		QColor m_blobHighlightColor;
		int m_recentlyUsedCount;
		bool m_openLastDB;
		bool m_openLastSqlFile;
		int m_readRows;
		QString m_lastDB;
        QString m_lastSqlFile;
        QString m_extensionDirectory;
        QStringList m_recentFiles;
		bool m_newInItemView;
        bool m_prefillNew;
		int m_GUItranslator;
		int m_GUIstyle;
		QFont m_GUIfont;
		bool m_cropColumns;
		QFont m_sqlFont;
		int m_sqlFontSize;
		bool m_activeHighlighting;
		QColor m_activeHighlightColor;
		bool m_textWidthMark;
		int m_textWidthMarkSize;
		bool m_codeCompletion;
		int m_codeCompletionLength;
		bool m_useShortcuts;
		QMap<QString,QVariant> m_shortcuts;
		// qscintilla syntax
		QColor m_syDefaultColor;
		QColor m_syKeywordColor;
		QColor m_syNumberColor;
		QColor m_syStringColor;
		QColor m_syCommentColor;
		// data export
		int m_exportFormat;
		int m_exportDestination;
		bool m_exportHeaders;
		QString m_exportEncoding;
		int m_exportEol;
        // extensions
        bool m_allowExtensionLoading;
        QStringList m_extensionList;
        int m_altertableHeight;
        int m_altertableWidth;
        int m_altertriggerHeight;
        int m_altertriggerWidth;
        int m_alterviewHeight;
        int m_alterviewWidth;
        int m_analyzeHeight;
        int m_analyzeWidth;
        int m_constraintsHeight;
        int m_constraintsWidth;
        int m_createindexHeight;
        int m_createindexWidth;
        int m_createtableHeight;
        int m_createtableWidth;
        int m_createtriggerHeight;
        int m_createtriggerWidth;
        int m_createviewHeight;
        int m_createviewWidth;
        int m_dataexportHeight;
        int m_dataexportWidth;
        int m_dataviewerHeight;
        int m_dataviewerWidth;
        int m_finddialogHeight;
        int m_finddialogWidth;
        int m_helpHeight;
        int m_helpWidth;
        int m_importtableHeight;
        int m_importtableWidth;
        int m_importtablelogHeight;
        int m_importtablelogWidth;
        int m_litemanwindowHeight;
        int m_litemanwindowWidth;
        int m_multieditHeight;
        int m_multieditWidth;
        int m_populatorHeight;
        int m_populatorWidth;
        int m_preferencesHeight;
        int m_preferencesWidth;
        int m_queryeditorHeight;
        int m_queryeditorWidth;
        int m_shortcutHeight;
        int m_shortcutWidth;
        int m_vacuumHeight;
        int m_vacuumWidth;
        bool m_dataviewerShown;
        bool m_objectbrowserShown;
        bool m_sqleditorShown;
        QByteArray m_dataviewersplitterState;
        QByteArray m_helpsplitterState;
        QByteArray m_litemansplitterState;
        QByteArray m_sqlsplitterState;
        QByteArray m_sqleditorState;

		// used in MultieditDialog
		QString m_dateTimeFormat;
};

#endif
