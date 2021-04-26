/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef CREATETRIGGERDIALOG_H
#define CREATETRIGGERDIALOG_H

#include <qwidget.h>

#include "dialogcommon.h"
#include "ui_createtriggerdialog.h"

class QTreeWidgetItem;

/*! \brief GUI for trigger creation
\author Petr Vanek <petr@scribus.info>
*/
class CreateTriggerDialog : public DialogCommon // ->QDialog
{
	Q_OBJECT

	public:
		CreateTriggerDialog(QTreeWidgetItem * item, LiteManWindow * parent = 0);
		~CreateTriggerDialog();

		bool m_updated;

	protected:
		Ui::CreateTriggerDialog ui;

	private slots:
		void createButton_clicked();
};

#endif
