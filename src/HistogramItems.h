/*
 *   File name: HistogramView.h
 *   Summary:	View widget for histogram rendering for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#ifndef HistogramItems_h
#define HistogramItems_h

#include <QGraphicsRectItem>

#include "HistogramView.h"


namespace QDirStat
{
    /**
     * QGraphicsItem class for a histogram bar: solely to be able
     * to pick up hover events.
     *
     * This creates an invisible full-height item so it is easy to
     * highlight a bucket and get a tooltip, and a visible child
     * rectangle to display the bucket height.
     **/
    class HistogramBar: public QGraphicsRectItem
    {
    public:
	/**
	 * Constructor: 'number' is the number of the bar (0 being the
	 * leftmost) in the histogram
	 **/
	HistogramBar( HistogramView * parent,
		      int	      number,
		      const QRectF &  rect,
		      qreal	      fillHeight );


    protected:
	/**
	 * Mouse hover events
	 *
	 * Reimplemented from QGraphicsItem
	 **/
	void hoverEnterEvent( QGraphicsSceneHoverEvent * ) override;
	void hoverLeaveEvent( QGraphicsSceneHoverEvent * ) override;

	/**
	 * Common function for hover event actions
	 **/
	void hoverEvent( HistogramView::GraphicsItemLayer layer,
			 qreal                            leftAdjustment,
			 qreal                            rightAdjustment );

    };

}	// namespace QDirStat


#endif // ifndef HistogramItems_h
