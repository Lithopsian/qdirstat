/*
 *   File name: MainWindow.cpp
 *   Summary:	QDirStat main window
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QApplication>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QClipboard>

#include "MainWindow.h"
#include "ActionManager.h"
#include "BusyPopup.h"
#include "CleanupCollection.h"
#include "CleanupConfigPage.h"
#include "ConfigDialog.h"
#include "DataColumns.h"
//#include "DebugHelpers.h"
#include "DirTree.h"
#include "DirTreeCache.h"
#include "DirTreeModel.h"
#include "DiscoverActions.h"
#include "Exception.h"
#include "FormatUtil.h"
#include "FileAgeStatsWindow.h"
#include "FileDetailsView.h"
#include "FileSearchFilter.h"
#include "FileSizeStatsWindow.h"
#include "FilesystemsWindow.h"
#include "FileTypeStatsWindow.h"
#include "FindFilesDialog.h"
#include "GeneralConfigPage.h"
#include "HistoryButtons.h"
#include "Logger.h"
#include "OpenDirDialog.h"
#include "OpenPkgDialog.h"
#include "PanelMessage.h"
#include "PkgManager.h"
#include "PkgQuery.h"
#include "QDirStatApp.h"
#include "SelectionModel.h"
#include "Settings.h"
#include "SettingsHelpers.h"
#include "SignalBlocker.h"
#include "SysUtil.h"
#include "UnreadableDirsWindow.h"
#include "Version.h"

#define UPDATE_MILLISEC		200

#define USE_CUSTOM_OPEN_DIR_DIALOG 1

using namespace QDirStat;


MainWindow::MainWindow( bool slowUpdate ):
    QMainWindow(),
    _ui { new Ui::MainWindow },
    _configDialog { nullptr },
    _discoverActions { new DiscoverActions( this ) },
    _enableDirPermissionsWarning { false },
    _verboseSelection { false },
    _urlInWindowTitle { false },
    _statusBarTimeout { 3000 }, // millisec
    _longStatusBarTimeout { 30000 }, // millisec
    _treeLevelMapper { nullptr },
    _sortCol { QDirStat::DataColumns::toViewCol( QDirStat::SizeCol ) },
    _sortOrder { Qt::DescendingOrder }
{
    CHECK_NEW( _ui );
    _ui->setupUi( this );
    _ui->menubar->setCornerWidget( new QLabel( MENUBAR_VERSION ) );

    CHECK_NEW( _discoverActions );

    _historyButtons = new HistoryButtons( _ui->actionGoBack, _ui->actionGoForward );
    CHECK_NEW( _historyButtons );

    ActionManager::instance()->addWidgetTree( this );

    _updateTimer.setInterval( UPDATE_MILLISEC );

    // The first call to app() creates the QDirStatApp and with it
    // - the DirTreeModel
    // - the DirTree (owned and managed by the DirTreeModel)
    // - the SelectionModel
    // - the CleanupCollection.

    if ( slowUpdate )
        app()->dirTreeModel()->setSlowUpdate();

    _ui->dirTreeView->setModel( app()->dirTreeModel() );
    _ui->dirTreeView->setSelectionModel( app()->selectionModel() );

    _ui->treemapView->setDirTree( app()->dirTree() );
    _ui->treemapView->setSelectionModel( app()->selectionModel() );

    _futureSelection.setTree( app()->dirTree() );
    _futureSelection.setUseRootFallback( false );
    _futureSelection.setUseParentFallback( true );

    connectSignals();
    connectMenuActions();               // see MainWindowMenus.cpp
    readSettings();

    // Set the boldItemFont for the DirTreeModel.
    //
    // It can't fetch that by itself from the DirTreeView;
    // we'd get an initialization sequence problem.
    // So this has to be done from the outside when both are created.
    QFont boldItemFont = _ui->dirTreeView->font();
    boldItemFont.setWeight( QFont::Bold );
    app()->dirTreeModel()->setBoldItemFont( boldItemFont );

    // Initialize cleanups
    app()->cleanupCollection()->addToMenu   ( _ui->menuCleanup,
                                              true ); // keepUpdated
    app()->cleanupCollection()->addToToolBar( _ui->toolBar,
                                              true ); // keepUpdated

    _ui->dirTreeView->setCleanupCollection( app()->cleanupCollection() );
    _ui->treemapView->setCleanupCollection( app()->cleanupCollection() );
    _ui->treemapView->hideTreemap();

#ifdef Q_OS_MACX
    // This makes the application to look like more "native" on macOS
    setUnifiedTitleAndToolBarOnMac( true );
    _ui->toolBar->setMovable( false );
#endif

    checkPkgManagerSupport();

    toggleVerboseSelection( _ui->actionVerboseSelection->isChecked() );
    updateActions();
}


MainWindow::~MainWindow()
{
    // logDebug() << "Destroying main window" << Qt::endl;

    writeSettings();

    // Relying on the QObject hierarchy to properly clean this up resulted in a
    //	segfault; there was probably a problem in the deletion order.

    if ( _configDialog )
	delete _configDialog;

    delete _ui->dirTreeView;
    delete _ui;
    delete _historyButtons;

//    qDeleteAll( _layouts );

//    QDirStatApp::deleteInstance(); // static now

    // logDebug() << "Main window destroyed" << Qt::endl;
}


void MainWindow::checkPkgManagerSupport()
{
    if ( ! PkgQuery::haveGetInstalledPkgSupport() ||
	 ! PkgQuery::haveFileListSupport()	    )
    {
	logInfo() << "No package manager support "
		  << "for getting installed packages or file lists"
		  << Qt::endl;

	_ui->actionOpenPkg->setEnabled( false );
    }

    const PkgManager * pkgManager = PkgQuery::primaryPkgManager();
    if ( ! pkgManager || ! pkgManager->supportsFileListCache() )
    {
	logInfo() << "No package manager support "
		  << "for getting a file lists cache"
		  << Qt::endl;

	_ui->actionOpenUnpkg->setEnabled( false );
    }
}


void MainWindow::connectSignals()
{
    connect( app()->dirTreeModel(), &DirTreeModel::layoutChanged,
	     this,		    &MainWindow::layoutChanged );

    connect( app()->selectionModel(),	  SIGNAL( currentBranchChanged( QModelIndex ) ),
	     _ui->dirTreeView,		  SLOT  ( closeAllExcept      ( QModelIndex ) ) );

    connect( app()->dirTree(),		  SIGNAL( startingReading() ),
	     this,			  SLOT  ( startingReading() ) );

    connect( app()->dirTree(),		  SIGNAL( finished()	    ),
	     this,			  SLOT  ( readingFinished() ) );

    connect( app()->dirTree(),		  SIGNAL( aborted()	   ),
	     this,			  SLOT  ( readingAborted() ) );

    connect( app()->selectionModel(),	  SIGNAL( currentItemChanged( FileInfo *, FileInfo * ) ),
	     _ui->breadcrumbNavigator,	  SLOT  ( setPath	    ( FileInfo *	     ) ) );

    connect( app()->selectionModel(),	  SIGNAL( currentItemChanged( FileInfo *, FileInfo * ) ),
	     _historyButtons,		  SLOT  ( addToHistory	    ( FileInfo *		    ) ) );

    connect( _historyButtons,		  SIGNAL( navigateToUrl( QString ) ),
	     this,			  SLOT  ( navigateToUrl( QString ) ) );

    connect( _ui->breadcrumbNavigator,	  SIGNAL( pathClicked   ( QString ) ),
	     app()->selectionModel(),	  SLOT  ( setCurrentItem( QString ) ) );

    connect( _ui->treemapView,		  SIGNAL( treemapChanged() ),
	     this,			  SLOT  ( updateActions()  ) );

    connect( app()->cleanupCollection(),  SIGNAL( startingCleanup( QString ) ),
	     this,			  SLOT  ( startingCleanup( QString ) ) );

    connect( app()->cleanupCollection(),  SIGNAL( cleanupFinished( int ) ),
	     this,			  SLOT  ( cleanupFinished( int ) ) );

    connect( &_updateTimer,		  SIGNAL( timeout()	    ),
	     this,			  SLOT  ( showElapsedTime() ) );

//    connect( &_treeExpandTimer,		  SIGNAL( timeout() ),
//	     _ui->actionExpandTreeLevel1, SLOT	( trigger() ) );

    connect( _ui->treemapView,		  SIGNAL( hoverEnter ( FileInfo * ) ),
	     this,			  SLOT	( showCurrent( FileInfo * ) ) );

    connect( _ui->treemapView,		  SIGNAL( hoverLeave ( FileInfo * ) ),
	     this,			  SLOT	( showSummary()		  ) );

    connect( app()->selectionModel(),	  SIGNAL( selectionChanged() ),
	     this,			  SLOT	( selectionChanged() ) );

    connect( app()->selectionModel(),	  SIGNAL( currentItemChanged( FileInfo *, FileInfo * ) ),
	     this,			  SLOT	( currentItemChanged( FileInfo *, FileInfo * ) ) );
}


void MainWindow::updateActions()
{
    const bool reading	     = app()->dirTree()->isBusy();
    FileInfo * currentItem   = app()->currentItem();
    FileInfo * firstToplevel = app()->root();
    const bool pkgView       = firstToplevel && firstToplevel->isPkgInfo();

    _ui->actionStopReading->setEnabled	( reading );
    _ui->actionRefreshAll->setEnabled	( !reading && firstToplevel );
    _ui->actionAskReadCache->setEnabled	( !reading );
    _ui->actionAskWriteCache->setEnabled( !reading && !pkgView && firstToplevel);

    _ui->actionCopyPath->setEnabled    ( currentItem );
    _ui->actionGoUp->setEnabled        ( currentItem && currentItem->treeLevel() > 1 );
    _ui->actionGoToToplevel->setEnabled( firstToplevel );

    const FileInfoSet selectedItems  = app()->selectionModel()->selectedItems();
    const bool        selSizeOne     = selectedItems.size() == 1;
    const FileInfo  * sel            = selectedItems.first();
    const bool        oneDirSelected = selSizeOne && sel && sel->isDir() && !pkgView;
    _ui->actionRefreshSelected->setEnabled( selSizeOne && !pkgView && !sel->isMountPoint() && !sel->isExcluded() );
    _ui->actionContinueReading->setEnabled( oneDirSelected && sel->isMountPoint() );
    _ui->actionReadExcluded->setEnabled   ( oneDirSelected && sel->isExcluded()   );

    const bool pseudoDirSelected = selectedItems.containsPseudoDir();
    const bool pkgSelected	 = selectedItems.containsPkg();
    _ui->actionMoveToTrash->setEnabled( sel && !pseudoDirSelected && !pkgSelected && !reading );

    const bool nothingOrOneDirInfo = !reading && (selectedItems.isEmpty() || ( selSizeOne && sel->isDirInfo() ) );
    // Notice that DotEntry, PkgInfo, Attic also inherit DirInfo
    _ui->actionFileSizeStats->setEnabled( nothingOrOneDirInfo );
    _ui->actionFileTypeStats->setEnabled( nothingOrOneDirInfo );
    _ui->actionFileAgeStats->setEnabled ( nothingOrOneDirInfo );

    const bool showingTreemap = _ui->treemapView->isVisible();
    _ui->actionTreemapAsSidePanel->setEnabled( showingTreemap );
    _ui->actionTreemapZoomTo->setEnabled     ( showingTreemap && _ui->treemapView->canZoomIn() );
    _ui->actionTreemapZoomIn->setEnabled     ( showingTreemap && _ui->treemapView->canZoomIn() );
    _ui->actionTreemapZoomOut->setEnabled    ( showingTreemap && _ui->treemapView->canZoomOut() );
    _ui->actionResetTreemapZoom->setEnabled  ( showingTreemap && _ui->treemapView->canZoomOut() );
    _ui->actionTreemapRebuild->setEnabled    ( showingTreemap );

    _historyButtons->updateActions();
}


void MainWindow::updateSettings( bool urlInWindowTitle,
                                 bool useTreemapHover,
                                 int statusBarTimeout,
                                 int longStatusBarTimeout )
{
    _urlInWindowTitle = urlInWindowTitle;
    _ui->treemapView->setUseTreemapHover( useTreemapHover );
    _statusBarTimeout = statusBarTimeout;
    _longStatusBarTimeout = longStatusBarTimeout;
    updateWindowTitle( app()->dirTree()->url() );
}


void MainWindow::readSettings()
{
    QDirStat::Settings settings;
    settings.beginGroup( "MainWindow" );

    restoreState(settings.value("State").toByteArray());

    _statusBarTimeout		= settings.value( "StatusBarTimeoutMillisec",  3000 ).toInt();
    _longStatusBarTimeout	= settings.value( "LongStatusBarTimeout"    , 30000 ).toInt();

    const bool showTreemap	= settings.value( "ShowTreemap"		      , true  ).toBool();
    const bool treemapOnSide	= settings.value( "TreemapOnSide"	      , false ).toBool();

    _verboseSelection		= settings.value( "VerboseSelection"	      , false ).toBool();
    _urlInWindowTitle		= settings.value( "UrlInWindowTitle"	      , false ).toBool();
    const bool useTreemapHover  = settings.value( "UseTreemapHover"	      , false ).toBool();
    const QString layoutName	= settings.value( "Layout"		      , "L2"  ).toString();

    settings.endGroup();

    settings.beginGroup( "MainWindow-Subwindows" );
    const QByteArray mainSplitterState = settings.value( "MainSplitter" , QByteArray() ).toByteArray();
    const QByteArray topSplitterState	 = settings.value( "TopSplitter"  , QByteArray() ).toByteArray();
    settings.endGroup();

    _ui->treemapView->setUseTreemapHover( useTreemapHover );
    _ui->actionShowTreemap->setChecked( showTreemap );
    _ui->actionTreemapAsSidePanel->setChecked( treemapOnSide );
    treemapAsSidePanel( treemapOnSide );

    _ui->actionVerboseSelection->setChecked( _verboseSelection );

    readWindowSettings( this, "MainWindow" );

    if ( ! mainSplitterState.isNull() )
	_ui->mainWinSplitter->restoreState( mainSplitterState );

    if ( ! topSplitterState.isNull() )
	_ui->topViewsSplitter->restoreState( topSplitterState );
//    else
//    {
	// The Qt designer refuses to let me set a reasonable size for that
	// widget, so let's set one here. Yes, that's not really how this is
	// supposed to be, but I am fed up with that stuff.

//	_ui->fileDetailsPanel->resize( QSize( 300, 300 ) ); // seems fine in 5.15
//    }

    initLayouts( layoutName );
}


void MainWindow::writeSettings()
{
    QDirStat::Settings settings;
    settings.beginGroup( "MainWindow" );

    settings.setValue( "ShowTreemap"	 , _ui->actionShowTreemap->isChecked() );
    settings.setValue( "TreemapOnSide"	 , _ui->actionTreemapAsSidePanel->isChecked() );
    settings.setValue( "VerboseSelection", _verboseSelection );
    settings.setValue( "Layout"		 , currentLayoutName() );

    settings.setValue( "StatusBarTimeoutMillisec", _statusBarTimeout );
    settings.setValue( "LongStatusBarTimeout"    , _longStatusBarTimeout );
    settings.setValue( "UrlInWindowTitle"	 , _urlInWindowTitle );
    settings.setValue( "UseTreemapHover"	 , _ui->treemapView->useTreemapHover() );

    settings.setValue( "State", saveState() );

    settings.endGroup();

    writeWindowSettings( this, "MainWindow" );

    settings.beginGroup( "MainWindow-Subwindows" );
    settings.setValue( "MainSplitter", _ui->mainWinSplitter->saveState()  );
    settings.setValue( "TopSplitter",  _ui->topViewsSplitter->saveState() );
    settings.endGroup();

    writeLayoutSettings();  // see MainWindowLayout.cpp
}


void MainWindow::showTreemapView( bool show )
{
    if ( show )
    {
	if ( !_updateTimer.isActive() )
	    _ui->treemapView->showTreemap();
    }
    else
    {
	_ui->treemapView->hideTreemap();
    }
}


void MainWindow::treemapAsSidePanel( bool asSidePanel )
{
    if ( asSidePanel )
	_ui->mainWinSplitter->setOrientation( Qt::Horizontal );
    else
	_ui->mainWinSplitter->setOrientation( Qt::Vertical );
}


void MainWindow::busyDisplay()
{
    //logDebug() << Qt::endl;
    _ui->treemapView->disable();
    updateActions();
    ActionManager::instance()->swapActions( _ui->toolBar, _ui->actionRefreshAll, _ui->actionStopReading );

    // If it is open, close the window that lists unreadable directories:
    // With the next directory read, things might have changed; the user may
    // have fixed permissions or ownership of those directories.
    UnreadableDirsWindow::closeSharedInstance();

    if ( _dirPermissionsWarning )
        _dirPermissionsWarning->deleteLater();

    _updateTimer.start();

    // It would be nice to sort by read jobs during reading, but this confuses
    // the hell out of the Qt side of the data model; so let's sort by name
    // instead.  Seems OK in 5.15, so sort by sizeCol just for the read.
    SignalBlocker signalBlocker( app()->dirTreeModel() );
    const int sortCol = QDirStat::DataColumns::toViewCol( QDirStat::SizeCol );
    _ui->dirTreeView->sortByColumn( sortCol, Qt::DescendingOrder );

    if ( ! PkgFilter::isPkgUrl( app()->dirTree()->url() ) )
//    expandTreeToLevel( 1 );
	QTimer::singleShot( 0, [this]() { expandTreeToLevel( 1 ); } );
}


void MainWindow::idleDisplay()
{
    //logInfo() << Qt::endl;

    // Safe for the treemap to start work now
    _updateTimer.stop();
    showTreemapView( _ui->actionShowTreemap->isChecked() );

    updateActions();
    ActionManager::instance()->swapActions( _ui->toolBar, _ui->actionStopReading, _ui->actionRefreshAll );

    // Go back to the sort order before the read
    _ui->dirTreeView->sortByColumn( _sortCol, _sortOrder );

    if ( _futureSelection.subtree() )
    {
	logDebug() << "Using future selection " << _futureSelection.subtree() << Qt::endl;
//        _treeExpandTimer.stop();
        applyFutureSelection();
    }
    else if ( ! app()->selectionModel()->currentBranch() )
    {
	//logDebug() << "No current branch - expanding tree to level 1" << Qt::endl;
	expandTreeToLevel( 1 );
    }

    updateFileDetailsView();
}


void MainWindow::updateFileDetailsView()
{
    if ( _ui->fileDetailsView->isVisible() )
    {
	const FileInfoSet sel = app()->selectionModel()->selectedItems();

	if ( sel.isEmpty() )
	    _ui->fileDetailsView->showDetails( app()->selectionModel()->currentItem() );
	else
	{
	    if ( sel.count() == 1 )
		_ui->fileDetailsView->showDetails( sel.first() );
	    else
		_ui->fileDetailsView->showDetails( sel );
	}
    }
}


void MainWindow::setDetailsPanelVisible( bool detailsPanelVisible )
{
    updateLayout( detailsPanelVisible );

    if ( detailsPanelVisible )
        updateFileDetailsView();
}


void MainWindow::startingReading()
{
    _stopWatch.start();
    busyDisplay();
}


void MainWindow::readingFinished()
{
    idleDisplay();

    const QString elapsedTime = formatMillisec( _stopWatch.elapsed() );
    _ui->statusBar->showMessage( tr( "Finished. Elapsed time: " ) + elapsedTime, _longStatusBarTimeout );
    logInfo() << "Reading finished after " << elapsedTime << Qt::endl;

    if ( app()->root() &&
	 app()->root()->errSubDirCount() > 0 )
    {
	showDirPermissionsWarning();
    }

    // Debug::dumpModelTree( app()->dirTreeModel(), QModelIndex(), "" );
}


void MainWindow::readingAborted()
{
    idleDisplay();

    const QString elapsedTime = formatMillisec( _stopWatch.elapsed() );
    _ui->statusBar->showMessage( tr( "Aborted. Elapsed time: " ) + elapsedTime, _longStatusBarTimeout );
    logInfo() << "Reading aborted after " << elapsedTime << Qt::endl;
}


void MainWindow::layoutChanged( const QList<QPersistentModelIndex> &, QAbstractItemModel::LayoutChangeHint changeHint )
{
    if ( changeHint == QAbstractItemModel::VerticalSortHint )
    {
	// Remember this order to restore after the next tree read
	_sortCol = app()->dirTreeModel()->sortColumn();
	_sortOrder = app()->dirTreeModel()->sortOrder();
    }

    logDebug() << changeHint << Qt::endl;
}


void MainWindow::openUrl( const QString & url )
{
    _historyButtons->clearHistory();

    if ( PkgFilter::isPkgUrl( url ) )
	readPkg( url );
    else if ( isUnpkgUrl( url ) )
	showUnpkgFiles( url );
    else
	openDir( url );
}


void MainWindow::openDir( const QString & url )
{
    _enableDirPermissionsWarning = true;
    try
    {
//	if ( url.startsWith( "/" ) )
//            _futureSelection.setUrl( url );
//        else
//            _futureSelection.clear();

	app()->dirTreeModel()->openUrl( url );
	const QString dirTreeUrl = app()->dirTree()->url();
	updateWindowTitle( dirTreeUrl );
        _futureSelection.setUrl( dirTreeUrl );
    }
    catch ( const SysCallFailedException & ex )
    {
	CAUGHT( ex );
        showOpenDirErrorPopup( ex );
	askOpenDir();
    }

    updateActions();
//    expandTreeToLevel( 1 );
}


void MainWindow::showOpenDirErrorPopup( const SysCallFailedException & ex )
{
    updateWindowTitle( "" );
    app()->dirTree()->sendFinished();

    const QString msg = pad( tr( "Could not open directory " ) + ex.resourceName(), 50 );
    QMessageBox errorPopup( QMessageBox::Warning, tr( "Error" ), msg );
    errorPopup.setDetailedText( ex.what() );
    errorPopup.exec();
}


void MainWindow::askOpenDir()
{
    DirTree * tree = app()->dirTree();

    bool crossFilesystems = tree->crossFilesystems();

#if USE_CUSTOM_OPEN_DIR_DIALOG
    const QString path = QDirStat::OpenDirDialog::askOpenDir( this, &crossFilesystems );
#else
    const QString path = QFileDialog::getExistingDirectory( this, tr("Select directory to scan") );
#endif

    if ( ! path.isEmpty() )
    {
	tree->reset();
	tree->setCrossFilesystems( crossFilesystems );
	openDir( path );
    }
}


void MainWindow::askOpenPkg()
{
    bool canceled;
    PkgFilter pkgFilter = OpenPkgDialog::askPkgFilter( &canceled );

    if ( ! canceled )
    {
	app()->dirTree()->reset();
	readPkg( pkgFilter );
    }
}


void MainWindow::readPkg( const PkgFilter & pkgFilter )
{
    // logInfo() << "URL: " << pkgFilter.url() << Qt::endl;

    _futureSelection.setUrl( "Pkg:/" );
    updateWindowTitle( pkgFilter.url() );
    pkgQuerySetup();
    BusyPopup msg( tr( "Reading package database..." ), this );

//    expandTreeToLevel( 0 );   // Performance boost: Down from 25 to 6 sec.
    app()->dirTreeModel()->readPkg( pkgFilter );
    app()->selectionModel()->setCurrentItem( app()->root() );
}


void MainWindow::pkgQuerySetup()
{
    if ( _dirPermissionsWarning )
        delete _dirPermissionsWarning;

    _ui->breadcrumbNavigator->clear();
    _ui->fileDetailsView->clear();
    app()->dirTreeModel()->clear();
    ActionManager::instance()->swapActions( _ui->toolBar, _ui->actionRefreshAll, _ui->actionStopReading );
}


void MainWindow::askFindFiles()
{
    bool canceled;
    const FileSearchFilter filter = FindFilesDialog::askFindFiles( &canceled );

    if ( ! canceled )
	_discoverActions->findFiles( filter );
}


void MainWindow::refreshAll()
{
    _enableDirPermissionsWarning = true;
    _futureSelection.set( app()->selectionModel()->selectedItems().first() );
    _ui->treemapView->saveTreemapRoot();

    const QString url = app()->dirTree()->url();
    if ( ! url.isEmpty() )
    {
	//logDebug() << "Refreshing " << url << Qt::endl;

	if ( PkgFilter::isPkgUrl( url ) )
	    //app()->dirTreeModel()->readPkg( url );
	    readPkg( url );
	else
	{
	    app()->dirTreeModel()->openUrl( url );
//	    expandTreeToLevel( 1 );
	}

        // No need to check if the URL is an unpkg:/ URL:
        //
        // In that case, the previous filters are still set, and just reading
        // the dir tree again from disk with openUrl() will filter out the
        // unwanted packaged files, ignored extensions and excluded directories
        // again.

	updateActions();
    }
    else
    {
	askOpenDir();
    }
}


void MainWindow::refreshSelected()
{
    // logDebug() << "Setting future selection: " << _futureSelection.subtree() << Qt::endl;
    _futureSelection.set( app()->selectionModel()->selectedItems().first() );
    _ui->treemapView->saveTreemapRoot();
    busyDisplay();
    app()->dirTreeModel()->refreshSelected();
    updateActions();
}


void MainWindow::applyFutureSelection()
{
    FileInfo * sel = _futureSelection.subtree();
    DirInfo  * branch = _futureSelection.dir();
    _futureSelection.clear();

    // logDebug() << "Using future selection: " << sel << Qt::endl;

    if ( sel )
    {
//        _treeExpandTimer.stop();

        if ( branch )
            app()->selectionModel()->setCurrentBranch( branch );

        app()->selectionModel()->setCurrentItem( sel,
                                                 true);  // select

        if ( sel->isMountPoint() || sel->isDirInfo() ) // || app()->dirTree()->isToplevel( sel ) )
            _ui->dirTreeView->setExpanded( sel, true );

	// _ui->treemapView->applyFutureTreemapDepth();
    }
}


void MainWindow::stopReading()
{
    if ( app()->dirTree()->isBusy() )
    {
	app()->dirTree()->abortReading();
	_ui->statusBar->showMessage( tr( "Reading aborted." ), _longStatusBarTimeout );
    }
}


void MainWindow::readCache( const QString & cacheFileName )
{
    app()->dirTreeModel()->clear();
    _historyButtons->clearHistory();

    if ( cacheFileName.isEmpty() )
	return;

    if ( !app()->dirTree()->readCache( cacheFileName ) )
    {
	idleDisplay();
	const QString msg = pad( tr( "Can't read cache file " ) + cacheFileName, 50 );
	QMessageBox::warning( this, tr( "Error" ), msg );
    }
}


void MainWindow::askReadCache()
{
    const QString fileName = QFileDialog::getOpenFileName( this, // parent
							   tr( "Select QDirStat cache file" ),
							   DEFAULT_CACHE_NAME );
    if ( ! fileName.isEmpty() )
	readCache( fileName );

    updateActions();
}


void MainWindow::askWriteCache()
{
    const QString fileName = QFileDialog::getSaveFileName( this, // parent
							   tr( "Enter name for QDirStat cache file"),
							   DEFAULT_CACHE_NAME );
    if ( fileName.isEmpty() )
	return;

    const bool ok = app()->dirTree()->writeCache( fileName );
    if ( ok )
    {
	showProgress( tr( "Directory tree written to file " ) + fileName );
    }
    else
    {
	QMessageBox::warning( this,
			      tr( "Error" ), // Title
			      tr( "ERROR writing cache file " ) + fileName );
    }
}


void MainWindow::updateWindowTitle( const QString & url )
{
    QString windowTitle = "QDirStat";

    if ( SysUtil::runningAsRoot() )
	windowTitle += tr( " [root]" );

    if ( _urlInWindowTitle )
	windowTitle += " " + url;

    setWindowTitle( windowTitle );
}


void MainWindow::showProgress( const QString & text )
{
    _ui->statusBar->showMessage( text, _statusBarTimeout );
}


void MainWindow::showElapsedTime()
{
    showProgress( tr( "Reading... " ) + formatMillisec( _stopWatch.elapsed(), false ) );
}


void MainWindow::showCurrent( FileInfo * item )
{
    if ( item )
    {
	QString msg = QString( "%1  (%2%3)" )
	    .arg( item->debugUrl() )
	    .arg( item->sizePrefix() )
	    .arg( formatSize( item->totalSize() ) );

	if ( item->readState() == DirPermissionDenied )
	    msg += tr( "  [Permission Denied]" );
        else if ( item->readState() == DirError )
	    msg += tr( "  [Read Error]" );

	_ui->statusBar->showMessage( msg );
    }
    else
    {
	_ui->statusBar->clearMessage();
    }
}


void MainWindow::showSummary()
{
    FileInfoSet sel = app()->selectionModel()->selectedItems();
    const int count = sel.size();

    if ( count <= 1 )
	showCurrent( app()->currentItem() );
    else
    {
	sel = sel.normalized();

	_ui->statusBar->showMessage( tr( "%1 items selected (%2 total)" )
				     .arg( count )
				     .arg( formatSize( sel.totalSize() ) ) );
    }
}


void MainWindow::startingCleanup( const QString & cleanupName )
{
    // Notice that this is not called for actions that are not owned by the
    // CleanupCollection such as _ui->actionMoveToTrash().

    _futureSelection.set( app()->selectionModel()->selectedItems().first() );

    showProgress( tr( "Starting cleanup action " ) + cleanupName );
}


void MainWindow::cleanupFinished( int errorCount )
{
    // Notice that this is not called for actions that are not owned by the
    // CleanupCollection such as _ui->actionMoveToTrash().

    //logDebug() << "Error count: " << errorCount << Qt::endl;

    if ( errorCount == 0 )
	showProgress( tr( "Cleanup action finished successfully." ) );
    else
	showProgress( tr( "Cleanup action finished with %1 errors." ).arg( errorCount ) );
}


void MainWindow::copyCurrentPathToClipboard()
{
    const FileInfo * currentItem = app()->currentItem();

    if ( currentItem )
    {
	QClipboard * clipboard = QApplication::clipboard();
	const QString path = currentItem->path();
	clipboard->setText( path );
	showProgress( tr( "Copied to system clipboard: " ) + path );
    }
    else
    {
	showProgress( tr( "No current item" ) );
    }
}


void MainWindow::expandTreeToLevel( int level )
{
    //logDebug() << "Expanding tree to level " << level << Qt::endl;

    if ( level < 1 )
	_ui->dirTreeView->collapseAll();
    else
	_ui->dirTreeView->expandToDepth( level - 1 );
}


void MainWindow::navigateUp()
{
    const FileInfo * currentItem = app()->currentItem();

    if ( currentItem )
    {
	FileInfo * parent = currentItem->parent();

        if ( parent && parent != app()->dirTree()->root() )
        {
            // Close and re-open the parent to enforce a screen update:
            // Sometimes the bold font is not taken into account when moving
            // upwards, and so every column is cut off (probably a Qt bug)
            _ui->dirTreeView->setExpanded( parent, false );

            app()->selectionModel()->setCurrentItem( parent,
                                                     true ); // select
            // Re-open the parent
            _ui->dirTreeView->setExpanded( parent, true );
        }
    }
}


void MainWindow::navigateToToplevel()
{
    FileInfo * toplevel = app()->root();

    if ( toplevel )
    {
	expandTreeToLevel( 1 );
	app()->selectionModel()->setCurrentItem( toplevel,
                                                 true ); // select
    }
}


void MainWindow::navigateToUrl( const QString & url )
{
    // logDebug() << "Navigating to " << url << Qt::endl;

    if ( ! url.isEmpty() )
    {
        FileInfo * sel = app()->dirTree()->locate( url,
                                                   true ); // findPseudoDirs

        if ( sel )

        {
            app()->selectionModel()->setCurrentItem( sel,
                                                     true ); // select
            _ui->dirTreeView->setExpanded( sel, true );
        }
    }
}


void MainWindow::moveToTrash()
{
    // Note that _ui->actionMoveToTrash() is not actually a subclass of Cleanup

    // Save the selection - at least the first selected item
//    FileInfo * sel = selectedItems.first();
    _futureSelection.set( app()->selectionModel()->currentItem() );
    //logDebug() << "Storing future selection " << sel << Qt::endl;

    app()->cleanupCollection()->moveToTrash();
}


void MainWindow::openConfigDialog()
{
    if ( _configDialog && _configDialog->isVisible() )
	return;

    // For whatever crazy reason it is considerably faster to delete that
    // complex dialog and recreate it from scratch than to simply leave it
    // alive and just show it again. Well, whatever - so be it.
    //
    // Hiding and showing seems to work fine as of 5.15, but stay with the
    // destroy-and-recreate model.  However, actually delete the dialog when
    // it is closed instead of just before it is re-opened.

    _configDialog = new ConfigDialog( this );
    CHECK_PTR( _configDialog );
    _configDialog->setAttribute(Qt::WA_DeleteOnClose);

    _configDialog->cleanupConfigPage()->setCleanupCollection( app()->cleanupCollection() );

    connect( _configDialog,	SIGNAL( finished( int ) ),
	     this,		SLOT  ( configDialogFinished( int ) ) );

    if ( ! _configDialog->isVisible() )
    {
	_configDialog->setup();
	_configDialog->show();
    }
}


void MainWindow::configDialogFinished( int )
{
    // Dangling pointer, config has been destroyed
    _configDialog = 0;
}


void MainWindow::showFileTypeStats()
{
    FileTypeStatsWindow::populateSharedInstance( this, app()->selectedDirInfoOrRoot() );
}


void MainWindow::showFileSizeStats()
{
    FileSizeStatsWindow::populateSharedInstance( this, app()->selectedDirInfoOrRoot() );
}


void MainWindow::showFileAgeStats()
{
    if ( ! _fileAgeStatsWindow )
    {
	// This deletes itself when the user closes it. The associated QPointer
	// keeps track of that and sets the pointer to 0 when it happens.

	_fileAgeStatsWindow = new FileAgeStatsWindow( this );

        connect( app()->selectionModel(), SIGNAL( currentItemChanged( FileInfo *, FileInfo * ) ),
                 _fileAgeStatsWindow,     SLOT  ( syncedPopulate    ( FileInfo *             ) ) );

        connect( _fileAgeStatsWindow,     SIGNAL( locateFilesFromYear   ( QString, short ) ),
                 _discoverActions,        SLOT  ( discoverFilesFromYear ( QString, short ) ) );

        connect( _fileAgeStatsWindow,     SIGNAL( locateFilesFromMonth  ( QString, short, short ) ),
                 _discoverActions,        SLOT  ( discoverFilesFromMonth( QString, short, short ) ) );
    }

    _fileAgeStatsWindow->populate( app()->selectedDirInfoOrRoot() );
    _fileAgeStatsWindow->show();
}


void MainWindow::showFilesystems()
{
    if ( ! _filesystemsWindow )
    {
	// This deletes itself when the user closes it. The associated QPointer
	// keeps track of that and sets the pointer to 0 when it happens.

	_filesystemsWindow = new FilesystemsWindow( this );

        connect( _filesystemsWindow, SIGNAL( readFilesystem( QString ) ),
                 this,               SLOT  ( openDir       ( QString ) ) );
    }

    _filesystemsWindow->populate();
    _filesystemsWindow->show();
}


void MainWindow::showDirPermissionsWarning()
{
//    if ( _dirPermissionsWarning || ! _enableDirPermissionsWarning )
	return; // never display, I know already

    PanelMessage * msg = new PanelMessage( _ui->messagePanel,
					   tr( "Some directories could not be read." ),
					   tr( "You might not have sufficient permissions." ) );
    CHECK_NEW( msg );

    msg->setDetails( tr( "Show unreadable directories..." ) );
    msg->connectDetailsLink( this, SLOT( showUnreadableDirs() ) );
    msg->setIcon( QPixmap( ":/icons/lock-closed.png" ) );

    _ui->messagePanel->add( msg );
    _dirPermissionsWarning = msg;
    _enableDirPermissionsWarning = false;
}


void MainWindow::showUnreadableDirs()
{
    UnreadableDirsWindow::populateSharedInstance( app()->root() );
}

#if 0
void MainWindow::itemClicked( const QModelIndex & index )
{
    if ( ! _verboseSelection )
	return;

    if ( index.isValid() )
    {
	const FileInfo * item = static_cast<const FileInfo *>( index.internalPointer() );

	logDebug() << "Clicked row " << index.row()
		   << " col " << index.column()
		   << " (" << QDirStat::DataColumns::fromViewCol( index.column() ) << ")"
		   << "\t" << item
		   << Qt::endl;
	// << " data(0): " << index.model()->data( index, 0 ).toString()
	// logDebug() << "Ancestors: " << Debug::modelTreeAncestors( index ).join( " -> " ) << Qt::endl;
    }
    else
    {
	logDebug() << "Invalid model index" << Qt::endl;
    }

    // app()->dirTreeModel()->dumpPersistentIndexList();
}
#endif

void MainWindow::selectionChanged()
{
    showSummary();
    updateFileDetailsView();

    if ( _verboseSelection )
    {
	logNewline();
	app()->selectionModel()->dumpSelectedItems();
    }

    updateActions();
}


void MainWindow::currentItemChanged( FileInfo * newCurrent, FileInfo * oldCurrent )
{
    showSummary();

    if ( ! oldCurrent )
	updateFileDetailsView();

    if ( _verboseSelection )
    {
	logDebug() << "new current: " << newCurrent << Qt::endl;
	logDebug() << "old current: " << oldCurrent << Qt::endl;
	app()->selectionModel()->dumpSelectedItems();
    }

    _ui->dirTreeView->setFocus();

    updateActions();
}


void MainWindow::changeEvent( QEvent * event )
{
    if ( event->type() == QEvent::PaletteChange )
	updateFileDetailsView();

    QWidget::changeEvent( event );
}


void MainWindow::mousePressEvent( QMouseEvent * event )
{
    if ( event )
    {
        QAction * action = 0;

        switch ( event->button() )
        {
            // Handle the back / forward buttons on the mouse to act like the
            // history back / forward buttons in the tool bar

            case Qt::BackButton:
                // logDebug() << "BackButton" << Qt::endl;
                action = _ui->actionGoBack;
                break;

            case Qt::ForwardButton:
                // logDebug() << "ForwardButton" << Qt::endl;
                action = _ui->actionGoForward;
                break;

            default:
                QMainWindow::mousePressEvent( event );
                break;
        }

        if ( action && action->isEnabled() )
            action->trigger();
    }
}


void MainWindow::quit()
{
    QCoreApplication::quit();
}


//---------------------------------------------------------------------------
//			       Debugging Helpers
//---------------------------------------------------------------------------


void MainWindow::toggleVerboseSelection( bool verboseSelection)
{
    // Verbose selection is toggled with Shift-F7
    _verboseSelection = verboseSelection;

    if ( app()->selectionModel() )
	app()->selectionModel()->setVerbose( _verboseSelection );

    logInfo() << "Verbose selection is now " << ( _verboseSelection ? "on" : "off" )
	      << ". Change this with Shift-F7." << Qt::endl;
}


// For more MainWindow:: methods, See also:
//
//   - MainWindowHelp.cpp
//   - MainWindowLayout.cpp
//   - MainWindowMenus.cpp
//   - MainWindowUnpkg.cpp

