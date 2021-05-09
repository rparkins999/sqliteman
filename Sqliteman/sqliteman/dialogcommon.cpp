/*
 * This file written by and copyright © 2021 Richard P. Parkins, M. A.,
 * and released under the GPL.
 *
 * This is some common code abstracted out of several sqliteman dialogs.
 *
 */

#include <QFontMetrics>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextDocument>

#include "database.h"
#include "dialogcommon.h"

/* contructor */
DialogCommon::DialogCommon(LiteManWindow * parent)
: QDialog((QWidget *)parent)
{
    m_updated = false;
    m_creator = parent;
    m_databaseName = QString();
    m_tableName = QString();

    // SEGV if setResultEdit() isn't called before resultAppend()
    m_resultEdit = NULL;
}

/* destructor - does nothing so far */
DialogCommon::~DialogCommon(){}

/* Initilialiser for resultAppend.
 * Must be called, with <resultEdit> a pointer to its QTextEdit in which to
 * display messages, in the contructor for any derived class which calls
 * resultAppend().
 */
void DialogCommon::setResultEdit(QTextEdit * resultEdit)
{
    m_resultEdit = resultEdit;
    m_resultEdit->setHtml("");
    m_resultEdit->setVisible(false);
}

/* Puts a <text> (usually an error message) into the QTextEdit
 * near the bottom of the dialog, and adjust the size of the QTextEdit
 * to be big enough to contain it.
 * Any dialog that calls resultAppend() must have such a QTextEdit,
 * and must call setResultEdit() with a pointer to its QTextEdit
 * in its contructor.
 */
void DialogCommon::resultAppend(QString text)
{
	m_resultEdit->append(text);
    m_resultEdit->setVisible(true);
    int height = m_resultEdit->document()->size().height();
    m_resultEdit->setFixedHeight(height);
}

/* Runs SQL <statement>:
 * if it succeeds, returns true;
 * if it fails, displays <message>, the SQL error message, and
 * <statement>, using resultAppend, and returns false.
 * As for resultAppend, m_resultEdit must point to a suitable QTextEdit.
 * This function is not useful for queries that are expected to return a table:
 * it is used to run SQL statements for their effects on the database.
 */
bool DialogCommon::execSql(const QString & statement, const QString & message)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
        if (!message.isNull()) {
            QString errtext = message
                            + ":<br/><span style=\" color:#ff0000;\">"
                            + query.lastError().text()
                            + "<br/></span>" + tr("using sql statement:")
                            + "<br/><code>" + statement;
            resultAppend(errtext);
        }
        query.clear();
		return false;
	}
	query.clear();
	return true;
}
