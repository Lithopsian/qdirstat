/*
 *   File name: FilesystemsWindow.cpp
 *   Summary:	QDirStat "Mounted Filesystems" window
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QClipboard>
#include <QContextMenuEvent>
#include <QFileIconProvider>
#include <QMenu>

#include "FilesystemsWindow.h"
#include "MountPoints.h"
#include "SettingsHelpers.h"
#include "HeaderTweaker.h"
#include "PanelMessage.h"
#include "FormatUtil.h"
#include "Logger.h"
#include "Exception.h"

#define WARN_PERCENT	10.0

using namespace QDirStat;


FilesystemsWindow::FilesystemsWindow( QWidget * parent ):
    QDialog( parent ),
    _ui( new Ui::FilesystemsWindow )
{
    CHECK_NEW( _ui );
    _ui->setupUi( this );
    MountPoints::reload();
    initWidgets();
    readWindowSettings( this, "FilesystemsWindow" );
}


FilesystemsWindow::~FilesystemsWindow()
{
    writeWindowSettings( this, "FilesystemsWindow" );
    delete _ui;
}


void FilesystemsWindow::populate()
{
    QFileIconProvider iconProvider;
    clear();

    const auto mountPoints = MountPoints::normalMountPoints();
    for ( MountPoint * mountPoint : mountPoints )
    {
	CHECK_PTR( mountPoint);

	FilesystemItem * item = new FilesystemItem( mountPoint, _ui->fsTree );
	CHECK_NEW( item );

	QIcon icon = iconProvider.icon( mountPoint->isNetworkMount() ?
					 QFileIconProvider::Network :
					 QFileIconProvider::Drive      );
	item->setIcon( 0, icon );
    }

    if ( MountPoints::hasBtrfs() )
	showBtrfsFreeSizeWarning();
}


void FilesystemsWindow::showBtrfsFreeSizeWarning()
{
    PanelMessage * msg = new PanelMessage( _ui->messagePanel );
    CHECK_NEW( msg );

    msg->setHeading( tr( "Btrfs free and used size information are misleading!" ) );
    msg->setText( tr( "Snapshots and copy-on-write may consume additional disk space." ) );
    msg->setDetailsUrl( "https://github.com/shundhammer/qdirstat/blob/master/doc/Btrfs-Free-Size.md" );
    _ui->messagePanel->add( msg );
}


void FilesystemsWindow::refresh()
{
    MountPoints::reload();
    populate();
}


void FilesystemsWindow::reject()
{
    deleteLater();
}


void FilesystemsWindow::clear()
{
    _ui->fsTree->clear();
    _ui->messagePanel->clear();
}


void FilesystemsWindow::initWidgets()
{
    QStringList headers = { tr( "Device" ), tr( "Mount Point" ), tr( "Type" ) };

    if ( MountPoints::hasSizeInfo() )
    {
	const QStringList sizeHeaders = { tr( "Size" ), tr( "Used" ), tr( "Reserved" ), tr( "Free" ), tr( "Free %" ) };
	headers += sizeHeaders;
    }

    _ui->fsTree->setHeaderLabels( headers );
    _ui->fsTree->header()->setStretchLastSection( false );

    // Center the column headers

    QTreeWidgetItem * hItem = _ui->fsTree->headerItem();

    for ( int col = 0; col < headers.size(); ++col )
	hItem->setTextAlignment( col, Qt::AlignHCenter );

    hItem->setToolTip( FS_ReservedSizeCol, tr( "Reserved for root" ) );
    hItem->setToolTip( FS_FreeSizeCol,	   tr( "Free for unprivileged users" ) );

    HeaderTweaker::resizeToContents( _ui->fsTree->header() );
    _ui->fsTree->sortItems( FS_DeviceCol, Qt::AscendingOrder );
    enableActions();

    connect( _ui->refreshButton, &QAbstractButton::clicked,
	     this,		 &FilesystemsWindow::refresh );

    connect( _ui->fsTree,	 &QTreeWidget::customContextMenuRequested,
	      this,		 &FilesystemsWindow::contextMenu);

    connect( _ui->fsTree,	 &QTreeWidget::itemDoubleClicked,
	     _ui->actionRead,	 &QAction::triggered );

    connect( _ui->readButton,	 &QAbstractButton::clicked,
	     _ui->actionRead,	 &QAction::triggered );

    connect( _ui->actionRead,	 &QAction::triggered,
	     this,		 &FilesystemsWindow::readSelectedFilesystem );

    connect( _ui->actionCopy,	 &QAction::triggered,
	     this,		 &FilesystemsWindow::copyDeviceToClipboard );

    connect( _ui->fsTree,	 &QTreeWidget::itemSelectionChanged,
	     this,		 &FilesystemsWindow::enableActions );
}


void FilesystemsWindow::enableActions()
{
    _ui->readButton->setEnabled( ! selectedPath().isEmpty() );
}


void FilesystemsWindow::readSelectedFilesystem()
{
    const QString path = selectedPath();
    if ( ! path.isEmpty() )
    {
	//logDebug() << "Read " << path << Qt::endl;
	emit readFilesystem( path );
    }
}


QString FilesystemsWindow::selectedPath() const
{
    QString result;

    const QList<QTreeWidgetItem *> sel = _ui->fsTree->selectedItems();
    if ( ! sel.isEmpty() )
    {
	FilesystemItem * item = dynamic_cast<FilesystemItem *>( sel.first() );
	if ( item )
	    result = item->mountPath();
    }

    return result;
}


void FilesystemsWindow::copyDeviceToClipboard()
{
    const FilesystemItem * currentItem = dynamic_cast<FilesystemItem *>( _ui->fsTree->currentItem() );
    if ( currentItem )
	QApplication::clipboard()->setText( currentItem->device().trimmed() );
}


void FilesystemsWindow::contextMenu( const QPoint & pos )
{
    // See if the right click was actually on an item
    if ( !_ui->fsTree->itemAt( pos ) )
	return;

    // The clicked item will always be the current item now
    _ui->actionRead->setText( tr( "Read at " ) + selectedPath() );

    QMenu menu;
    menu.addAction( _ui->actionRead );
    menu.addAction( _ui->actionCopy );

    menu.exec( _ui->fsTree->mapToGlobal( pos ) );
}


void FilesystemsWindow::keyPressEvent( QKeyEvent * event )
{
    if ( !selectedPath().isEmpty() && QList<int>( { Qt::Key_Return, Qt::Key_Enter } ).contains( event->key() ) )
	readSelectedFilesystem();
    else
	QDialog::keyPressEvent( event );
}



FilesystemItem::FilesystemItem( MountPoint  * mountPoint,
				QTreeWidget * parent ):
    QTreeWidgetItem( parent ),
    _device	    ( mountPoint->device()	    ),
    _mountPath	    ( mountPoint->path()	    ),
    _fsType	    ( mountPoint->filesystemType()  ),
    _totalSize	    ( mountPoint->totalSize()	    ),
    _usedSize	    ( mountPoint->usedSize()	    ),
    _reservedSize   ( mountPoint->reservedSize()    ),
    _freeSize	    ( mountPoint->freeSizeForUser() ),
    _isNetworkMount ( mountPoint->isNetworkMount()  ),
    _isReadOnly	    ( mountPoint->isReadOnly()	    )
{
    QString dev    = _device;

    if ( dev.startsWith( "/dev/mapper/luks-" ) )
    {
        // Cut off insanely long generated device mapper LUKS names

        int limit = sizeof( "/dev/mapper/luks-010203" ) - 1;

        if ( dev.size() > limit )
        {
            dev = dev.left( limit ) + "...";
            setToolTip( FS_DeviceCol, _device );
        }
    }

    setText( FS_DeviceCol,    dev + QString( 4, ' ' ) );
    setText( FS_MountPathCol, _mountPath );
    setText( FS_TypeCol,      _fsType );

    setTextAlignment( FS_TypeCol, Qt::AlignHCenter );

    if ( parent->columnCount() >= FS_TotalSizeCol && _totalSize >= 0 )
    {
	const QString blanks = QString( 3, ' ' ); // Enforce left margin

	setText( FS_TotalSizeCol, blanks + formatSize( _totalSize	      ) );
	setText( FS_UsedSizeCol,  blanks + formatSize( _usedSize	      ) );

	if ( _reservedSize > 0 )
	    setText( FS_ReservedSizeCol, blanks + formatSize( _reservedSize    ) );

	if ( _isReadOnly )
	    setText( FS_FreeSizeCol, QObject::tr( "read-only" ) );
	else
	{
	    setText( FS_FreeSizeCol, blanks + formatSize( _freeSize ) );

	    float freePercent = 0.0;

	    if ( _totalSize > 0 )
	    {
		freePercent = 100.0 * _freeSize / _totalSize;
		setText( FS_FreePercentCol, formatPercent( freePercent ) );

		if ( freePercent < WARN_PERCENT )
		{
		    setForeground( FS_FreeSizeCol,    Qt::red );
		    setForeground( FS_FreePercentCol, Qt::red );
		}
	    }
	}

	for ( int col = FS_TotalSizeCol; col < parent->columnCount(); ++col )
	    setTextAlignment( col, Qt::AlignRight );
    }
}


bool FilesystemItem::operator<( const QTreeWidgetItem & rawOther ) const
{
    const FilesystemItem & other = dynamic_cast<const FilesystemItem &>( rawOther );

    const int col = treeWidget() ? treeWidget()->sortColumn() : FS_DeviceCol;

    switch ( col )
    {
	case FS_DeviceCol:
	    if ( ! isNetworkMount() &&	 other.isNetworkMount() ) return true;
	    if ( isNetworkMount()   && ! other.isNetworkMount() ) return false;
	    return device() < other.device();

	case FS_MountPathCol:		return mountPath()    < other.mountPath();
	case FS_TypeCol:		return fsType()	      < other.fsType();
	case FS_TotalSizeCol:		return totalSize()    < other.totalSize();
	case FS_UsedSizeCol:		return usedSize()     < other.usedSize();
	case FS_ReservedSizeCol:	return reservedSize() < other.reservedSize();
	case FS_FreePercentCol:
	case FS_FreeSizeCol:		return freeSize()      < other.freeSize();
	default:			return QTreeWidgetItem::operator<( rawOther );
    }
}
