/*
 *   File name: PercentBar.cpp
 *   Summary:	Functions and item delegate for percent bar
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QPainter>
#include <QTreeView>

#include "PercentBar.h"
#include "Settings.h"
#include "SettingsHelpers.h"
#include "Exception.h"
#include "Logger.h"

using namespace QDirStat;


PercentBarDelegate::PercentBarDelegate( QTreeView * treeView,
                                        int         percentBarCol,
					int         startColorIndex,
					int         invisibleLevels ):
    QStyledItemDelegate { treeView },
    _treeView { treeView },
    _percentBarCol { percentBarCol },
    _startColorIndex { startColorIndex },
    _invisibleLevels { invisibleLevels }
{
    readSettings();
}


PercentBarDelegate::~PercentBarDelegate()
{
    writeSettings();
}


ColorList PercentBarDelegate::defaultFillColors() const
{
    ColorList colors;

    colors << QColor(   0,   0, 255 )
	   << QColor( 128,   0, 128 )
	   << QColor( 231, 147,  43 )
	   << QColor(   4, 113,   0 )
	   << QColor( 176,   0,   0 )
	   << QColor( 204, 187,   0 )
	   << QColor( 162,  98,  30 )
	   << QColor(   0, 148, 146 )
	   << QColor( 217,  94,   0 )
	   << QColor(   0, 194,  65 )
	   << QColor( 194, 108, 187 )
	   << QColor(   0, 179, 255 );

    return colors;
}


void PercentBarDelegate::readSettings()
{
    Settings settings;
    settings.beginGroup( "PercentBar" );

    _fillColors	   = readColorListEntry( settings, "Colors"    , defaultFillColors() );
    _barBackground = readColorEntry    ( settings, "Background", QColor( 160, 160, 160 ) );
    _sizeHintWidth = settings.value( "PercentBarColumnWidth", 180 ).toInt();

    settings.endGroup();
}


void PercentBarDelegate::writeSettings()
{
    Settings settings;
    settings.beginGroup( "PercentBar" );

    writeColorListEntry( settings, "Colors"    , _fillColors	);
    writeColorEntry    ( settings, "Background", _barBackground );
    settings.setValue( "PercentBarColumnWidth",  _sizeHintWidth	);

    settings.endGroup();
}


void PercentBarDelegate::paint( QPainter		   * painter,
				const QStyleOptionViewItem & option,
				const QModelIndex	   & index ) const
{
    if ( ! index.isValid() || index.column() != _percentBarCol )
	return QStyledItemDelegate::paint( painter, option, index );

    QVariant data = percentData( index );

    if ( data.isValid() )
    {
	bool ok = true;
	float percent = data.toFloat( &ok );

	if ( ok && percent >= 0.0 )
	{
	    if ( percent > 100.0f )
	    {
		if ( percent > 103.0f )
		    logError() << "Percent maxed out: " << percent << Qt::endl;
		percent = 100.0f;
	    }

	    int depth = treeLevel( index ) - _invisibleLevels;
            int colorIndex = depth + _startColorIndex;
	    int indentPixel  = ( depth * _treeView->indentation() ) / 2;
	    QColor fillColor = _fillColors.at( colorIndex % _fillColors.size() );

	    PercentBarDelegate::paintPercentBar( percent,
			     painter,
			     indentPixel,
			     option.rect,
			     fillColor,
			     _barBackground );
	}
	else // percent < 0.0 => tree is busy => use as read job column
	{
	    return QStyledItemDelegate::paint( painter, option, index );
	}
    }
}


QVariant PercentBarDelegate::percentData( const QModelIndex & index ) const
{
    QVariant data = index.data( Qt::DisplayRole );
    if ( !data.isValid() )
	return QVariant();

    // This expects a string value formatted like "42.0%"
    QString text = data.toString();
    if ( text.isEmpty() )
	return data;

    text.remove( '%' );

    bool ok = true;
    const float percent = text.toFloat( &ok );
    if ( ! ok )
    {
	logWarning() << "float conversion failed from \"" << text << "\"" << Qt::endl;
	return data;
    }

    return QVariant( percent );
}


int PercentBarDelegate::treeLevel( const QModelIndex & index ) const
{
    int level = 0;

    for ( QModelIndex item = index; item.isValid(); item = item.parent() )
	++level;

    return level;
}


QSize PercentBarDelegate::sizeHint( const QStyleOptionViewItem & option,
                                    const QModelIndex	       & index) const
{
    return QStyledItemDelegate::sizeHint( option, index );
}


void PercentBarDelegate::paintPercentBar( float		 percent,
					  QPainter *	 painter,
					  int		 indentPixel,
					  const QRect  & cellRect,
					  const QColor & fillColor,
					  const QColor & barBackground )
{
    const int penWidth = 2;
    const int extraMargin = cellRect.height() / 6;
    const int itemMargin = 4;
    const int x = cellRect.x() + itemMargin + indentPixel;
    const int y = cellRect.y() + extraMargin;
    const int w = cellRect.width() - 2 * itemMargin - indentPixel;
    const int h = cellRect.height() - 2 * extraMargin;

    painter->eraseRect( cellRect );

    if ( w > 0 )
    {
	QPen pen( painter->pen() );
	pen.setWidth( 0 );
	painter->setPen( pen );
	painter->setBrush( Qt::NoBrush );
	const int fillWidth = ( w - 2 * penWidth ) * percent / 100;


	// Fill bar background.
	painter->fillRect( x + penWidth, y + penWidth,
			   w - 2 * penWidth + 1, h - 2 * penWidth + 1,
			   barBackground );
	/*
	 * Notice: The Xlib XDrawRectangle() function always fills one
	 * pixel less than specified. Although this is very likely just a
	 * plain old bug, it is documented that way. Obviously, Qt just
	 * maps the fillRect() call directly to XDrawRectangle() so they
	 * inherited that bug (although the Qt doc stays silent about
	 * it). So it is really necessary to compensate for that missing
	 * pixel in each dimension.
	 *
	 * If you don't believe it, see for yourself.
	 * Hint: Try the xmag program to zoom into the drawn pixels.
	 **/

	// Fill the desired percentage.
	painter->fillRect( x + penWidth, y + penWidth,
			   fillWidth+1, h - 2 * penWidth+1,
			   fillColor );

	// Draw 3D shadows.
	const QColor background = painter->background().color();

	pen.setColor( contrastingColor ( Qt::black, background ) );
	painter->setPen( pen );
	painter->drawLine( x, y, x+w, y );
	painter->drawLine( x, y, x, y+h );

	pen.setColor( contrastingColor( barBackground.darker(), background ) );
	painter->setPen( pen );
	painter->drawLine( x+1, y+1, x+w-1, y+1 );
	painter->drawLine( x+1, y+1, x+1, y+h-1 );

	pen.setColor( contrastingColor( barBackground.lighter(), background ) );
	painter->setPen( pen );
	painter->drawLine( x+1, y+h, x+w, y+h );
	painter->drawLine( x+w, y, x+w, y+h );

	pen.setColor( contrastingColor( Qt::white, background ) );
	painter->setPen( pen );
	painter->drawLine( x+2, y+h-1, x+w-1, y+h-1 );
	painter->drawLine( x+w-1, y+1, x+w-1, y+h-1 );
    }
}


QColor PercentBarDelegate::contrastingColor( const QColor &desiredColor,
					     const QColor &contrastColor )
{
    if ( desiredColor != contrastColor )
	return desiredColor;

    if ( contrastColor != contrastColor.lighter() )
	// try a little lighter
	return contrastColor.lighter();
    else
	// try a little darker
	return contrastColor.darker();
}
