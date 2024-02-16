/*
 *   File name: FileSizeLabel.cpp
 *   Summary:	Specialized QLabel for a file size for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QMenu>
#include <QMouseEvent>

#include "FileSizeLabel.h"
#include "FormatUtil.h"
#include "Logger.h"

using namespace QDirStat;


void FileSizeLabel::clear()
{
    _value = -1;
    _prefix.clear();
    setToolTip( QString() );
    QLabel::clear();
//    PopupLabel::clear();
}


void FileSizeLabel::setValue( FileSize val, const QString & prefix )
{
    _value  = val;
    _prefix = prefix;
    setToolTip( "" );

    if ( _value < 0 )
    {
	QLabel::setText( "" );
    }
    else
    {
	QLabel::setText( _prefix + formatSize( _value ) );

	if ( _value > 1024 )
	    setToolTip( _prefix + formatByteSize( _value ) );
    }
}


void FileSizeLabel::setText( const QString & newText,
			     FileSize	     newValue,
			     const QString & newPrefix )
{
    _value  = newValue;
    _prefix = newPrefix;
    setToolTip( "" );

    QLabel::setText( newText );
    if ( _value > 1024 )
        setToolTip( _prefix + formatByteSize( _value ) );
}

/*
bool FileSizeLabel::haveContextMenu() const
{
    if ( ! _contextText.isEmpty() )
	return true;

    return _value >= 1024; // Doesn't make sense below 1 kB
}


QString FileSizeLabel::contextText() const
{
    return _contextText.isEmpty() ?
	_prefix + formatByteSize( _value ) : _contextText;
}
*/


void FileSizeLabel::setBold( bool bold )
{
    QFont textFont = font();
    textFont.setBold( bold );
    setFont( textFont );
}
