/* Copyright © 2007-2009 Petr Vanek and 2015-2025 Richard Parkins
 *
 * For general Sqliteman copyright and licensing information please refer
 * to the COPYING file provided with the program. Following this notice may exist
 * a copyright and/or license notice that predates the release of Sqliteman
 * for which a new license (GPL+exception) is in place.
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

    connect(tabWidget, SIGNAL(currentChanged(int)),
            this, SLOT(tabWidget_currentChanged(int)));
    Preferences * prefs = Preferences::instance();
	resize(prefs->multieditWidth(), prefs->multieditHeight());
}

MultiEditDialog::~MultiEditDialog()
{
    Preferences * prefs = Preferences::instance();
    prefs->setmultieditHeight(height());
    prefs->setmultieditWidth(width());
}

void MultiEditDialog::setData(const QVariant & data, bool editable)
{
	m_data = data;
    m_editable = editable;
	m_edited = false;
	textEdit->setPlainText(data.toString());
    textEdit->setReadOnly(!editable);
	dateFormatEdit->setText(Preferences::instance()->dateTimeFormat());
	dateTimeEdit->setDate(QDateTime::currentDateTime().date());
	blobPreviewLabel->setBlobData(data);
    if (editable) {
        // Allow loading blob from file.
        connect(blobFileEdit, SIGNAL(textChanged(const QString &)),
                this, SLOT(blobFileEdit_textChanged(const QString &)));
        blobFileEdit->setEnabled(true);
        if (data.isNull()) {
            // We disable the "Insert NULL" check box if the data is NULL.
            // If the data is edited, we re-enable it again, but we don't
            // disable it again if the data becomes NULL again. because the
            // user is allowed to change their mind and reset it to NULL.
            nullCheckBox->setEnabled(false);
        } else {
            // Allow replacement of non-NULL content with NULL
            nullCheckBox->setEnabled(true);
            connect(nullCheckBox, SIGNAL(stateChanged(int)),
                    this, SLOT(nullCheckBox_stateChanged(int)));
        }
    } else {
        // If not editable, user can't change anything.
        nullCheckBox->setEnabled(false);
    }
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
        m_edited = true;
        // Allow user to go back to NULL
        nullCheckBox->setEnabled(true);
        connect(nullCheckBox, SIGNAL(stateChanged(int)),
                this, SLOT(nullCheckBox_stateChanged(int)),
                    Qt::UniqueConnection);
		checkButtonStatus();
	}
}

void MultiEditDialog::blobRemoveButton_clicked()
{
	setData(QVariant(), m_editable);
	m_edited = true;
	checkButtonStatus();
}

void MultiEditDialog::blobSaveButton_clicked()
{
	QString fileName = QFileDialog::getSaveFileName(this,
													tr("Open File"),
			   										blobFileEdit->text(),
													tr("All Files (* *.*)"));
	if (fileName.isNull()) { return; }
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
    // Allow user to go back to NULL
    nullCheckBox->setEnabled(true);
    connect(nullCheckBox, SIGNAL(stateChanged(int)),
            this, SLOT(nullCheckBox_stateChanged(int)),
                Qt::UniqueConnection);
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
    // If user has asked to insert a NULL,
    // they can't do anything other than change their mind about it.
	if (nullCheckBox->isChecked())
    {
        tabWidget->setEnabled(false);
    } else {
        tabWidget->setEnabled(true);
    }
    if (m_data.type() == QVariant::ByteArray) // blob
    {
        // Prevent possible text related modification of BLOBs.
        tabWidget->setTabEnabled(0, false);
        tabWidget->setTabEnabled(1, true);
        tabWidget->setTabEnabled(2, false);
        tabWidget->setCurrentIndex(1);
        // Allow saving blob to file even if not editable
        connect(blobSaveButton, SIGNAL(clicked()),
                this, SLOT(blobSaveButton_clicked()),
                Qt::UniqueConnection);
        blobSaveButton->setEnabled(true);
    } else {
        tabWidget->setTabEnabled(0, true);
        // Can load a blob only if editable
        tabWidget->setTabEnabled(1, m_editable);
        // Date formatter is only useful if editable
        tabWidget->setTabEnabled(2, m_editable);
        tabWidget->setCurrentIndex(0);
        // If it isn't a blob, user can't save it
        disconnect(blobSaveButton, SIGNAL(clicked()),
                this, SLOT(blobSaveButton_clicked()));
        blobSaveButton->setEnabled(false);
    }
	if (m_editable) {
        if (m_data.type() == QVariant::ByteArray) // blob
        {
            disconnect(textEdit, SIGNAL(textChanged()),
                    this, SLOT(textEdit_textChanged()));
            textEdit->setEnabled(false);
            // Allow blob to be removed
            connect(blobRemoveButton, SIGNAL(clicked()),
                    this, SLOT(blobRemoveButton_clicked()),
                    Qt::UniqueConnection);
            blobRemoveButton->setEnabled(true);
        }
        else // non-blobs are treated like text
        {
            connect(textEdit, SIGNAL(textChanged()),
                    this, SLOT(textEdit_textChanged()),
                    Qt::UniqueConnection);
            textEdit->setEnabled(true);
            // We don't have a blob to remove
            disconnect(blobRemoveButton, SIGNAL(clicked()),
                    this, SLOT(blobRemoveButton_clicked()));
            blobRemoveButton->setEnabled(false);
        }
    }
    // If data isn't editable, we don't need to disconnect signals
    // because they were never connected.
    // Note this could change if we recycle this dialog instead of
    // creating it when needed.

    // If not editable, OK and Cancel do the same thing:
    // if editable, OK is disabled until some editing is done.
	bool e = true;
    if (m_editable) {
        // Inserting a NULL is only allowed if the original data was not NULL,
        // and in that case it's always a change
        if (!nullCheckBox->isChecked())
        {
            switch (tabWidget->currentIndex())
            {
                case 0: // Text
                case 1: // Blob
                    e = m_edited;
                    //FALLTHRU
                case 2: // Date to String
                    break;
            }
        }
    }
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(e);
}

void MultiEditDialog::nullCheckBox_stateChanged(int)
{
    if (nullCheckBox->isChecked()) { m_edited = true; }
	checkButtonStatus();
}
