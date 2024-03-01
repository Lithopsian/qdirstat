/*
 *   File name: DirTreeView.cpp
 *   Summary:	Tree view widget for directory tree
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QMenu>
#include <QKeyEvent>

#include "DirTreeView.h"
#include "DirTreeModel.h"
#include "SelectionModel.h"
#include "ActionManager.h"
#include "CleanupCollection.h"
#include "PercentBar.h"
#include "SizeColDelegate.h"
#include "HeaderTweaker.h"
#include "DirTree.h"
#include "FormatUtil.h"
#include "Exception.h"
#include "Logger.h"

using namespace QDirStat;


DirTreeView::DirTreeView( QWidget * parent ):
    QTreeView ( parent ),
    _percentBarDelegate { new PercentBarDelegate( this, PercentBarCol, 0, 2 ) },
    _sizeColDelegate { new SizeColDelegate( this ) },
    _headerTweaker { new HeaderTweaker( header(), this ) },
    _cleanupCollection { nullptr }
{
    CHECK_NEW( _headerTweaker );
    CHECK_NEW( _percentBarDelegate );
    CHECK_NEW( _sizeColDelegate );

    setItemDelegateForColumn( PercentBarCol, _percentBarDelegate );
    setItemDelegateForColumn( SizeCol,       _sizeColDelegate );

//    setRootIsDecorated( true );
//    setSortingEnabled( true );
//    setSelectionMode( ExtendedSelection );
//    setContextMenuPolicy( Qt::CustomContextMenu );
//    setTextElideMode( Qt::ElideMiddle );
//    setUniformRowHeights( true ); // Important for very large directories

    connect( this, &DirTreeView::customContextMenuRequested,
	     this, &DirTreeView::contextMenu );
}


DirTreeView::~DirTreeView()
{
    delete _headerTweaker;
    delete _percentBarDelegate;
    delete _sizeColDelegate;
}


void DirTreeView::currentChanged( const QModelIndex & current,
				  const QModelIndex & oldCurrent )
{
    //logDebug() << "New current " << current << ", old current" << oldCurrent << Qt::endl;
    QTreeView::currentChanged( current, oldCurrent );
    scrollTo( current );
}


void DirTreeView::contextMenu( const QPoint & pos )
{
    QModelIndex index = indexAt( pos );
    if ( ! index.isValid() )
    {
	logDebug() << "No item at this position" << Qt::endl;
	return;
    }

    QMenu menu;

    // The first action should not be a destructive one like "move to trash":
    // It's just too easy to select and execute the first action accidentially,
    // especially on a laptop touchpad.
    const QStringList actions1 = { "actionGoUp",
                                   "actionGoToToplevel",
                                 };
    ActionManager::instance()->addActions( &menu, actions1 );

    const QStringList actions2 = { "actionStopReading",
	                           "actionRefreshAll",
                                   "---",
	                           "actionRefreshSelected",
	                           "actionReadExcluded",
	                           "actionContinueReading",
                                   "---",
                                   "actionCopyPath",
                                   "actionMoveToTrash",
                                   "---",
	                         };
    ActionManager::instance()->addEnabledActions( &menu, actions2 );

    // User-defined cleanups
    if ( _cleanupCollection )
	_cleanupCollection->addEnabledToMenu( &menu );

    // Submenu for the auxiliary views to keep the context menu short.
    //
    // Those actions are strictly speaking irrelevant in most cases, and so
    // they should be omitted from a context menu. But here this serves for
    // discoverability: Most users don't even know that it is an option to
    // start any of those views from a subdirectory in the tree. As a
    // compromise to keep the context menu short, those auxiliary views go to a
    // submenu of the context menu. Submenus in context menus are generally
    // also discouraged, but here discoverability of these features is more
    // important.

    FileInfo * item = static_cast<FileInfo *>( index.internalPointer() );
    CHECK_MAGIC( item );
    if ( item->isDirInfo() )    // Not for files, symlinks etc.
    {
        menu.addSeparator();
        QMenu * subMenu = menu.addMenu( tr( "View in" ) );
        QStringList actions = { "actionFileSizeStats",
                                "actionFileTypeStats",
                                "actionFileAgeStats",
                              };
        ActionManager::instance()->addActions( subMenu, actions );
    }

    menu.exec( mapToGlobal( pos ) );
}


QModelIndexList DirTreeView::expandedIndexes() const
{
    if ( !model() )
	return QModelIndexList();

    DirTreeModel * dirTreeModel = dynamic_cast<DirTreeModel *>( model() );
    if ( !dirTreeModel )
    {
	logError() << "Wrong model type to get this information" << Qt::endl;
	return QModelIndexList();
    }

    QModelIndexList expandedList;
    const auto indexList = dirTreeModel->persistentIndexList();
    for ( const QModelIndex & index : indexList )
    {
	if ( isExpanded( index ) )
	    expandedList << index;
    }

    return expandedList;
}


void DirTreeView::closeAllExcept( const QModelIndex & )
{
//    QModelIndexList branchesToClose = expandedIndexes();

    // Remove all ancestors of 'branch' from branchesToClose

//    for ( QModelIndex index = branch; index.isValid(); index = index.parent() )
    {
	// logDebug() << "Not closing " << index << Qt::endl;
//	branchesToClose.removeAll( index );
    }

    // Close all items in branchesToClose

//    foreach ( index, branchesToClose ) // too slow!!!!
    {
	// logDebug() << "Closing " << index << Qt::endl;
	collapseAll();
//	collapse( index );
    }

    // This re-opens the relevant branch
    scrollTo( currentIndex(), QAbstractItemView::PositionAtCenter );
}


void DirTreeView::setExpanded( FileInfo * item, bool expanded )
{
    const DirTreeModel * dirTreeModel = dynamic_cast<DirTreeModel *>( model() );
    if ( !dirTreeModel )
    {
	logError() << "Wrong model type" << Qt::endl;
        return;
    }

    const QModelIndex index = dirTreeModel->modelIndex( item );
    if ( index.isValid() )
        QTreeView::setExpanded( index, expanded );
}


void DirTreeView::mousePressEvent( QMouseEvent * event )
{
    // Leave the the back / forward buttons on the mouse to act like the
    // history back / forward buttons in the tool bar.
    //
    // By default, the QTreeView parent class uses them to act as
    // cursor up / cursor down in the tree which defeats the idea of
    // using them as history consistently throughout the application,
    // making those mouse buttons pretty much unusable.
    //
    // So this makes sure those events are immediately propagated up to
    // the parent widget.
    if ( event && (event->button() == Qt::BackButton || event->button() == Qt::ForwardButton ) )
    {
	event->ignore();
	return;
    }

    QTreeView::mousePressEvent( event );
}

/*
void DirTreeView::keyPressEvent( QKeyEvent * event )
{
    // By default, this opens all tree branches which completely
    // kills our performance, negating all our lazy sorting in
    // each branch in the DirTreeModel / DirInfo classes.
    //
    // So let's just ignore this key; we have better alternatives
    // with "Tree" -> "Expand to Level" -> "Level 0" .. "Level 5".
    if ( event && event->key() == Qt::Key_Asterisk )
	return;

    QTreeView::keyPressEvent( event );
}
*/
