/*
 *   File name: FileSizeStatsWindow.cpp
 *   Summary:	QDirStat size type statistics window
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QCommandLinkButton>
#include <QDesktopServices>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "FileSizeStatsWindow.h"
#include "FileSizeStats.h"
#include "BucketsTableModel.h"
#include "DirTree.h"
#include "HeaderTweaker.h"
#include "HistogramView.h"
#include "MainWindow.h"
#include "SettingsHelpers.h"
//#include "QDirStatApp.h"
#include "FormatUtil.h"
#include "Logger.h"
#include "Exception.h"

using namespace QDirStat;


/**
 * Set the font to bold for all items in a table row.
 **/
static void setRowBold( QTableWidget * table, int row )
{
    for ( int col=0; col < table->columnCount(); ++col )
    {
	QTableWidgetItem * item = table->item( row, col );
	if ( item )
	{
	    QFont font = item->font();
	    font.setBold( true );
	    item->setFont( font );
	}
    }
}

#if 0
/**
 * Set the foreground (the text color) for all items in a table row.
 **/
static void setRowForeground( QTableWidget * table, int row, const QBrush & brush )
{
    for ( int col=0; col < table->columnCount(); ++col )
    {
	QTableWidgetItem * item = table->item( row, col );
	if ( item )
	    item->setForeground( brush );
    }
}
#endif

/**
 * Set the background for all items in a table row.
 **/
static void setRowBackground( QTableWidget * table, int row, const QBrush & brush )
{
    for ( int col=0; col < table->columnCount(); ++col )
    {
	QTableWidgetItem * item = table->item( row, col );
	if ( item )
	    item->setBackground( brush );
    }
}


/**
 * Set the text alignment for all items in a table column.
 **/
static void setColAlignment( QTableWidget * table, int col, Qt::Alignment alignment )
{
    for ( int row=0; row < table->rowCount(); ++row )
    {
	QTableWidgetItem * item = table->item( row, col );
	if ( item )
	    item->setTextAlignment( alignment | Qt::AlignVCenter );
    }
}


/**
 * Add an item to a table.
 **/
static QTableWidgetItem * addItem( QTableWidget	 * table,
				   int		   row,
				   int		   col,
				   const QString & text )
{
    QTableWidgetItem * item = new QTableWidgetItem( text );
    CHECK_NEW( item );
    table->setItem( row, col, item );

    return item;
}



FileSizeStatsWindow::FileSizeStatsWindow( QWidget * parent ):
    QDialog ( parent ),
    _ui { new Ui::FileSizeStatsWindow },
    _stats { nullptr }
{
    //logDebug() << "init" << Qt::endl;

    setAttribute( Qt::WA_DeleteOnClose );

    CHECK_NEW( _ui );

    _ui->setupUi( this );
    initWidgets();
    readWindowSettings( this, "FileSizeStatsWindow" );
}


FileSizeStatsWindow::~FileSizeStatsWindow()
{
    //logDebug() << "destroying" << Qt::endl;

    writeWindowSettings( this, "FileSizeStatsWindow" );
    delete _stats;
    delete _ui;
}


FileSizeStatsWindow * FileSizeStatsWindow::sharedInstance( QWidget * mainWindow )
{
    //logDebug() << _sharedInstance << Qt::endl;

    static QPointer<FileSizeStatsWindow> _sharedInstance = nullptr;

    if ( !_sharedInstance )
    {
	_sharedInstance = new FileSizeStatsWindow( mainWindow );
	CHECK_NEW( _sharedInstance );
    }

    return _sharedInstance;
}



void FileSizeStatsWindow::initWidgets()
{
//    QFont font = _ui->heading->font();
//    font.setBold( true );
//    _ui->heading->setFont( font );

    _bucketsTableModel = new BucketsTableModel( this, _ui->histogramView );
    CHECK_NEW( _bucketsTableModel );
    _ui->bucketsTable->setModel( _bucketsTableModel );

    const QList<QCommandLinkButton *> helpButtons = _ui->helpPage->findChildren<QCommandLinkButton *>();
    for ( const QCommandLinkButton * helpButton : helpButtons )
    {
	connect( helpButton, &QAbstractButton::clicked,
		 this,	     &FileSizeStatsWindow::showHelp );
    }

    _ui->optionsPanel->hide();

    connect( _ui->closeOptionsButton,	    &QPushButton::clicked,
	     this,			    &FileSizeStatsWindow::closeOptions );

    connect( _ui->openOptionsButton,	    &QPushButton::clicked,
	     this,			    &FileSizeStatsWindow::openOptions );

    connect( _ui->autoButton,		    &QPushButton::clicked,
	     this,			    &FileSizeStatsWindow::autoPercentiles );

    connect( _ui->startPercentileSlider,    &QSlider::valueChanged,
	     this,			    &FileSizeStatsWindow::applyOptions );

    connect( _ui->startPercentileSpinBox,   qOverload<int>( &QSpinBox::valueChanged ),
	     this,			    &FileSizeStatsWindow::applyOptions );

    connect( _ui->endPercentileSlider,	    &QSlider::valueChanged,
	     this,			    &FileSizeStatsWindow::applyOptions );

    connect( _ui->endPercentileSpinBox,	    qOverload<int>( &QSpinBox::valueChanged ),
	     this,			    &FileSizeStatsWindow::applyOptions );

    connect( _ui->percentileFilterCheckBox, &QCheckBox::stateChanged,
	     this,			    &FileSizeStatsWindow::fillPercentileTable );
}


void FileSizeStatsWindow::populateSharedInstance( QWidget	* mainWindow,
						  FileInfo	* subtree,
						  const QString & suffix  )
{
    if ( !subtree )
	return;

    sharedInstance( mainWindow )->populate( subtree, suffix );
    sharedInstance( mainWindow )->show();
}


void FileSizeStatsWindow::populate( FileInfo * subtree, const QString & suffix )
{
    QString url = subtree->debugUrl();
    if ( url == "<root>" )
	url = subtree->tree()->url();

    _ui->headingUrl->setText( suffix.isEmpty() ? url : tr( "%1 in %2" ).arg( suffix ).arg( url ) );

    delete _stats;
    if ( suffix.isEmpty() )
	_stats = new FileSizeStats( subtree );
    else
	_stats = new FileSizeStats( subtree, suffix );
    CHECK_NEW( _stats );

    fillHistogram();
    fillPercentileTable();
}


void FileSizeStatsWindow::fillPercentileTable()
{
    int step = _ui->percentileFilterCheckBox->isChecked() ? 1 : 5;
    fillQuantileTable( _ui->percentileTable, 100, "P", _stats->percentileSums(), step, 2 );
}


QStringList FileSizeStatsWindow::quantile( int order, const QString & name )
{
    QStringList text;

    if ( _stats->dataSize() < 2 * order )
	return text;

    for ( int i=1; i < order; ++i )
    {
	text << QString( "%1. %2: %3" )
	    .arg( i )
	    .arg( name )
	    .arg( formatSize( _stats->quantile( order, i ) ) );
    }

    text << "";

    return text;
}


void FileSizeStatsWindow::fillQuantileTable( QTableWidget *	    table,
					     int		    order,
					     const QString &	    namePrefix,
					     const PercentileSums & sums,
					     int		    step,
					     int		    extremesMargin )
{
    enum TableColumns
    {
	NumberCol,
	ValueCol,
	NameCol,
	SumCol,
	CumulativeSumCol
    };

    table->clear();
    table->setRowCount( order + 1 );

    QStringList headers;

    switch ( order )
    {
	case 100:	headers << tr( "Percentile"  ); break;
	case  10:	headers << tr( "Decile"      ); break;
	case   4:	headers << tr( "Quartile"    ); break;
	default:	headers << tr( "%1-Quantile" ).arg( order ); break;
    }

    headers << tr( "Size cutoff" ) << tr( "Name" );

    if ( !sums.isEmpty() )
    {
	headers << tr( "Sum %1(n-1)..%2(n)" ).arg( namePrefix ).arg( namePrefix );
	headers << tr( "Cumulative sum" );
    }

    table->setColumnCount( headers.size() );
    table->setHorizontalHeaderLabels( headers );

    const int median = order / 2;
    const int quartile_1 = order % 4 == 0 ? order / 4      : -1;
    const int quartile_3 = order % 4 == 0 ? quartile_1 * 3 : -1;

    int row = 0;

    for ( int i=0; i <= order; ++i )
    {
	if ( step > 1 && i > extremesMargin && i < order - extremesMargin && i % step != 0 )
	    continue;

	addItem( table, row, NumberCol, namePrefix + QString::number( i ) );
	addItem( table, row, ValueCol, formatSize( _stats->quantile( order, i ) ) );
	addItem( table, row, SumCol, i > 0 ? formatSize( sums.individual().at( i ) ) : "" );
	addItem( table, row, CumulativeSumCol, i > 0 ? formatSize( sums.cumulative().at( i ) ) : "" );
/*
	if ( i > 0 && i < sums.size() )
	{
	    addItem( table, row, SumCol, formatSize( sums.individual().at( i ) ) );
	    addItem( table, row, CumulativeSumCol, formatSize( sums.cumulative().at( i ) ) );
	}
*/
	QString text;
	if 	( i == 0 )		text = tr( "Min" );
	else if ( i == order  )		text = tr( "Max" );
	else if ( i == median )		text = tr( "Median" );
	else if ( i == quartile_1 )	text = tr( "1. Quartile" );
	else if ( i == quartile_3 )	text = tr( "3. Quartile" );

	if ( !text.isEmpty() )
	{
	    addItem( table, row, NameCol, text );
	    setRowBold( table, row );
//	    setRowForeground( table, row, QBrush( QColor( Qt::blue ) ) ); // very bad in dark themes
	}
//	else if ( order > 20 && i % 10 == 0 && step <= 1 )

	if ( i % 10 == 0 && step <= 1 )
	{
	    addItem( table, row, NameCol, "" ); // Fill the empty cell

	    // Derive a color with some contrast in light or dark themes.
	    QColor base = table->palette().color( QPalette::Base );
	    const int lightness = base.lightness();
	    base.setHsl( base.hue(), base.saturation(), lightness > 128 ? lightness - 64 : lightness + 64 );
	    setRowBackground( table, row, base );
	}

	++row;
    }

    table->setRowCount( row );

    setColAlignment( table, NumberCol, Qt::AlignRight );
    setColAlignment( table, ValueCol,  Qt::AlignRight );
    setColAlignment( table, NameCol,   Qt::AlignCenter);
    setColAlignment( table, SumCol,    Qt::AlignRight );
    setColAlignment( table, CumulativeSumCol, Qt::AlignRight );

    HeaderTweaker::resizeToContents( table->horizontalHeader() );
}


void FileSizeStatsWindow::fillHistogram()
{
    HistogramView * histogram = _ui->histogramView;
    CHECK_PTR( histogram );

    histogram->clear();
    histogram->setPercentiles( _stats->percentileList() );
    histogram->setPercentileSums( _stats->percentileSums().individual() );
    histogram->autoStartEndPercentiles();
    updateOptions();
    fillBuckets();
    histogram->autoLogHeightScale();
    histogram->build();
}


void FileSizeStatsWindow::fillBuckets()
{
    const int startPercentile = _ui->histogramView->startPercentile();
    const int endPercentile   = _ui->histogramView->endPercentile();
    const int percentileCount = endPercentile - startPercentile;
    const int dataCount       = qRound( _stats->dataSize() * percentileCount / 100.0 );
    const int bucketCount     = _ui->histogramView->bestBucketCount( dataCount );

    const QRealList buckets   = _stats->fillBuckets( bucketCount, startPercentile, endPercentile );
    _ui->histogramView->setBuckets( buckets );
    fillBucketsTable();
}


void FileSizeStatsWindow::fillBucketsTable()
{
    _bucketsTableModel->reset();
    HeaderTweaker::resizeToContents( _ui->bucketsTable->horizontalHeader() );
}

/*
void FileSizeStatsWindow::reject()
{
    deleteLater();
}
*/

void FileSizeStatsWindow::openOptions()
{
    _ui->optionsPanel->show();
    _ui->openOptionsButton->hide();
    updateOptions();
}


void FileSizeStatsWindow::closeOptions()
{
    _ui->optionsPanel->hide();
    _ui->openOptionsButton->show();
}


void FileSizeStatsWindow::applyOptions()
{
    logDebug() << Qt::endl;
    HistogramView * histogram = _ui->histogramView;

    const int newStart = _ui->startPercentileSlider->value();
    const int newEnd   = _ui->endPercentileSlider->value();

    if ( newStart != histogram->startPercentile() || newEnd != histogram->endPercentile() )
    {
	logDebug() << "New start: " << newStart << " new end: " << newEnd << Qt::endl;

	histogram->setStartPercentile( newStart );
	histogram->setEndPercentile  ( newEnd	);
	fillBuckets();
	histogram->autoLogHeightScale(); // FIXME
	histogram->build();
    }
}


void FileSizeStatsWindow::autoPercentiles()
{
    _ui->histogramView->autoStartEndPercentiles();

    updateOptions();
    fillBuckets();
    _ui->histogramView->autoLogHeightScale(); // FIXME
    _ui->histogramView->build();
}


void FileSizeStatsWindow::updateOptions()
{
    const HistogramView * histogram = _ui->histogramView;

    _ui->startPercentileSlider->setValue ( histogram->startPercentile() );
    _ui->startPercentileSpinBox->setValue( histogram->startPercentile() );

    _ui->endPercentileSlider->setValue ( histogram->endPercentile() );
    _ui->endPercentileSpinBox->setValue( histogram->endPercentile() );
}


void FileSizeStatsWindow::showHelp()
{
//    const QString topic = "Statistics.md";
    const QWidget * button = qobject_cast<QWidget *>( sender() );
    if ( !button )
	return;

    const QString helpUrl = "https://github.com/shundhammer/qdirstat/blob/master/doc/stats/" + button->statusTip();
    QDesktopServices::openUrl( helpUrl );
//    QString program = "/usr/bin/xdg-open";

    // logInfo() << "Starting  " << program << " " << helpUrl << Qt::endl;
//    QProcess::startDetached( program, QStringList() << helpUrl );
}
