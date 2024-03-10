/*
 *   File name: TreemapTile.cpp
 *   Summary:    Treemap rendering for QDirStat
 *   License:    GPL V2 - See file LICENSE for details.
 *
 *   Author:    Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#include <math.h>

#include <QElapsedTimer>
#include <QImage>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPainter>

#include "TreemapTile.h"
#include "ActionManager.h"
#include "CleanupCollection.h"
#include "Exception.h"
#include "FileInfoIterator.h"
#include "MimeCategorizer.h"
#include "SelectionModel.h"
#include "TreemapView.h"
#include "Logger.h"


using namespace QDirStat;

// constructor with no parent tile, only used for the root tile
TreemapTile::TreemapTile( TreemapView * parentView,
                          FileInfo * orig,
                          const QRectF & rect ):
    QGraphicsRectItem ( rect ),
    _parentView { parentView },
//    _parentTile { nullptr },
    _orig { orig },
    _cushionSurface { _parentView->cushionHeights() }, // initial cushion surface
    _highlighter { nullptr },
    _firstTile { true },
    _lastTile { false }
{
    //logDebug() << "Creating root tile " << orig << "    " << rect << Qt::endl;
    init();

    if ( itemTotalSize( _orig ) == 0 )    // Prevent division by zero
        return;

    if ( _parentView->squarify() )
        createSquarifiedChildren(rect);
    else if ( rect.width() > rect.height() )
        createChildrenHorizontal( rect );
    else
        createChildrenVertical( rect );

    _stopwatch.start();

    int threads = 0;
    for ( auto future = _parentView->tileFuturesBegin() ; future != _parentView->tileFuturesEnd(); ++future, ++threads )
        ( *future ).waitForFinished();

//    logDebug() << _stopwatch.restart() << "ms for " << threads << " threads to finish" << (_parentView->treemapCancelled() ? " (cancelled)" : "") << Qt::endl;
}

// constructor for simple (non-squarified) children
TreemapTile::TreemapTile( TreemapTile * parentTile,
                          FileInfo * orig,
                          const QRectF & rect ):
    QGraphicsRectItem ( rect, parentTile ),
    _parentView { parentTile->_parentView },
//    _parentTile { parentTile },
    _orig { orig },
    _cushionSurface { parentTile->_cushionSurface, _parentView->cushionHeights() }, // copy the parent cushion and scale the height
    _highlighter { nullptr },
    _firstTile { false },
    _lastTile { false }
{
//    logDebug() << "Creating non-squarified child for " << orig << " (in " << parentTile->_orig << ")" << Qt::endl;

    init();
}

HorizontalTreemapTile::HorizontalTreemapTile( TreemapTile * parentTile,
                                              FileInfo * orig,
                                              const QRectF & rect ) :
    TreemapTile ( parentTile, orig, rect )
{
    if ( orig->isDirInfo() && !_parentView->treemapCancelled() )
        createChildrenHorizontal( rect );
}

VerticalTreemapTile::VerticalTreemapTile( TreemapTile * parentTile,
                                          FileInfo * orig,
                                          const QRectF & rect ) :
    TreemapTile ( parentTile, orig, rect )
{
    if ( orig->isDirInfo() && !_parentView->treemapCancelled() )
        createChildrenVertical( rect );
}

// constructor for squarified layout, with the cushion specified explicitly to allow for a row cushion
TreemapTile::TreemapTile( TreemapTile * parentTile,
                          FileInfo    * orig,
                          const QRectF & rect,
                          const CushionSurface & cushionSurface ):
    QGraphicsRectItem ( rect, parentTile ),
    _parentView { parentTile->_parentView },
//    _parentTile { parentTile },
    _orig { orig },
    _cushionSurface { cushionSurface }, // uses the default copy constructor on a row cushion
    _highlighter { nullptr },
    _firstTile ( false ),
    _lastTile { false }
{
    //logDebug() << "Creating squarified tile for " << orig << "  " << rect << Qt::endl;

    init();

    if ( orig->isDirInfo() && !_parentView->treemapCancelled() )
        createSquarifiedChildren( rect );
}

TreemapTile::~TreemapTile()
{
    // DO NOT try to delete the _highlighter: it is owned by the TreemapView /
    // QGraphicsScene and deleted together with all other QGraphicsItems
    // in the TreemapView destructor.
}

void TreemapTile::init()
{
    setPen( Qt::NoPen );

//    _parentView->setLastTile( this ); // only for logging

    setFlags( ItemIsSelectable );

    if ( (  _orig->isDir() && _orig->totalSubDirs() == 0 ) || _orig->isDotEntry() )
        setAcceptHoverEvents( true );
}

void TreemapTile::createChildrenHorizontal( const QRectF & rect )
{
    FileInfoSortedBySizeIterator it( _orig, itemTotalSize );
    FileSize totalSize = it.totalSize();

    if ( totalSize == 0 )
        return;

    _cushionSurface.addVerticalRidge( rect.top(), rect.bottom() );

    // All stripes are scaled by the same amount
    const double scale = rect.width() / totalSize;

    // To avoid rounding errors accumulating, every tile is positioned relative to the parent
    // Items that don't reach a pixel from the previous item are silently dropped
    FileSize cumulativeSize = 0;
    double offset = 0;
    double nextOffset = qMin( rect.width(), _parentView->minTileSize() );
    while ( *it && offset < rect.width() )
    {
        cumulativeSize += itemTotalSize( *it );
        const double newOffset = round( scale * cumulativeSize );
        if ( newOffset >= nextOffset )
        {
            QRectF childRect = QRectF( rect.left() + offset, rect.top(), newOffset - offset, rect.height() );
            TreemapTile *tile = new VerticalTreemapTile( this, *it, childRect );
            CHECK_NEW( tile );
            tile->cushionSurface().addHorizontalRidge( childRect.left(), childRect.right() );

            if ( ( *it )->isDirInfo() )
                addRenderThread( tile, 4 );
//                tile->_cushion = tile->renderCushion( childRect );

            offset = newOffset;
            nextOffset = qMin( rect.width(), newOffset + _parentView->minTileSize() );
        }

        ++it;
    }
}

void TreemapTile::createChildrenVertical( const QRectF & rect )
{
    FileInfoSortedBySizeIterator it( _orig, itemTotalSize );
    FileSize totalSize = it.totalSize();

    if (totalSize == 0)
        return;

    _cushionSurface.addHorizontalRidge( rect.left(), rect.right() );

    // All stripes are scaled by the same amount
    const double scale = rect.height() / totalSize;

    // To avoid rounding errors accumulating, every tile is positioned relative to the parent
    // Items that don't reach a pixel from the previous item are silently dropped
    FileSize cumulativeSize = 0;
    double offset = 0;
    double nextOffset = qMin( rect.height(), _parentView->minTileSize() );
    while ( *it && offset < rect.height() )
    {
        cumulativeSize += itemTotalSize( *it );
        const double newOffset = round( scale * cumulativeSize );
        if ( newOffset >= nextOffset )
        {
            QRectF childRect = QRectF( rect.left(), rect.top() + offset, rect.width(), newOffset - offset );
            TreemapTile *tile = new HorizontalTreemapTile( this, *it, childRect );
            CHECK_NEW( tile );
            tile->cushionSurface().addVerticalRidge( childRect.top(), childRect.bottom() );

            if ( ( *it )->isDirInfo() )
                addRenderThread( tile, 4 );
//                tile->_cushion = tile->renderCushion( childRect );

            offset = newOffset;
            nextOffset = qMin( rect.height(), newOffset + _parentView->minTileSize() );
        }

        ++it;
    }
}

void TreemapTile::createSquarifiedChildren( const QRectF & rect )
{
    // Get all the children of this tile and total them up
    FileInfoSortedBySizeIterator it( _orig, itemTotalSize );
    FileSize remainingTotal = it.totalSize();

    // Don't show completely empty directories in the treemap, avoids divide by zero issues
    if ( remainingTotal == 0 )
        return;

    QRectF childrenRect = rect;
    FileInfo *rowEnd = *it;
    while ( rowEnd && childrenRect.height() >= 0 && childrenRect.width() >= 0 )
    {
        // Square treemaps always layout the next row of tiles along the shortest dimension
        const Orientation dir = childrenRect.width() < childrenRect.height() ? TreemapHorizontal : TreemapVertical;
        const double primary = dir == TreemapHorizontal ? childrenRect.width() : childrenRect.height();
        const double secondary = dir == TreemapHorizontal ? childrenRect.height() : childrenRect.width();

        // Find the set of items that fill a row with tiles as near as possible to squares
        FileInfoListPos rowStartIt = it.currentPos();
        FileSize rowTotal = squarify( childrenRect, it, remainingTotal );

        // Rows 0.5-1.0 pixels high all get rounded up so we'll probably run out of space, but just in case ...
        // ... rows < 0.5 pixels high will never get rounded up, so force them
        double height = secondary * rowTotal / remainingTotal;
        while ( height <= _parentView->minSquarifiedTileHeight() && height < secondary )
        {
            // Aspect ratio hardly matters any more, so fast forward enough items to make half a pixel
            // (many of these tiny items will be dropped while laying out a row of tiles)
            if ( *it )
            {
                rowTotal += itemTotalSize( *it );
                ++it;
            }
            else
                // If we run out of items, force the dregs to take up any space still left
                rowTotal = remainingTotal;

            height = secondary * rowTotal / remainingTotal;
        }
        rowEnd = *it;

        it.setPos( rowStartIt );
        layoutRow( dir, childrenRect, it, rowEnd, rowTotal, primary, round( height ) );

        remainingTotal -= rowTotal;
    }
}

FileSize TreemapTile::squarify( const QRectF & rect,
                                FileInfoSortedBySizeIterator & it,
                                FileSize remainingTotal )
{
    //logDebug() << "squarify() " << this << " " << rect << Qt::endl;

    // We only care about ratios, so scale everything for speed of calculation
    // rowHeightScale = rowHeight / remainingTotal, scale this to 1
    // rowWidthScale = rowWidth, scaled to rowWidth / rowHeight * remainingTotal
    const double rowRatio = rect.width() < rect.height() ? rect.width() / rect.height() : rect.height() / rect.width();
    const double rowWidthScale = rowRatio * remainingTotal; // really rectWidth

    const FileSize firstSize = itemTotalSize( *it );
    FileSize sum = 0;
    double bestAspectRatio = 0;
    while ( *it )
    {
        FileSize size = itemTotalSize( *it );
        if ( size > 0 )
        {
            sum += size;

            // Again, only ratios matter, so avoid the size / sum division by multiplying both by sum
            const double rowHeight = ( double )sum * sum; // * really sum * rowHeight / remainingTotal
            const double rowScale = rowWidthScale; // really rowWidth * size / sum
            const double aspectRatio = qMin( rowHeight / (rowScale * firstSize), rowScale * size / rowHeight );
            if ( aspectRatio < bestAspectRatio )
            {
                // "Forget" the offending tile that made things worse
                // Leave the iterator pointing to the first item after this row
                sum -= size;
                break;
            }

            // Aspect ratio of the two (or perhaps only one so far) end tiles still approaching one
            bestAspectRatio = aspectRatio;
        }

        ++it;
    }

    return sum;
}

void TreemapTile::layoutRow( Orientation dir,
                             QRectF & rect,
                             FileInfoSortedBySizeIterator & it,
                             const FileInfo *rowEnd,
                             FileSize rowTotal,
                             double primary,
                             double height )
{

    //logDebug() << this << " - " << rect << " - height= " << height << Qt::endl;

    const double rectX = rect.x();
    const double rectY = rect.y();

    // All the row tiles have the same coefficients on the short axis of the row
    // .. so just calculate them once on a hypothetical row cushion
    CushionSurface rowCushionSurface = CushionSurface( _cushionSurface, _parentView->cushionHeights() );
    if ( dir == TreemapHorizontal )
    {
        const double newY = rectY + height;
        rowCushionSurface.addVerticalRidge( rectY, newY );
        rect.setY( newY );
    }
    else
    {
        const double newX = rectX + height;
        rowCushionSurface.addHorizontalRidge( rectX, newX );
        rect.setX( newX );
    }

    // logDebug() << this << " - " << rect << " - height= " << height << Qt::endl;

    const double rowScale = primary / rowTotal;
    double cumulativeSize = 0;
    double offset = 0;
    double nextOffset = qMin( primary, _parentView->minTileSize() );
    while ( *it != rowEnd && offset < primary )
    {
        // Position tiles relative to the row start based on the cumulative size of tiles
        //logDebug() << rect << *it << Qt::endl;
        cumulativeSize += itemTotalSize( *it );
        const double newOffset = round( cumulativeSize * rowScale );

        // Drop tiles that don't reach to the minimum pixel size or fill the row
        if ( newOffset >= nextOffset )
        {
            QRectF childRect = dir == TreemapHorizontal ?
                QRectF( rectX + offset, rectY, newOffset - offset, height ) :
                QRectF( rectX, rectY + offset, height, newOffset - offset );

            TreemapTile * tile = new TreemapTile( this, *it, childRect, rowCushionSurface );
            CHECK_NEW( tile );

            // Don't need to finish calculating cushions once all the leaf-level children have been created
            if ( ( *it )->isDirInfo() )
//                tile->_cushion = tile->renderCushion( childRect );
                addRenderThread( tile, 6 );
            else if ( dir == TreemapHorizontal )
                tile->_cushionSurface.addHorizontalRidge( childRect.left(), childRect.right() );
            else
                tile->_cushionSurface.addVerticalRidge( childRect.top(), childRect.bottom() );

            offset = newOffset;
            nextOffset = qMin( primary, newOffset + _parentView->minTileSize() );
        }

        ++it;
    }

    // Subtract this row from the remaining rectangle
//    if (dir == TreemapHorizontal)
//        rect.adjust(0, height, 0, 0);
//    else
//        rect.adjust(height, 0, 0, 0);

    //logDebug() << "Left over:" << " " << newRect << " " << this << Qt::endl;
}

inline FileSize TreemapTile::itemTotalSize( FileInfo *it )
{
    return it->totalAllocatedSize() ? it->totalAllocatedSize() : it->totalSize();
}

void TreemapTile::addRenderThread( TreemapTile *tile, int minThreadTileSize )
{
    // If the tile's parent is smaller than the threshold and not the root tile, then no thread
//    if ( _parentTile && _orig->totalUnignoredItems() < _parentView->threadThreshold() )
    if ( parentItem() && rect().width() < _parentView->maxTileThreshold() &&
         rect().height() < _parentView->maxTileThreshold() )
        return;

    // Not worth a thread for a tiny directory
    if ( tile->rect().width() < minThreadTileSize || tile->rect().height() < minThreadTileSize )
        return;

    // If the tile itself is larger than the threshold and its children are sub-directories, no thread
//    if ( tile->_orig->totalUnignoredItems() >= _parentView->threadThreshold() &&
    if ( ( tile->rect().width() >= _parentView->maxTileThreshold() ||
           tile->rect().height() >= _parentView->maxTileThreshold() ) &&
         ( !tile->_orig->firstChild() || tile->_orig->firstChild()->isDirInfo() ) )
         return;

    //logDebug() << QThreadPool::globalInstance()->activeThreadCount() << " threads active" << Qt::endl;
#if (QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 ))
    _parentView->appendTileFuture( QtConcurrent::run( tile, &TreemapTile::renderChildCushions ) );
#else
    _parentView->appendTileFuture( QtConcurrent::run( &TreemapTile::renderChildCushions, tile ) );
#endif
}

void TreemapTile::renderChildCushions()
{
    if ( _parentView->treemapCancelled() )
        return;

    const auto items = childItems();
    for ( QGraphicsItem *graphicsItem : items )
    {
        // nothing other than tiles in the tree at this point
        TreemapTile *tile = ( TreemapTile * )( graphicsItem );

        if ( tile->_orig->isDirInfo() )
            tile->renderChildCushions();
        else if ( _parentView->doCushionShading() )
            tile->_cushion = tile->renderCushion( tile->rect() );
        else
            //tile->_pixmap = tile->renderPlainTile( tile->rect() );
            tile->setBrush( tileColor( tile->_orig ) );
    }
}

void TreemapTile::paint( QPainter            * painter,
                         const QStyleOptionGraphicsItem * option,
                         QWidget            * widget )
{
    CHECK_MAGIC( _orig );

//    if ( _firstTile )
    {
//        logDebug() << Qt::endl;
//        _parentView->rootTile()->_stopwatch.start();
    }

    // Don't paint tiles with children, the children will cover the parent, but double-check
    // it actually has child tiles (no tile will be created for zero-sized children)
    if ( _orig->hasChildren() && childItems().size() > 0 )
    {
//        logDebug() << this << "(" << boundingRect() << ")" << ( isObscured() ? " - obscured" : "" ) << Qt::endl;
        return;
    }

    const QRectF rect = QGraphicsRectItem::rect();

    if ( _orig->isDirInfo() )
    {
//        logDebug() << _parentView->rootTile()->_stopwatch.restart() << "ms for " << rect << Qt::endl;

        // Relatively rare visible directory, fill it with a gradient or plain colour
        if ( brush().style() == Qt::NoBrush )
            setBrush( _parentView->dirBrush() );
        QGraphicsRectItem::paint( painter, option, widget );

        // Outline otherwise completely-plain empty-directory tiles
        if ( brush().style() == Qt::SolidPattern && _parentView->outlineColor().isValid() )
            drawOutline( painter, rect, _parentView->outlineColor(), 5 );
    }
    else if ( _parentView->doCushionShading() )
    {
        //logDebug() << rect << ", " << opaqueArea().boundingRect() << Qt::endl;

        // The cushion pixmap is rendered when the treemap is built, but may be deleted to re-colour the map
        if ( _cushion.isNull() )
            _cushion = renderCushion( rect );

        if ( !_cushion.isNull() )
            painter->drawPixmap( rect.topLeft(), _cushion );

        // Draw a clearly visible tile boundary if configured
        if ( _parentView->forceCushionGrid() )
            drawOutline( painter, rect, _parentView->cushionGridColor(), 10 );
    }
    else
    {
        // No cushion shading, use flat coloured tiles, may have already been set in the render thread
//        if ( _pixmap.isNull() )
//            _pixmap = renderPlainTile(rect);

        if ( brush().style() == Qt::NoBrush )
            setBrush( tileColor( _orig ) );
        QGraphicsRectItem::paint( painter, option, widget );

        // Always try to draw an outline since there is no other indication of the tiles
        if ( _parentView->outlineColor().isValid() )
            drawOutline( painter, rect, _parentView->outlineColor(), 5 );

//        if ( !_pixmap.isNull() )
//            painter->drawPixmap( rect.topLeft(), _pixmap );
    }

    if ( isSelected() )
    {
        //logDebug() << " highlight " << _parentView->rootTile()->_stopwatch.restart() << "ms for " << rect << Qt::endl;

        // Highlight this tile. This makes only sense if this is a leaf
        // tile (i.e., if the corresponding FileInfo doesn't have any
        // children), because otherwise the children will obscure this
        // tile anyway. In that case, we have to rely on a
        // HighlightRect to be created. But we can save some memory if
        // we don't do that for every tile, so we draw that highlight
        // frame manually if this is a leaf tile.

        painter->setBrush( Qt::NoBrush );
        QRectF selectionRect = rect;
        selectionRect.setSize( rect.size() - QSize( 1.0, 1.0 ) );
        painter->setPen( QPen( _parentView->selectedItemsColor(), 1 ) );
        painter->drawRect( selectionRect );
    }

//    if (_lastTile)
//        logDebug() << _parentView->rootTile()->_stopwatch.restart() << "ms" << Qt::endl;
}

void TreemapTile::drawOutline( QPainter * painter, const QRectF & rect, const QColor & color, int penScale )
{
    // Draw the outline as thin as practical
    const float sizeForPen = qMin( rect.width(), rect.height() );
    const float penSize = sizeForPen < penScale ? sizeForPen / penScale : 1.0;
    painter->setPen( QPen( color, penSize ) );

    // Draw along the top and left edges only to avoid doubling the line thickness
    if ( rect.x() > 0 )
        painter->drawLine( rect.topLeft(), rect.bottomLeft() );
    if ( rect.y() > 0 )
        painter->drawLine( rect.topLeft(), rect.topRight() );
}

QPixmap TreemapTile::renderCushion( const QRectF & rect )
{
    //logDebug() << rect << Qt::endl;

    static const double ambientIntensity = ( double )_parentView->ambientLight() / 255;
    static const double intensityScaling = 1.0 - ambientIntensity;
    static const double lightX = _parentView->lightX() * intensityScaling;
    static const double lightY = _parentView->lightY() * intensityScaling;
    static const double lightZ = _parentView->lightZ() * intensityScaling;

    const QColor color = tileColor( _orig );

    QImage image( rect.width(), rect.height(), QImage::Format_RGB32 );
    QRgb *data = reinterpret_cast<QRgb*>( image.bits() );

    const double xx22 = 2.0 * _cushionSurface.xx2();
    const double yy22 = 2.0 * _cushionSurface.yy2();
    const double nx0 = _cushionSurface.xx1() + xx22 * ( rect.x() + 0.5 );
    const double ny0 = _cushionSurface.yy1() + yy22 * ( rect.y() + 0.5 );

    double nx, ny;
    int x, y;
    for ( y = 0, ny = ny0; y < rect.height(); ++y, ny += yy22 )
    {
        for ( x = 0, nx = nx0; x < rect.width(); ++data, ++x, nx += xx22 )
        {
            double cosa  = ( lightZ + ny*lightY + nx*lightX ) / sqrt( nx*nx + ny*ny + 1.0 );
            cosa = ambientIntensity + qMax( 0.0, cosa );

            const int red = cosa * color.red() + 0.5;
            const int green = cosa * color.green() + 0.5;
            const int blue = cosa * color.blue() + 0.5;
            *data = qRgb( red, green, blue );
        }
    }

//    if (_parentView->forceCushionGrid())
//        renderOutline(image, width, height, _parentView->cushionGridColor(), color);

    if ( _parentView->enforceContrast() )
        enforceContrast( image );

    return QPixmap::fromImage( image );
}

const QColor & TreemapTile::tileColor( FileInfo * file ) const
{
    return _parentView->fixedColor().isValid() ? _parentView->fixedColor() : MimeCategorizer::instance()->color( file );
}

void TreemapTile::enforceContrast( QImage & image )
{
    if ( image.width() > 5 )
    {
        // Check contrast along the right image boundary:
        //
        // Compare samples from the outmost boundary to samples a few pixels to
        // the inside and count identical pixel values. A number of identical
        // pixels are tolerated, but not too many.
        int x1 = image.width() - 6;
        int x2 = image.width() - 1;
        int interval = qMax( image.height() / 10, 5 );
        int sameColorCount = 0;

        // Take samples
        for ( int y = interval; y < image.height(); y+= interval )
        {
            if ( image.pixel( x1, y ) == image.pixel( x2, y ) )
            sameColorCount++;
        }

        if ( sameColorCount * 10 > image.height() )
        {
            // Add a line at the right boundary
            QRgb val = contrastingColor( image.pixel( x2, image.height() / 2 ) );
            for ( int y = 0; y < image.height(); y++ )
                image.setPixel( x2, y, val );
        }
    }

    if ( image.height() > 5 )
    {
        // Check contrast along the bottom boundary

        int y1 = image.height() - 6;
        int y2 = image.height() - 1;
        int interval = qMax( image.width() / 10, 5 );
        int sameColorCount = 0;

        for ( int x = interval; x < image.width(); x += interval )
        {
            if ( image.pixel( x, y1 ) == image.pixel( x, y2 ) )
            sameColorCount++;
        }

        if ( sameColorCount * 10 > image.height() )
        {
            // Add a grey line at the bottom boundary
            QRgb val = contrastingColor( image.pixel( image.width() / 2, y2 ) );
            for ( int x = 0; x < image.width(); x++ )
                image.setPixel( x, y2, val );
        }
    }
}

QRgb TreemapTile::contrastingColor( QRgb col )
{
    if ( qGray( col ) < 128 )
        return qRgb( qRed( col ) * 2, qGreen( col ) * 2, qBlue( col ) * 2 );
    else
        return qRgb( qRed( col ) / 2, qGreen( col ) / 2, qBlue( col ) / 2 );
}

const CushionHeightSequence * TreemapTile::calculateCushionHeights( double cushionHeight, double scaleFactor )
{
    // This class always constructs a list of the correct size
    static CushionHeightSequence heights;

    // Start with the configured cushion height, times 4 from the coefficients
    double height = 4.0 * cushionHeight;

    // Fill the sequence with heights calculated from the configured scale factor
    for ( auto it = heights.begin(); it != heights.end(); ++it, height *= scaleFactor )
        *it = height;

    return &heights;
}

void TreemapTile::invalidateCushions()
{
    _cushion = QPixmap();
    setBrush( QBrush() );

    const auto items = childItems();
    for ( QGraphicsItem *graphicsItem : items )
    {
        TreemapTile * tile = dynamic_cast<TreemapTile *>( graphicsItem );
        if ( tile )
            tile->invalidateCushions();
    }
}

QVariant TreemapTile::itemChange( GraphicsItemChange   change,
                                  const QVariant     & value)
{
    //logDebug() << this << Qt::endl;

    if ( change == ItemSelectedChange && _orig->hasChildren() ) // tiles with no children are highlighted in paint()
    {
        bool selected = value.toBool();
        //logDebug() << this << ( selected ? " is selected" : " is deselected" ) << Qt::endl;

        if ( !selected )
        {
            if ( _highlighter )
            {
                //logDebug() << "Hiding highlighter for " << this << Qt::endl;
                _highlighter->hide();
            }
        }
        else if ( this != _parentView->rootTile() ) // don't highlight the root tile
        {
            if ( ! _highlighter )
            {
                //logDebug() << "Creating highlighter for " << this << Qt::endl;
                _highlighter = new SelectedTileHighlighter( _parentView, this );
                CHECK_NEW( _highlighter );
            }

            if ( ! _highlighter->isVisible() )
            {
                //logDebug() << "Showing highlighter for " << this << " (style=" << _highlighter->pen().style() << ")" << Qt::endl;
                _highlighter->show();
            }
        }
    }

    return QGraphicsRectItem::itemChange( change, value );
}

void TreemapTile::mousePressEvent( QGraphicsSceneMouseEvent * event )
{
    if ( ! _parentView->selectionModel() )
        return;

    switch ( event->button() )
    {
    case Qt::LeftButton:
        // isSelected() is unreliable here since in QGraphicsItem some
        // stuff is done in the mousePressEvent, while some other stuff is
        // done in the mouseReleaseEvent. Just setting the current item
        // here to avoid having a yellow highlighter rectangle upon mouse
        // press and then a red one upon mouse release. No matter if the
        // item ends up selected or not, the mouse press makes it the
        // current item, so let's update the red highlighter rectangle
        // here.

        QGraphicsRectItem::mousePressEvent( event );
        //logDebug() << this << " mouse pressed (" << event->modifiers() << ")" << Qt::endl;
        _parentView->setCurrentTile( this );
        break;

    case Qt::MiddleButton:
        // logDebug() << "Middle click on " << _orig << Qt::endl;

        // Handle item selection (with or without Ctrl) ourselves here;
        // unlike for a left click, the QGraphicsItem base class does
        // not do this for us already.

        if ( ( event->modifiers() & Qt::ControlModifier ) == 0 )
            scene()->clearSelection();

        setSelected( ! isSelected() );

        _parentView->toggleParentsHighlight( this );
        _parentView->setCurrentTile( this );
        break;

    case Qt::RightButton:
        // logDebug() << this << " right mouse pressed" << Qt::endl;
        _parentView->setCurrentTile( this );
        break;
/*
    case Qt::ExtraButton1:
        // logDebug() << this << " back mouse button pressed" << Qt::endl;
        _parentView->resetZoom();
        break;

    case Qt::ExtraButton2:
        // logDebug() << this << " forward mouse button pressed" << Qt::endl;
        _parentView->zoomTo();
        break;
*/
    default:
        QGraphicsRectItem::mousePressEvent( event );
        break;
    }
}

void TreemapTile::mouseReleaseEvent( QGraphicsSceneMouseEvent * event )
{
    if ( ! _parentView->selectionModel() )
        return;

    QGraphicsRectItem::mouseReleaseEvent( event );

    switch ( event->button() )
    {
    case Qt::LeftButton:
        // The current item was already set in the mouse press event,
        // but it might have changed its 'selected' status right now,
        // so let the view update it.
        _parentView->setCurrentTile( this );
        // logDebug() << this << " clicked; selected: " << isSelected() << Qt::endl;
        break;

    default:
        break;
    }

    _parentView->sendSelection( this );
}

void TreemapTile::mouseDoubleClickEvent( QGraphicsSceneMouseEvent * event )
{
    if ( ! _parentView->selectionModel() )
        return;

    switch ( event->button() )
    {
    case Qt::LeftButton:
        // logDebug() << "Zooming treemap in" << Qt::endl;
        _parentView->zoomIn();
        break;

    case Qt::MiddleButton:
        // logDebug() << "Zooming treemap out" << Qt::endl;
        _parentView->zoomOut();
        break;

    case Qt::RightButton:
        // This doesn't work at all since the first click already opens the
        // context menu which grabs the focus to that pop-up menu.
        break;

    default:
        break;
    }
}

void TreemapTile::wheelEvent( QGraphicsSceneWheelEvent * event )
{
    if ( ! _parentView->selectionModel() )
        return;

    if ( event->delta() < 0 )
        _parentView->zoomOut();
    else
    {
        // If no current item, or it is the root already, pick a new current item so we can zoom
        FileInfo *currentItem = _parentView->selectionModel()->currentItem();
        if ( !currentItem || currentItem == _parentView->rootTile()->_orig )
            if ( this->parentItem() != _parentView->rootTile() ) // ... unless we just can't zoom any further
            _parentView->setCurrentTile( this );

        _parentView->zoomIn();
    }
}

void TreemapTile::contextMenuEvent( QGraphicsSceneContextMenuEvent * event )
{
    if ( ! _parentView->selectionModel() )
        return;

    FileInfoSet selectedItems = _parentView->selectionModel()->selectedItems();
    if ( ! selectedItems.contains( _orig ) )
    {
        //logDebug() << "Abandoning old selection" << Qt::endl;
        _parentView->selectionModel()->setCurrentItem( _orig, true );
        selectedItems = _parentView->selectionModel()->selectedItems();
    }

//    if ( _parentView->selectionModel()->verbose() )
//        _parentView->selectionModel()->dumpSelectedItems();

    //logDebug() << "Context menu for " << this << Qt::endl;

    QMenu menu;

    // The first action should not be a destructive one like "move to trash":
    // It's just too easy to select and execute the first action accidentially,
    // especially on a laptop touchpad.
    const QStringList actions1 = { "actionTreemapZoomTo",
                                   "actionTreemapZoomIn",
                                   "actionTreemapZoomOut",
                                   "actionResetTreemapZoom",
                                   "---",
                                   "actionCopyPath",
                                   "actionMoveToTrash",
                                   "---"
                                 };
    ActionManager::instance()->addActions( &menu, actions1 );

    // User-defined cleanups
    if ( _parentView->cleanupCollection() )
    _parentView->cleanupCollection()->addEnabledToMenu( &menu );

    menu.exec( event->screenPos() );
}

void TreemapTile::hoverEnterEvent( QGraphicsSceneHoverEvent * )
{
    // logDebug() << "Hovering over " << this << Qt::endl;
    _parentView->sendHoverEnter( _orig );
}

void TreemapTile::hoverLeaveEvent( QGraphicsSceneHoverEvent * )
{
    // logDebug() << "  Leaving " << this << Qt::endl;
    _parentView->sendHoverLeave( _orig );
}

//
//---------------------------------------------------------------------------
//

void CushionSurface::addHorizontalRidge( double start, double end )
{
    const double reciprocal = coefficientReciprocal( start, end );
    _xx2 -= squareCoefficient( reciprocal );
    _xx1 += linearCoefficient( start, end, reciprocal );
}

void CushionSurface::addVerticalRidge( double start, double end )
{
    const double reciprocal = coefficientReciprocal( start, end );
    _yy2 -= squareCoefficient( reciprocal );
    _yy1 += linearCoefficient( start, end, reciprocal );
}
