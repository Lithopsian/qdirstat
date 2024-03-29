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
    QGraphicsRectItem ( rect.normalized() )
{
    setFlags( ItemHasNoContents );
    setAcceptHoverEvents( true );
    setZValue( HistogramView::BarLayer );

    const int numFiles = parent->bucket( number );
    const QString tooltip = QObject::tr( "Bucket #%1<br/>%L2 %3<br/>%4 ... %5" )
	.arg( number + 1 )
	.arg( numFiles )
	.arg( numFiles == 1 ? "file" : "files" )
	.arg( formatSize( parent->bucketStart( number ) ) )
	.arg( formatSize( parent->bucketEnd  ( number ) ) );
    setToolTip( whitespacePre( tooltip ) );

    // Filled rectangle is relative to its parent
    QRectF filledRect( rect.x(), 0, rect.width(), -fillHeight);
    QGraphicsRectItem * filledBar = new QGraphicsRectItem( filledRect.normalized(), this );
    CHECK_NEW( filledBar );

    filledBar->setPen( parent->barPen() );
    filledBar->setBrush( parent->barBrush() );
}


void HistogramBar::hoverEvent( HistogramView::GraphicsItemLayer layer,
			       qreal                            leftAdjustment,
			       qreal                            rightAdjustment )
{
    QList<QGraphicsItem *> children = childItems();
    QGraphicsRectItem * filledBar = dynamic_cast<QGraphicsRectItem *>( children.first() );
    if ( filledBar )
    {
	filledBar->setRect( filledBar->rect().adjusted( leftAdjustment, 0, rightAdjustment, 0 ) );
	setZValue( layer );
    }
}


void HistogramBar::hoverEnterEvent( QGraphicsSceneHoverEvent * )
{
    hoverEvent( HistogramView::HoverBarLayer, -2.0, 2.0 );
}


void HistogramBar::hoverLeaveEvent( QGraphicsSceneHoverEvent * )
{
    hoverEvent( HistogramView::BarLayer, 2.0, -2.0 );
}
