/*
 *   File name: UnreadableDirsWindow.cpp
 *   Summary:	QDirStat file type statistics window
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "UnreadableDirsWindow.h"
#include "Attic.h"
#include "DirTree.h"
#include "DirTreeModel.h"
#include "FormatUtil.h"
#include "HeaderTweaker.h"
#include "QDirStatApp.h"        // dirTreeModel
#include "SelectionModel.h"
#include "SettingsHelpers.h"
#include "Logger.h"
#include "Exception.h"

using namespace QDirStat;


QPointer<UnreadableDirsWindow> UnreadableDirsWindow::_sharedInstance = 0;


UnreadableDirsWindow::UnreadableDirsWindow( QWidget * parent ):
    QDialog ( parent ),
    _ui { new Ui::UnreadableDirsWindow }
{
    // logDebug() << "init" << Qt::endl;;

    CHECK_NEW( _ui );
    _ui->setupUi( this );
    initWidgets();
    readWindowSettings( this, "UnreadableDirsWindow" );

    connect( _ui->treeWidget,	 &QTreeWidget::currentItemChanged,
	     this,		 &UnreadableDirsWindow::selectResult );
}


UnreadableDirsWindow::~UnreadableDirsWindow()
{
    // logDebug() << "destroying" << Qt::endl;;
    writeWindowSettings( this, "UnreadableDirsWindow" );
    delete _ui;
}


UnreadableDirsWindow * UnreadableDirsWindow::sharedInstance()
{
    if ( ! _sharedInstance )
    {
	_sharedInstance = new UnreadableDirsWindow( app()->findMainWindow() );
	CHECK_NEW( _sharedInstance );
    }

    return _sharedInstance;
}


void UnreadableDirsWindow::clear()
{
    _ui->treeWidget->clear();
}


void UnreadableDirsWindow::initWidgets()
{
//    QFont font = _ui->heading->font();
//    font.setBold( true );
//    _ui->heading->setFont( font );

    const QStringList headerLabels = { tr( "Directory" ),
				       tr( "User" ),
				       tr( "Group" ),
				       tr( "Permissions" ),
				       tr( "Perm." )
				     };

    _ui->treeWidget->setIconSize( app()->dirTreeModel()->unreadableDirIcon().actualSize( QSize( 22, 22 ) ) );
    _ui->treeWidget->setColumnCount( headerLabels.size() );
    _ui->treeWidget->setHeaderLabels( headerLabels );

    // Center the column headers except the first
    for ( int col = 1; col < headerLabels.size(); ++col )
	_ui->treeWidget->headerItem()->setTextAlignment( col, Qt::AlignHCenter );

//    _ui->treeWidget->setSortingEnabled( false );
//    _ui->treeWidget->header()->setStretchLastSection( false );
    HeaderTweaker::resizeToContents( _ui->treeWidget->header() );
}


void UnreadableDirsWindow::reject()
{
    deleteLater();
}


void UnreadableDirsWindow::populateSharedInstance( FileInfo * subtree )
{
    if ( ! subtree )
        return;

    sharedInstance()->populate( subtree );
    sharedInstance()->show();
}


void UnreadableDirsWindow::closeSharedInstance()
{
    if ( _sharedInstance )
        _sharedInstance->deleteLater();

    // The QPointer will automatically reset itself
}


void UnreadableDirsWindow::populate( FileInfo * newSubtree )
{
    clear();
    _subtree = newSubtree;

    //logDebug() << "Locating all unreadable dirs below " << _subtree.url() << Qt::endl;;

    populateRecursive( newSubtree ? newSubtree : _subtree() );
    _ui->treeWidget->sortByColumn( 0, Qt::AscendingOrder );

    _ui->totalLabel->setText( "Total: " + _ui->treeWidget->topLevelItemCount() );

    //logDebug() << count << " directories" << Qt::endl;;

    // Make sure something is selected, even if this window is not the active
    // one (for example because the user just clicked on another suffix in the
    // file type stats window). When the window is activated, the tree widget
    // automatically uses the topmost item as the current item, and in the
    // default selection mode, this item is also selected. When the window is
    // not active, this does not happen yet - until the window is activated.
    //
    // In the context of QDirStat, this means that this is also signaled to the
    // SelectionModel, the corresponding branch in the main window's dir tree
    // is opened, and the matching files are selected in the dir tree and in
    // the treemap.
    //
    // It is very irritating if this only happens sometimes - when the "locate
    // files" window is created, but not when it is just populated with new
    // content from the outside (from the file type stats window).
    //
    // So let's make sure the topmost item is always selected.

    _ui->treeWidget->setCurrentItem( _ui->treeWidget->topLevelItem( 0 ) );
}


void UnreadableDirsWindow::populateRecursive( FileInfo * subtree )
{
    if ( ! subtree || ! subtree->isDirInfo() )
	return;

    const DirInfo * dir = subtree->toDirInfo();

    if ( dir->readError() )
    {
	UnreadableDirListItem * searchResultItem =
	    new UnreadableDirListItem( dir->url(),
				       dir->userName(),
				       dir->groupName(),
				       dir->symbolicPermissions(),
				       dir->octalPermissions() );
	CHECK_NEW( searchResultItem );

	_ui->treeWidget->addTopLevelItem( searchResultItem );
    }


    // Recurse through any subdirectories

    FileInfo * child = dir->firstChild();

    while ( child )
    {
	if ( child->isDir() )
	    populateRecursive( child );

	child = child->next();
    }

    if ( dir->attic() )
        populateRecursive( dir->attic() );

    // No need to recurse through dot entries; they can't have any read error
    // or any subdirectory children which might have a read error.
}


void UnreadableDirsWindow::selectResult( QTreeWidgetItem * item )
{
    if ( ! item )
	return;

    const UnreadableDirListItem * searchResult = dynamic_cast<UnreadableDirListItem *>( item );
    CHECK_DYNAMIC_CAST( searchResult, "UnreadableDirListItem" );
    CHECK_PTR( _subtree.tree() );

    FileInfo * dir = _subtree.tree()->locate( searchResult->path() );

    // logDebug() << "Selecting " << searchResult->path() << ": " << dir << Qt::endl;;

    app()->selectionModel()->setCurrentItem( dir,
                                             true ); // select
}






UnreadableDirListItem::UnreadableDirListItem( const QString & path,
					      const QString & userName,
					      const QString & groupName,
					      const QString & symbolicPermissions,
					      const QString & octalPermissions ) :
    QTreeWidgetItem ( QTreeWidgetItem::UserType ),
    _path { path }
{
//    static QIcon lockedDirIcon( ":/icons/tree-medium/unreadable-dir.png" );

    int col = 0;

    set( col, path + "    ", Qt::AlignLeft );
    setIcon( col, app()->dirTreeModel()->unreadableDirIcon() );
    set( ++col, userName, Qt::AlignLeft );
    set( ++col, groupName, Qt::AlignLeft );
    set( ++col, symbolicPermissions, Qt::AlignRight );
    set( ++col, octalPermissions, Qt::AlignRight );
}


void UnreadableDirListItem::set( int col, const QString & text, Qt::Alignment alignment )
{
    setText( col, text );
    setTextAlignment( col, alignment | Qt::AlignVCenter );
}


bool UnreadableDirListItem::operator<( const QTreeWidgetItem & rawOther ) const
{
    // Since this is a reference, the dynamic_cast will throw a std::bad_cast
    // exception if it fails. Not catching this here since this is a genuine
    // error which should not be silently ignored.
    const UnreadableDirListItem & other = dynamic_cast<const UnreadableDirListItem &>( rawOther );

    return path() < other.path();
}

