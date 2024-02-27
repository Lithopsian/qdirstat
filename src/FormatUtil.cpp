/*
 *   File name: FormatUtil.cpp
 *   Summary:	String formatting utilities for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#include <QObject>
#include <QDateTime>
#include <QTextStream>

#include "FormatUtil.h"
#include "BrokenLibc.h"     // ALLPERMS

using namespace QDirStat;


QString QDirStat::formatSize( FileSize lSize, int precision )
{
    static QStringList units = { QObject::tr( " bytes" ),
	                         QObject::tr( " kB" ),
	                         QObject::tr( " MB" ),
	                         QObject::tr( " GB" ),
	                         QObject::tr( " TB" ),
	                         QObject::tr( " PB" ),
	                         QObject::tr( " EB" ),
	                         QObject::tr( " ZB" ),
	                         QObject::tr( " YB" )
                               };
    if ( lSize < 1000 )
    {
	// Exact number of bytes, no decimals
	return QString::number( lSize ) + units.at( 0 );
    }
    else
    {
	int    unitIndex = 1;
	double size = lSize / 1024.0;

	// Restrict to three digits before the decimal point
	while ( size >= 1000.0 && unitIndex < units.size() - 1 )
	{
	    size /= 1024.0;
	    ++unitIndex;
	}

	return QString::number( size, 'f', precision ) + units.at( unitIndex );
    }
}


QString QDirStat::formatPercent( float percent )
{
    if ( percent < 0.0 )	// Invalid percentage?
	return "";

    return QString::number( percent, 'f', 1 ) + '%';
}


QString QDirStat::formatTime( time_t rawTime )
{
    if ( rawTime == (time_t) 0 )
	return "";

#if QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
    const QDateTime time = QDateTime::fromTime_t( rawTime );
#else
    const QDateTime time = QDateTime::fromSecsSinceEpoch( rawTime );
#endif
    return QLocale().toString( time, QLocale::ShortFormat );
//    return time.toString( Qt::DefaultLocaleShortDate );
}


QString QDirStat::symbolicMode( mode_t mode )
{
    QString result;

    // Type
    if	    ( S_ISDIR ( mode ) )	  result = "d";
    else if ( S_ISCHR ( mode ) )	  result = "c";
    else if ( S_ISBLK ( mode ) )	  result = "b";
    else if ( S_ISFIFO( mode ) )	  result = "p";
    else if ( S_ISLNK ( mode ) )	  result = "l";
    else if ( S_ISSOCK( mode ) )	  result = "s";

    // User
    result += ( mode & S_IRUSR ) ? "r" : "-";
    result += ( mode & S_IWUSR ) ? "w" : "-";

    if ( mode & S_ISUID )
	result += "s";
    else
	result += ( mode & S_IXUSR ) ? "x" : "-";

    // Group
    result += ( mode & S_IRGRP ) ? "r" : "-";
    result += ( mode & S_IWGRP ) ? "w" : "-";

    if ( mode & S_ISGID )
	result += "s";
    else if ( mode & S_IXGRP )
	result += "x";
    else
	result += "-";

    // Other
    result += ( mode & S_IROTH ) ? "r" : "-";
    result += ( mode & S_IWOTH ) ? "w" : "-";

    if ( mode & S_ISVTX )
	result += "t";
    else if ( mode & S_IXOTH )
	result += "x";
    else
	result += "-";

    return result;
}


QString QDirStat::formatMillisec( qint64 millisec, bool showMillisec )
{
    QString formattedTime;

    const int hours = millisec / 3600000L;	// 60*60*1000
    millisec %= 3600000L;

    const int min = millisec / 60000L;	// 60*1000
    millisec %= 60000L;

    const int sec = millisec / 1000L;
    millisec %= 1000L;

    if ( hours < 1 && min < 1 && sec < 60 )
    {
	formattedTime.setNum( sec );

	if ( showMillisec )
	{
	    formattedTime += QString( ".%1" ).arg( millisec,
						   3,	// fieldWidth
						   10,	// base
						   QChar( '0' ) ); // fillChar
	}

	formattedTime += " " + QObject::tr( "sec" );
    }
    else
    {
	formattedTime = QString( "%1:%2:%3" )
	    .arg( hours, 2, 10, QChar( '0' ) )
	    .arg( min,	 2, 10, QChar( '0' ) )
	    .arg( sec,	 2, 10, QChar( '0' ) );
    }

    return formattedTime;
}
