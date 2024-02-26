/*
 *   File name: FormatUtil.h
 *   Summary:	String formatting utilities for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#ifndef FormatUtil_h
#define FormatUtil_h


#include <sys/types.h>
#include <sys/stat.h>

#include <QString>
#include <QTextStream>

#include "FileSize.h"


namespace QDirStat
{
    /**
     * Format a file / subtree size human readable, i.e. in "GB" / "MB"
     * etc. rather than huge numbers of digits. 'precision' is the number of
     * digits after the decimal point.
     *
     * Note: For logDebug() etc., operator<< is overwritten to do exactly that:
     *
     *	   logDebug() << "Size: " << x->totalSize() << Qt::endl;
     **/
    QString formatSize( FileSize size );

    /**
     * Can't use a default argument when using this as a function pointer,
     * so we really need the above overloaded version.
     **/
    QString formatSize( FileSize size, int precision );

    /**
     * Format a file / subtree size as bytes, but still human readable with a
     * thousands separator.
     **/
    QString formatByteSize( FileSize size );

    /**
     * Format a file size string with no thousands separators and "B" for the units.
     * This is only intended for small values, typically less than 1,000.
     **/
    QString formatShortByteSize( FileSize size );


    /**
     * Format a string of the form "/ 3 links" for describing hard links.  If the
     * number of links is less than 2, an empty string is returned.
     **/
    QString formatLinksInline( nlink_t numLinks );

    /**
     * Format a string of the form "<br/>3 links" for describing hard links on a
     * separate line, typically in a tooltip.  If the number of links is less than 2,
     * an empty string is returned.
     **/
    QString formatLinksRichText( nlink_t numLinks );

    /**
     * Wraps the text in html formatting to prevent line breaks except at explicit
     * newlines and break tags.
     **/
    QString whitespacePre( const QString & text );

    /**
     * Format a timestamp (like the latestMTime()) human-readable.
     **/
    QString formatTime( time_t rawTime );

    /**
     * Format a millisecond-based time
     **/
    QString formatMillisec( qint64 millisec, bool showMillisec = true );

    /**
     * Format a percentage.
     **/
    QString formatPercent( float percent );

    /**
     * Return the mode (the permission bits) returned from stat() like the
     * "ls -l" shell command does, e.g.
     *
     *	   drwxr-xr-x
     **/
    QString symbolicMode( mode_t perm );

    /**
     * Format a number in octal with a leading zero.
     **/
    QString formatOctal( int number );

    /**
     * Format a file stat mode as octal.
     **/
    QString octalMode( mode_t mode );

    /**
     * Format the mode (the permissions bits) returned from the stat() system
     * call in the commonly used formats, both symbolic and octal, e.g.
     *	   drwxr-xr-x  0755
     **/
    QString formatPermissions( mode_t mode );

    /**
     * Human-readable output of a file size in a debug stream.
     **/
    inline QTextStream & operator<< ( QTextStream & stream, FileSize lSize )
    {
	stream << formatSize( lSize );

	return stream;
    }

}       // namespace QDirStat

#endif  // FormatUtil_h
