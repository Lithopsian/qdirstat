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
#include "DiscoverActions.h"
#include "HistoryButtons.h"
#include "QDirStatApp.h"
#include "SelectionModel.h"
#include "SysUtil.h"
#include "Version.h"
#include "Logger.h"


using namespace QDirStat;


void MainWindow::connectMenuActions()
{
    _ui->actionGoUp->setShortcutContext ( Qt::ApplicationShortcut );
    _ui->actionGoToToplevel->setShortcutContext( Qt::ApplicationShortcut );
    _ui->actionWhatsNew->setStatusTip( RELEASE_URL ); // defined in Version.h

    // CleanupCollection::add() handles the cleanup actions

    connectTriggerActions();
    connectToggleActions();
    connectTreemapActions();
    connectHistoryButtons();
    connectDiscoverActions();
    connectSelectionModelActions();
}


void MainWindow::connectTriggerActions()
{
    QList<QPair<QAction *, void( MainWindow::* )( void )>> actions(
    {
        // File menu
        { _ui->actionOpenDir,            &MainWindow::askOpenDir },
        { _ui->actionOpenPkg,            &MainWindow::askOpenPkg },
        { _ui->actionOpenUnpkg,          &MainWindow::askOpenUnpkg },
        { _ui->actionRefreshAll,         &MainWindow::refreshAll },
        { _ui->actionRefreshSelected,    &MainWindow::refreshSelected },
        { _ui->actionReadExcluded,       &MainWindow::refreshSelected },
        { _ui->actionContinueReading,    &MainWindow::refreshSelected },
        { _ui->actionStopReading,        &MainWindow::stopReading },
        { _ui->actionAskReadCache,       &MainWindow::askReadCache },
        { _ui->actionAskWriteCache,      &MainWindow::askWriteCache },
        { _ui->actionQuit,               &MainWindow::quit },
        // Edit menu
        { _ui->actionCopyPath,           &MainWindow::copyCurrentPathToClipboard },
        { _ui->actionMoveToTrash,        &MainWindow::moveToTrash },
        { _ui->actionFindFiles,          &MainWindow::askFindFiles },
        { _ui->actionConfigure,          &MainWindow::openConfigDialog },
        // View menu
        { _ui->actionLayout1,            &MainWindow::changeLayoutSlot },
        { _ui->actionLayout2,            &MainWindow::changeLayoutSlot },
        { _ui->actionLayout3,            &MainWindow::changeLayoutSlot },
        { _ui->actionFileSizeStats,      &MainWindow::showFileSizeStats },
        { _ui->actionFileTypeStats,      &MainWindow::showFileTypeStats },
        { _ui->actionFileAgeStats,       &MainWindow::showFileAgeStats },
        { _ui->actionShowFilesystems,    &MainWindow::showFilesystems },
        // Go menu
        { _ui->actionGoUp,               &MainWindow::navigateUp },
        { _ui->actionGoToToplevel,       &MainWindow::navigateToToplevel },
        // Help menu
        { _ui->actionHelp,               &MainWindow::openActionUrl },
        { _ui->actionAbout,              &MainWindow::showAboutDialog },
        { _ui->actionAboutQt,            &MainWindow::showAboutQtDialog },
        { _ui->actionTreemapHelp,        &MainWindow::openActionUrl },
        { _ui->actionPkgViewHelp,        &MainWindow::openActionUrl },
        { _ui->actionUnpkgViewHelp,      &MainWindow::openActionUrl },
        { _ui->actionFileAgeStatsHelp,   &MainWindow::openActionUrl },
        { _ui->actionWhatsNew,           &MainWindow::openActionUrl },
        { _ui->actionCantMoveDirToTrash, &MainWindow::openActionUrl },
        { _ui->actionBtrfsSizeReporting, &MainWindow::openActionUrl },
        { _ui->actionShadowedByMount,    &MainWindow::openActionUrl },
        { _ui->actionHeadlessServers,    &MainWindow::openActionUrl },
        { _ui->actionDonate,             &MainWindow::showDonateDialog },
    } );

    for ( auto & action : actions )
        connect( action.first, &QAction::triggered, this, action.second );

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
}


void MainWindow::mapTreeExpandAction( QAction * action, int level )
{
    connect( action, &QAction::triggered,
             this,   [ this, level ]() { expandTreeToLevel( level ); } );
}


void MainWindow::connectToggleActions()
{
    QList<QPair<QAction *, void( MainWindow::* )( bool )>> actions(
    {
        // View menu
        { _ui->actionShowBreadcrumbs,    &MainWindow::setBreadcrumbsVisible },
        { _ui->actionShowDetailsPanel,   &MainWindow::setDetailsPanelVisible },
        { _ui->actionShowTreemap,        &MainWindow::showTreemapView },
        { _ui->actionTreemapAsSidePanel, &MainWindow::treemapAsSidePanel },
        // Invisible
        { _ui->actionVerboseSelection,   &MainWindow::toggleVerboseSelection },
    } );

    for ( auto action : actions )
        connect( action.first, &QAction::toggled, this, action.second );
}


void MainWindow::connectTreemapActions()
{
    QList<QPair<QAction *, void( TreemapView::* )( void )>> actions(
    {
        { _ui->actionTreemapZoomTo,    &TreemapView::zoomTo },
        { _ui->actionTreemapZoomIn,    &TreemapView::zoomIn },
        { _ui->actionTreemapZoomOut,   &TreemapView::zoomOut },
        { _ui->actionResetTreemapZoom, &TreemapView::resetZoom },
        { _ui->actionTreemapRebuild,   &TreemapView::rebuildTreemap },
    } );

    for ( auto action : actions )
        connect( action.first, &QAction::triggered, _ui->treemapView, action.second );
}


void MainWindow::connectDiscoverActions()
{
    QList<QPair<QAction *, void( DiscoverActions::* )( void )>> actions(
    {
        { _ui->actionDiscoverLargestFiles,    &DiscoverActions::discoverLargestFiles },
        { _ui->actionDiscoverNewestFiles,     &DiscoverActions::discoverNewestFiles },
        { _ui->actionDiscoverOldestFiles,     &DiscoverActions::discoverOldestFiles },
        { _ui->actionDiscoverHardLinkedFiles, &DiscoverActions::discoverHardLinkedFiles },
        { _ui->actionDiscoverBrokenSymLinks,  &DiscoverActions::discoverBrokenSymLinks },
        { _ui->actionDiscoverSparseFiles,     &DiscoverActions::discoverSparseFiles },
    } );

    for ( auto action : actions )
        connect( action.first, &QAction::triggered, _discoverActions, action.second );
}


void MainWindow::connectHistoryButtons()
{
    connect( _ui->actionGoBack,    &QAction::triggered,
	     _historyButtons,      &HistoryButtons::historyGoBack );

    connect( _ui->actionGoForward, &QAction::triggered,
             _historyButtons,      &HistoryButtons::historyGoForward );
}


void MainWindow::connectSelectionModelActions()
{
    // Invisible debug actions
    addAction( _ui->actionVerboseSelection );    // Shift-F7
    addAction( _ui->actionDumpSelection );       // F7

    connect( _ui->actionDumpSelection, &QAction::triggered,
             app()->selectionModel(),  &SelectionModel::dumpSelectedItems );
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
