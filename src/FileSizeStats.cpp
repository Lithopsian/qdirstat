/*
 *   File name: FileSizeStats.cpp
 *   Summary:	Statistics classes for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "FileSizeStats.h"
#include "FileInfoIterator.h"
#include "FormatUtil.h"
#include "Exception.h"
#include "Logger.h"


#define VERBOSE_SORT_THRESHOLD  50000

using namespace QDirStat;


FileSizeStats::FileSizeStats( FileInfo * subtree ):
    PercentileStats()
{
    CHECK_PTR ( subtree );

    collect( subtree );
    sort();
}


void FileSizeStats::collect( FileInfo * subtree )
{
    Q_CHECK_PTR( subtree );

    if ( _data.isEmpty() )
        _data.reserve( subtree->totalFiles() );

    if ( subtree->isFile() )
        _data << subtree->size();

    for ( FileInfoIterator it( subtree ); *it; ++it )
    {
	// Disregard symlinks, block devices and other special files
	FileInfo * item = *it;
	if ( item->hasChildren() )
	    collect( item );
	else if ( item->isFile() )
            _data << item->size();
    }
}


void FileSizeStats::collect( FileInfo * subtree, const QString & suffix )
{
    Q_CHECK_PTR( subtree );

    if ( _data.isEmpty() )
        _data.reserve( subtree->totalFiles() );

    if ( subtree->isFile() && subtree->name().toLower().endsWith( suffix ) )
        _data << subtree->size();

    for ( FileInfoIterator it( subtree ); *it; ++it )
    {
	// Disregard symlinks, block devices and other special files
	FileInfo * item = *it;
	if ( item->hasChildren() )
	    collect( item, suffix );
	else if ( item->isFile() && item->name().toLower().endsWith( suffix ) )
            _data << item->size();
    }
}


QRealList FileSizeStats::fillBuckets( int bucketCount,
                                      int startPercentile,
                                      int endPercentile )
{
    CHECK_INDEX( startPercentile, 0, 100 );
    CHECK_INDEX( endPercentile,   0, 100 );

    if ( startPercentile >= endPercentile )
        THROW( Exception( "startPercentile must be less than endPercentile" ) );

    if ( bucketCount < 1 )
        THROW( Exception( QString( "Invalid bucket count %1" ).arg( bucketCount ) ) );

    QRealList buckets;
    buckets.reserve( bucketCount );
    for ( int i=0; i < bucketCount; ++i )
        buckets << 0.0;

    if ( _data.isEmpty() )
        return buckets;

    // The first call to percentile() or quantile() will cause the data to be
    // sorted, so there is no need to sort them again here.

    qreal startVal = percentile( startPercentile );
    qreal endVal   = percentile( endPercentile );
    qreal bucketWidth = ( endVal - startVal ) / bucketCount;

#if 1
    logDebug() << "startPercentile: " << startPercentile
               << " endPercentile: " << endPercentile
               << " startVal: " << formatSize( startVal )
               << " endVal: " << formatSize( endVal )
               << " bucketWidth: " << formatSize( bucketWidth )
               << Qt::endl;
#endif

    auto it = _data.cbegin();
    while ( it != _data.cend() && *it < startVal )
	++it;

    while ( it != _data.cend() && *it <= endVal )
    {
        // TO DO: Optimize this by taking into account that the data are sorted
        // already. We don't really need that many divisions; just when leaving
        // the current bucket would be sufficient.

        const int index = qMin( ( *it - startVal ) / bucketWidth, bucketCount - 1.0 );
        ++buckets[ index ];

	++it;
    }

    return buckets;
}
