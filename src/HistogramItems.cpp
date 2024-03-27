/*
 *   File name: HistogramViewItems.cpp
 *   Summary:	QGraphicsItems for file size histogram for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QToolTip>

#include "HistogramItems.h"
#include "FormatUtil.h"
#include "Logger.h"
#include "Exception.h"

using namespace QDirStat;


HistogramBar::HistogramBar( HistogramView * parent,
			    int             number,
			    const QRectF  & rect,
			    qreal           fillHeight ):
    QGraphicsRectItem ( rect ),
    _parentView { parent },
    _number { number },
    _startVal { _parentView->bucketStart( number ) },
    _endVal { _parentView->bucketEnd  ( number ) }
{
    // Setting NoPen so this rectangle remains invisible: This full-height
    // rectangle is just for the tooltip. For the bar content, we create a visible
    // separate child item with the correct height.
    setPen( Qt::NoPen );

    const QString tooltip = QObject::tr( "Bucket #%1:\n%2 Files\n%3...%4" )
	.arg( _number + 1 )
	.arg( _parentView->bucket( _number ) )
	.arg( formatSize( _startVal ) )
	.arg( formatSize( _endVal ) );
    setToolTip( tooltip );

    // Filled rectangle is relative to its parent
    QRectF childRect( rect.x(), 0, rect.width(), -fillHeight);
    QGraphicsRectItem * filledRect = new QGraphicsRectItem( childRect, this );
    CHECK_NEW( filledRect );

    filledRect->setPen( _parentView->barPen() );
    filledRect->setBrush( _parentView->barBrush() );
    filledRect->setToolTip( tooltip );

    setZValue( HistogramView::InvisibleBarLayer );
    filledRect->setZValue( HistogramView::BarLayer );

    setFlags( ItemIsSelectable );
    _parentView->scene()->addItem( this );
}

/* whatever was going on here, it doesn't work
void HistogramBar::mousePressEvent( QGraphicsSceneMouseEvent * event )
{
    switch ( event->button() )
    {
	case Qt::LeftButton:
	    {
		QGraphicsRectItem::mousePressEvent( event );

#if 0
                // FIXME: This does not work. Why?

                const QPointF pos( event->scenePos() );
                QToolTip::showText( QPoint( pos.x(), pos.y() ), toolTip(), _parentView );
#endif

		logDebug() << "Histogram bar #" << _number
			   << ": " << _parentView->bucket( _number ) << " items;"
			   << " range: " << formatSize( _startVal )
			   << " .. " << formatSize( _endVal )
			   << Qt::endl;
	    }
	    break;

	default:
	    QGraphicsRectItem::mousePressEvent( event );
	    break;
    }
}
*/



PercentileMarker::PercentileMarker( HistogramView * parent,
				    int		    percentileIndex,
				    const QString & name,
				    const QLineF &  zeroLine,
				    const QPen &    pen ):
    QGraphicsLineItem ( translatedLine( zeroLine, percentileIndex, parent ) ),
    _parentView { parent },
    _name { name },
    _percentileIndex { percentileIndex }
{
    if ( _name.isEmpty() )
    {
	_name = QObject::tr( "Percentile P%1" ).arg( _percentileIndex );
	setZValue( HistogramView::MarkerLayer );
    }
    else
    {
	setZValue( HistogramView::SpecialMarkerLayer );
    }

    setToolTip( _name + "\n" + formatSize( _parentView->percentile( percentileIndex ) ) );
    setPen( pen );
    // setFlags( ItemIsSelectable );
    _parentView->scene()->addItem( this );
}


QLineF PercentileMarker::translatedLine( const QLineF &	 zeroLine,
					 int		 percentileIndex,
					 HistogramView * parent ) const
{
    const qreal value	= parent->percentile( percentileIndex );
    const qreal x	= parent->scaleValue( value );

    return zeroLine.translated( x, 0 );
}


qreal PercentileMarker::value() const
{
    return _parentView->percentile( _percentileIndex );
}

/*
void PercentileMarker::mousePressEvent( QGraphicsSceneMouseEvent * event )
{
    switch ( event->button() )
    {
	case Qt::LeftButton:
	    QGraphicsLineItem::mousePressEvent( event );
	    logDebug() << "Percentile marker #" << _percentileIndex
		       << ": " << _name
		       << ": " << formatSize( _parentView->percentile( _percentileIndex ) )
		       << Qt::endl;
	    break;

	default:
	    QGraphicsLineItem::mousePressEvent( event );
	    break;
    }
}
*/
