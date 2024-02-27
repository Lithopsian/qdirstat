/*
 *   File name: FileTypeStatsWindow.cpp
 *   Summary:	QDirStat file type statistics window
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <algorithm>

#include <QKeyEvent>
#include <QMenu>

#include "FileTypeStatsWindow.h"
#include "FileTypeStats.h"
#include "FileSizeStatsWindow.h"
#include "LocateFileTypeWindow.h"
#include "MimeCategory.h"
#include "SettingsHelpers.h"
#include "HeaderTweaker.h"
//#include "QDirStatApp.h"
#include "FormatUtil.h"
#include "Logger.h"
#include "Exception.h"


// Number of suffixes in the "other" category
#define TOP_X	20

using namespace QDirStat;


QPointer<LocateFileTypeWindow> FileTypeStatsWindow::_locateFileTypeWindow = 0;
QPointer<FileTypeStatsWindow>  FileTypeStatsWindow::_sharedInstance       = 0;


FileTypeStatsWindow::FileTypeStatsWindow( QWidget * parent ):
    QDialog( parent ),
    _ui( new Ui::FileTypeStatsWindow )
{
    // logDebug() << "init" << Qt::endl;

    CHECK_NEW( _ui );
    _ui->setupUi( this );
    initWidgets();
    readWindowSettings( this, "FileTypeStatsWindow" );

    connect( _ui->treeWidget,	   &QTreeWidget::currentItemChanged,
	     this,		   &FileTypeStatsWindow::enableActions );

    connect( _ui->treeWidget,	   &QTreeWidget::customContextMenuRequested,
	      this,		   &FileTypeStatsWindow::contextMenu);

    connect( _ui->treeWidget,	   &QTreeWidget::itemDoubleClicked,
	     this,		   &FileTypeStatsWindow::locateCurrentFileType );

//    connect( _ui->treeWidget,	   &QTreeWidget::itemPressed,
//	     this,		   &FileTypeStatsWindow::mouseClick );

    connect( _ui->refreshButton,   &QAbstractButton::clicked,
	     this,		   &FileTypeStatsWindow::refresh );

    connect( _ui->locateButton,	   &QAbstractButton::clicked,
	     _ui->actionLocate,	   &QAction::triggered );

    connect( _ui->actionLocate,	   &QAction::triggered,
	     this,		   &FileTypeStatsWindow::locateCurrentFileType );

    connect( _ui->sizeStatsButton, &QAbstractButton::clicked,
	     _ui->actionSizeStats, &QAction::triggered );

    connect( _ui->actionSizeStats, &QAction::triggered,
	     this,		   &FileTypeStatsWindow::sizeStatsForCurrentFileType );

    _stats = new FileTypeStats( this );
    CHECK_NEW( _stats );
}


FileTypeStatsWindow::~FileTypeStatsWindow()
{
    // logDebug() << "destroying" << Qt::endl;
    writeWindowSettings( this, "FileTypeStatsWindow" );
    delete _ui;
}


void FileTypeStatsWindow::clear()
{
    _stats->clear();
    _ui->treeWidget->clear();
    enableActions(0);
}


void FileTypeStatsWindow::initWidgets()
{
//    QFont font = _ui->heading->font();
//    font.setBold( true );
//    _ui->heading->setFont( font );

    _ui->treeWidget->setColumnCount( FT_ColumnCount );
    _ui->treeWidget->setHeaderLabels( { tr( "Name" ), tr( "Number" ), tr( "Total Size" ), tr( "Percentage" ) } );
    _ui->treeWidget->header()->setStretchLastSection( false );
    HeaderTweaker::resizeToContents( _ui->treeWidget->header() );
}


void FileTypeStatsWindow::refresh()
{
    populate( _subtree() );
}


FileTypeStatsWindow * FileTypeStatsWindow::sharedInstance( QWidget * mainWindow )
{
    if ( ! _sharedInstance )
    {
	_sharedInstance = new FileTypeStatsWindow( mainWindow );
	CHECK_NEW( _sharedInstance );
    }

    return _sharedInstance;
}


void FileTypeStatsWindow::populateSharedInstance( QWidget * mainWindow, FileInfo * subtree )
{
    if ( ! subtree )
        return;

    sharedInstance( mainWindow )->populate( subtree );
    sharedInstance( mainWindow )->show();
}


void FileTypeStatsWindow::populate( FileInfo * newSubtree )
{
    clear();
    _subtree = newSubtree;
    _stats->calc( newSubtree ? newSubtree : _subtree() );

    _ui->heading->setText( tr( "File type statistics for %1" ).arg( _subtree.url() ) );

    // Don't sort until all items are added
    _ui->treeWidget->setSortingEnabled( false );

    //
    // Create toplevel items for the categories
    //
    QMap<const MimeCategory *, CategoryFileTypeItem *> categoryItem;
    CategoryFileTypeItem * otherCategoryItem = 0;

    for ( CategoryFileSizeMapIterator it = _stats->categorySumBegin(); it != _stats->categorySumEnd(); ++it )
    {
	const MimeCategory * category = it.key();

	if ( category )
	{
	    //
            // Add a category item
            //
	    FileSize sum   = it.value();
            int	     count = _stats->categoryCount( category );
	    CategoryFileTypeItem * catItem = addCategoryItem( category->name(), count, sum );
	    categoryItem[ category ] = catItem;

	    if ( category == _stats->otherCategory() )
		otherCategoryItem = catItem;
            else if ( _stats->categoryNonSuffixRuleCount( category ) > 0 )
	    {
		// Add an <Other> item below the category for files
		// matching any non-suffix rules
		SuffixFileTypeItem * item = addNonSuffixRuleItem( category );
		catItem->addChild( item );
	    }
	}
    }

    // Prepare to collect items for a category "other"
    QList<FileTypeItem *> otherItems;
    int	     otherCount = 0;
    FileSize otherSum	= 0LL;

    //
    // Create items for each individual suffix (below a category)
    //
    for ( StringFileSizeMapIterator it = _stats->suffixSumBegin();
	  it != _stats->suffixSumEnd();
	  ++it )
    {
	const QString suffix = it.key().first;
	const MimeCategory * category = it.key().second;
	const FileSize sum = it.value();
	const int count = _stats->suffixCount( suffix, category );
	SuffixFileTypeItem * item = addSuffixFileTypeItem( suffix, count, sum );

	if ( category )
	{
	    QTreeWidgetItem * parentItem = categoryItem.value( category, 0 );

	    if ( parentItem )
		parentItem->addChild( item );
	    else
	    {
		logError() << "ERROR: No parent category item for " << suffix << Qt::endl;
		otherItems << item;
		otherCount += count;
		otherSum   += sum;
	    }
	}
	else // No category for this suffix
	{
	    logDebug() << "other" << Qt::endl;
	    otherItems << item;
	    otherCount += count;
	    otherSum   += sum;
	}
    }

    // Put remaining "other" items below a separate category

    if ( ! otherItems.isEmpty() )
    {
	otherCategoryItem = addCategoryItem( _stats->otherCategory()->name(),
                                             otherCount,
                                             otherSum );

	const QString name = otherItems.size() > TOP_X ? tr( "Other (Top %1)" ).arg( TOP_X ) : tr( "Other" );
        otherCategoryItem->setText( 0, name );

	addTopXOtherItems( otherCategoryItem, otherItems );
    }

    _ui->treeWidget->setSortingEnabled( true );
    _ui->treeWidget->sortByColumn( FT_TotalSizeCol, Qt::DescendingOrder );
}


CategoryFileTypeItem * FileTypeStatsWindow::addCategoryItem( const QString & name, int count, FileSize sum )
{
    const double percentage = _stats->percentage( sum );

    CategoryFileTypeItem * item = new CategoryFileTypeItem( name, count, sum, percentage );
    CHECK_NEW( item );

    _ui->treeWidget->addTopLevelItem( item );
    item->setBold();

    return item;
}


SuffixFileTypeItem * FileTypeStatsWindow::addNonSuffixRuleItem( const MimeCategory * category )
{
    const QString  suffix	= NON_SUFFIX_RULE;
    const FileSize sum		= _stats->categoryNonSuffixRuleSum  ( category );
    const int	   count	= _stats->categoryNonSuffixRuleCount( category );

    SuffixFileTypeItem * item = addSuffixFileTypeItem( suffix, count, sum );

    return item;
}


SuffixFileTypeItem * FileTypeStatsWindow::addSuffixFileTypeItem( const QString & suffix,
                                                                 int             count,
                                                                 FileSize        sum     )
{
    const double percentage = _stats->percentage( sum );

    SuffixFileTypeItem * item = new SuffixFileTypeItem( suffix, count, sum, percentage );
    CHECK_NEW( item );

    return item;
}


void FileTypeStatsWindow::addTopXOtherItems( CategoryFileTypeItem  * otherCategoryItem,
                                             QList<FileTypeItem *> & otherItems        )
{
    FileTypeItemCompare cmp;
    std::sort( otherItems.begin(), otherItems.end(), cmp );

    const int top_x = qMin( TOP_X, otherItems.size() );

    for ( int i=0; i < top_x; ++i )
    {
        // Take the X first items out of the otherItems list
        // and add them as children of the "Other" category

        FileTypeItem * item = otherItems.takeFirst();
        otherCategoryItem->addChild( item );
    }

    if ( ! otherItems.empty() )
    {
#if 1
        QStringList suffixes;

        for ( FileTypeItem * item : otherItems )
            suffixes << item->text(0);

        logDebug() << "Discarding " << (int)otherItems.size()
                   << " suffixes below <other>: "
                   << suffixes.join( ", " )
                   << Qt::endl;
#endif
        // Delete all items that are not in the top X
        qDeleteAll( otherItems );
    }
}


void FileTypeStatsWindow::locateCurrentFileType()
{
    const QString suffix = currentSuffix();

    if ( suffix.isEmpty() )
    {
	if ( _locateFileTypeWindow )
	    _locateFileTypeWindow->hide();

	return;
    }

    // logDebug() << "Locating " << current->suffix() << Qt::endl;

    if ( ! _locateFileTypeWindow )
    {
	_locateFileTypeWindow = new LocateFileTypeWindow( qobject_cast<QWidget *>( parent() ) );
	CHECK_NEW( _locateFileTypeWindow );
	_locateFileTypeWindow->show();

	// Not using 'this' as parent so the user can close the file types
	// stats window, but keep the locate files window open; if 'this' were
	// used, the destructor of the file type stats window would
	// automatically delete the locate files window, too since it would be
	// part of its children hierarchy.
	//
	// On the downside, that means we have to actively raise() it because
	// it might get hidden behind the stats window.
    }
    else // Reusing existing window
    {
	_locateFileTypeWindow->show();
	_locateFileTypeWindow->raise();
    }

    _locateFileTypeWindow->populate( suffix, _subtree() );
}


void FileTypeStatsWindow::sizeStatsForCurrentFileType()
{
    const QString suffix = currentSuffix().toLower();
    FileInfo * dir = _subtree();

    if ( suffix.isEmpty() || ! dir )
        return;

    //logDebug() << "Size stats for " << suffix << Qt::endl;

    FileSizeStatsWindow::populateSharedInstance( this->parentWidget(), dir, suffix );
}


QString FileTypeStatsWindow::currentSuffix() const
{
    const SuffixFileTypeItem * current = dynamic_cast<SuffixFileTypeItem *>( _ui->treeWidget->currentItem() );
    if ( current && current->suffix() != NO_SUFFIX && current->suffix() != NON_SUFFIX_RULE )
	return current->suffix();

    return QString();
}


void FileTypeStatsWindow::enableActions( QTreeWidgetItem * currentItem )
{
    bool enabled = false;

    if ( currentItem )
    {
	const SuffixFileTypeItem * suffixItem = dynamic_cast<SuffixFileTypeItem *>( currentItem );

	enabled = suffixItem && suffixItem->suffix() != NO_SUFFIX && suffixItem->suffix() != NON_SUFFIX_RULE;
    }

//    _ui->actionLocate->setEnabled( enabled );
//    _ui->actionSizeStats->setEnabled( enabled );
//    _ui->menuButton->setEnabled( enabled );
    _ui->locateButton->setEnabled( enabled );
    _ui->sizeStatsButton->setEnabled( enabled );
}


void FileTypeStatsWindow::reject()
{
    deleteLater();
}


void FileTypeStatsWindow::contextMenu( const QPoint & pos )
{
    // See if the right click was actually on an item
    if ( !_ui->treeWidget->itemAt( pos ) )
	return;

    // The clicked item will always be the current item now
    QString suffix = currentSuffix();
    if ( suffix.isEmpty() )
	return;

    suffix.remove( 0, 1 );
    _ui->actionLocate->setText( tr( "&Locate files with suffix " ) + suffix );
    _ui->actionSizeStats->setText( tr( "Si&ze statistics for suffix " ) + suffix );

    QMenu menu;
    menu.addAction( _ui->actionLocate );
    menu.addAction( _ui->actionSizeStats );

    menu.exec( _ui->treeWidget->mapToGlobal( pos ) );
}


void FileTypeStatsWindow::keyPressEvent( QKeyEvent * event )
{
    if ( !QList<int>( { Qt::Key_Return, Qt::Key_Enter } ).contains( event->key() ) )
    {
	QDialog::keyPressEvent( event );
	return;
    }

    QTreeWidgetItem * item = _ui->treeWidget->currentItem();
    CategoryFileTypeItem * categoryItem = dynamic_cast<CategoryFileTypeItem *>( item );
    if ( categoryItem )
	// For category headings, toggle the expanded state
	categoryItem->setExpanded( !categoryItem->isExpanded() );
    else
	// Enter/return on other items may activate the locate files window
	locateCurrentFileType();
}



SuffixFileTypeItem::SuffixFileTypeItem( const QString & suffix,
					int		count,
					FileSize	totalSize,
					float		percentage ):
    FileTypeItem( "*." + suffix,
		  count,
		  totalSize,
		  percentage ),
    _suffix( suffix )
{
	if ( suffix == NO_SUFFIX )
	    setText( FT_NameCol,  QObject::tr( "<No Extension>" ) );
	else if ( suffix == NON_SUFFIX_RULE )
	    setText( FT_NameCol,  QObject::tr( "<Other>" ) );
	else
	    _suffix = "*." + suffix;
}


FileTypeItem::FileTypeItem( const QString & name,
			    int		    count,
			    FileSize	    totalSize,
			    float	    percentage ):
    QTreeWidgetItem( QTreeWidgetItem::UserType ),
    _name( name ),
    _count( count ),
    _totalSize( totalSize ),
    _percentage( percentage )
{
    QString percentStr;
    percentStr.setNum( percentage, 'f', 2 );
    percentStr += "%";

    setText( FT_NameCol,       name );
    setText( FT_CountCol,      QString( "%1" ).arg( count ) );
    setText( FT_TotalSizeCol,  formatSize( totalSize ) );
    setText( FT_PercentageCol, percentStr );

    setTextAlignment( FT_NameCol,	Qt::AlignLeft  );
    setTextAlignment( FT_CountCol,	Qt::AlignRight );
    setTextAlignment( FT_TotalSizeCol,	Qt::AlignRight );
    setTextAlignment( FT_PercentageCol, Qt::AlignRight );
}


bool FileTypeItem::operator<(const QTreeWidgetItem & rawOther) const
{
    // Since this is a reference, the dynamic_cast will throw a std::bad_cast
    // exception if it fails. Not catching this here since this is a genuine
    // error which should not be silently ignored.
    const FileTypeItem & other = dynamic_cast<const FileTypeItem &>( rawOther );

    const int col = treeWidget() ? treeWidget()->sortColumn() : FT_TotalSizeCol;

    switch ( col )
    {
	case FT_NameCol:	return name()	    < other.name();
	case FT_CountCol:	return count()	    < other.count();
	case FT_TotalSizeCol:	return totalSize()  < other.totalSize();
	case FT_PercentageCol:	return percentage() < other.percentage();
	default:		return QTreeWidgetItem::operator<( rawOther );
    }
}


void FileTypeItem::setBold()
{
    QFont boldFont = font( 0 );
    boldFont.setBold( true );

    for ( int col=0; col < FT_ColumnCount; ++col )
	setFont( col, boldFont );
}
