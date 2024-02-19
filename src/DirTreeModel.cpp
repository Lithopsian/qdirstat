/*
 *   File name: DirTreeModel.cpp
 *   Summary:	Qt data model for directory tree
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QStringBuilder>

#include "DirTreeModel.h"
#include "DirInfo.h"
#include "DirTree.h"
#include "FileInfoIterator.h"
#include "DataColumns.h"
#include "SelectionModel.h"
#include "Settings.h"
#include "SettingsHelpers.h"
#include "Logger.h"
#include "FormatUtil.h"
#include "Exception.h"
#include "DebugHelpers.h"

// Number of clusters up to which a file will be considered small and will also
// display the allocated size like (4k).
#define SMALL_FILE_CLUSTERS     2

// Used used percent below which a small file will also display the allocated size
// like (4k)
#define SMALL_FILE_SHOW_ALLOC_THRESHOLD         75

using namespace QDirStat;


DirTreeModel::DirTreeModel( QObject * parent ):
    QAbstractItemModel( parent ),
    _tree(0),
    _selectionModel(0),
    _readJobsCol( PercentBarCol ),
//    _updateTimerMillisec( 250 ),
//    _slowUpdateMillisec( 3000 ),
    _slowUpdate( false ),
    _sortCol( NameCol ),
    _sortOrder( Qt::AscendingOrder ),
    _removingRows( false )
{
    readSettings();
    createTree();
    loadIcons();
    _updateTimer.setInterval( _updateTimerMillisec );

    connect( &_updateTimer, SIGNAL( timeout()		 ),
	     this,	    SLOT  ( sendPendingUpdates() ) );
}


DirTreeModel::~DirTreeModel()
{
    writeSettings();

    if ( _tree )
	delete _tree;
}


void DirTreeModel::updateSettings( bool crossFilesystems,
                                   bool useBoldForDominant,
				   const QString & treeIconDir,
				   int updateTimerMillisec )
{
    // Avoid overwriting the dialog setting unless there is an actual change
    if ( _crossFilesystems != crossFilesystems )
	_tree->setCrossFilesystems( _crossFilesystems );
    _crossFilesystems = crossFilesystems;
    _useBoldForDominantItems = useBoldForDominant;
    _treeIconDir = treeIconDir;
    _updateTimerMillisec = updateTimerMillisec;
    _updateTimer.setInterval( updateTimerMillisec );

    loadIcons();
    emit layoutChanged();
}


void DirTreeModel::readSettings()
{
    Settings settings;
    settings.beginGroup( "DirectoryTree" );

    _crossFilesystems		= settings.value( "CrossFilesystems", false ).toBool();
    _useBoldForDominantItems	= settings.value( "UseBoldForDominant",  true ).toBool();
    FileInfo::setIgnoreHardLinks( settings.value( "IgnoreHardLinks",  false ).toBool() );
    _treeIconDir		= settings.value( "TreeIconDir", ":/icons/tree-medium/" ).toString();
    _updateTimerMillisec	= settings.value( "UpdateTimerMillisec", 250 ).toInt();
    _slowUpdateMillisec		= settings.value( "SlowUpdateMillisec", 3000 ).toInt();

    settings.endGroup();

    if ( usingLightTheme() )
    {
	settings.beginGroup( "TreeTheme-light" );

	_dirReadErrColor     = readColorEntry( settings, "DirReadErrColor",	QColor( Qt::red		 ) );
	_subtreeReadErrColor = readColorEntry( settings, "SubtreeReadErrColor", QColor( 0xa0, 0x00, 0x00 ) );

	settings.endGroup();
    }
    else // dark theme
    {
	settings.beginGroup( "TreeTheme-dark" );

	_dirReadErrColor     = readColorEntry( settings, "DirReadErrColor",	QColor( Qt::red		 ) );
	_subtreeReadErrColor = readColorEntry( settings, "SubtreeReadErrColor", QColor( Qt::yellow	 ) );

	settings.endGroup();
    }
}


void DirTreeModel::writeSettings()
{
    Settings settings;
    settings.beginGroup( "DirectoryTree" );

    settings.setValue( "SlowUpdateMillisec", _slowUpdateMillisec  );

    settings.setValue( "CrossFilesystems",    _crossFilesystems );
    settings.setValue( "UseBoldForDominant",  _useBoldForDominantItems );
    settings.setValue( "IgnoreHardLinks",     FileInfo::ignoreHardLinks() );
    settings.setValue( "TreeIconDir",	     _treeIconDir		 );
    settings.setValue( "UpdateTimerMillisec", _updateTimerMillisec	 );

    settings.endGroup();


    settings.beginGroup( usingLightTheme() ? "TreeTheme-light" : "TreeTheme-dark" );

    writeColorEntry( settings, "DirReadErrColor",     _dirReadErrColor	   );
    writeColorEntry( settings, "SubtreeReadErrColor", _subtreeReadErrColor );

    settings.endGroup();
}


void DirTreeModel::setSlowUpdate()
{
    logInfo() << "Display update every " << _updateTimer.interval() << " millisec" << Qt::endl;

    _slowUpdate = true;
    _updateTimer.setInterval( _slowUpdateMillisec );
}


void DirTreeModel::createTree()
{
    _tree = new DirTree();
    CHECK_NEW( _tree );

    _tree->setCrossFilesystems( _crossFilesystems );

    connect( _tree, SIGNAL( startingReading() ),
	     this,  SLOT  ( busyDisplay() ) );

    connect( _tree, SIGNAL( finished()	      ),
	     this,  SLOT  ( readingFinished() ) );

    connect( _tree, SIGNAL( aborted()	      ),
	     this,  SLOT  ( readingFinished() ) );

    connect( _tree, SIGNAL( readJobFinished( DirInfo * ) ),
	     this,  SLOT  ( readJobFinished( DirInfo * ) ) );

    connect( _tree, SIGNAL( deletingChild( FileInfo * ) ),
	     this,  SLOT  ( deletingChild( FileInfo * ) ) );

    connect( _tree, SIGNAL( clearingSubtree( DirInfo * ) ),
	     this,  SLOT  ( clearingSubtree( DirInfo * ) ) );

    connect( _tree, SIGNAL( subtreeCleared( DirInfo * ) ),
	     this,  SLOT  ( subtreeCleared( DirInfo * ) ) );

    connect( _tree, SIGNAL( childDeleted() ),
	     this,  SLOT  ( childDeleted() ) );
}


void DirTreeModel::clear()
{
    if ( _tree )
    {
	beginResetModel();

	// logDebug() << "After beginResetModel()" << Qt::endl;
	// dumpPersistentIndexList();

	_tree->clear();
	endResetModel();

	// logDebug() << "After endResetModel()" << Qt::endl;
	// dumpPersistentIndexList();
    }
}


void DirTreeModel::openUrl( const QString & url )
{
    CHECK_PTR( _tree );

    if ( _tree->root() &&  _tree->root()->hasChildren() )
	clear();

    _updateTimer.start();
    _tree->startReading( url );
}


void DirTreeModel::readPkg( const PkgFilter & pkgFilter )
{
    // logDebug() << "Reading " << pkgFilter << Qt::endl;
    CHECK_PTR( _tree );

    if ( _tree->root() &&  _tree->root()->hasChildren() )
	clear();

    _updateTimer.start();
    _tree->readPkg( pkgFilter );
}


void DirTreeModel::loadIcons()
{
    if ( _treeIconDir.isEmpty() )
    {
	logWarning() << "No tree icons" << Qt::endl;
	return;
    }

    if ( ! _treeIconDir.endsWith( "/" ) )
	_treeIconDir += "/";

    _dirIcon	       = QIcon( _treeIconDir + "dir.png"	    );
    _dotEntryIcon      = QIcon( _treeIconDir + "dot-entry.png"      );
    _fileIcon	       = QIcon( _treeIconDir + "file.png"	    );
    _symlinkIcon       = QIcon( _treeIconDir + "symlink.png"	    );
    _unreadableDirIcon = QIcon( _treeIconDir + "unreadable-dir.png" );
    _mountPointIcon    = QIcon( _treeIconDir + "mount-point.png"    );
    _stopIcon	       = QIcon( _treeIconDir + "stop.png"	    );
    _excludedIcon      = QIcon( _treeIconDir + "excluded.png"	    );
    _blockDeviceIcon   = QIcon( _treeIconDir + "block-device.png"   );
    _charDeviceIcon    = QIcon( _treeIconDir + "char-device.png"    );
    _specialIcon       = QIcon( _treeIconDir + "special.png"	    );
    _pkgIcon	       = QIcon( _treeIconDir + "folder-pkg.png"     );
    _atticIcon         = _dirIcon;
}


void DirTreeModel::setColumns( const DataColumnList & columns )
{
    beginResetModel();
    DataColumns::instance()->setColumns( columns );
    endResetModel();
}


FileInfo * DirTreeModel::findChild( DirInfo * parent, int childNo ) const
{
    CHECK_PTR( parent );

    const FileInfoList & childrenList =
	parent->sortedChildren( _sortCol, _sortOrder,
				true );	    // includeAttic

    if ( childNo < 0 || childNo >= childrenList.size() )
    {
	logError() << "Child #" << childNo << " is out of range: 0.."
		   << childrenList.size()-1 << " children for "
		   << parent << Qt::endl;
	Debug::dumpChildrenList( parent, childrenList );

	return 0;
    }

    // Debug::dumpChildrenList( parent, childrenList );

    return childrenList.at( childNo );
}


int DirTreeModel::rowNumber( FileInfo * child ) const
{
    if ( ! child->parent() )
	return 0;

    const FileInfoList & childrenList =
	child->parent()->sortedChildren( _sortCol, _sortOrder,
					 true ); // includeAttic

    const int row = childrenList.indexOf( child );

    if ( row < 0 )
    {
	// Not found
	logError() << "Child " << child
		   << " (" << (void *) child << ")"
		   << " not found in \""
		   << child->parent() << "\"" << Qt::endl;

	Debug::dumpDirectChildren( child->parent() );
    }

    return row;
}


FileInfo * DirTreeModel::itemFromIndex( const QModelIndex & index )
{
    FileInfo * item = 0;

    if ( index.isValid() )
    {
        item = static_cast<FileInfo *>( index.internalPointer() );
        CHECK_MAGIC( item );
    }

    return item;
}



//
// Reimplemented from QAbstractItemModel
//

int DirTreeModel::rowCount( const QModelIndex & parentIndex ) const
{
    if ( ! _tree )
	return 0;

    FileInfo * item = 0;

    if ( parentIndex.isValid() )
    {
	item = static_cast<FileInfo *>( parentIndex.internalPointer() );
	CHECK_MAGIC( item );
    }
    else
	item = _tree->root();

    if ( ! item->isDirInfo() )
	return 0;

    if ( item->toDirInfo()->isLocked() )
    {
	// logDebug() << item << " is locked - returning 0" << Qt::endl;
	return 0;
    }

    int count = 0;
    switch ( item->readState() )
    {
	case DirQueued:
	    count = 0;	// Nothing yet
	    break;

	case DirReading:

	    // Don't mess with directories that are currently being read: If we
	    // tell our view about them, the view might begin fetching model
	    // indexes for them, and when the tree later sends the
	    // readJobFinished() signal, the beginInsertRows() call in our
	    // readJobFinished() slot will confuse the view; it would assume
	    // that the number of children reported in that beginInsertRows()
	    // call needs to be added to the number reported here. We'd have to
	    // keep track how many children we already reported, and how many
	    // new ones to report later.
	    //
	    // Better keep it simple: Don't report any children until they
	    // are complete.

	    count = 0;
	    break;

	case DirError:
	case DirPermissionDenied:

	    // This is a hybrid case: Depending on the dir reader, the dir may
	    // or may not be finished at this time. For a local dir, it most
	    // likely is; for a cache reader, there might be more to come.

	    if ( _tree->isBusy() )
		count = 0;
	    else
		count = directChildrenCount( item );
	    break;

	case DirFinished:
	case DirOnRequestOnly:
	case DirCached:
	case DirAborted:
	    count = directChildrenCount( item );
	    break;

	// intentionally omitting 'default' case so the compiler can report
	// missing enum values
    }

    // logDebug() << dirName << ": " << count << Qt::endl;
    return count;
}


QVariant DirTreeModel::data( const QModelIndex & index, int role ) const
{
    if ( ! index.isValid() )
	return QVariant();

    const DataColumn col  = DataColumns::fromViewCol( index.column() );
    FileInfo * item = static_cast<FileInfo *>( index.internalPointer() );
    CHECK_MAGIC( item );

    switch ( role )
    {
	case Qt::DisplayRole: // text
	    {
		const QVariant result = columnText( item, col );

		if ( item && item->isDirInfo() )
		{
		    // logDebug() << "Touching " << col << "\tof " << item << Qt::endl;
		    item->toDirInfo()->touch();
		}

		return result;
	    }

	case Qt::ForegroundRole: // text colour
	    {
		if ( item->isIgnored() || item->isAttic() )
		    return QGuiApplication::palette().brush( QPalette::Disabled, QPalette::WindowText );

		if ( item->isDir() )
		{
		    if ( item->readError() )
			return _dirReadErrColor;

		    if ( item->errSubDirCount() > 0 )
			return _subtreeReadErrColor;
		}

		return QVariant();
	    }

	case Qt::DecorationRole: // icon
	    return columnIcon( item, col );

	case Qt::TextAlignmentRole:
	    return columnAlignment( item, col );

	case RawDataRole: // Send raw data to our item delegate (the PercentBarDelegate)
	    return columnRawData( item, col );

	case Qt::ToolTipRole:
	    {
		const bool limitedInfo = item->isPseudoDir() || item->readState() == DirCached || item->isPkgInfo();
		switch ( col )
		{
		    case PercentBarCol:
			return formatPercent( item->subtreeAllocatedPercent() );

		    case SizeCol:
			return sizeColTooltip( item );

		    case PermissionsCol:
			return limitedInfo ? QVariant() : item->octalPermissions();

		    case OctalPermissionsCol:
			return limitedInfo ? QVariant() : item->symbolicPermissions();

		    default:
			return QVariant();
		}
		break;
	    }

	case Qt::FontRole:
	    return columnFont( item, col );

	default:
	    return QVariant();
    }

    /*NOTREACHED*/
    return QVariant();
}



QVariant DirTreeModel::sizeColTooltip( FileInfo * item ) const
{
    if ( item->isDirInfo() )
	return item->sizePrefix() + formatByteSize( item->totalAllocatedSize() );

    QString text = item->sizePrefix() + formatByteSize( item->rawByteSize() );

    if ( item->allocatedSize() > item->rawByteSize() || item->isSparseFile() )
    {
	text += item->isSparseFile() ? " sparse data" : " used";
	text += "<br/>";
	text += formatByteSize( item->totalAllocatedSize() );
	text += " allocated";
    }

    return whitespacePre( text + formatLinksRichText( item->links() ) );
}


QVariant DirTreeModel::headerData( int		   section,
				   Qt::Orientation orientation,
				   int		   role ) const
{
    if ( orientation != Qt::Horizontal )
	return QVariant();

    switch ( role )
    {
	case Qt::DisplayRole:
	    switch ( DataColumns::fromViewCol( section ) )
	    {
		case NameCol:		  return tr( "Name"		  );
		case PercentBarCol:	  return tr( "Subtree Percentage" );
		case PercentNumCol:	  return tr( "%"		  );
		case SizeCol:		  return tr( "Size"		  );
		case TotalItemsCol:	  return tr( "Items"		  );
		case TotalFilesCol:	  return tr( "Files"		  );
		case TotalSubDirsCol:	  return tr( "Subdirs"		  );
		case LatestMTimeCol:	  return tr( "Last Modified"	  );
		case OldestFileMTimeCol:  return tr( "Oldest File"	  );
		case UserCol:		  return tr( "User"		  );
		case GroupCol:		  return tr( "Group"		  );
		case PermissionsCol:	  return tr( "Permissions"	  );
		case OctalPermissionsCol: return tr( "Perm."		  );
		default:		  return QVariant();
	    }

	case Qt::TextAlignmentRole:
	    switch ( DataColumns::fromViewCol( section ) )
	    {
		case PercentBarCol:
		case PercentNumCol:
		case SizeCol:
		case TotalItemsCol:
		case TotalFilesCol:
		case TotalSubDirsCol:
		case LatestMTimeCol:
		case OldestFileMTimeCol:
		case PermissionsCol:
		case OctalPermissionsCol: return Qt::AlignHCenter;
		default:		  return Qt::AlignLeft;
	    }

	default:
	    return QVariant();
    }
}


Qt::ItemFlags DirTreeModel::flags( const QModelIndex & index ) const
{
    if ( ! index.isValid() )
	return Qt::NoItemFlags;

    FileInfo * item = static_cast<FileInfo *>( index.internalPointer() );
    CHECK_MAGIC( item );

    Qt::ItemFlags baseFlags = Qt::ItemIsEnabled;

    if ( ! item->isDirInfo() )
	baseFlags |= Qt::ItemNeverHasChildren;

    // logDebug() << "Flags for " << index << Qt::endl;
    const DataColumn col = DataColumns::fromViewCol( index.column() );

    switch ( col )
    {
	case PercentBarCol:
	    return baseFlags;

	default:
	    return baseFlags | Qt::ItemIsSelectable;
    }
}


QModelIndex DirTreeModel::index( int row, int column, const QModelIndex & parentIndex ) const
{
    if ( ! _tree  || ! _tree->root() || ! hasIndex( row, column, parentIndex ) )
	return QModelIndex();

    FileInfo *parent;

    if ( parentIndex.isValid() )
    {
	parent = static_cast<FileInfo *>( parentIndex.internalPointer() );
	CHECK_MAGIC( parent );
    }
    else
	parent = _tree->root();

    if ( parent->isDirInfo() )
    {
	FileInfo * child = findChild( parent->toDirInfo(), row );
	CHECK_PTR( child );

	if ( child )
	    return createIndex( row, column, child );
    }

    return QModelIndex();
}


QModelIndex DirTreeModel::parent( const QModelIndex & index ) const
{
    if ( ! index.isValid() )
	return QModelIndex();

    const FileInfo * child = static_cast<FileInfo*>( index.internalPointer() );
    if ( ! child || ! child->checkMagicNumber() )
	return QModelIndex();

    FileInfo * parent = child->parent();

    if ( ! parent || parent == _tree->root() )
	return QModelIndex();

    const int row = rowNumber( parent );
    // logDebug() << "Parent of " << child << " is " << parent << " #" << row << Qt::endl;

    return createIndex( row, 0, parent );
}


void DirTreeModel::sort( int column, Qt::SortOrder order )
{
    if ( column == _sortCol && order == _sortOrder )
        return;

    logDebug() << "Sorting by " << static_cast<DataColumn>( column )
	       << ( order == Qt::AscendingOrder ? " ascending" : " descending" )
	       << Qt::endl;

    // logDebug() << "Before layoutAboutToBeChanged()" << Qt::endl;
    // dumpPersistentIndexList();

    emit layoutAboutToBeChanged();

    _sortCol = DataColumns::fromViewCol( column );
    _sortOrder = order;

    updatePersistentIndexes();
    emit layoutChanged();

    //logDebug() << "After layoutChanged()" << Qt::endl;
    // dumpPersistentIndexList();
}


//---------------------------------------------------------------------------


void DirTreeModel::busyDisplay()
{
    emit layoutAboutToBeChanged();

    _sortCol = NameCol;
    logDebug() << "Sorting by " << _sortCol << " during reading" << Qt::endl;

    updatePersistentIndexes();
    emit layoutChanged();
}


void DirTreeModel::idleDisplay()
{
    emit layoutAboutToBeChanged();

    _sortCol = PercentNumCol;
    //logDebug() << "Sorting by " << _sortCol << " after reading is finished" << Qt::endl;

    updatePersistentIndexes();
    emit layoutChanged();
}


QModelIndex DirTreeModel::modelIndex( FileInfo * item, int column ) const
{
    CHECK_PTR( _tree );
    CHECK_PTR( _tree->root() );

    if (  ! item || ! item->checkMagicNumber() || item == _tree->root() )
	return QModelIndex();
    else
    {
	const int row = rowNumber( item );
	// logDebug() << item << " is row #" << row << " of " << item->parent() << Qt::endl;
	return row < 0 ? QModelIndex() : createIndex( row, column, item );
    }
}



QVariant DirTreeModel::columnText( FileInfo * item, int col ) const
{
    CHECK_PTR( item );

    if ( col == _readJobsCol && item->isBusy() )
	return tr( "[%1 Read Jobs]" ).arg( item->pendingReadJobs() );

    const bool limitedInfo = item->isPseudoDir() || item->readState() == DirCached || item->isPkgInfo();

    if ( item->isAttic() && col == PercentNumCol )
	return QVariant();

    if ( item->isPkgInfo() &&
	 item->readState() == DirAborted &&
	 ! item->firstChild() &&
	 col != NameCol )
    {
	return "?";
    }

    switch ( col )
    {
	case NameCol:		  return item->name();
	case PercentBarCol:	  return item->isExcluded() ? tr( "[Excluded]" ) : QVariant();
	case PercentNumCol:	  return item == _tree->firstToplevel() ? QVariant() : formatPercent( item->subtreeAllocatedPercent() );
	case SizeCol:		  return sizeColText( item );
	case LatestMTimeCol:	  return QString( "  " % formatTime( item->latestMtime() ) );
	case UserCol:		  return limitedInfo ? QVariant() : item->userName();
	case GroupCol:		  return limitedInfo ? QVariant() : item->groupName();
	case PermissionsCol:	  return limitedInfo ? QVariant() : item->symbolicPermissions();
	case OctalPermissionsCol: return limitedInfo ? QVariant() : item->octalPermissions();
    }

    if ( item->isDirInfo() )
    {
	if ( item->readError() )
	{
	    switch ( col )
	    {
		case TotalItemsCol:
		case TotalFilesCol:
		case TotalSubDirsCol:
		    return "?";

		default:
		    break;
	    }
	}

	const QString prefix = item->sizePrefix();

	switch ( col )
	{
	    case TotalItemsCol:	  return QString( prefix % "%1" ).arg( item->totalItems() );
	    case TotalFilesCol:	  return QString( prefix % "%1" ).arg( item->totalFiles() );
	    case TotalSubDirsCol:
		if ( item->isDotEntry() )
		    return QVariant();
		else
		    return QString( prefix % "%1" ).arg( item->totalSubDirs() );

	    case OldestFileMTimeCol:  return QString( "	 " % formatTime( item->oldestFileMtime() ) );
	}
    }

    return QVariant();
}


QVariant DirTreeModel::columnAlignment( FileInfo *, int col ) const
{
    int alignment = Qt::AlignVCenter;

    switch ( col )
    {
	case PercentBarCol:
	case PercentNumCol:
	case SizeCol:
	case TotalItemsCol:
	case TotalFilesCol:
	case TotalSubDirsCol:
	case PermissionsCol:
	case OctalPermissionsCol:
	case LatestMTimeCol:
	case OldestFileMTimeCol:
	    alignment |= Qt::AlignRight;
	    break;

	case NameCol:
	case UserCol:
	case GroupCol:
	default:
	    alignment |= Qt::AlignLeft;
	    break;
    }

    return alignment;
}


QVariant DirTreeModel::columnFont( FileInfo * item, int col ) const
{
    if ( col == PermissionsCol )
    {
	QFont font;
	font.setFamily( "monospace" );
	return font;
    }

    if ( _useBoldForDominantItems && item && item->isDominant() )
	return dominantItemColumnFont( item, col );

    return QVariant();
}


QVariant DirTreeModel::dominantItemColumnFont( FileInfo *, int col ) const
{
    switch ( _sortCol )
    {
	// Only if sorting by size or percent
	case PercentBarCol:
	case PercentNumCol:
	case SizeCol:
	    break;

	default:
	    return QVariant();
    }

    if ( _sortOrder != Qt::DescendingOrder )
	return QVariant();

    switch ( col )
    {
	case NameCol:
	case PercentNumCol:
	case SizeCol:

	    // Notice that the SizeColDelegate will override this
	    // for tiny files or symlinks for the size column
	    return _boldItemFont;
	    break;

	default:
	    break;
    }

    return QVariant();
}


QVariant DirTreeModel::columnRawData( FileInfo * item, int col ) const
{
    switch ( col )
    {
	case NameCol:		  return item->name();
	case PercentBarCol:
	    {
		if ( ( item->parent() && item->parent()->isBusy() ) ||
		     item == _tree->firstToplevel() ||
		     item->isAttic() )
		{
		    return -1.0;
		}
		else
		{
		    return item->subtreeAllocatedPercent();
		}
	    }
	case PercentNumCol:	  return item->subtreeAllocatedPercent();
	case SizeCol:		  return item->totalSize();
	case TotalItemsCol:	  return item->totalItems();
	case TotalFilesCol:	  return item->totalFiles();
	case TotalSubDirsCol:	  return item->totalSubDirs();
	case LatestMTimeCol:	  return (qulonglong) item->latestMtime();
	case OldestFileMTimeCol:  return (qulonglong) item->oldestFileMtime();
	case UserCol:		  return item->uid();
	case GroupCol:		  return item->gid();
	case PermissionsCol:	  return item->mode();
	case OctalPermissionsCol: return item->mode();
	default:		  return QVariant();
    }
}


int DirTreeModel::directChildrenCount( FileInfo * subtree ) const
{
    if ( ! subtree )
	return 0;

    int count = subtree->directChildrenCount();

    if ( subtree->attic() )
	++count;

    return count;
}


QString DirTreeModel::specialSizeText( FileInfo * item, QString (*fmtSz)(FileSize) )
{
    if ( ! item->isFile() )
	return QString();

    const QString links = formatLinksInline( item->links() );

    if ( item->isSparseFile() )
	return fmtSz( item->rawByteSize() ) % links % " (" % fmtSz( item->rawAllocatedSize() ) % ")";
    else if ( !links.isEmpty() )
	return fmtSz( item->rawByteSize() ) % links;
    else
	return QString();
}


QString DirTreeModel::smallSizeText( FileInfo * item )
{
    if ( !isSmallFileOrSymLink( item ) )
	return QString();

    const FileSize allocated = item->allocatedSize();
    const FileSize size      = item->size();

    if ( allocated >= 1000 &&             // at least 1k so the (?k) makes sense
         allocated % 1024 == 0 &&         // exact number of kB
	 allocated < 1024 * 1024 )        // below 1 MB
         // && item->usedPercent() < SMALL_FILE_SHOW_ALLOC_THRESHOLD &&
    {
	const QString text = size < 1000 ? formatShortByteSize( size ) : formatSize( size );
	return QString( "%1 (%2k)" ).arg( text ).arg( allocated / 1024 );
    }

    return formatSize( size );
}


bool DirTreeModel::isSmallFileOrSymLink( FileInfo * item )
{
    if ( !item || !item->tree() || item->blocks() == 0 || !( item->isFile() || item->isSymLink() ) )
	return false;

    const FileSize clusterSize = item->tree()->clusterSize();
    const int numClusters = item->allocatedSize() / clusterSize;

    // More than 3 allocated clusters isn't "small"
    if ( clusterSize == 0 || numClusters > SMALL_FILE_CLUSTERS + 1 )
	return false;

    // Two or less allocated clusters is definitely "small"
    if ( numClusters <= SMALL_FILE_CLUSTERS )
	return true;

    // 3 allocated clusters, but less than 2.5 actually used is "small"
    // 'unused' might be negative for sparse files, but the check will still be valid
    const FileSize unused = item->allocatedSize() - item->rawByteSize();
    if ( unused > clusterSize / 2 )
	return true;

    return false;
}


QVariant DirTreeModel::sizeColText( FileInfo * item ) const
{
    if ( item->isDevice() )
	return QVariant();

    const QString leftMargin( 2, ' ' );

    if ( item->isDirInfo() )
	return QString( leftMargin % item->sizePrefix() % formatSize( item->totalAllocatedSize() ) );

    // Start by trying hard links and sparse files ...
    QString text = specialSizeText( item, formatSize );

    // .. then try small files and symlinks ...
    if ( text.isEmpty() )
	text = smallSizeText( item );

    // ... and standard formatting for everything else
    if ( text.isEmpty() )
	text = leftMargin % formatSize( item->size() );

    return text;
}


QVariant DirTreeModel::columnIcon( FileInfo * item, int col ) const
{
    if ( col != NameCol )
	return QVariant();

    const QIcon icon = itemTypeIcon( item );
    if ( icon.isNull() )
	return QVariant();

    const bool  useDisabled = item->isIgnored() || item->isAttic();
    const QSize iconSize( icon.actualSize( QSize( 1024, 1024 ) ) );

    return icon.pixmap( iconSize, useDisabled ? QIcon::Disabled : QIcon::Normal );
}


QIcon DirTreeModel::itemTypeIcon( FileInfo * item ) const
{
    if ( ! item )
        return QIcon();

    if	    ( item->isDotEntry() )  return _dotEntryIcon;
    else if ( item->isAttic() )	    return _atticIcon;
    else if ( item->isPkgInfo()	 )  return _pkgIcon;
    else if ( item->isExcluded() )  return _excludedIcon;
    else if ( item->isDir()	 )
    {
	if	( item->readState() == DirAborted )   return _stopIcon;
	else if ( item->readError()		  )   return _unreadableDirIcon;
	else if ( item->isMountPoint()		  )   return _mountPointIcon;
	else if ( item->isIgnored()		  )   return _atticIcon;
	else					      return _dirIcon;
    }
    else // ! item->isDir()
    {
	if	( item->readError()		  )   return _unreadableDirIcon;
	else if ( item->isFile()		  )   return _fileIcon;
	else if ( item->isSymLink()		  )   return _symlinkIcon;
	else if ( item->isBlockDevice()		  )   return _blockDeviceIcon;
	else if ( item->isCharDevice()		  )   return _charDeviceIcon;
	else if ( item->isSpecial()		  )   return _specialIcon;
    }

    return QIcon();
}


void DirTreeModel::readJobFinished( DirInfo * dir )
{
    // logDebug() << dir << Qt::endl;
    delayedUpdate( dir );

    if ( anyAncestorBusy( dir ) )
    {
//	if  ( dir && ! dir->isMountPoint() )
//	    logDebug() << "Ancestor busy - ignoring readJobFinished for " << dir << Qt::endl;
    }
    else
    {
	newChildrenNotify( dir );
    }
}


bool DirTreeModel::anyAncestorBusy( FileInfo * item ) const
{
    while ( item )
    {
	if ( item->readState() == DirQueued ||
	     item->readState() == DirReading )
	{
	    return true;
	}

	item = item->parent();
    }

    return false;
}


void DirTreeModel::newChildrenNotify( DirInfo * dir )
{
    // logDebug() << dir << Qt::endl;

    if ( ! dir )
    {
	logError() << "NULL DirInfo *" << Qt::endl;
	return;
    }

    if ( ! dir->isTouched() && dir != _tree->root() && dir != _tree->firstToplevel() )
    {
	// logDebug() << "Remaining silent about untouched dir " << dir << Qt::endl;
	return;
    }

    const QModelIndex index = modelIndex( dir );
    const int count = directChildrenCount( dir );
    // Debug::dumpDirectChildren( dir );

    if ( count > 0 )
    {
	// logDebug() << "Notifying view about " << count << " new children of " << dir << Qt::endl;

	dir->lock();
	beginInsertRows( index, 0, count - 1 );
	dir->unlock();
	endInsertRows();
    }

    // If any readJobFinished signals were ignored because a parent was not
    // finished yet, now is the time to notify the view about those children,
    // too.
    FileInfoIterator it( dir );

    while ( *it )
    {
	if ( (*it)->isDirInfo() &&
	     (*it)->readState() != DirReading &&
	     (*it)->readState() != DirQueued	)
	{
	    newChildrenNotify( (*it)->toDirInfo() );
	}
	++it;
    }
}


void DirTreeModel::delayedUpdate( DirInfo * dir )
{
    while ( dir && dir != _tree->root() )
    {
	if ( dir->isTouched() )
	    _pendingUpdates.insert( dir );

	dir = dir->parent();
    }
}


void DirTreeModel::sendPendingUpdates()
{
    //logDebug() << "Sending " << _pendingUpdates.size() << " updates" << Qt::endl;

    foreach ( DirInfo * dir, _pendingUpdates )
	dataChangedNotify( dir );

    _pendingUpdates.clear();
}


void DirTreeModel::dataChangedNotify( DirInfo * dir )
{
    if ( ! dir || dir == _tree->root() )
	return;

    if ( dir->isTouched() ) // only if the view ever requested data about this dir
    {
	const int colCount = DataColumns::instance()->colCount();
	const QModelIndex topLeft	= modelIndex( dir, 0 );
	const QModelIndex bottomRight	= createIndex( topLeft.row(), colCount - 1, dir );

	emit dataChanged( topLeft, bottomRight, { Qt::DisplayRole } );

	// logDebug() << "Data changed for " << dir << Qt::endl;

	// If the view is still interested in this dir, it will fetch data, and
	// then the dir will be touched again. For all we know now, this dir
	// might easily be out of scope for the view, so let's not bother the
	// view again about this dir until it's clear that the view still wants
	// updates about it.

	dir->clearTouched();
    }
}


void DirTreeModel::readingFinished()
{
    _updateTimer.stop();
    idleDisplay();
    sendPendingUpdates();

    // dumpPersistentIndexList();
    // Debug::dumpDirectChildren( _tree->root(), "root" );
}


void DirTreeModel::dumpPersistentIndexList() const
{
    QModelIndexList persistentList = persistentIndexList();

    logDebug() << persistentList.size() << " persistent indexes" << Qt::endl;

    for ( int i=0; i < persistentList.size(); ++i )
    {
	const QModelIndex index = persistentList.at(i);

	FileInfo * item = static_cast<FileInfo *>( index.internalPointer() );
	CHECK_MAGIC( item );

	logDebug() << "#" << i
		   << " Persistent index "
		   << index
		   << Qt::endl;
    }
}


void DirTreeModel::updatePersistentIndexes()
{
    QModelIndexList persistentList = persistentIndexList();

    for ( int i=0; i < persistentList.size(); ++i )
    {
	const QModelIndex oldIndex = persistentList.at(i);

	if ( oldIndex.isValid() )
	{
	    FileInfo * item = static_cast<FileInfo *>( oldIndex.internalPointer() );
	    const QModelIndex newIndex = modelIndex( item, oldIndex.column() );
#if 0
	    logDebug() << "Updating #" << i
		       << " " << item
		       << " col " << oldIndex.column()
		       << " row " << oldIndex.row()
		       << " --> " << newIndex.row()
		       << Qt::endl;
#endif
	    changePersistentIndex( oldIndex, newIndex );
	}
    }
}


void DirTreeModel::beginRemoveRows( const QModelIndex & parent, int first, int last )
{
    if ( _removingRows )
    {
	logError() << "Removing rows already in progress" << Qt::endl;
	return;
    }


    if ( ! parent.isValid() )
    {
	logError() << "Invalid QModelIndex" << Qt::endl;
	return;
    }

    _removingRows = true;
    QAbstractItemModel::beginRemoveRows( parent, first, last );
}


void DirTreeModel::endRemoveRows()
{
    if ( _removingRows )
    {
	QAbstractItemModel::endRemoveRows();
	_removingRows = false;
    }
}


void DirTreeModel::deletingChild( FileInfo * child )
{
    logDebug() << "Deleting child " << child << Qt::endl;

    if ( child->parent() &&
	 ( child->parent() == _tree->root() ||
	   child->parent()->isTouched()	 ) )
    {
	const QModelIndex parentIndex = modelIndex( child->parent(), 0 );
	const int row = rowNumber( child );
	//logDebug() << "beginRemoveRows for " << child << " row " << row << Qt::endl;
	beginRemoveRows( parentIndex, row, row );
    }

    invalidatePersistent( child, true );
}


void DirTreeModel::childDeleted()
{
    endRemoveRows();
}


void DirTreeModel::clearingSubtree( DirInfo * subtree )
{
    //logDebug() << "Deleting all children of " << subtree << Qt::endl;

    if ( subtree == _tree->root() || subtree->isTouched() )
    {
	const QModelIndex subtreeIndex = modelIndex( subtree, 0 );
	const int count = directChildrenCount( subtree );

	if ( count > 0 )
	{
	    //logDebug() << "beginRemoveRows for " << subtree << " row 0 to " << count - 1 << Qt::endl;
	    beginRemoveRows( subtreeIndex, 0, count - 1 );
	}
    }

    invalidatePersistent( subtree, false );
}


void DirTreeModel::subtreeCleared( DirInfo * )
{
    endRemoveRows();
}


void DirTreeModel::invalidatePersistent( FileInfo * subtree,
					 bool	    includeParent )
{
    foreach ( const QModelIndex & index, persistentIndexList() )
    {
	FileInfo * item = static_cast<FileInfo *>( index.internalPointer() );
	CHECK_PTR( item );

	if ( ! item->checkMagicNumber() ||
	     item->isInSubtree( subtree ) )
	{
	    if ( item != subtree || includeParent )
	    {
#if 1
		logDebug() << "Invalidating " << index << Qt::endl;
#endif
		changePersistentIndex( index, QModelIndex() );
	    }
	}
    }

}


QVariant DirTreeModel::formatPercent( float percent ) const
{
    const QString text = ::formatPercent( percent );

    return text.isEmpty() ? QVariant() : text;
}


void DirTreeModel::refreshSelected()
{
    CHECK_PTR( _selectionModel );
    FileInfo * sel = _selectionModel->selectedItems().first();

    while ( sel &&
	    ( ! sel->isDir() || sel->isPseudoDir() ) &&
	    sel->parent() )
    {
	sel = sel->parent();
    }

    if ( sel && sel->isDirInfo() )
    {
	logDebug() << "Refreshing " << sel << Qt::endl;
	busyDisplay();
	FileInfoSet refreshSet;
	refreshSet << sel;
	_selectionModel->prepareRefresh( refreshSet );
	_tree->refresh( sel->toDirInfo() );
    }
    else
    {
	logWarning() << "NOT refreshing " << sel << Qt::endl;
    }
}
