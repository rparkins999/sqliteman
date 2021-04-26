/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef ALTERTRIGGERDIALOG_H
#define ALTERTRIGGERDIALOG_H

#include "createtriggerdialog.h"

class QTreeWidgetItem;

/*! \brief GUI for trigger altering
\author Petr Vanek <petr@scribus.info>
*/
class AlterTriggerDialog : public CreateTriggerDialog // ->DialogCommon->QDialog
{
	Q_OBJECT

	public:
		AlterTriggerDialog(QTreeWidgetItem * item, LiteManWindow * parent = 0);
		~AlterTriggerDialog();

 		bool m_updated;

	private:
		QString m_name;

	private slots:
		void alterButton_clicked();
};

#endif // ALTERTRIGGERDIALOG_H
