/*
 *   File name: MainWindowMenus.cpp
 *   Summary:	Connecting menu actions in the QDirStat main window
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#include <QDesktopServices>
#include <QMouseEvent>

#include "MainWindow.h"
#include "CleanupCollection.h"
#include "DirTree.h"
#include "DiscoverActions.h"
#include "HistoryButtons.h"
#include "QDirStatApp.h"
#include "SelectionModel.h"
//#include "SysUtil.h"
#include "Version.h"
#include "Logger.h"


using namespace QDirStat;


void MainWindow::connectMenuActions()
{
    _ui->actionGoUp->setShortcutContext ( Qt::ApplicationShortcut );
    _ui->actionGoToToplevel->setShortcutContext( Qt::ApplicationShortcut );
    _ui->actionWhatsNew->setStatusTip( RELEASE_URL ); // defined in Version.h

    // Invisible, not on any menu or toolbar
    addAction( _ui->actionVerboseSelection );    // Shift-F7
    addAction( _ui->actionDumpSelection );       // F7
    connect( _ui->actionDumpSelection, &QAction::triggered,
             app()->selectionModel(),  &SelectionModel::dumpSelectedItems );

    // CleanupCollection::add() handles the cleanup actions

    // File menu
    connectAction( _ui->actionOpenDir,            &MainWindow::askOpenDir );
    connectAction( _ui->actionOpenPkg,            &MainWindow::askOpenPkg );
    connectAction( _ui->actionOpenUnpkg,          &MainWindow::askOpenUnpkg );
    connectAction( _ui->actionRefreshAll,         &MainWindow::refreshAll );
    connectAction( _ui->actionRefreshSelected,    &MainWindow::refreshSelected );
    connectAction( _ui->actionReadExcluded,       &MainWindow::refreshSelected );
    connectAction( _ui->actionContinueReading,    &MainWindow::refreshSelected );
    connectAction( _ui->actionStopReading,        &MainWindow::stopReading );
    connectAction( _ui->actionAskReadCache,       &MainWindow::askReadCache );
    connectAction( _ui->actionAskWriteCache,      &MainWindow::askWriteCache );
    connectAction( _ui->actionQuit,               &MainWindow::quit );

    // Edit menu
    connectAction( _ui->actionCopyPath,           &MainWindow::copyCurrentPathToClipboard );
    connectAction( _ui->actionMoveToTrash,        &MainWindow::moveToTrash );
    connectAction( _ui->actionFindFiles,          &MainWindow::askFindFiles );
    connectAction( _ui->actionConfigure,          &MainWindow::openConfigDialog );

    // View menu
    connectAction( _ui->actionLayout1,            &MainWindow::changeLayoutSlot );
    connectAction( _ui->actionLayout2,            &MainWindow::changeLayoutSlot );
    connectAction( _ui->actionLayout3,            &MainWindow::changeLayoutSlot );
    connectAction( _ui->actionFileSizeStats,      &MainWindow::showFileSizeStats );
    connectAction( _ui->actionFileTypeStats,      &MainWindow::showFileTypeStats );
    connectAction( _ui->actionFileAgeStats,       &MainWindow::showFileAgeStats );
    connectAction( _ui->actionShowFilesystems,    &MainWindow::showFilesystems );

    // Go menu
    connectAction( _ui->actionGoUp,               &MainWindow::navigateUp );
    connectAction( _ui->actionGoToToplevel,       &MainWindow::navigateToToplevel );

    // Help menu
    connectAction( _ui->actionHelp,               &MainWindow::openActionUrl );
    connectAction( _ui->actionAbout,              &MainWindow::showAboutDialog );
    connectAction( _ui->actionAboutQt,            &MainWindow::showAboutQtDialog );
    connectAction( _ui->actionTreemapHelp,        &MainWindow::openActionUrl );
    connectAction( _ui->actionPkgViewHelp,        &MainWindow::openActionUrl );
    connectAction( _ui->actionUnpkgViewHelp,      &MainWindow::openActionUrl );
    connectAction( _ui->actionFileAgeStatsHelp,   &MainWindow::openActionUrl );
    connectAction( _ui->actionWhatsNew,           &MainWindow::openActionUrl );
    connectAction( _ui->actionCantMoveDirToTrash, &MainWindow::openActionUrl );
    connectAction( _ui->actionBtrfsSizeReporting, &MainWindow::openActionUrl );
    connectAction( _ui->actionShadowedByMount,    &MainWindow::openActionUrl );
    connectAction( _ui->actionHeadlessServers,    &MainWindow::openActionUrl );
    connectAction( _ui->actionDonate,             &MainWindow::showDonateDialog );

    // Toggle actions
    connectToggleAction( _ui->actionShowBreadcrumbs,    &MainWindow::setBreadcrumbsVisible );
    connectToggleAction( _ui->actionShowDetailsPanel,   &MainWindow::setDetailsPanelVisible );
    connectToggleAction( _ui->actionShowTreemap,        &MainWindow::showTreemapView );
    connectToggleAction( _ui->actionTreemapAsSidePanel, &MainWindow::treemapAsSidePanel );
    connectToggleAction( _ui->actionVerboseSelection,   &MainWindow::toggleVerboseSelection );

    // Treemap actions
    connectTreemapAction( _ui->actionTreemapZoomTo,    &TreemapView::zoomTo );
    connectTreemapAction( _ui->actionTreemapZoomIn,    &TreemapView::zoomIn );
    connectTreemapAction( _ui->actionTreemapZoomOut,   &TreemapView::zoomOut );
    connectTreemapAction( _ui->actionResetTreemapZoom, &TreemapView::resetZoom );
    connectTreemapAction( _ui->actionTreemapRebuild,   &TreemapView::rebuildTreemapSlot );

    // Expand tree to level actions
    mapTreeExpandAction( _ui->actionCloseAllTreeLevels, 0 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel0, 0 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel1, 1 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel2, 2 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel3, 3 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel4, 4 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel5, 5 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel6, 6 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel7, 7 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel8, 8 );
    mapTreeExpandAction( _ui->actionExpandTreeLevel9, 9 );

    // History button actions
    connectHistoryButton( _ui->actionGoBack,    &HistoryButtons::historyGoBack );
    connectHistoryButton( _ui->actionGoForward, &HistoryButtons::historyGoForward );

    // Connect the (non-class) DiscoverActions functions
    connect( _ui->actionDiscoverLargestFiles,    &QAction::triggered, &DiscoverActions::discoverLargestFiles );
    connect( _ui->actionDiscoverNewestFiles,     &QAction::triggered, &DiscoverActions::discoverNewestFiles );
    connect( _ui->actionDiscoverOldestFiles,     &QAction::triggered, &DiscoverActions::discoverOldestFiles );
    connect( _ui->actionDiscoverHardLinkedFiles, &QAction::triggered, &DiscoverActions::discoverHardLinkedFiles );
    connect( _ui->actionDiscoverBrokenSymLinks,  &QAction::triggered, &DiscoverActions::discoverBrokenSymLinks );
    connect( _ui->actionDiscoverSparseFiles,     &QAction::triggered, &DiscoverActions::discoverSparseFiles );
}


void MainWindow::connectAction( QAction * action, void( MainWindow::*actee )( void ) )
{
    connect( action, &QAction::triggered, this, actee );
}


void MainWindow::connectToggleAction( QAction * action, void( MainWindow::*actee )( bool ) )
{
    connect( action, &QAction::toggled, this, actee );
}


void MainWindow::mapTreeExpandAction( QAction * action, int level )
{
    connect( action, &QAction::triggered,
             this,   [ this, level ]() { expandTreeToLevel( level ); } );
}


void MainWindow::connectTreemapAction( QAction * action, void( TreemapView::*actee )( void ) )
{
    connect( action, &QAction::triggered, _ui->treemapView, actee );
}


void MainWindow::connectHistoryButton( QAction * action, void( HistoryButtons::*actee )( void ) )
{
    connect( action, &QAction::triggered, _historyButtons, actee );
}


void MainWindow::openActionUrl()
{
    // Use a QAction that was set up in Qt Designer to just open an URL in an
    // external web browser.  The url is stored in the status tip, and so
    // it also appears in the status bar.
    QAction * action = qobject_cast<QAction *>( sender() );
    if ( !action )
	return;

    const QString url = action->statusTip();
    if ( url.isEmpty() )
	logError() << "No URL in statusTip() for action " << action->objectName() << Qt::endl;
    else
	QDesktopServices::openUrl( url );
//	SysUtil::openInBrowser( url );
}


void MainWindow::updateActions()
{
    const bool reading       = app()->dirTree()->isBusy();
    FileInfo * currentItem   = app()->currentItem();
    FileInfo * firstToplevel = app()->root();
    const bool isTree        = firstToplevel && !reading;
    const bool pkgView       = firstToplevel && firstToplevel->isPkgInfo();

    _ui->actionStopReading->setEnabled  ( reading );
    _ui->actionRefreshAll->setEnabled   ( isTree );
    _ui->actionAskReadCache->setEnabled ( !reading );
    _ui->actionAskWriteCache->setEnabled( isTree && !pkgView );

    _ui->actionFindFiles->setEnabled   ( firstToplevel );
    _ui->actionCopyPath->setEnabled    ( currentItem );
    _ui->actionGoUp->setEnabled        ( currentItem && currentItem->treeLevel() > 1 );
    _ui->actionGoToToplevel->setEnabled( firstToplevel );

    const FileInfoSet selectedItems  = app()->selectionModel()->selectedItems();
    const bool        selSizeOne     = !reading && selectedItems.size() == 1;
    const FileInfo  * sel            = selectedItems.first();
    const bool        oneDirSelected = selSizeOne && sel && sel->isDir() && !pkgView;
    _ui->actionRefreshSelected->setEnabled( selSizeOne && !pkgView && !sel->isMountPoint() && !sel->isExcluded() );
    _ui->actionContinueReading->setEnabled( oneDirSelected && sel->isMountPoint() );
    _ui->actionReadExcluded->setEnabled   ( oneDirSelected && sel->isExcluded()   );

    const bool pseudoDirSelected = selectedItems.containsPseudoDir();
    const bool pkgSelected       = selectedItems.containsPkg();
    _ui->actionMoveToTrash->setEnabled( !reading && sel && !pseudoDirSelected && !pkgSelected );

    _ui->actionFileSizeStats->setEnabled( firstToplevel );
    _ui->actionFileTypeStats->setEnabled( firstToplevel );
    _ui->actionFileAgeStats->setEnabled ( firstToplevel );

    _ui->actionCloseAllTreeLevels->setEnabled( firstToplevel );
    _ui->menuExpandTreeToLevel->setEnabled   ( firstToplevel );

    const bool showingTreemap = _ui->treemapView->isVisible();
    _ui->actionTreemapAsSidePanel->setEnabled( showingTreemap );
    _ui->actionTreemapZoomTo->setEnabled     ( showingTreemap && _ui->treemapView->canZoomIn() );
    _ui->actionTreemapZoomIn->setEnabled     ( showingTreemap && _ui->treemapView->canZoomIn() );
    _ui->actionTreemapZoomOut->setEnabled    ( showingTreemap && _ui->treemapView->canZoomOut() );
    _ui->actionResetTreemapZoom->setEnabled  ( showingTreemap && _ui->treemapView->canZoomOut() );
    _ui->actionTreemapRebuild->setEnabled    ( showingTreemap );

    for ( QAction * action : _ui->menuDiscover->actions() )
	action->setEnabled( firstToplevel );

    _historyButtons->updateActions();
}


void MainWindow::mousePressEvent( QMouseEvent * event )
{
    if ( event )
    {
        switch ( event->button() )
        {
            // Handle the back / forward buttons on the mouse to act like the
            // history back / forward buttons in the tool bar

            case Qt::BackButton:
                // logDebug() << "BackButton" << Qt::endl;
		if ( _ui->actionGoBack->isEnabled() )
		    _ui->actionGoBack->trigger();
                break;

            case Qt::ForwardButton:
                // logDebug() << "ForwardButton" << Qt::endl;
		if ( _ui->actionGoForward->isEnabled() )
		    _ui->actionGoForward->trigger();
                break;

            default:
                QMainWindow::mousePressEvent( event );
                break;
        }
    }
}


// For more MainWindow:: methods, See also:
//
//   - MainWindow.cpp
//   - MainWindowLayout.cpp
//   - MainWindowUnpkg.cpp
