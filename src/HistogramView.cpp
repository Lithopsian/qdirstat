/*
 *   File name: HistogramView.cpp
 *   Summary:	View widget for histogram rendering for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <math.h>
#include <algorithm>

#include <QGraphicsItem>
#include <QResizeEvent>
#include <QTextDocument>

#include "HistogramView.h"
#include "HistogramItems.h"
#include "AdaptiveTimer.h"
#include "FormatUtil.h"
#include "Logger.h"
#include "Exception.h"


#define VERBOSE_HISTOGRAM 0

#define CHECK_PERCENTILE_INDEX( INDEX ) \
    CHECK_INDEX_MSG( (INDEX), 0, 100, "Percentile index out of range" );

#define MinHistogramWidth	 150.0
#define MinHistogramHeight	  80.0
#define UnicodeMathSigma	0x2211	// 'n-ary summation'

#define MAX_BUCKET_COUNT 100


using namespace QDirStat;


HistogramView::HistogramView( QWidget * parent ):
    QGraphicsView ( parent ),
    _showMedian { true },
    _showQuartiles { true },
    _percentileStep { 0 },
    _leftMarginPercentiles { 0 },
    _rightMarginPercentiles { 5 },
    _leftBorder { 40.0 },
    _rightBorder { 10.0 },
    _topBorder { 30.0 },
    _bottomBorder { 50.0 },
    _viewMargin { 10.0 },
    _markerExtraHeight { 15.0 },
    _overflowWidth { 150.0 },
    _overflowLeftBorder { 10.0 },
    _overflowRightBorder { 10.0 },
    _overflowSpacing { 15.0 },
    _pieDiameter { 60.0 },
    _pieSliceOffset { 10.0 }
{
    init();
}


void HistogramView::init()
{
    _histogramPanel = 0;
    _geometryDirty  = true;

    _bucketMaxValue	    = 0;
    _startPercentile	    = 0;    // data min
    _endPercentile	    = 100;  // data max
    _useLogHeightScale	    = false;
/*
    _showMedian		    = true;
    _showQuartiles	    = true;
    _percentileStep	    = 0;
    _leftMarginPercentiles  = 0;
    _rightMarginPercentiles = 5;
*/
    // TO DO: read from and write to QSettings

    _histogramHeight	  = 250.0;
    _histogramWidth	  = 600.0;
/*
    _leftBorder		  = 40.0;
    _rightBorder	  = 10.0;
    _topBorder		  = 30.0;
    _bottomBorder	  = 50.0;
    _viewMargin		  = 10.0;

    _markerExtraHeight	  = 15.0;

    _overflowWidth	  = 150.0;
    _overflowLeftBorder	  = 10.0;
    _overflowRightBorder  = 10.0;
    _overflowSpacing	  = 15.0;
    _pieDiameter	  = 60.0;
    _pieSliceOffset	  = 10.0;

    _panelBackground	  = scene()->palette().base();
    _barBrush		  = QBrush( QColor( 0xB0, 0xB0, 0xD0 ) );
    _barPen		  = QPen( QColor( 0x40, 0x40, 0x50 ), 1 );

    _medianPen		  = QPen( Qt::magenta, 2 );
    _quartilePen	  = QPen( Qt::blue, 2 );
    _percentilePen	  = QPen( QColor( 0xA0, 0xA0, 0xA0 ), 1 );
    _decilePen		  = QPen( QColor( 0x30, 0x80, 0x30 ), 1 );

    _piePen		  = QPen( Qt::black, 2 );
    _overflowSliceBrush	  = QBrush( QColor( 0xD0, 0x40, 0x20 ) );
*/
}


void HistogramView::clear()
{
    _buckets.clear();
    _percentiles.clear();
    _percentileSums.clear();

    init();

    if ( scene() )
    {
	scene()->clear();
	scene()->invalidate( scene()->sceneRect() );
    }
}


qreal HistogramView::bestBucketCount( int n )
{
    if ( n < 2 )
	return 1;

    // Using the "Rice Rule" which gives more reasonable values for the numbers
    // we are likely to encounter in the context of QDirStat.

    qreal result = 2 * pow( n, 1.0/3.0 );

    if ( result > MAX_BUCKET_COUNT )
    {
#if VERBOSE_HISTOGRAM
	logInfo() << "Limiting bucket count to " << MAX_BUCKET_COUNT
		  << " instead of " << result
		  << Qt::endl;
#endif
	// Enforcing an upper limit so each histogram bar remains wide enough
	// to be clicked on or for tooltips etc.

	result = MAX_BUCKET_COUNT;
    }
    else if ( result < 1.0 )
    {
	result = 1.0;
    }

    return result;
}


qreal HistogramView::bucketWidth( qreal min, qreal max, int bucketCount )
{
    if ( bucketCount < 1 )
	return 0;

    return ( max - min ) / bucketCount;
}


qreal HistogramView::bucketStart( int index ) const
{
    const qreal offset = percentile( _startPercentile );
    return offset + index * bucketWidth();
}


qreal HistogramView::bucketEnd( int index ) const
{
    const qreal offset = percentile( _startPercentile );
    return offset + ( index + 1 ) * bucketWidth();
}


void HistogramView::setBuckets( const QRealList & newBuckets )
{
    _buckets	    = newBuckets;
    _bucketMaxValue = 0;

    for ( const qreal bucket : _buckets )
	_bucketMaxValue = qMax( _bucketMaxValue, bucket );
}


void HistogramView::setPercentiles( const QRealList & newPercentiles )
{
    CHECK_INDEX_MSG( newPercentiles.size(), 101, 101, "Percentiles size out of range" );

    _percentiles = newPercentiles;
}


void HistogramView::setPercentile( int index, qreal value )
{
    CHECK_PERCENTILE_INDEX( index );

    _percentiles[ index ] = value;
}


qreal HistogramView::percentile( int index ) const
{
    CHECK_PERCENTILE_INDEX( index );

    return _percentiles[ index ];
}


void HistogramView::setStartPercentile( int index )
{
    CHECK_PERCENTILE_INDEX( index );

    const bool oldNeedOverflowPanel = needOverflowPanel();
    _startPercentile = index;

    if ( oldNeedOverflowPanel != needOverflowPanel() )
        _geometryDirty = true;
}


void HistogramView::setEndPercentile( int index )
{
    CHECK_PERCENTILE_INDEX( index );

    const bool oldNeedOverflowPanel = needOverflowPanel();
    _endPercentile = index;

    if ( oldNeedOverflowPanel != needOverflowPanel() )
        _geometryDirty = true;

#if VERBOSE_HISTOGRAM
    if ( _startPercentile >= _endPercentile )
    {
	logError() << "startPercentile must be less than endPercentile: "
		   << _startPercentile << ".." << _endPercentile
		   << Qt::endl;
    }
#endif
}


void HistogramView::setPercentileSums( const QRealList & newPercentileSums )
{
    CHECK_INDEX_MSG( newPercentileSums.size(), 101, 101, "Percentile sums size out of range" );

    _percentileSums = newPercentileSums;
}


qreal HistogramView::percentileSum( int index ) const
{
    CHECK_PERCENTILE_INDEX( index );

    if ( _percentileSums.isEmpty() )
	return 0.0;

    return _percentileSums[ index ];
}


qreal HistogramView::percentileSum( int fromIndex, int toIndex ) const
{
    CHECK_PERCENTILE_INDEX( fromIndex );
    CHECK_PERCENTILE_INDEX( toIndex );

    if ( _percentileSums.isEmpty() )
	return 0.0;

    qreal sum = 0.0;

    for ( const qreal percentileSum : _percentileSums )
	sum += percentileSum;

    return sum;
}


qreal HistogramView::bucket( int index ) const
{
    CHECK_INDEX( index, 0, _buckets.size() - 1 );

    return _buckets[ index ];
}


qreal HistogramView::bucketWidth() const
{
    if ( _buckets.isEmpty() || _percentiles.isEmpty() )
	return 0;

    const qreal startVal = percentile( _startPercentile );
    const qreal endVal   = percentile( _endPercentile	  );

    if ( startVal > endVal )
	THROW( Exception( "Invalid percentile data" ) );

    return ( endVal - startVal ) / (qreal) _buckets.size();
}


qreal HistogramView::bucketsTotalSum() const
{
    qreal sum = 0;

    for ( const qreal bucket : _buckets )
	sum += bucket;

//    for ( int i=0; i < _buckets.size(); ++i )
//	sum += _buckets[i];

    return sum;
}


void HistogramView::autoStartEndPercentiles()
{
    if ( _percentiles.isEmpty() )
    {
	logError() << "No percentiles set" << Qt::endl;
	return;
    }

    const qreal q1	= percentile( 25 );
    const qreal q3	= percentile( 75 );
    const qreal qDist	= q3 - q1;

    const qreal minVal = qMax( q1 - 3 * qDist, 0.0 );
    const qreal maxVal = qMin( q3 + 3 * qDist, percentile( 100 ) );

    const bool oldNeedOverflowPanel = needOverflowPanel();

    for ( _startPercentile = 0; _startPercentile <= 25; ++_startPercentile )
    {
	if ( percentile( _startPercentile ) >= minVal )
	    break;
    }

    for ( _endPercentile = 100; _endPercentile >= 75; --_endPercentile )
    {
	if ( percentile( _endPercentile ) <= maxVal )
	    break;
    }

    if ( oldNeedOverflowPanel != needOverflowPanel() )
        _geometryDirty = true;

#if VERBOSE_HISTOGRAM
    logInfo() << "Q1: " << formatSize( q1 )
	      << "  Q3: " << formatSize( q3 )
	      << "  minVal: " << formatSize( minVal )
	      << "  maxVal: " << formatSize( maxVal )
	      << Qt::endl;
    logInfo() << "startPercentile: " << _startPercentile
	      << "  " << formatSize( percentile( _startPercentile ) )
	      << "  endPercentile: " << _endPercentile
	      << "  " << formatSize( percentile( _endPercentile	 ) )
	      << Qt::endl;
#endif
}


bool HistogramView::autoLogHeightScale()
{
    if ( _buckets.isEmpty() )
    {
	logError() << "No buckets set" << Qt::endl;
	return false;
    }

    _useLogHeightScale = false;

    if ( _buckets.size() > 3 )
    {
	QRealList data = _buckets;

	std::sort( data.begin(), data.end() );
	const qreal largest = data.last();

	// We compare the largest bucket with the P90 percentile of the buckets
	// (not to confuse with the P90 percentile with the data the buckets
	// were collected from!)

	const int referencePercentile = 85;
	const int pos = data.size() / 100.0 * referencePercentile;
	const qreal referencePercentileValue = data.at( pos );

	_useLogHeightScale = largest > referencePercentileValue * 10;

#if VERBOSE_HISTOGRAM
	logInfo() << "Largest bucket: " << largest
		  << " bucket P" << referencePercentile
		  << ": " << referencePercentileValue
		  << "	 -> use log height scale: " << _useLogHeightScale
		  << Qt::endl;
#endif
    }

    return _useLogHeightScale;
}


void HistogramView::calcGeometry( const QSize & newSize )
{
    _histogramWidth  = newSize.width();
    _histogramWidth -= _leftBorder + _rightBorder + 2 * _viewMargin;

    if ( needOverflowPanel() )
    {
	_histogramWidth -= _overflowSpacing + _overflowWidth;
        _histogramWidth -= _overflowLeftBorder + _overflowRightBorder;
    }

    if ( _histogramWidth < MinHistogramWidth )
	_histogramWidth = MinHistogramWidth;

    _histogramHeight  = newSize.height();
    _histogramHeight -= _bottomBorder + _topBorder + 2 * _viewMargin;
    _histogramHeight -= 30.0; // compensate for text above

    _histogramHeight  = qBound( MinHistogramHeight, _histogramHeight, 1.5 * _histogramWidth );
    _geometryDirty    = false;

#if VERBOSE_HISTOGRAM
    logDebug() << "Histogram width: " << _histogramWidth
	       << " height: " << _histogramHeight
	       << Qt::endl;
#endif
}


void HistogramView::autoResize()
{
    calcGeometry( viewport()->size() );
}


void HistogramView::resizeEvent( QResizeEvent * event )
{
    // logDebug() << "Event size: " << event->size() << Qt::endl;

    QGraphicsView::resizeEvent( event );
    calcGeometry( event->size() );

    // Seems fast enough that the delay is unhelpful
    build();
}


void HistogramView::fitToViewport()
{
    // This is the black magic that everybody hates from the bottom of his
    // heart about that drawing stuff: Making sure the graphics actually is
    // visible on the screen without unnecessary scrolling.
    //
    // You would think that a widget as sophisticated as QGraphicsView does
    // this all by itself, but no: Everybody has to waste hours upon hours of
    // life time with this crap.

    QRectF rect = scene()->sceneRect().normalized();

    scene()->setSceneRect( rect );

    rect.adjust( -_viewMargin, -_viewMargin, _viewMargin, _viewMargin );

    const QSize visibleSize = viewport()->size();

    if ( rect.width()  <= visibleSize.width() && rect.height() <= visibleSize.height() )
    {
#if VERBOSE_HISTOGRAM
	logDebug() << "Histogram in " << rect.size()
		   << " fits into visible size " << visibleSize
		   << Qt::endl;
#endif
	setTransform( QTransform() ); // Reset scaling etc.
	ensureVisible( rect, 0, 0 );
    }
    else
    {
#if VERBOSE_HISTOGRAM
	logDebug() << "Scaling down histogram in " << rect.size()
		   << " to fit into visible size " << visibleSize
		   << Qt::endl;
#endif
	fitInView( rect, Qt::KeepAspectRatio );
    }
}


void HistogramView::build()
{
     //logInfo() << "Building histogram" << Qt::endl;

    // Don't try this if the viewport geometry isn't set yet
    if ( !isVisible() )
	return;

    if ( _geometryDirty )
        autoResize();

    // QGraphicsScene never resets the min and max in both dimensions where it
    // ever created QGraphicsItems, which makes its sceneRect() call pretty
    // useless. Let's create a new one without those bad old memories.

    delete scene();

    QGraphicsScene * newScene = new QGraphicsScene( this );
    CHECK_NEW( newScene);
    setScene( newScene );

    const QPalette palette = scene()->palette();
    scene()->setBackgroundBrush( palette.base() );

    _panelBackground	  = palette.alternateBase();
    _barBrush		  = QColor( 0xB0, 0xB0, 0xD0 );
    _barPen		  = QColor( 0x40, 0x40, 0x50 );
    _medianPen		  = palette.linkVisited().color();
    _quartilePen	  = palette.link().color();
    _percentilePen	  = palette.buttonText().color();
    _decilePen		  = palette.highlightedText().color();
    _piePen		  = palette.text().color();
    _overflowSliceBrush	  = QColor( 0xD0, 0x40, 0x20 );

    if ( _buckets.size() < 1 || _percentiles.size() != 101 )
    {
	scene()->addText( "No data yet" );
	logInfo() << "No data yet" << Qt::endl;
	return;
    }

    addHistogram();
    addOverflowPanel();

    fitToViewport();
}


qreal HistogramView::scaleValue( qreal value )
{
    const qreal startVal   = _percentiles[ _startPercentile ];
    const qreal endVal     = _percentiles[ _endPercentile   ];
    const qreal totalWidth = endVal - startVal;
    const qreal result     = ( value - startVal ) / totalWidth * _histogramWidth;

    // logDebug() << "Scaling " << formatSize( value ) << " to " << result << Qt::endl;

    return result;
}


void HistogramView::addHistogram()
{
    addHistogramBackground();
    addAxes();
    addYAxisLabel();
    addXAxisLabel();
    addXStartEndLabels();
    addQuartileText();
    addHistogramBars();
    addMarkers();
}


void HistogramView::addHistogramBackground()
{
    const QRectF rect(	-_leftBorder,
			_bottomBorder,
			_histogramWidth   + _leftBorder + _rightBorder,
			-( _histogramHeight + _topBorder  + _bottomBorder ) );

    _histogramPanel = scene()->addRect( rect, QPen( Qt::NoPen ), _panelBackground );
    _histogramPanel->setZValue( PanelBackgroundLayer );
}


void HistogramView::addAxes()
{
    QPen pen( QColor( scene()->palette().text().color() ), 1 );

    QGraphicsItem * xAxis = scene()->addLine( 0, 0, _histogramWidth, 0, pen );
    QGraphicsItem * yAxis = scene()->addLine( 0, 0, 0, -_histogramHeight, pen );

    xAxis->setZValue( AxisLayer );
    yAxis->setZValue( AxisLayer );
}


void HistogramView::addYAxisLabel()
{
    QGraphicsTextItem * item = scene()->addText( "" );
    item->setHtml( _useLogHeightScale ? "log<sub>2</sub>(n)   -->" : "n" );
    setBold( item );

    const qreal   textWidth   = item->boundingRect().width();
    const qreal   textHeight  = item->boundingRect().height();
    const QPointF labelCenter = QPoint( -_leftBorder / 2, -_histogramHeight / 2 );

    if ( _useLogHeightScale )
    {
	item->setRotation( 270 );
	item->setPos( labelCenter.x() - textHeight / 2, labelCenter.y() + textWidth  / 2 );
    }
    else
    {
	item->setPos( labelCenter.x() - textWidth  / 2, labelCenter.y() - textHeight / 2 );
    }

    item->setZValue( TextLayer );
}


void HistogramView::addXAxisLabel()
{
    const QString labelText = tr( "File Size" ) + "  -->";

    QGraphicsTextItem * item = scene()->addText( labelText );
    setBold( item );

    const qreal   textWidth	= item->boundingRect().width();
    const qreal   textHeight	= item->boundingRect().height();
    const QPointF labelCenter	= QPoint( _histogramWidth / 2, _bottomBorder );
    item->setPos( labelCenter.x() - textWidth / 2, labelCenter.y() - textHeight ); // Align bottom

    item->setZValue( TextLayer );
}


void HistogramView::addXStartEndLabels()
{
    QString startLabel = _startPercentile == 0 ? tr( "Min" ) : QString( "P%1" ).arg( _startPercentile );
    startLabel += "\n" + formatSize( percentile( _startPercentile ) );

    QString endLabel = _endPercentile == 100 ? tr( "Max" ) : QString( "P%1" ).arg( _endPercentile );
    endLabel += "\n" + formatSize( percentile( _endPercentile ) );

    QGraphicsTextItem * startItem = scene()->addText( startLabel );
    QGraphicsTextItem * endItem   = scene()->addText( endLabel );

    const qreal endTextWidth = endItem->boundingRect().width();
    endItem->setTextWidth( endTextWidth );
    endItem->document()->setDefaultTextOption( QTextOption( Qt::AlignRight ) );

    startItem->setPos( 0, _bottomBorder - startItem->boundingRect().height() );
    endItem->setPos( _histogramWidth - endTextWidth, _bottomBorder - endItem->boundingRect().height() );

    startItem->setZValue( TextLayer );
    endItem->setZValue( TextLayer );

}


void HistogramView::addQuartileText()
{
    const qreal textBorder  = 10.0;
    const qreal textSpacing = 30.0;

    qreal x = 0;
    qreal y = -_histogramHeight - _topBorder - textBorder;
    const qreal n = bucketsTotalSum();

    if ( n > 0 ) // Only useful if there are any data at all
    {
	const QString q1Text = tr( "Q1: %1" ).arg( formatSize( percentile( 25 ) ) );
	const QString q3Text = tr( "Q3: %1" ).arg( formatSize( percentile( 75 ) ) );
	const QString medianText = tr( "Median: %1" ).arg( formatSize( percentile( 50 ) ) );

	QGraphicsTextItem * q1Item     = scene()->addText( q1Text );
	QGraphicsTextItem * q3Item     = scene()->addText( q3Text );
	QGraphicsTextItem * medianItem = scene()->addText( medianText );

	q1Item->setDefaultTextColor( _quartilePen.color() );
	q3Item->setDefaultTextColor( _quartilePen.color() );
	medianItem->setDefaultTextColor( _medianPen.color() );

	setBold( medianItem);
	setBold( q1Item);
	setBold( q3Item);

	y -= medianItem->boundingRect().height();

	const qreal q1Width	  = q1Item->boundingRect().width();
	const qreal q3Width	  = q3Item->boundingRect().width();
	const qreal medianWidth = medianItem->boundingRect().width();

	q1Item->setPos( x, y );	     x += q1Width     + textSpacing;
	medianItem->setPos( x, y );  x += medianWidth + textSpacing;
	q3Item->setPos( x, y );	     x += q3Width     + textSpacing;

	q1Item->setZValue( TextLayer );
	q3Item->setZValue( TextLayer );
	medianItem->setZValue( TextLayer );
    }

    // Add text for the total number of files
    QGraphicsTextItem * nTextItem = scene()->addText( tr( "Files (n): %1" ).arg( n ) );
    setBold( nTextItem );

    const QFontMetrics metrics( nTextItem->font() );
    const QChar sigma( UnicodeMathSigma );
    if ( metrics.inFont( sigma ) )
	nTextItem->setPlainText( QString( "%1n: %2" ).arg( sigma ).arg( n ) );

    if ( n == 0 )
	y -= nTextItem->boundingRect().height();

    nTextItem->setPos( x, y );
    nTextItem->setZValue( TextLayer );
}


void HistogramView::addHistogramBars()
{
    const qreal barWidth = _histogramWidth / _buckets.size();
    const qreal maxVal = _useLogHeightScale ? log2( _bucketMaxValue ) : _bucketMaxValue;

    for ( int i=0; i < _buckets.size(); ++i )
    {
	// logDebug() << "Adding bar #" << i << " with value " << _buckets[ i ] << Qt::endl;
	const QRectF rect( i * barWidth, 0, barWidth, -_histogramHeight );

	qreal val = _buckets[i];
	if ( _useLogHeightScale && val > 1.0 )
	    val = log2( val );

	const qreal fillHeight = maxVal == 0 ? 0.0 : val / maxVal * _histogramHeight;
	HistogramBar * bar = new HistogramBar( this, i, rect, fillHeight );
	CHECK_NEW( bar );
    }
}


void HistogramView::addMarkers()
{
    const qreal totalWidth = _percentiles[ _endPercentile   ] - _percentiles[ _startPercentile ];

    if ( totalWidth < 1 )
	return;

    const QLineF zeroLine( 0, _markerExtraHeight, 0, -( _histogramHeight + _markerExtraHeight ) );

    // Show ordinary percentiles (all except Q1, Median, Q3)

    for ( int i = _startPercentile + 1; i < _endPercentile; ++i )
    {
	if ( i == 0 || i == 100 )
	    continue;

	if ( i == 50 && _showMedian )
	    continue;

	if ( ( i == 25 || i == 75 ) && _showQuartiles )
	    continue;

	if ( _percentileStep != 1 )
        {
            bool skip = true;

            if ( i <= _startPercentile + _leftMarginPercentiles && i < 25 )
                skip = false;

            if ( i >= _endPercentile - _rightMarginPercentiles && i > 75 )
                skip = false;

            if  ( skip && _percentileStep != 0 && i % _percentileStep == 0 )
                skip = false;

            if ( skip )
                continue;
        }

	const QPen pen = _percentileStep != 0 && _percentileStep != 5 && i % 10 == 0 ? _decilePen : _percentilePen;
	new PercentileMarker( this, i, "", zeroLine, pen );
    }

    if ( _showQuartiles )
    {
	if ( percentileDisplayed( 25 ) )
	    new PercentileMarker( this, 25, tr( "Q1 (1st Quartile)" ), zeroLine, _quartilePen );

	if ( percentileDisplayed( 75 ) )
	    new PercentileMarker( this, 75, tr( "Q3 (3rd Quartile)" ), zeroLine, _quartilePen );
    }

    if ( _showMedian && percentileDisplayed( 50 ) )
	new PercentileMarker( this, 50, tr( "Median" ), zeroLine, _medianPen );
}


QGraphicsTextItem * HistogramView::addText( const QPointF & pos, const QString & text )
{
    QGraphicsTextItem * textItem = scene()->addText( text );
    textItem->setPos( pos );
    textItem->setDefaultTextColor( scene()->palette().text().color() );

    return textItem;
}

QPointF HistogramView::addText( const QPointF & pos, const QStringList & lines )
{
    QGraphicsTextItem * textItem = addText( pos, lines.join( "\n" ) );

    return QPoint( pos.x(), pos.y() + textItem->boundingRect().height() );
}


QPointF HistogramView::addBoldText( const QPointF & pos, const QString & text )
{
    QGraphicsTextItem * textItem = addText( pos, text );
    setBold( textItem );

    return QPoint( pos.x(), pos.y() + textItem->boundingRect().height() );
}


void HistogramView::addOverflowPanel()
{
    if ( ! needOverflowPanel() )
	return;

    // Panel for the overflow area
    const QRectF histPanelRect = _histogramPanel->boundingRect().normalized();

    const QRectF rect( histPanelRect.topRight().x() + _overflowSpacing,
		 histPanelRect.topRight().y(),
		 _overflowWidth + _overflowLeftBorder + _overflowRightBorder,
		 histPanelRect.height() );

    QGraphicsRectItem * cutoffPanel =
	scene()->addRect( rect, QPen( Qt::NoPen ), _panelBackground );

    // Headline
    QPointF nextPos( rect.x() + _overflowLeftBorder, rect.y() );

    nextPos = addBoldText( nextPos, tr( "Cut off" ) );

    // Text about cut-off percentiles and size
    const qreal filesInHistogram = bucketsTotalSum();
    const qreal totalFiles = bucketsTotalSum() / ( _endPercentile - _startPercentile ) * 100.0;
    const int missingFiles = totalFiles - filesInHistogram;

    const QStringList lines =
	{ "",
	  tr( "Min (P0) ... P%1" ).arg( _startPercentile ),
	  formatSize( percentile( 0 ) ) + "..." + formatSize( percentile( _startPercentile ) ),
	  "",
	  tr( "P%1 ... Max (P100)" ).arg( _endPercentile ),
	  formatSize( percentile( _endPercentile ) ) + "..." + formatSize( percentile( 100 ) ),
	};
    nextPos = addText( nextPos, lines );

    // Upper pie chart: Number of files cut off
    nextPos.setY( nextPos.y() + _pieSliceOffset );
    QRectF pieRect( QRectF( nextPos, QSizeF( _pieDiameter, _pieDiameter ) ) );

    const int cutoff = _startPercentile + 100 - _endPercentile;
    nextPos = addPie( pieRect,
		      100 - cutoff, cutoff,
		      _barBrush, _overflowSliceBrush );

    // Caption for the upper pie chart
    const QStringList pieCaption = { missingFiles == 1 ? tr( "1 file" ) : tr( "%1 files" ).arg( missingFiles ),
				     tr( "%1% of all files" ).arg( cutoff ),
                                     ""
                                   };
    nextPos = addText( nextPos, pieCaption );

    // Lower pie chart: Disk space disregarded
    const qreal histogramDiskSpace = percentileSum( _startPercentile, _endPercentile );
    qreal cutoffDiskSpace    = percentileSum( 0, _startPercentile );

    if ( _endPercentile < 100 )
        cutoffDiskSpace += percentileSum( _endPercentile, 100 );

    const qreal cutoffSpacePercent = 100.0 * cutoffDiskSpace / ( histogramDiskSpace + cutoffDiskSpace );

    nextPos.setY( nextPos.y() + _pieSliceOffset );
    pieRect = QRectF( nextPos, QSizeF( _pieDiameter, _pieDiameter ) );

    if ( cutoffDiskSpace > histogramDiskSpace )
    {
	nextPos = addPie( pieRect,
			  cutoffDiskSpace, histogramDiskSpace,
			  _overflowSliceBrush, _barBrush );
    }
    else
    {
	nextPos = addPie( pieRect,
			  histogramDiskSpace, cutoffDiskSpace,
			  _barBrush, _overflowSliceBrush );
    }


    // Caption for the lower pie chart
    const QStringList pieCaption2 = { formatSize( cutoffDiskSpace ),
				      tr( "%1% of disk space" ).arg( cutoffSpacePercent, 0, 'f', 1 ),
                                      ""
                                    };
    nextPos = addText( nextPos, pieCaption2 );

    // Make sure the panel is tall enough to fit everything in
    if ( nextPos.y() > cutoffPanel->rect().bottom() )
    {
	QRectF rect( cutoffPanel->rect() );
	rect.setBottomLeft( QPointF( rect.x(), nextPos.y()  ) );
	cutoffPanel->setRect( rect );
    }
}


QPointF HistogramView::addPie( const QRectF & rect,
			       qreal	      val1,
			       qreal	      val2,
			       const QBrush & brush1,
			       const QBrush & brush2 )
{
    if ( val1 == 0.0 && val2 == 0.0 )
	return rect.topLeft();

    const qreal FullCircle = 360.0 * 16.0; // Qt uses 1/16 degrees
    const qreal angle1 = val1 / ( val1 + val2 ) * FullCircle;
    const qreal angle2 = FullCircle - angle1;

    QGraphicsEllipseItem * slice1 = scene()->addEllipse( rect );
    slice1->setStartAngle( angle2 / 2.0 );
    slice1->setSpanAngle( angle1 );
    slice1->setBrush( brush1 );
    slice1->setPen( _piePen );

    QRectF rect2( rect );
    rect2.moveTopLeft( rect.topLeft() + QPoint( _pieSliceOffset, 0.0 ) );

    QGraphicsEllipseItem * slice2 = scene()->addEllipse( rect2 );
    slice2->setStartAngle( -angle2 / 2.0 );
    slice2->setSpanAngle( angle2 );
    slice2->setBrush( brush2 );
    slice2->setPen( _piePen );

    QGraphicsItemGroup * pie = scene()->createItemGroup( { slice1, slice2 } );
    const QPointF pieCenter = rect.center();

    // Figuring out the following arcane sequence took me well over 2 hours.
    //
    // Seriously, trolls, WTF?! One of the most common things to do is to
    // rotate a QGraphicsItem around its center. But this is the most difficult
    // thing to do, and, adding insult to injury, IT IS NOT EXPLAINED IN THE
    // DOCUMENTATION!

    QTransform transform;
    transform.translate( pieCenter.x(), pieCenter.y() );
    transform.rotate( -45.0 );
    transform.translate( -pieCenter.x(), -pieCenter.y() );
    pie->setTransform( transform );

    return QPoint( rect.x(), rect.y() + pie->boundingRect().height() );
}

/*
void HistogramView::setBold( QGraphicsTextItem * item )
{
    QFont font( item->font() );
    font.setBold( true );
    item->setFont( font );
}


void HistogramView::setBold( QGraphicsSimpleTextItem * item )
{
    QFont font( item->font() );
    font.setBold( true );
    item->setFont( font );
}

*/
