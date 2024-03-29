/*
 *   File name: PercentileStats.cpp
 *   Summary:	Statistics classes for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <math.h>	// ceil()
#include <algorithm>    // std::sort()

#include "PercentileStats.h"
#include "Exception.h"
#include "Logger.h"


#define VERBOSE_SORT_THRESHOLD	50000

using namespace QDirStat;

/*
void PercentileStats::clear()
{
    // Just _data.clear() does not free any memory; we need to assign an empty
    // list to _data.

    _data = QRealList();
}
*/

void PercentileStats::sort()
{
    if ( size() > VERBOSE_SORT_THRESHOLD )
	logDebug() << "Sorting " << size() << " elements" << Qt::endl;

    std::sort( begin(), end() );
    _sorted = true;

    if ( size() > VERBOSE_SORT_THRESHOLD )
	logDebug() << "Sorting done." << Qt::endl;
}


qreal PercentileStats::median()
{
    if ( isEmpty() )
	return 0;

    if ( !_sorted )
	sort();

    const int centerPos = size() / 2;

    // Since we are doing integer division, the center is already rounded down
    // if there is an odd number of data items, so don't use the usual -1 to
    // compensate for the index of the first data element being 0, not 1: if
    // size() is 5, we get _data[2] which is the center of
    // [0, 1, 2, 3, 4].
    const qreal result = at( centerPos );

    if ( size() % 2 == 0 ) // Even number of items
    {
	// Use the average of the two center items. We already accounted for
	// the upper one with the above integer division, so now we need to
	// account for the lower one: If size() is 6, we already got
	// _data[3], and now we need to average this with _data[2] of
	// [0, 1, 2, 3, 4, 5].
	return ( result + at( centerPos - 1 ) ) / 2.0;
    }

    return result;
}

/*
qreal PercentileStats::average()
{
    if ( isEmpty() )
	return 0.0;

    const qreal sum = std::accumulate( cbegin(), cend(), 0.0 );
    const int count = size();

    return sum / count;
}


qreal PercentileStats::min()
{
    if ( isEmpty() )
	return 0.0;

    if ( !_sorted )
	sort();

    return first();
}


qreal PercentileStats::max()
{
    if ( isEmpty() )
	return 0.0;

    if ( !_sorted )
	sort();

    return last();
}
*/

qreal PercentileStats::quantile( int order, int number )
{
    if ( isEmpty() )
	return 0.0;

    if ( number > order )
    {
	QString msg = QString( "Cannot determine quantile #%1 for %2-quantile" ).arg( number ).arg( order );
	THROW( Exception( msg ) );
    }

    if ( order < 2 )
    {
	QString msg = QString( "Invalid quantile order %1" ).arg( order );
	THROW( Exception( msg ) );
    }

    if ( !_sorted )
	sort();

    if ( number == 0 )
	return first();

    if ( number == order )
	return last();

    int pos = ( size() * number ) / order;

    // Same as in median(): The integer division already cut off any non-zero
    // decimal place, so don't subtract 1 to compensate for starting _data with
    // index 0.

    qreal result = at( pos );

    if ( ( size() * number ) % order == 0 )
    {
	// Same as in median: We hit between two elements, so use the average
	// between them.

	result = ( result + at( pos - 1 ) ) / 2.0;
    }

    return result;
}


QRealList PercentileStats::percentileList()
{
    QRealList percentiles;
    percentiles.reserve( 100 );

    for ( int i=0; i <= 100; ++i )
	percentiles << percentile( i );

    return percentiles;
}


PercentileSums PercentileStats::percentileSums()
{
    if ( !_sorted )
	sort();

    PercentileSums sums;
    const qreal percentileSize = size() / 100.0;

    for ( int i=0; i < size(); ++i )
    {
	int percentile = qMax( 1.0, ceil( i / percentileSize ) );
	sums._individual[ percentile ] += at(i);
    }

    qreal runningTotal = 0.0;

    for ( int i=0; i < sums._individual.size(); i++ )
    {
	runningTotal	 += sums._individual.at(i);
	sums._cumulative.append( runningTotal );
    }

#if 0
    for ( int i=0; i < sums.size(); ++i )
    {
	logDebug() << "sum[ "	  << i << " ] : " << formatSize( sums._individual[i] ) << Qt::endl;
	logDebug() << "cum_sum[ " << i << " ] : " << formatSize( sums._cumulative[i] ) << Qt::endl;
    }
#endif

    return sums;
}
