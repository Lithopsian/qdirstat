/*
 *   File name: CleanupCollection.cpp
 *   Summary:	QDirStat classes to reclaim disk space
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QMenu>
#include <QToolBar>
#include <QMessageBox>

#include "CleanupCollection.h"
#include "Cleanup.h"
#include "StdCleanup.h"
#include "DirTree.h"
#include "FormatUtil.h"
#include "Settings.h"
#include "SettingsHelpers.h"
#include "SelectionModel.h"
#include "OutputWindow.h"
#include "Refresher.h"
#include "Trash.h"
#include "Logger.h"
#include "Exception.h"

#define MAX_URLS_IN_CONFIRMATION_POPUP 7

using namespace QDirStat;


CleanupCollection::CleanupCollection( SelectionModel * selectionModel,
				      QObject	     * parent ):
    QObject( parent ),
    _selectionModel( selectionModel )
//    _listMover( _cleanupList )
{
    readSettings();

    // Just initialize to show the current status in the log;
    // the contents are cached anyway.

    (void) Cleanup::desktopSpecificApps();

    connect( selectionModel, SIGNAL( selectionChanged() ),
	     this,	     SLOT  ( updateActions()	) );
}


CleanupCollection::~CleanupCollection()
{
//    writeSettings( _cleanupList );
    clear();
}


void CleanupCollection::add( Cleanup * cleanup )
{
    _cleanupList << cleanup;

    connect( cleanup, SIGNAL( triggered() ),
	     this,    SLOT  ( execute()	  ) );

    updateMenusAndToolBars();
}

/*
void CleanupCollection::remove( Cleanup * cleanup )
{
    const int index = indexOf( cleanup );

    if ( index == -1 )
    {
	logError() << "No such cleanup: " << cleanup << Qt::endl;
	return;
    }

    _cleanupList.removeAt( index );
    delete cleanup;

    // No need for updateMenusAndToolBars() since QObject/QWidget will take care of
    // deleted actions all by itself.
}
*/

void CleanupCollection::addStdCleanups()
{
    for ( Cleanup * cleanup : StdCleanup::stdCleanups( this ) )
    {
	CHECK_PTR( cleanup );
	add( cleanup );
    }

    writeSettings( _cleanupList );

    CleanupSettings settings;
    settings.setValue( "StdCleanupsAdded", true );
}


int CleanupCollection::indexOf( Cleanup * cleanup ) const
{
    const int index = _cleanupList.indexOf( cleanup );

    if ( index == -1 )
	logError() << "Cleanup " << cleanup << " is not in this collection" << Qt::endl;

    return index;
}


Cleanup * CleanupCollection::at( int index ) const
{
    if ( index >= 0 && index < _cleanupList.size() )
	return _cleanupList.at( index );
    else
	return nullptr;
}


void CleanupCollection::clear()
{
    qDeleteAll( _cleanupList );
    _cleanupList.clear();

    // No need for updateMenusAndToolBars() since QObject/QWidget will take
    // care of deleted actions all by itself.
}


void CleanupCollection::updateMenusAndToolBars()
{
    updateMenus();
    updateToolBars();
}


void CleanupCollection::updateActions()
{
    const FileInfoSet sel = _selectionModel->selectedItems();

    const bool empty		= sel.isEmpty();
    const bool dirSelected	= sel.containsDir();
    const bool fileSelected	= sel.containsFile();
    const bool pkgSelected      = sel.containsPkg();
    const bool dotEntrySelected = sel.containsDotEntry();
    const bool busy		= sel.containsBusyItem();
    const bool treeBusy		= sel.treeIsBusy();

    for ( Cleanup * cleanup : _cleanupList )
    {
	if ( ! cleanup->active() ||
	     ( treeBusy		&& cleanup->refreshPolicy() != Cleanup::NoRefresh ) ||
	     ( dirSelected	&& ! cleanup->worksForDir() ) ||
	     ( dotEntrySelected	&& ! cleanup->worksForDotEntry() ) ||
	     ( fileSelected	&& ! cleanup->worksForFile() ) ||
             pkgSelected ||
	     busy ||
	     empty )
	{
	    cleanup->setEnabled( false );
	}
	else
	{
	    cleanup->setEnabled( true );
	}
    }
}


void CleanupCollection::updateMenus()
{
    _menus.removeAll( nullptr ); // Remove QPointers that have become invalid

    for ( QMenu * menu : _menus )
    {
	if ( menu )
	{
	    // Remove all Cleanups from this menu
	    for ( QAction * action : menu->actions() )
	    {
		Cleanup * cleanup = dynamic_cast<Cleanup *>( action );

		if ( cleanup )
		    menu->removeAction( cleanup );
	    }

	    // Add the current cleanups in the current order
	    addToMenu( menu );
	}
    }
}


void CleanupCollection::updateToolBars()
{
    _toolBars.removeAll( nullptr ); // Remove QPointers that have become invalid

    for ( QToolBar * toolBar : _toolBars )
    {
	if ( toolBar )
	{
	    // Remove all Cleanups from this tool bar
	    for ( QAction * action : toolBar->actions() )
	    {
		Cleanup * cleanup = dynamic_cast<Cleanup *>( action );

		if ( cleanup )
		    toolBar->removeAction( cleanup );
	    }

	    // Add the current cleanups in the current order
	    addToToolBar( toolBar );
	}
    }
}


void CleanupCollection::execute()
{
    Cleanup * cleanup = qobject_cast<Cleanup *>( sender() );

    if ( ! cleanup )
    {
	logError() << "Wrong sender type: "
		   << sender()->metaObject()->className() << Qt::endl;
	return;
    }

    const FileInfoSet selection = _selectionModel->selectedItems();

    if ( selection.isEmpty() )
    {
	logWarning() << "Nothing selected" << Qt::endl;
	return;
    }

    if ( cleanup->askForConfirmation() && ! confirmation( cleanup, selection ) )
    {
	logDebug() << "User declined confirmation" << Qt::endl;
	return;
    }

    emit startingCleanup( cleanup->cleanTitle() );

    OutputWindow * outputWindow = new OutputWindow( qApp->activeWindow(), cleanup->outputWindowAutoClose() );
    CHECK_NEW( outputWindow );

    switch ( cleanup->outputWindowPolicy() )
    {
	case Cleanup::ShowAlways:
	    outputWindow->show();
	    break;

	case Cleanup::ShowAfterTimeout:
	    outputWindow->showAfterTimeout( cleanup->outputWindowTimeout() );
	    break;

	case Cleanup::ShowIfErrorOutput:
	    outputWindow->setShowOnStderr( true );
	    break;

	case Cleanup::ShowNever:
	    break;
    }

    if ( cleanup->refreshPolicy() == Cleanup::RefreshThis ||
	 cleanup->refreshPolicy() == Cleanup::RefreshParent )
    {
	createRefresher( outputWindow, cleanup->refreshPolicy() == Cleanup::RefreshParent ?
	    Refresher::parents( selection ) :
	    selection );
    }

    connect( outputWindow, SIGNAL( lastProcessFinished( int ) ),
	     this,	   SIGNAL( cleanupFinished    ( int ) ) );


    // Intentionally not using the normalized FileInfoSet here: If a user
    // selects a file and one of its ancestors, they might be interested to
    // perform an action on each of them individually. We can't know if the
    // action on the ancestor affects any of its children.

    for ( FileInfo * item : selection )
    {
	if ( cleanup->worksFor( item ) )
	{
	    cleanup->execute( item, outputWindow );
	}
	else
	{
	    logWarning() << "Cleanup " << cleanup
			 << " does not work for " << item << Qt::endl;
	}
    }

    if ( cleanup->refreshPolicy() == Cleanup::AssumeDeleted )
    {
	connect( outputWindow,	SIGNAL( lastProcessFinished( int ) ),
		 this,		SIGNAL( assumedDeleted() ) );

        // It is important to use the normalized FileInfoSet here to avoid a
        // segfault because we are iterating over items whose ancestors we just
        // deleted (thus invalidating pointers to it). Normalizing removes
        // items from the set that also have any ancestors in the set.

        for ( FileInfo * item : selection.invalidRemoved().normalized() )
        {
            DirTree * tree = item->tree();

            if ( tree->isBusy() )
                logWarning() << "Ignoring AssumeDeleted: DirTree is being read" << Qt::endl;
            else
                tree->deleteSubtree( item );
        }
    }

    outputWindow->noMoreProcesses();
}


bool CleanupCollection::confirmation( Cleanup * cleanup, const FileInfoSet & items )
{
    // Pad the title to avoid tiny dialog boxes
    const QString title = pad( cleanup->cleanTitle(), 40 );

    QString msg = "<html>";
    if ( items.size() == 1 ) // The most common case
    {
	const FileInfo * item = items.first();

	if ( item->isDir() || item->isPseudoDir() )
	    msg += tr( "<h3>%1</h3>for <b>directory</b> %2" ).arg( title ).arg( item->url() );
	else
	    msg += tr( "<h3>%1</h3>for file %2" ).arg( title ).arg( item->url() );

	msg += "<br>";
    }
    else // Multiple items selected
    {
	QStringList urls;
	if ( items.containsDir() && items.containsFile() )
	{
	    QStringList dirs	= filteredUrls( items, true, false,    // dirs, nonDirs
						true );		       // extraHighlight
	    QStringList nonDirs = filteredUrls( items, false, true  ); // dirs, nonDirs

	    dirs    = dirs.mid	 ( 0, MAX_URLS_IN_CONFIRMATION_POPUP );
	    nonDirs = nonDirs.mid( 0, MAX_URLS_IN_CONFIRMATION_POPUP );

	    urls << dirs << "" << nonDirs;
	}
	else // ! isMixed
	{
	    // Build a list of the first couple of selected items (7 max)

	    urls = filteredUrls( items, true, true ); // dirs, nonDirs
	    urls = urls.mid( 0, MAX_URLS_IN_CONFIRMATION_POPUP );
	}

	if ( urls.size() < items.size() ) // Only displaying part of the items?
	{
	    urls << "...";
	    urls << tr( "<i>(%1 items total)</i>" ).arg( items.size() );
	}

	msg += tr( "<h3>%1</h3> for:<br>\n%2<br>" ).arg( title ).arg( urls.join( "<br>" ) );
    }

    const int ret = QMessageBox::question( qApp->activeWindow(),
					   tr( "Please Confirm" ), // title
					   msg,		     // text
					   QMessageBox::Yes | QMessageBox::No );
    return ret == QMessageBox::Yes;
}


QStringList CleanupCollection::filteredUrls( const FileInfoSet & items,
					     bool		 dirs,
					     bool		 nonDirs,
					     bool		 extraHighlight ) const
{
    QStringList urls;

    for ( FileInfoSet::const_iterator it = items.begin(); it != items.end(); ++it )
    {
	if ( ( dirs    &&   (*it)->isDir() ) ||
	     ( nonDirs && ! (*it)->isDir() )   )
	{
	    const QString name = (*it)->url();

	    if ( (*it)->isDir() )
	    {
		if ( extraHighlight )
		    urls << tr( "<b>Directory <font color=blue>%1</font></b>" ).arg( name );
		else
		    urls << tr( "<b>Directory</b> %1" ).arg( name );
	    }
	    else
		urls << name;
	}
    }

    return urls;
}


void CleanupCollection::addToMenu( QMenu * menu, bool keepUpdated )
{
    CHECK_PTR( menu );

    for ( Cleanup * cleanup : _cleanupList )
    {
	if ( cleanup->active() )
	    menu->addAction( cleanup );
    }

    if ( keepUpdated && ! _menus.contains( menu ) )
	_menus << menu;
}


void CleanupCollection::addEnabledToMenu( QMenu * menu ) const
{
    CHECK_PTR( menu );

    for ( Cleanup * cleanup : _cleanupList )
    {
	if ( cleanup->active() && cleanup->isEnabled() )
	    menu->addAction( cleanup );
    }
}


void CleanupCollection::addToToolBar( QToolBar * toolBar, bool keepUpdated )
{
    CHECK_PTR( toolBar );

    for ( Cleanup * cleanup : _cleanupList )
    {
	// Add only cleanups that have an icon to avoid overcrowding the
	// toolbar with actions that only have a text.
	if ( cleanup->active() && ! cleanup->icon().isNull() )
	    toolBar->addAction( cleanup );
    }

    if ( keepUpdated && ! _toolBars.contains( toolBar ) )
	_toolBars << toolBar;
}


void CleanupCollection::readSettings()
{
    clear();

    CleanupSettings settings;
    const QStringList cleanupGroups = settings.findGroups( settings.groupPrefix() );

    if ( ! cleanupGroups.isEmpty() )
    {
	// Read all settings groups [Cleanup_xx] that were found

	for ( const QString & groupName : cleanupGroups )
	{
	    settings.beginGroup( groupName );

	    // Read one cleanup

	    const QString title    = settings.value( "Title"	).toString();
	    const QString command  = settings.value( "Command"	).toString();
	    const QString iconName = settings.value( "Icon"	).toString();
	    const QString hotkey   = settings.value( "Hotkey"	).toString();
	    const QString shell    = settings.value( "Shell"	).toString();

	    const bool active		     = settings.value( "Active"			, true	).toBool();
	    const bool worksForDir	     = settings.value( "WorksForDir"		, true	).toBool();
	    const bool worksForFile	     = settings.value( "WorksForFile"		, true	).toBool();
	    const bool worksForDotEntry      = settings.value( "WorksForDotEntry"	, true	).toBool();
	    const bool recurse		     = settings.value( "Recurse"		, false ).toBool();
	    const bool askForConfirmation    = settings.value( "AskForConfirmation"	, false ).toBool();
	    const bool outputWindowAutoClose = settings.value( "OutputWindowAutoClose"	, false ).toBool();
	    const int  outputWindowTimeout   = settings.value( "OutputWindowTimeout"	, 0	).toInt();

	    const int refreshPolicy	 = readEnumEntry( settings, "RefreshPolicy",
							  Cleanup::NoRefresh,
							  Cleanup::refreshPolicyMapping() );

	    const int outputWindowPolicy = readEnumEntry( settings, "OutputWindowPolicy",
							  Cleanup::ShowAfterTimeout,
							  Cleanup::outputWindowPolicyMapping() );

	    if ( command.isEmpty() || title.isEmpty() )
	    {
		logError() << "Need at least Command and Title for a cleanup" << Qt::endl;
	    }
	    else
	    {
//		Cleanup * cleanup = new Cleanup( command, title, this );
		Cleanup * cleanup = new Cleanup( this, active, title, command,
						 recurse, askForConfirmation,
						 static_cast<Cleanup::RefreshPolicy>( refreshPolicy ),
						 worksForDir, worksForFile, worksForDotEntry,
						 static_cast<Cleanup::OutputWindowPolicy>( outputWindowPolicy ),
						 outputWindowTimeout, outputWindowAutoClose,
						 shell );
		CHECK_NEW( cleanup );
		add( cleanup );
/*
		cleanup->setActive	    ( active	       );
		cleanup->setWorksForDir	    ( worksForDir      );
		cleanup->setWorksForFile    ( worksForFile     );
		cleanup->setWorksForDotEntry( worksForDotEntry );
		cleanup->setRecurse	    ( recurse	       );
		cleanup->setShell	    ( shell	       );
		cleanup->setAskForConfirmation	 ( askForConfirmation	 );
		cleanup->setOutputWindowAutoClose( outputWindowAutoClose );
		cleanup->setOutputWindowTimeout	 ( outputWindowTimeout	 );
		cleanup->setRefreshPolicy     ( static_cast<Cleanup::RefreshPolicy>( refreshPolicy ) );
		cleanup->setOutputWindowPolicy( static_cast<Cleanup::OutputWindowPolicy>( outputWindowPolicy ) );
*/
		if ( ! iconName.isEmpty() )
		    cleanup->setIcon( iconName );

		if ( ! hotkey.isEmpty() )
		    cleanup->setShortcut( hotkey );

		// if ( ! shell.isEmpty() )
		//    logDebug() << "Using custom shell " << shell << " for " << cleanup << Qt::endl;
	    }

	    settings.endGroup(); // [Cleanup_01], [Cleanup_02], ...
	}
    }

    if ( _cleanupList.isEmpty() && !settings.value( "StdCleanupsAdded", false ).toBool() )
        addStdCleanups();

    updateActions();
}


void CleanupCollection::writeSettings( const CleanupList & newCleanups)
{
    CleanupSettings settings;

    // Remove all leftover cleanup descriptions
    settings.removeGroups( settings.groupPrefix() );

    // Using a separate group for each cleanup for better readability in the
    // file.
    //
    // Settings arrays are hard to read and to edit if there are more than,
    // say, 2-3 entries for each array index. Plus, a user editing the file
    // would have to take care of the array count - which is very error prone.
    //
    // We are using [Cleanup_01], [Cleanup_02], ... here just because that's
    // easiest to generate automatically; upon reading, the numbers are
    // irrelevant. It's just important that each group name is
    // unique. readSettings() will happily pick up any group that starts with
    // "Cleanup_".

    for ( int i=0; i < newCleanups.size(); ++i )
    {
	settings.beginGroup( "Cleanup", i+1 );

	const Cleanup * cleanup = newCleanups.at(i);

	settings.setValue( "Command"		  , cleanup->command()		     );
	settings.setValue( "Title"		  , cleanup->title()		     );
	settings.setValue( "Active"		  , cleanup->active()		     );
	settings.setValue( "WorksForDir"	  , cleanup->worksForDir()	     );
	settings.setValue( "WorksForFile"	  , cleanup->worksForFile()	     );
	settings.setValue( "WorksForDotEntry"	  , cleanup->worksForDotEntry()	     );
	settings.setValue( "Recurse"		  , cleanup->recurse()		     );
	settings.setValue( "AskForConfirmation"	  , cleanup->askForConfirmation()    );
	settings.setValue( "OutputWindowAutoClose", cleanup->outputWindowAutoClose() );

	if ( cleanup->outputWindowTimeout() > 0 )
	    settings.setValue( "OutputWindowTimeout" , cleanup->outputWindowTimeout() );

	writeEnumEntry( settings, "RefreshPolicy",
			cleanup->refreshPolicy(),
			Cleanup::refreshPolicyMapping() );

	writeEnumEntry( settings, "OutputWindowPolicy",
			cleanup->outputWindowPolicy(),
			Cleanup::outputWindowPolicyMapping() );

	if ( ! cleanup->shell().isEmpty() )
	     settings.setValue( "Shell", cleanup->shell() );

	if ( ! cleanup->iconName().isEmpty() )
	    settings.setValue( "Icon", cleanup->iconName() );

	if ( ! cleanup->shortcut().isEmpty() )
	    settings.setValue( "Hotkey", cleanup->shortcut().toString() );

	settings.endGroup(); // [Cleanup_01], [Cleanup_02], ...
    }

    // Load the new settings into the real cleanup collection
    readSettings();
}


void CleanupCollection::moveToTrash()
{
    const FileInfoSet selectedItems = _selectionModel->selectedItems();

    // Prepare output window
    OutputWindow * outputWindow = new OutputWindow( qApp->activeWindow(), true );
    CHECK_NEW( outputWindow );

    // Prepare refresher
    createRefresher( outputWindow, Refresher::parents( selectedItems ) );

    // Window will never show for quick and successful trashes
    outputWindow->showAfterTimeout();

    // Move all selected items to trash
    for ( const FileInfo * item : selectedItems )
    {
	QCoreApplication::processEvents(); // give the output window a chance
	if ( Trash::trash( item->path() ) )
	    outputWindow->addStdout( tr( "Moved to trash: " ) + item->path() );
	else
	    outputWindow->addStderr( tr( "Move to trash failed for " ) + item->path() );
    }

    outputWindow->noMoreProcesses();
}


void CleanupCollection::createRefresher( OutputWindow * outputWindow, const FileInfoSet & refreshSet )
{
    _selectionModel->prepareRefresh( refreshSet );
    Refresher * refresher  = new Refresher( this, refreshSet );
    CHECK_NEW( refresher );

    connect( outputWindow, &OutputWindow::lastProcessFinished,
	     refresher,	   &Refresher::refresh );
}
