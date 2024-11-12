/* Copyright © 2007-2009 Petr Vanek and 2015-2024 Richard Parkins
 *
 * For general Sqliteman copyright and licensing information please refer
 * to the COPYING file provided with the program. Following this notice may exist
 * a copyright and/or license notice that predates the release of Sqliteman
 * for which a new license (GPL+exception) is in place.
 */

#include <QComboBox>
#include <QLineEdit>

#include "termstabwidget.h"
#include "utils.h"

TermsTabWidget::TermsTabWidget(QWidget * parent): QWidget(parent)
{
	ui.setupUi(this);
    m_noTermsAllowed = true; // for all except FindDialog
	ui.termsTable->setColumnCount(3);
	ui.termsTable->horizontalHeader()->hide();
	ui.termsTable->verticalHeader()->hide();

    // needed because the LineEdit's frame has vanished
	ui.termsTable->setShowGrid(true);

	connect(ui.termMoreButton, SIGNAL(clicked()),
		this, SLOT(moreTerms()));
	connect(ui.termLessButton, SIGNAL(clicked()),
		this, SLOT(lessTerms()));
}

void TermsTabWidget::allowNoTerms(bool allowed) { m_noTermsAllowed = allowed; }

void TermsTabWidget::paintEvent(QPaintEvent * event)
{
    // Force at least one term for find dialog,
    // We couldn't do this in our creator becuse m_noTermsAllowed
    // and m_columnList weren't initialised yet.
    if ((ui.termsTable->rowCount() == 0) && !m_noTermsAllowed)
    {
        moreTerms();
        emit firstTerm();
    }
	Utils::setColumnWidths(ui.termsTable);
	QWidget::paintEvent(event); // now call the superclass to do the paint
}

void TermsTabWidget::moreTerms()
{
	int i = ui.termsTable->rowCount();
	ui.termsTable->setRowCount(i + 1);
	QComboBox * fields = new QComboBox();
	fields->addItems(m_columnList);
	ui.termsTable->setCellWidget(i, 0, fields);
	QComboBox * relations = new QComboBox();
	relations->addItems(QStringList() << tr("Contains") << tr("Doesn't contain")
									  << tr("Starts with")
									  << tr("Equals") << tr("Not equals")
									  << tr("Bigger than") << tr("Smaller than")
									  << tr("Is null") << tr("Is not null")
									  << tr("Is empty") << tr("Is not empty"));
	ui.termsTable->setCellWidget(i, 1, relations);
	connect(relations, SIGNAL(currentIndexChanged(const QString &)),
			this, SLOT(relationsIndexChanged(const QString &)));
	QLineEdit * value = new QLineEdit();
	ui.termsTable->setCellWidget(i, 2, value);
	ui.termsTable->resizeColumnsToContents();
	ui.termLessButton->setEnabled((i > 0) || m_noTermsAllowed);
	update();
}

void TermsTabWidget::lessTerms()
{
	int i = ui.termsTable->rowCount() - 1;
	ui.termsTable->removeRow(i);
	ui.termsTable->resizeColumnsToContents();
	ui.termLessButton->setEnabled((i > 1) || m_noTermsAllowed);
	update();
}

void TermsTabWidget::relationsIndexChanged(const QString &)
{
	QComboBox * relations = qobject_cast<QComboBox *>(sender());
	for (int i = 0; i < ui.termsTable->rowCount(); ++i)
	{
		if (relations == ui.termsTable->cellWidget(i, 1))
		{
			switch (relations->currentIndex())
			{
				case 0: // Contains
				case 1: // Doesn't contain
				case 2: // Starts with
				case 3: // Equals
				case 4: // Not equals
				case 5: // Bigger than
				case 6: // Smaller than
					if (!(ui.termsTable->cellWidget(i, 2)))
					{
						ui.termsTable->setCellWidget(i, 2, new QLineEdit());
					}
					break;

				case 7: // is null
				case 8: // is not null
                case 9: // is empty
                case 10: //is not empty
					ui.termsTable->removeCellWidget(i, 2);
					break;
			}
		}
	}
    ui.termsTable->resizeColumnsToContents();
    update();
}
