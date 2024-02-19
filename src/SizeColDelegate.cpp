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
//#include "Qt4Compat.h"
#include "Exception.h"
#include "FileInfo.h"
#include "Logger.h"

#define SPARSE_COLOR            "#FF4444"
#define ALLOC_COLOR_NORMAL      "#4444FF"
#define ALLOC_COLOR_DARK_THEME  "#BBBBFF"

#define MARGIN_RIGHT    1
#define MARGIN_LEFT     8
#define MARGIN_TOP      1
#define MARGIN_BOTTOM   1

using namespace QDirStat;


void SizeColDelegate::paint( QPainter			* painter,
                             const QStyleOptionViewItem	& option,
                             const QModelIndex		& index ) const
{
    ensureModel( index );

    if ( _model )
    {
        FileInfo * item = _model->itemFromIndex( index );
	if ( isDelegateItem( item ) )
	{
	    // logDebug() << "Small file " << item << Qt::endl;

	    QString text = _model->data( index, Qt::DisplayRole ).toString();
	    QStringList fields = text.split( " (" );  //  "137 B (4k)"
	    if ( fields.size() == 2 )
	    {
		const int alignment  = Qt::AlignRight | Qt::AlignVCenter;
		QRect rect               = option.rect;
		const QPalette & palette = option.palette;

		if ( option.state & QStyle::State_Selected )
		    painter->fillRect( rect, palette.highlight() );

		const QString sizeText  = fields.takeFirst();             // "137 B"
		const QString allocText = " (" + fields.takeFirst();      // " (4k)"

		// Draw the size ("137 B").
		//
		// Since we align right, we need to move the rectangle to the left
		// to reserve some space for the allocated size.
#if (QT_VERSION < QT_VERSION_CHECK( 5, 11, 0 ))
		const int allocWidth = QFontMetrics( option.font ).width( allocText );
#else
		const int allocWidth = QFontMetrics( option.font ).horizontalAdvance( allocText );
#endif
		rect.setWidth( rect.width() - allocWidth );
		painter->setPen( palette.color( QPalette::Text ) );
		painter->drawText( rect, alignment, sizeText );

		// Draw the allocated size (" (4k)").
//		rect = option.rect;
//		rect.setWidth( rect.width() );
		painter->setPen( highlightedText( option, item ) );
		painter->drawText( option.rect, alignment, allocText );

		return;
            }
        }
    }

    // Use the standard delegate for all other items
    return QStyledItemDelegate::paint( painter, option, index );
}


QSize SizeColDelegate::sizeHint( const QStyleOptionViewItem & option,
                                 const QModelIndex	    & index) const
{
    ensureModel( index );

    if ( _model )
    {
	FileInfo * item = _model->itemFromIndex( index );
	if ( isDelegateItem( item ) )
	{
	    const QString text = _model->data( index, Qt::DisplayRole ).toString();
	    const QFontMetrics fontMetrics( option.font );
#if (QT_VERSION < QT_VERSION_CHECK( 5, 11, 0 ))
	    const int width  = fontMetrics.width( text );
#else
	    const int width  = fontMetrics.horizontalAdvance( text );
#endif
	    const int height = fontMetrics.height();
	    const QSize size( width  + MARGIN_RIGHT + MARGIN_LEFT,
			      height + MARGIN_TOP   + MARGIN_BOTTOM );
#if 0
	    logDebug() << "size hint for \"" << text << "\": "
		       << size.width() << ", " << size.height() << Qt::endl;
#endif
	    return size;
	}
    }

    // logDebug() << "Using fallback" << Qt::endl;

#if 0
    const QSize size = QStyledItemDelegate::sizeHint( option, index );
    return QSize( size.width() + MARGIN_RIGHT + MARGIN_LEFT, size.height() );
#endif

    return QStyledItemDelegate::sizeHint( option, index );
}


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


bool SizeColDelegate::isDelegateItem( FileInfo * item ) const
{
    if ( !item || item->isIgnored() )
	return false;

    return ( DirTreeModel::isSmallFileOrSymLink( item ) && item->links() == 1 ) || item->isSparseFile();
}


QColor SizeColDelegate::highlightedText( const QStyleOptionViewItem & option, FileInfo * item ) const
{
    if ( item->isSparseFile() )
	return QColor( SPARSE_COLOR );

    const bool selected = option.state & QStyle::State_Selected;
    const QPalette palette = option.palette;
    return QColor( (selected ? palette.highlight() : palette.base() ).color().lightness() < 160 ?
		   ALLOC_COLOR_DARK_THEME :
		   ALLOC_COLOR_NORMAL );
}
