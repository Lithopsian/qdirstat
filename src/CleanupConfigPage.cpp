/*
 *   File name: CleanupConfigPage.h
 *   Summary:	QDirStat configuration dialog classes
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "CleanupConfigPage.h"
#include "CleanupCollection.h"
#include "Cleanup.h"
#include "SettingsHelpers.h"
#include "Logger.h"
#include "Exception.h"


#define DEFAULT_OUTPUT_WINDOW_SHOW_TIMEOUT	500

// This is a mess that became necessary because Qt's moc cannot handle template
// classes. Yes, this is ugly.
#define CLEANUP_CAST(VOID_PTR) (static_cast<Cleanup *>(VOID_PTR))

using namespace QDirStat;


CleanupConfigPage::CleanupConfigPage( QWidget * parent ):
    ListEditor ( parent ),
    _ui { new Ui::CleanupConfigPage },
    _cleanupCollection { nullptr }
{
    CHECK_NEW( _ui );

    _ui->setupUi( this );
    setListWidget( _ui->listWidget );

    setMoveUpButton	 ( _ui->moveUpButton	   );
    setMoveDownButton	 ( _ui->moveDownButton	   );
    setMoveToTopButton	 ( _ui->moveToTopButton	   );
    setMoveToBottomButton( _ui->moveToBottomButton );
    setAddButton	 ( _ui->addButton	   );
    setRemoveButton	 ( _ui->removeButton	   );

    enableEditWidgets( false );

    connect( _ui->titleLineEdit, &QLineEdit::textChanged,
	     this,		 &CleanupConfigPage::titleChanged );
}


CleanupConfigPage::~CleanupConfigPage()
{
    // logDebug() << "CleanupConfigPage destructor" << Qt::endl;

    // Delete the working cleanup clones
    for ( int i = 0; i < listWidget()->count(); ++i )
	delete CLEANUP_CAST( value( listWidget()->item( i ) ) );

    delete _ui;
}


void CleanupConfigPage::setCleanupCollection( CleanupCollection * collection )
{
    _cleanupCollection = collection;
}


void CleanupConfigPage::setup()
{
    fillListWidget();
    _ui->toolBox->setCurrentIndex( 0 );
    updateActions();
}


void CleanupConfigPage::applyChanges()
{
    // logDebug() << Qt::endl;

    // The values for the current cleanup action might have been modified and not yet saved
    save( value( listWidget()->currentItem() ) );

    // Build a list of the working cleanups to write out to the settings file
    CleanupList cleanups;
    for ( int i = 0; i < listWidget()->count(); ++i )
    {
	Cleanup * cleanup = CLEANUP_CAST( value( listWidget()->item( i ) ) );
	cleanups << cleanup;
    }

    // Check if anything changed before writing, just for fun
    for ( int iOld = 0, iNew = 0; iNew < cleanups.size() || iOld <_cleanupCollection->size(); ++iOld, ++iNew )
    {
	// If we ran past the end of either list, or the cleanups don't match ...
	if ( iNew == cleanups.size() ||
	     iOld == _cleanupCollection->size() ||
	     !equal( cleanups.at( iNew ), _cleanupCollection->at( iOld ) ) )
	{
	    _cleanupCollection->writeSettings( cleanups );
	    return;
	}
    }
}


void CleanupConfigPage::discardChanges()
{
    // logDebug() << Qt::endl;

//    listWidget()->clear();
//    _cleanupCollection->clear();
//    _cleanupCollection->addStdCleanups();
//    _cleanupCollection->readSettings();
}


void CleanupConfigPage::fillListWidget()
{
    CHECK_PTR( _cleanupCollection );
    listWidget()->clear();

    for ( const Cleanup * cleanup : _cleanupCollection->cleanupList() )
    {
	// Make a deep copy so the config dialog can work without disturbing the real rules
	Cleanup * newCleanup = new Cleanup( cleanup );

	QListWidgetItem * item = new ListEditorItem( newCleanup->cleanTitle(), newCleanup );
	CHECK_NEW( item );
	listWidget()->addItem( item );
    }

    QListWidgetItem * firstItem = listWidget()->item(0);

    if ( firstItem )
	listWidget()->setCurrentItem( firstItem );
}


void CleanupConfigPage::titleChanged( const QString & newTitle )
{
    QListWidgetItem * currentItem = listWidget()->currentItem();

    if ( currentItem )
    {
	Cleanup * cleanup = CLEANUP_CAST( value( currentItem ) );
	cleanup->setTitle( newTitle );
	currentItem->setText( cleanup->cleanTitle() );
    }
}


void CleanupConfigPage::save( void * value )
{
    Cleanup * cleanup = CLEANUP_CAST( value );
    // logDebug() << cleanup << Qt::endl;

    if ( ! cleanup || updatesLocked() )
	return;

    cleanup->setActive ( _ui->activeGroupBox->isChecked() );
    cleanup->setTitle  ( _ui->titleLineEdit->text()	  );
    cleanup->setCommand( _ui->commandLineEdit->text()	  );

    if ( _ui->shellComboBox->currentText().startsWith( "$SHELL" ) )
	cleanup->setShell( "" );
    else
	cleanup->setShell( _ui->shellComboBox->currentText() );

    cleanup->setRecurse( _ui->recurseCheckBox->isChecked() );
    cleanup->setAskForConfirmation( _ui->askForConfirmationCheckBox->isChecked() );

    const int refreshPolicy = _ui->refreshPolicyComboBox->currentIndex();
    cleanup->setRefreshPolicy( static_cast<Cleanup::RefreshPolicy>( refreshPolicy ) );

    cleanup->setWorksForDir	  ( _ui->worksForDirCheckBox->isChecked()	 );
    cleanup->setWorksForFile	  ( _ui->worksForFilesCheckBox->isChecked()	 );
    cleanup->setWorksForDotEntry  ( _ui->worksForDotEntriesCheckBox->isChecked() );

    const int outputPolicy = _ui->outputWindowPolicyComboBox->currentIndex();
    cleanup->setOutputWindowPolicy( static_cast<Cleanup::OutputWindowPolicy>( outputPolicy ) );

    int timeout = _ui->outputWindowTimeoutSpinBox->value() * 1000;

    if ( timeout == DEFAULT_OUTPUT_WINDOW_SHOW_TIMEOUT ) // FIXME: Get this from OutputWindow
	timeout = 0;

    cleanup->setOutputWindowTimeout( timeout );

    cleanup->setOutputWindowAutoClose( _ui->outputWindowAutoCloseCheckBox->isChecked() );

}


void CleanupConfigPage::enableEditWidgets( bool enable )
{
    _ui->activeGroupBox->setEnabled( enable );
}


void CleanupConfigPage::load( void * value )
{
    Cleanup * cleanup = CLEANUP_CAST( value );
    // logDebug() << cleanup << Qt::endl;

    if ( updatesLocked() )
	return;

    if ( ! cleanup )
    {
	enableEditWidgets( false );

	return;
    }

    enableEditWidgets( true );

    _ui->activeGroupBox->setChecked( cleanup->active() );
    _ui->titleLineEdit->setText( cleanup->title() );
    _ui->icon->setPixmap( cleanup->iconName() );
    _ui->commandLineEdit->setText( cleanup->command() );

    if ( cleanup->shell().isEmpty() )
	_ui->shellComboBox->setCurrentIndex( 0 );
    else
	_ui->shellComboBox->setEditText( cleanup->shell() );

    _ui->recurseCheckBox->setChecked	       ( cleanup->recurse()	       );
    _ui->askForConfirmationCheckBox->setChecked( cleanup->askForConfirmation() );
    _ui->refreshPolicyComboBox->setCurrentIndex( cleanup->refreshPolicy() );
    _ui->worksForDirCheckBox->setChecked       ( cleanup->worksForDir()	       );
    _ui->worksForFilesCheckBox->setChecked     ( cleanup->worksForFile()       );
    _ui->worksForDotEntriesCheckBox->setChecked( cleanup->worksForDotEntry()   );

    _ui->outputWindowPolicyComboBox->setCurrentIndex( cleanup->outputWindowPolicy() );

    int timeout = cleanup->outputWindowTimeout();

    if ( timeout == 0 )
	timeout = DEFAULT_OUTPUT_WINDOW_SHOW_TIMEOUT; // FIXME: Get this from OutputWindow

    _ui->outputWindowTimeoutSpinBox->setValue( timeout / 1000.0 );
    _ui->outputWindowAutoCloseCheckBox->setChecked( cleanup->outputWindowAutoClose() );

}


void * CleanupConfigPage::createValue()
{
    Cleanup * cleanup = new Cleanup();
    CHECK_NEW( cleanup );

    return cleanup;
}


void CleanupConfigPage::removeValue( void * value )
{
    Cleanup * cleanup = CLEANUP_CAST( value );
    CHECK_PTR( cleanup );

    delete cleanup;
}


QString CleanupConfigPage::valueText( void * value )
{
    const Cleanup * cleanup = CLEANUP_CAST( value );
    CHECK_PTR( cleanup );

    return cleanup->cleanTitle();
}


bool CleanupConfigPage::equal( const Cleanup * cleanup1, const Cleanup * cleanup2 ) const
{
    if ( !cleanup1 || !cleanup2 )
        return false;

    if ( cleanup1 == cleanup2 )
        return true;

    if ( cleanup1->active()                != cleanup2->active()                ||
         cleanup1->title()                 != cleanup2->title()                 ||
         cleanup1->command()               != cleanup2->command()               ||
//         cleanup1->iconName()              != cleanup2->iconName()              || // not currently in the
//         cleanup1->shortcut()              != cleanup2->shortcut()              || // config dialog
         cleanup1->recurse()               != cleanup2->recurse()               ||
         cleanup1->askForConfirmation()    != cleanup2->askForConfirmation()    ||
         cleanup1->refreshPolicy()         != cleanup2->refreshPolicy()         ||
         cleanup1->worksForDir()           != cleanup2->worksForDir()           ||
         cleanup1->worksForFile()          != cleanup2->worksForFile()          ||
         cleanup1->worksForDotEntry()      != cleanup2->worksForDotEntry()      ||
         cleanup1->outputWindowPolicy()    != cleanup2->outputWindowPolicy()    ||
         cleanup1->outputWindowTimeout()   != cleanup2->outputWindowTimeout()   ||
         cleanup1->outputWindowAutoClose() != cleanup2->outputWindowAutoClose() ||
         cleanup1->shell()                 != cleanup2->shell() )
    {
        return false;
    }

    return true;
}
