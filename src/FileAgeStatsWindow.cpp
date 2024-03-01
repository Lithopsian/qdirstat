/*
 *   File name: FileAgeStatsWindow.h
 *   Summary:	QDirStat "File Age Statistics" window
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "FileAgeStatsWindow.h"
#include "DirTree.h"
#include "HeaderTweaker.h"
#include "PercentBar.h"
#include "Settings.h"
#include "SettingsHelpers.h"
#include "FormatUtil.h"
#include "Logger.h"
#include "Exception.h"

// Remember to adapt the tooltip text for the "Locate" button in the .ui file
// and the method docs in the .h file if this value is changed

#define MAX_LOCATE_FILES        1000

using namespace QDirStat;


FileAgeStatsWindow::FileAgeStatsWindow( QWidget * parent ):
    QDialog( parent ),
    _ui( new Ui::FileAgeStatsWindow ),
    _stats( new FileAgeStats( 0 ) ),
    _filesPercentBarDelegate( 0 ),
    _sizePercentBarDelegate( 0 ),
    _startGapsWithCurrentYear( true )
{
    // logDebug() << "init" << Qt::endl;

    CHECK_NEW( _ui );
    CHECK_NEW( _stats );

    _ui->setupUi( this );
    initWidgets();
    readSettings();
}


FileAgeStatsWindow::~FileAgeStatsWindow()
{
    writeSettings();

    delete _stats;
    delete _ui;
}


void FileAgeStatsWindow::clear()
{
    _stats->clear();
    _ui->treeWidget->clear();
}


void FileAgeStatsWindow::refresh()
{
    populate( _subtree() );
}


void FileAgeStatsWindow::initWidgets()
{
//    QFont font = _ui->heading->font();
//    font.setBold( true );
//    _ui->heading->setFont( font );

    // _ui->heading->setText( text );

    // _ui->treeWidget->setColumnCount( YearListColumnCount );

    const QStringList headers = { tr( "Year"    ),
                                  tr( "Files"   ),
                                  tr( "Files %" ),  // percent bar
                                  tr( "%"       ),  // percent value
                                  tr( "Size"    ),
                                  tr( "Size %"  ),  // percent bar
                                  tr( "%"       ),  // percent value
                                };

    _ui->treeWidget->setHeaderLabels( headers );
//    _ui->treeWidget->header()->setStretchLastSection( false );

    // Delegates for the percent bars

    _filesPercentBarDelegate = new PercentBarDelegate( _ui->treeWidget, YearListFilesPercentBarCol, 7, 1 );
    CHECK_NEW( _filesPercentBarDelegate );
    _ui->treeWidget->setItemDelegateForColumn( YearListFilesPercentBarCol, _filesPercentBarDelegate );

    _sizePercentBarDelegate = new PercentBarDelegate( _ui->treeWidget, YearListSizePercentBarCol, 1, 1 );
    CHECK_NEW( _sizePercentBarDelegate );
    _ui->treeWidget->setItemDelegateForColumn( YearListSizePercentBarCol, _sizePercentBarDelegate );


    // Center the column headers
    QTreeWidgetItem * hItem = _ui->treeWidget->headerItem();
    for ( int col = 0; col < headers.size(); ++col )
        hItem->setTextAlignment( col, Qt::AlignHCenter );

    HeaderTweaker::resizeToContents( _ui->treeWidget->header() );


    connect( _ui->refreshButton, &QPushButton::clicked,
             this,               &FileAgeStatsWindow::refresh );

    connect( _ui->locateButton,  &QPushButton::clicked,
             this,               &FileAgeStatsWindow::locateFiles );

    connect( _ui->treeWidget,	 &QTreeWidget::itemDoubleClicked,
	     this,		 &FileAgeStatsWindow::locateFiles );

    connect( _ui->treeWidget,    &QTreeWidget::itemSelectionChanged,
             this,               &FileAgeStatsWindow::enableActions );
}


void FileAgeStatsWindow::reject()
{
    deleteLater();
}


void FileAgeStatsWindow::syncedPopulate( FileInfo * newSubtree )
{
    if ( _ui->syncCheckBox->isChecked() &&
         newSubtree && newSubtree->isDir() )
    {
        populate( newSubtree );
    }
}


void FileAgeStatsWindow::populate( FileInfo * newSubtree )
{
    // logDebug() << "populating with " << newSubtree << Qt::endl;

    clear();
    _subtree = newSubtree;

    _ui->heading->setText( tr( "File age statistics for %1" ).arg( _subtree.url() ) );

    // For better Performance: Disable sorting while inserting many items
    _ui->treeWidget->setSortingEnabled( false );

    _stats->collect( _subtree() );
    populateListWidget();

    _ui->treeWidget->setSortingEnabled( true );
    _ui->treeWidget->sortByColumn( YearListYearCol, Qt::DescendingOrder );

    enableActions();
}


void FileAgeStatsWindow::populateListWidget()
{
    for ( short year : _stats->years() )
    {
        YearStats * yearStats = _stats->yearStats( year );

        if ( yearStats )
        {
            // Add a year item
            YearListItem * item = new YearListItem( *yearStats );
            CHECK_NEW( item );
            _ui->treeWidget->addTopLevelItem( item );

            // Add the month items if applicable
            if ( _stats->monthStatsAvailableFor( year ) )
            {
                for ( short month = 1; month <= 12; month++ )
                {
                    YearStats * monthStats = _stats->monthStats( year, month );

                    if ( monthStats )
                    {
                        YearListItem * monthItem = new YearListItem( *monthStats );
                        CHECK_NEW( monthItem );

                        if ( monthStats->filesCount == 0 )
                            monthItem->setFlags( Qt::NoItemFlags ); // disabled

                        item->addChild( monthItem );
                    }
                }
            }
        }
    }

    fillGaps();
}


void FileAgeStatsWindow::fillGaps()
{
    for ( short year : findGaps() )
    {
        YearListItem * item = new YearListItem( YearStats( year ) );
        CHECK_NEW( item );

        item->setFlags( Qt::NoItemFlags ); // disabled
        _ui->treeWidget->addTopLevelItem( item );
    }
}


YearsList FileAgeStatsWindow::findGaps()
{
    YearsList gaps;
    const YearsList & years = _stats->years(); // sorted in ascending order

    if ( years.isEmpty() )
        return gaps;

    const short lastYear = _startGapsWithCurrentYear ? FileAgeStats::thisYear() : years.last();

    if ( lastYear - years.first() == years.count() - 1 )
        return gaps;

    for ( short yr = years.first(); yr <= lastYear; yr++ )
    {
        if ( ! years.contains( yr ) )
            gaps << yr;
    }

    return gaps;
}


const YearListItem * FileAgeStatsWindow::selectedItem() const
{
    const QTreeWidgetItem * currentItem = _ui->treeWidget->currentItem();

    return currentItem ? dynamic_cast<const YearListItem *>( currentItem ) : 0;
}


void FileAgeStatsWindow::locateFiles()
{
    const YearListItem * item = selectedItem();

    if ( item )
    {
        const short month = item->stats().month;
        const short year  = item->stats().year;

        if ( month > 0 && year > 0 )
            emit locateFilesFromMonth( _subtree.url(), year, month );
        else if ( year > 0 )
            emit locateFilesFromYear( _subtree.url(), year );
    }
}


void FileAgeStatsWindow::enableActions()
{
    const YearListItem * sel = selectedItem();
    const bool locateEnabled = sel && sel->stats().filesCount > 0 && sel->stats().filesCount <= MAX_LOCATE_FILES;
    _ui->locateButton->setEnabled( locateEnabled );
}


void FileAgeStatsWindow::readSettings()
{
    Settings settings;

    settings.beginGroup( "FileAgeStatsWindow" );
    _ui->syncCheckBox->setChecked( settings.value( "SyncWithMainWindow", true ).toBool() );
    _startGapsWithCurrentYear = settings.value( "StartGapsWithCurrentYear", true ).toBool();
    settings.endGroup();

    readWindowSettings( this, "FileAgeStatsWindow" );
}


void FileAgeStatsWindow::writeSettings()
{
    Settings settings;

    settings.beginGroup( "FileAgeStatsWindow" );
    settings.setValue( "SyncWithMainWindow",       _ui->syncCheckBox->isChecked() );
    settings.setValue( "StartGapsWithCurrentYear", _startGapsWithCurrentYear      );
    settings.endGroup();

    writeWindowSettings( this, "FileAgeStatsWindow" );
}






YearListItem::YearListItem( const YearStats & yearStats ) :
    QTreeWidgetItem( QTreeWidgetItem::UserType ),
    _stats( yearStats )
{
    if ( _stats.month > 0 )
        setText( YearListYearCol,            monthAbbreviation( yearStats.month ) );
    else
        setText( YearListYearCol,            QString::number( _stats.year         ) + " " );

    if ( _stats.filesCount > 0 )
    {
        setText( YearListFilesCountCol,      QString::number( _stats.filesCount   ) + " " );
        setData( YearListFilesPercentBarCol, RawDataRole, _stats.sizePercent );
        setText( YearListFilesPercentCol,    formatPercent  ( _stats.filesPercent ) + " " );
        setText( YearListSizeCol,            QString( 4, ' ' ) + formatSize( _stats.size ) + " " );
        setData( YearListSizePercentBarCol,  RawDataRole, _stats.sizePercent );
        setText( YearListSizePercentCol,     formatPercent  ( _stats.sizePercent  ) + " " );
    }
}


QVariant YearListItem::data( int column, int role ) const
{
    if ( role == Qt::TextAlignmentRole )
    {
        // Vertical alignment can't be set in any easier way (?)
        int alignment = Qt::AlignVCenter;
        if ( column == YearListYearCol )
            alignment |= Qt::AlignLeft;
        else
            alignment |= Qt::AlignRight;

        return alignment;
    }

    return QTreeWidgetItem::data( column, role );
}


bool YearListItem::operator<( const QTreeWidgetItem & rawOther ) const
{
    // Since this is a reference, the dynamic_cast will throw a std::bad_cast
    // exception if it fails. Not catching this here since this is a genuine
    // error which should not be silently ignored.
    const YearListItem & other = dynamic_cast<const YearListItem &>( rawOther );

    const int col = treeWidget() ? treeWidget()->sortColumn() : YearListYearCol;

    switch ( col )
    {
	case YearListYearCol:
            {
                if ( _stats.month > 0 )
                    return _stats.month < other.stats().month;
                else
                    return _stats.year  < other.stats().year;
            }
	case YearListFilesCountCol:	return _stats.filesCount   < other.stats().filesCount;
	case YearListFilesPercentBarCol:
	case YearListFilesPercentCol:	return _stats.filesPercent < other.stats().filesPercent;
	case YearListSizeCol:           return _stats.size         < other.stats().size;
	case YearListSizePercentBarCol:
	case YearListSizePercentCol:	return _stats.sizePercent  < other.stats().sizePercent;
	default:		        return QTreeWidgetItem::operator<( rawOther );
    }
}
