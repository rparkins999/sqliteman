/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QtCore/QFileInfo>
#include <QFileDialog>
#include <QMessageBox>

#include "multieditdialog.h"
#include "preferences.h"


MultiEditDialog::MultiEditDialog(QWidget * parent)
	: QDialog(parent)
{
	setupUi(this);

	connect(textEdit, SIGNAL(textChanged()),
			this, SLOT(textEdit_textChanged()));
	connect(blobFileEdit, SIGNAL(textChanged(const QString &)),
			this, SLOT(blobFileEdit_textChanged(const QString &)));
	connect(tabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(tabWidget_currentChanged(int)));
	connect(blobFileButton, SIGNAL(clicked()),
			this, SLOT(blobFileButton_clicked()));
	connect(blobRemoveButton, SIGNAL(clicked()),
			this, SLOT(blobRemoveButton_clicked()));
	connect(blobSaveButton, SIGNAL(clicked()),
			this, SLOT(blobSaveButton_clicked()));
	connect(nullCheckBox, SIGNAL(stateChanged(int)),
			this, SLOT(nullCheckBox_stateChanged(int)));
    Preferences * prefs = Preferences::instance();
	resize(prefs->multieditWidth(), prefs->multieditHeight());
}

MultiEditDialog::~MultiEditDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setmultieditHeight(height());
    prefs->setmultieditWidth(width());
}

void MultiEditDialog::setData(const QVariant & data)
{
	m_data = data;
	textEdit->setPlainText(data.toString());
	dateFormatEdit->setText(Preferences::instance()->dateTimeFormat());
	dateTimeEdit->setDate(QDateTime::currentDateTime().date());
	blobPreviewLabel->setBlobData(data);

	// Prevent possible text related modification of BLOBs.
	if (data.type() == QVariant::ByteArray)
	{
		blobRemoveButton->setEnabled(true);
		blobSaveButton->setEnabled(true);
		tabWidget->setTabEnabled(0, false);
		tabWidget->setCurrentIndex(1);
	}
	else
	{
		blobRemoveButton->setEnabled(false);
		blobSaveButton->setEnabled(false);
		tabWidget->setTabEnabled(0, true);
		tabWidget->setCurrentIndex(0);
	}
	m_edited = false;
	checkButtonStatus();
}

QVariant MultiEditDialog::data()
{
	QVariant ret;
	if (!(nullCheckBox->isChecked()))
	{
		switch (tabWidget->currentIndex())
		{
			// handle text with EOLs
			case 0:
				ret = textEdit->toPlainText();
				break;
			// handle File2BLOB
			case 1:
			{
				QString s = blobFileEdit->text();
				if (s.isEmpty())
				{
					if (m_data.type() == QVariant::ByteArray)
					{
						// We already have a blob, but we don't know its filename
						ret = m_data;
					}
				}
				else
				{
					QFile f(s);
					if (f.open(QIODevice::ReadOnly))
						ret = QVariant(f.readAll());
				}
				break;
			}
			// handle DateTime to string
			case 2:
				Preferences::instance()
					->setDateTimeFormat(dateFormatEdit->text());
				ret =  dateTimeEdit->dateTime().toString(dateFormatEdit->text());
				break;
		}
	}
	return ret;
}

void MultiEditDialog::blobFileButton_clicked()
{
	QString fileName = QFileDialog::getOpenFileName(this,
													tr("Open File"),
													blobFileEdit->text(),
													tr("All Files (* *.*)"));
	if (!fileName.isNull())
	{
		blobFileEdit->setText(fileName);
		blobPreviewLabel->setBlobFromFile(fileName);
		checkButtonStatus();
	}
}

void MultiEditDialog::blobRemoveButton_clicked()
{
	setData(QVariant());
	m_edited = true;
	checkButtonStatus();
}

void MultiEditDialog::blobSaveButton_clicked()
{
	QString fileName = QFileDialog::getSaveFileName(this,
													tr("Open File"),
			   										blobFileEdit->text(),
													tr("All Files (* *.*)"));
	if (fileName.isNull())
		return;
	QFile f(fileName);
	if (!f.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, tr("BLOB Save Error"),
							 tr("Cannot open file %1 for writting").arg(fileName));
		return;
	}
	if (f.write(m_data.toByteArray()) == -1)
	{
		QMessageBox::warning(this, tr("BLOB Save Error"),
							 tr("Cannot write into file %1").arg(fileName));
		return;
	}
	f.close();
}

void MultiEditDialog::textEdit_textChanged()
{
	m_edited = true;
	checkButtonStatus();
}

void MultiEditDialog::blobFileEdit_textChanged(const QString &)
{
	checkButtonStatus();
}

void MultiEditDialog::tabWidget_currentChanged(int)
{
	checkButtonStatus();
}

void MultiEditDialog::checkButtonStatus()
{
	bool e = true;
	nullCheckBox->setDisabled(m_data.isNull());
	if (!(nullCheckBox->isEnabled() && nullCheckBox->isChecked()))
	{
		switch (tabWidget->currentIndex())
		{
			case 0:
				e = m_edited;
				break;
			case 1:
			{
				QString text(blobFileEdit->text().simplified());
				if (text.isNull() || text.isEmpty() || !QFileInfo(text).isFile())
					e = false;
				break;
			}
			case 2:
				break;
		}
	}
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(e);
}

void MultiEditDialog::nullCheckBox_stateChanged(int)
{
	tabWidget->setDisabled(nullCheckBox->isChecked());
	checkButtonStatus();
}
