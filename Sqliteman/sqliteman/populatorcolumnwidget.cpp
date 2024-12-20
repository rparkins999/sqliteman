/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include "populatorcolumnwidget.h"


PopulatorColumnWidget::PopulatorColumnWidget(Populator::PopColumn column,
											QWidget * parent)
	: QWidget(parent),
	  m_column(column)
{
	setupUi(this);

	m_column.action = defaultSuggestion();
//	actionCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	actionCombo->setCurrentIndex(m_column.action);
	actionCombo_currentIndexChanged(actionCombo->currentIndex());

	connect(actionCombo, SIGNAL(currentIndexChanged(int)),
			this, SLOT(actionCombo_currentIndexChanged(int)));
	connect(specEdit, SIGNAL(textChanged(const QString &)),
			this, SLOT(specEdit_textChanged(const QString &)));
}

int PopulatorColumnWidget::defaultSuggestion()
{
	QString t(m_column.type);
	t = t.remove(QRegExp("\\(\\d+\\)")).toUpper().simplified();

	if (m_column.pk)
		return Populator::T_AUTO;

	if (t == "BLOB" || t == "CLOB" || t == "LOB")
		return Populator::T_IGNORE;
	else if (t == "INTEGER" || t == "NUMBER")
		return Populator::T_NUMB;
	else if (t == "DATE" || t == "TIME" || t == "DATETIME" || t == "TIMESTAMP")
		return Populator::T_DT_RAND;
	else
		return Populator::T_TEXT;
}

void PopulatorColumnWidget::actionCombo_currentIndexChanged(int ix)
{
	bool enable = true;
	switch (ix)
	{
		case Populator::T_AUTO:
		case Populator::T_NUMB:
		case Populator::T_TEXT:
		case Populator::T_DT_NOW:
		case Populator::T_DT_NOW_UNIX:
		case Populator::T_DT_NOW_JULIAN:
		case Populator::T_DT_RAND:
		case Populator::T_DT_RAND_UNIX:
		case Populator::T_DT_RAND_JULIAN:
		case Populator::T_IGNORE:
			enable = false;
			break;
		default:
			break;
	};
	int spacing = QFontMetrics(actionCombo->font()).lineSpacing();
	actionCombo->setMinimumWidth(spacing * 9);
	if (enable)
	{
		specEdit->show();
		actionCombo->setMaximumWidth(spacing * 16);
	}
	else
	{
		specEdit->hide();
		actionCombo->setMaximumWidth(1677215);
	}

	m_column.action = ix;

	emit actionTypeChanged();
}

void PopulatorColumnWidget::specEdit_textChanged(const QString & t)
{
	m_column.userValue = t;
}
