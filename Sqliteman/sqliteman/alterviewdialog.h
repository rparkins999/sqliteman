/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef ALTERVIEWDIALOG_H
#define ALTERVIEWDIALOG_H

#include "dialogcommon.h"
#include "ui_alterviewdialog.h"

class LiteManWindow;

/*! \brief GUI for view altering
\author Petr Vanek <petr@scribus.info>
*/
class AlterViewDialog : public DialogCommon //->QDialog
{
	Q_OBJECT

	public:
		AlterViewDialog(const QString & name,
						const QString & schema, LiteManWindow * parent = 0);
		~AlterViewDialog();

		bool m_updated;
		void setText(const QString & text) { ui.sqlEdit->setText(text); };

	private:
		Ui::AlterViewDialog ui;
		
	private slots:
		void createButton_clicked();
};

#endif
