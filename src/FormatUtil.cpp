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


QString QDirStat::formatSize( FileSize lSize )
{
    return formatSize( lSize, 1 );
}


QString QDirStat::formatSize( FileSize lSize, int precision )
{
    QString sizeString;
    int	    unitIndex = 0;

    static QStringList units = { QObject::tr( "bytes" ),
	                         QObject::tr( "kB" ),
	                         QObject::tr( "MB" ),
	                         QObject::tr( "GB" ),
	                         QObject::tr( "TB" ),
	                         QObject::tr( "PB" ),
	                         QObject::tr( "EB" ),
	                         QObject::tr( "ZB" ),
	                         QObject::tr( "YB" )
                               };

    if ( lSize < 1000 )
    {
	sizeString.setNum( lSize );
	sizeString += " " + units.at( unitIndex );
    }
    else
    {
	double size = lSize;

	while ( size >= 1000.0 && unitIndex < units.size() - 1 )
	{
	    size /= 1024.0;
	    ++unitIndex;
	}

	sizeString.setNum( size, 'f', precision );
	sizeString += " " + units.at( unitIndex );
    }

    return sizeString;
}


QString QDirStat::formatByteSize( FileSize size )
{

    return QObject::tr( "%L1 bytes" ).arg( size );
}


QString QDirStat::formatShortByteSize( FileSize size )
{
    return QString( "%1 B" ).arg( size );
}


static QString formatLinks( nlink_t numLinks )
{
    return QObject::tr( "%2 links" ).arg( numLinks );
}


QString QDirStat::formatLinksInline( nlink_t numLinks )
{
    return numLinks > 1 ? " / " + formatLinks( numLinks ) : "";
}


QString QDirStat::formatLinksRichText( nlink_t numLinks )
{
    return numLinks > 1 ? "<br/>" + formatLinks( numLinks ) : "";
}


QString QDirStat::whitespacePre( const QString & text )
{
    return "<p style='white-space:pre'>" + text + "</p>";
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

    const QDateTime time = QDateTime::fromTime_t( rawTime );
    return QLocale().toString( time, QLocale::ShortFormat );
//    return time.toString( Qt::DefaultLocaleShortDate );
}


QString QDirStat::formatPermissions( mode_t mode )
{
    return symbolicMode( mode ) + "  " + formatOctal( ALLPERMS & mode );
}


QString QDirStat::formatOctal( int number )
{
    return QString( "0" ) + QString::number( number, 8 );
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


QString QDirStat::octalMode( mode_t mode )
{
    return formatOctal( ALLPERMS & mode );
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
