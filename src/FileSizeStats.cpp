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
    _data.reserve( subtree->totalFiles() );
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
        _data << subtree->size();

    for ( FileInfoIterator it( subtree ); *it; ++it )
	collect( *it );
}


void FileSizeStats::collect( const FileInfo * subtree, const QString & suffix )
{
    if ( subtree->isFile() && subtree->name().toLower().endsWith( suffix ) )
        _data << subtree->size();

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
