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


using namespace QDirStat;


FileSizeStats::FileSizeStats( FileInfo * subtree ):
    PercentileStats ()
{
    CHECK_PTR( subtree );

    // Avoid reallocations for potentially millions of list appends
    reserve( subtree->totalFiles() );
    collect( subtree );
    sort();
}


FileSizeStats::FileSizeStats( FileInfo * subtree, const QString & suffix ):
    PercentileStats ()
{
    CHECK_PTR( subtree );

    collect( subtree, suffix );
    sort();
}


void FileSizeStats::collect( const FileInfo * subtree )
{
    if ( subtree->isFile() )
        append( subtree->size() );

    for ( FileInfoIterator it( subtree ); *it; ++it )
	collect( *it );
}


void FileSizeStats::collect( const FileInfo * subtree, const QString & suffix )
{
    if ( subtree->isFile() && subtree->name().toLower().endsWith( suffix ) )
        append( subtree->size() );

    for ( FileInfoIterator it( subtree ); *it; ++it )
	collect( *it, suffix );
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

    QRealList buckets( bucketCount );

    if ( isEmpty() )
        return buckets;

    // The first call to percentile() or quantile() will cause the data to be
    // sorted, so there is no need to sort them again here.
    qreal startVal = percentile( startPercentile );
    qreal endVal   = percentile( endPercentile );
    qreal bucketWidth = ( endVal - startVal ) / bucketCount;

#if 0
    logDebug() << "startPercentile: " << startPercentile
               << " endPercentile: " << endPercentile
               << " startVal: " << formatSize( startVal )
               << " endVal: " << formatSize( endVal )
               << " bucketWidth: " << formatSize( bucketWidth )
               << Qt::endl;
#endif

    auto it = cbegin();
    while ( it != cend() && *it < startVal )
	++it;

    int index = 0;
    qreal nextBucket = startVal + bucketWidth;
    while ( it != cend() && *it <= endVal )
    {
	// Increment the bucket index when we hit the next bucket boundary
	while ( *it > nextBucket )
	{
	    index = qMin( index + 1, bucketCount - 1 ); // avoid rounding errors tipping us past the last bucket
	    nextBucket += bucketWidth;
	}

        ++buckets[ index ];

	++it;
    }

    return buckets;
}
