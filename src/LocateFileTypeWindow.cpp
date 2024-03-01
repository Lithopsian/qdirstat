/*
 *   File name: LocateFileTypeWindow.cpp
 *   Summary:	QDirStat "locate files by type" window
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "LocateFileTypeWindow.h"
#include "QDirStatApp.h"        // SelectionModel
#include "DirTree.h"
#include "DirTreeModel.h"
#include "DotEntry.h"
#include "SelectionModel.h"
#include "SettingsHelpers.h"
#include "HeaderTweaker.h"
#include "FormatUtil.h"
#include "Logger.h"
#include "Exception.h"

using namespace QDirStat;


LocateFileTypeWindow::LocateFileTypeWindow( QWidget * parent ):
    QDialog ( parent ),
    _ui { new Ui::LocateFileTypeWindow }
{
    // logDebug() << "init" << Qt::endl;

    CHECK_NEW( _ui );
    _ui->setupUi( this );
    initWidgets();
    readWindowSettings( this, "LocateFileTypeWindow" );

    connect( _ui->refreshButton, &QPushButton::clicked,
	     this,		 &LocateFileTypeWindow::refresh );

    connect( _ui->treeWidget,	 &QTreeWidget::currentItemChanged,
	     this,		 &LocateFileTypeWindow::selectResult );
}


LocateFileTypeWindow::~LocateFileTypeWindow()
{
    // logDebug() << "destroying" << Qt::endl;
    writeWindowSettings( this, "LocateFileTypeWindow" );
    delete _ui;
}


void LocateFileTypeWindow::clear()
{
    _searchSuffix.clear();
    _ui->treeWidget->clear();
}


void LocateFileTypeWindow::refresh()
{
    populate( searchSuffix(), _subtree() );
}


void LocateFileTypeWindow::initWidgets()
{
    _ui->treeWidget->setColumnCount( SSR_ColumnCount );
    _ui->treeWidget->setHeaderLabels( { tr( "Number" ), tr( "Total Size" ), tr( "Directory" ) } );
    _ui->treeWidget->headerItem()->setTextAlignment( SSR_CountCol, Qt::AlignHCenter );
    _ui->treeWidget->headerItem()->setTextAlignment( SSR_TotalSizeCol, Qt::AlignHCenter );

    _ui->treeWidget->setIconSize( app()->dirTreeModel()->dirIcon().actualSize( QSize( 22, 22 ) ) );

    HeaderTweaker::resizeToContents( _ui->treeWidget->header() );
}

/*
void LocateFileTypeWindow::reject()
{
    deleteLater();
}
*/

QString LocateFileTypeWindow::searchSuffix() const
{
    return QString( "*" ) + _searchSuffix;
}


void LocateFileTypeWindow::populate( const QString & suffix, FileInfo * newSubtree )
{
    clear();

    _searchSuffix = suffix;
    _subtree      = newSubtree;

    if ( _searchSuffix.startsWith( '*' ) )
	_searchSuffix.remove( 0, 1 ); // Remove the leading '*'

    if ( !_searchSuffix.startsWith( '.' ) )
	_searchSuffix.prepend( '.' );

//    logDebug() << "Locating all files ending with \""
//	       << _searchSuffix << "\" below " << _subtree.url() << Qt::endl;

    // For better Performance: Disable sorting while inserting many items
    _ui->treeWidget->setSortingEnabled( false );

    populateRecursive( newSubtree ? newSubtree : _subtree() );

    _ui->treeWidget->setSortingEnabled( true );
    _ui->treeWidget->sortByColumn( SSR_PathCol, Qt::AscendingOrder );

    const int count = _ui->treeWidget->topLevelItemCount();
//    logDebug() << count << " directories" << Qt::endl;

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

    QString intro = ( count == 1 ? tr( "1 directory" ) : tr( "%1 directories" ).arg( count ) );
    _ui->heading->setText( ( intro + " with %2 files in %3" ).arg( searchSuffix() ).arg( _subtree.url() ) );

    _ui->treeWidget->setCurrentItem( _ui->treeWidget->topLevelItem( 0 ) );
}


void LocateFileTypeWindow::populateRecursive( FileInfo * dir )
{
    if ( ! dir )
	return;

    FileInfoSet matches = matchingFiles( dir );

    if ( ! matches.isEmpty() )
    {
	// Create a search result for this path

	FileSize totalSize = 0LL;
	for ( FileInfo * file : matches )
	    totalSize += file->size();

	SuffixSearchResultItem * searchResultItem =
	    new SuffixSearchResultItem( dir->url(), matches.size(), totalSize );
	CHECK_NEW( searchResultItem );

	_ui->treeWidget->addTopLevelItem( searchResultItem );
    }

    // Recurse through any subdirectories
    for ( FileInfo * child = dir->firstChild(); child; child = child->next() )
    {
	if ( child->isDirInfo() )
	    populateRecursive( child );
    }

    // Notice that unlike in FileTypeStats, there is no need to recurse through
    // any dot entries: They are handled in matchingFiles() already.
}


FileInfoSet LocateFileTypeWindow::matchingFiles( FileInfo * item )
{
    FileInfoSet result;

    if ( !item || !item->isDirInfo() )
	return result;

    DirInfo * dir = item->toDirInfo();
    if ( dir->dotEntry() )
	dir = dir->dotEntry();

    for ( FileInfo * child = dir->firstChild(); child; child = child->next() )
    {
	if ( child->isFile() && child->name().endsWith( _searchSuffix, Qt::CaseInsensitive ) )
	    result << child;
    }

    return result;
}


void LocateFileTypeWindow::selectResult( QTreeWidgetItem * item )
{
    if ( !item )
	return;

    SuffixSearchResultItem * searchResult =
	dynamic_cast<SuffixSearchResultItem *>( item );
    CHECK_DYNAMIC_CAST( searchResult, "SuffixSearchResultItem" );
    CHECK_PTR( _subtree.tree() );

    FileInfo * dir = _subtree.tree()->locate( searchResult->path() );
    const FileInfoSet matches = matchingFiles( dir );

    // logDebug() << "Selecting " << searchResult->path() << " with " << matches.size() << " matches" << Qt::endl;

    if ( !matches.isEmpty() )
	app()->selectionModel()->setCurrentItem( matches.first(), true );

    app()->selectionModel()->setSelectedItems( matches );
}






SuffixSearchResultItem::SuffixSearchResultItem( const QString & path,
						int		count,
						FileSize	totalSize ):
    QTreeWidgetItem ( QTreeWidgetItem::UserType ),
    _path { path },
    _count { count },
    _totalSize { totalSize }
{
    set( SSR_CountCol,     QString::number( count ), Qt::AlignRight );
    set( SSR_TotalSizeCol, formatSize( totalSize ),  Qt::AlignRight );
    set( SSR_PathCol,      path,                     Qt::AlignLeft  );

    setIcon( SSR_PathCol,  QIcon( app()->dirTreeModel()->dirIcon() ) );
}


void SuffixSearchResultItem::set( int col, const QString & text, Qt::Alignment alignment )
{
    setText( col, text );
    setTextAlignment( col, alignment | Qt::AlignVCenter );
}


bool SuffixSearchResultItem::operator<( const QTreeWidgetItem & rawOther ) const
{
    // Since this is a reference, the dynamic_cast will throw a std::bad_cast
    // exception if it fails. Not catching this here since this is a genuine
    // error which should not be silently ignored.
    const SuffixSearchResultItem & other = dynamic_cast<const SuffixSearchResultItem &>( rawOther );

    int col = treeWidget() ? treeWidget()->sortColumn() : SSR_PathCol;

    switch ( col )
    {
	case SSR_PathCol:      return _path	 < other.path();
	case SSR_CountCol:     return _count	 < other.count();
	case SSR_TotalSizeCol: return _totalSize < other.totalSize();
	default:	       return QTreeWidgetItem::operator<( rawOther );
    }
}

