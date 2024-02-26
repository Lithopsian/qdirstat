/*
 *   File name: SizeColDelegate.cpp
 *   Summary:	DirTreeView delegate for the size column
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#include <QPainter>
#include <QPalette>
#include <QTreeView>
#include <QApplication>

#include "SizeColDelegate.h"
#include "Exception.h"
#include "FileInfo.h"
#include "Logger.h"

#define SPARSE_COLOR_NORMAL "#FF22AA"
#define SPARSE_COLOR_DARK   "#FF8888"
#define ALLOC_COLOR_NORMAL  "#2222FF"
#define ALLOC_COLOR_DARK    "#CCCCFF"
#define LIGHTNESS_THRESHOLD 144  // between the two alloc shades

#define TOP_MARGIN    1
#define BOTTOM_MARGIN 1
#define RIGHT_MARGIN  3
#define LEFT_MARGIN   5

using namespace QDirStat;


void SizeColDelegate::paint( QPainter			* painter,
                             const QStyleOptionViewItem	& option,
                             const QModelIndex		& index ) const
{
//    ensureModel( index );

//    if ( _model )
//    {
//        FileInfo * item = _model->itemFromIndex( index );
//	if ( isDelegateItem( item ) )
//	{
	    // logDebug() << "Small file " << item << Qt::endl;
	    QStringList data = index.data( RawDataRole ).toStringList();
	    const bool sparseFile = data.size() == 3;
	    const QString linksText = sparseFile ? data.takeLast() : "";

//	    QString text = _model->data( index, Qt::DisplayRole ).toString();
//	    QStringList fields = text.split( " (" );  //  "137 B (4k)"
	    if ( data.size() == 2 )
	    {
		const int alignment  = Qt::AlignRight | Qt::AlignVCenter;
		QRect rect               = option.rect;
		const QPalette & palette = option.palette;

		// We are responsible even for the highlight background of selected rows
		const bool selected = option.state & QStyle::State_Selected;
		if ( selected )
		    painter->fillRect( rect, palette.highlight() );

		const QString sizeText  = data.takeFirst(); // "137 B"
		const QString allocText = data.takeFirst(); // " (4k)"

		// Draw the size ("137 B").
		//
		// Since we align right, we need to move the rectangle to the left
		// to reserve some space for the allocated size and any links text.
#if (QT_VERSION < QT_VERSION_CHECK( 5, 11, 0 ))
		const int allocWidth = QFontMetrics( option.font ).width( allocText );
		const int linksWidth = QFontMetrics( option.font ).width( linksText );
#else
		const int allocWidth = QFontMetrics( option.font ).horizontalAdvance( allocText );
		const int linksWidth = QFontMetrics( option.font ).horizontalAdvance( linksText );
#endif
		rect.setWidth( rect.width() - allocWidth - linksWidth - RIGHT_MARGIN );
		painter->setPen( palette.color( selected ? QPalette::HighlightedText : QPalette::Text ) );
		painter->drawText( rect, alignment, sizeText );
		rect.setWidth( option.rect.width() - RIGHT_MARGIN );
		painter->drawText( rect, alignment, linksText );

		// Draw the allocated size (" (4k)").
//		rect = option.rect;
//		rect.setWidth( rect.width() );
		rect.setWidth( option.rect.width() - linksWidth - RIGHT_MARGIN );
		painter->setPen( highlightedText( option, sparseFile ) );
		painter->drawText( rect, alignment, allocText );

		return;
            }
//        }
//    }

    // Use the standard delegate for all other items
    QStyledItemDelegate::paint( painter, option, index );
}


QSize SizeColDelegate::sizeHint( const QStyleOptionViewItem & option,
                                 const QModelIndex	    & index) const
{
//    ensureModel( index );

//    if ( _model )
//    {
	QStringList data = index.data( RawDataRole ).toStringList();
	if ( data.size() == 2 || data.size() == 3 )

//	FileInfo * item = _model->itemFromIndex( index );
//	if ( isDelegateItem( item ) )
	{
	    const QString text = data.join( "" );
//	    const QString text = _model->data( index, Qt::DisplayRole ).toString();
	    const QFontMetrics fontMetrics( option.font );
#if (QT_VERSION < QT_VERSION_CHECK( 5, 11, 0 ))
	    const int width  = fontMetrics.width( text ) + LEFT_MARGIN + RIGHT_MARGIN;
#else
	    const int width  = fontMetrics.horizontalAdvance( text ) + TOP_MARGIN + BOTTOM_MARGIN;
#endif
	    const int height = fontMetrics.height() + TOP_MARGIN + BOTTOM_MARGIN;
	    const QSize size( width, height );
#if 0
	    logDebug() << "size hint for \"" << text << "\": " << width << ", " << height << Qt::endl;
#endif
	    return QSize( width, height );
//	}
    }

    // logDebug() << "Using fallback" << Qt::endl;

#if 0
    const QSize size = QStyledItemDelegate::sizeHint( option, index );
    return QSize( size.width() + MARGIN_RIGHT + MARGIN_LEFT, size.height() );
#endif

    return QStyledItemDelegate::sizeHint( option, index );
}

/*
void SizeColDelegate::ensureModel( const QModelIndex & index ) const
{
    if ( !_model && index.isValid() && index.column() == SizeCol )
    {
        const DirTreeModel * constModel = dynamic_cast<const DirTreeModel *>( index.model() );

        // This mess with const_cast and mutable DirTreeModel * is only
        // necessary because the Trolls in their infinite wisdom saw fit to
        // make this whole item/view stuff as inaccessible as they possibly
        // could: The QModelIndex only stores a CONST pointer to the model, and
        // the paint() method is const for whatever reason.
        //
        // Adding insult to injury, the creation order is view, delegate,
        // model, and then the model is put into the view; so we can't simply
        // put the model into the delegate in the constructor; we have to get
        // it out somewhere, and what better place is there than from a
        // QModelIndex? But no, they nail everything down with this "const"
        // insanity. There is nothing wrong with data encapsulation, but there
        // is such a thing as making classes pretty much unusable; Qt's model /
        // view classes are a classic example.
        //
        // I wish some day they might come out of their ivory tower and meet
        // the real life. Seriously: WTF?!

        if ( constModel )
            _model = const_cast<DirTreeModel *>( constModel );

        if ( ! _model )
            logError() << "WRONG_MODEL TYPE" << Qt::endl;
    }
}


bool SizeColDelegate::isDelegateItem( FileInfo * item )
{
    if ( !item || item->isIgnored() )
	return false;

    return ( DirTreeModel::useSmallFileSizeText( item ) && item->links() == 1 ) || item->isSparseFile();
}
*/

QColor SizeColDelegate::highlightedText( const QStyleOptionViewItem & option, bool sparseFile )
{
    const bool selected = option.state & QStyle::State_Selected;
    const QColor background = selected ? option.palette.highlight().color() : option.palette.base().color();
    const bool isDark = background.lightness() < LIGHTNESS_THRESHOLD;

    if ( sparseFile )
	return QColor( isDark ? SPARSE_COLOR_DARK : SPARSE_COLOR_NORMAL );

    return QColor( isDark ? ALLOC_COLOR_DARK : ALLOC_COLOR_NORMAL );
}
