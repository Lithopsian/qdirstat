/*
 *   File name: ShowUnpkgFilesDialog.cpp
 *   Summary:	QDirStat "show unpackaged files" dialog
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>

//#include "Qt4Compat.h"

#include "ShowUnpkgFilesDialog.h"
#include "ExistingDir.h"
#include "Settings.h"
#include "SettingsHelpers.h"
#include "Logger.h"
#include "Exception.h"


using namespace QDirStat;


ShowUnpkgFilesDialog::ShowUnpkgFilesDialog( QWidget * parent ):
    QDialog { parent },
    _ui { new Ui::ShowUnpkgFilesDialog }
{
    CHECK_NEW( _ui );
    _ui->setupUi( this );

    _okButton = _ui->buttonBox->button( QDialogButtonBox::Ok );
    CHECK_PTR( _okButton );

    QCompleter * completer = new ExistingDirCompleter( this );
    CHECK_NEW( completer );

    _ui->startingDirComboBox->setCompleter( completer );

    ExistingDirValidator * validator = new ExistingDirValidator( this );
    CHECK_NEW( validator );

    _ui->startingDirComboBox->setValidator( validator );

    QLineEdit * lineEdit = _ui->startingDirComboBox->lineEdit();
    if ( lineEdit )
        lineEdit->setClearButtonEnabled( true );

    connect( validator, SIGNAL( isOk( bool ) ),
	     _okButton, SLOT( setEnabled( bool ) ) );

    connect( this, SIGNAL( accepted() ),
	     this, SLOT( writeSettings() ) );

    QPushButton * resetButton = _ui->buttonBox->button( QDialogButtonBox::RestoreDefaults );
    CHECK_PTR( resetButton );

    connect( resetButton, SIGNAL( clicked() ),
	     this,	  SLOT( restoreDefaults() ) );

    readSettings();
}


ShowUnpkgFilesDialog::~ShowUnpkgFilesDialog()
{
    delete _ui;
}


QString ShowUnpkgFilesDialog::startingDir() const
{
    return result() == QDialog::Accepted ? _ui->startingDirComboBox->currentText() : "";
}


QStringList ShowUnpkgFilesDialog::cleanedLines( QPlainTextEdit *widget ) const
{
    const QString text = widget->toPlainText();
    const QStringList lines  = text.split( '\n', Qt::SkipEmptyParts );
    QStringList result;

    foreach ( QString line, lines )
    {
	line = line.trimmed();

	if ( ! line.isEmpty() )
	    result << line;
    }

    return result;
}


void ShowUnpkgFilesDialog::restoreDefaults()
{
    setValues( UnpkgSettings::defaultSettings() );
}


UnpkgSettings ShowUnpkgFilesDialog::values() const
{
    UnpkgSettings settings( startingDir(), excludeDirs(), ignorePatterns(), crossFilesystems() );
    settings.dump();

    return settings;
}


void ShowUnpkgFilesDialog::setValues( const UnpkgSettings & settings )
{
    // settings.dump();
    _ui->startingDirComboBox->setCurrentText( settings.startingDir() );
    _ui->excludeDirsTextEdit->setPlainText( settings.excludeDirs().join( "\n" ) );
    _ui->ignorePatternsTextEdit->setPlainText( settings.ignorePatterns().join( "\n" ) );
    _ui->crossFilesystemsCheckBox->setChecked( settings.crossFilesystems() );
}


void ShowUnpkgFilesDialog::readSettings()
{
    // logDebug() << Qt::endl;

    setValues( UnpkgSettings() );
    readWindowSettings( this, "ShowUnkpgFilesDialog" );
}


void ShowUnpkgFilesDialog::writeSettings()
{
    // logDebug() << Qt::endl;

    UnpkgSettings settings = values();
    settings.write();

    writeWindowSettings( this, "ShowUnkpgFilesDialog" );
}
