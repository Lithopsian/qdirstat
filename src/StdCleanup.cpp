/*
 *   File name: StdCleanup.cpp
 *   Summary:	QDirStat classes to reclaim disk space
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "Cleanup.h"
#include "StdCleanup.h"
#include "Exception.h"
#include "Logger.h"

using namespace QDirStat;


CleanupList StdCleanup::stdCleanups( QObject * parent )
{
    return { openFileManagerHere( parent ),
	     openTerminalHere   ( parent ),
	     checkFileType      ( parent ),
	     compressSubtree	( parent ),
	     makeClean		( parent ),
	     gitClean		( parent ),
	     deleteJunk		( parent ),
	     hardDelete		( parent ),
	     clearDirContents	( parent ),
#if USE_DEBUG_ACTIONS
	     echoargs		( parent ),
	     echoargsMixed	( parent ),
	     segfaulter		( parent ),
	     commandNotFound	( parent ),
	     sleepy		( parent ),
#endif
	   };
}



Cleanup * StdCleanup::openFileManagerHere( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "Open File Mana&ger Here" ),
				    "%filemanager %d &",
				    false,
				    false,
				    Cleanup::NoRefresh,
				    true,
				    true,
				    true,
				    Cleanup::ShowNever );
    CHECK_NEW( cleanup );
    cleanup->setIcon( ":/icons/file-manager.png" );
    cleanup->setShortcut( Qt::CTRL | Qt::Key_G );

    return cleanup;
}


Cleanup * StdCleanup::openTerminalHere( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "Open &Terminal Here" ),
				    "%terminal",
				    false,
				    false,
				    Cleanup::NoRefresh,
				    true,
				    true,
				    true,
				    Cleanup::ShowNever );
    CHECK_NEW( cleanup );
    cleanup->setIcon( ":/icons/terminal.png" );
    cleanup->setShortcut( Qt::CTRL | Qt::Key_T );

    return cleanup;
}


Cleanup * StdCleanup::checkFileType( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "Check File T&ype" ),
				    "file %n | sed -e 's/[:,] /\\n  /g'",
				    false,
				    false,
				    Cleanup::NoRefresh,
				    false,
				    true,
				    false,
				    Cleanup::ShowAlways );
    CHECK_NEW( cleanup );
    cleanup->setShortcut( Qt::CTRL | Qt::Key_Y );

    return cleanup;
}


Cleanup * StdCleanup::compressSubtree( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "&Compress" ),
				    "cd ..; tar cjvf %n.tar.bz2 %n && rm -rf %n",
				    false,
				    false,
				    Cleanup::RefreshParent,
				    true,
				    false,
				    false );
    CHECK_NEW( cleanup );

    return cleanup;
}


Cleanup * StdCleanup::makeClean( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "&make clean" ),
				    "make clean",
				    false,
				    false,
				    Cleanup::RefreshThis,
				    true,
				    false,
				    true );
    CHECK_NEW( cleanup );

    return cleanup;
}


Cleanup * StdCleanup::gitClean( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "&git clean" ),
				    "git clean -dfx",
				    false,
				    true,
				    Cleanup::RefreshThis,
				    true,
				    false,
				    true,
				    Cleanup::ShowAlways );
    CHECK_NEW( cleanup );

    return cleanup;
}


Cleanup * StdCleanup::deleteJunk( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "Delete &Junk Files" ),
				    "rm -f *~ *.bak *.auto core",
				    true,
				    false,
				    Cleanup::RefreshThis,
				    true,
				    false,
				    true );
    CHECK_NEW( cleanup );
    cleanup->setShell( "/bin/bash" );

    return cleanup;
}


Cleanup * StdCleanup::hardDelete( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "&Delete (no way to undelete!)" ),
				    "rm -rf %p",
				    false,
				    true,
				    Cleanup::RefreshParent,
				    true,
				    true,
				    false );
    CHECK_NEW( cleanup );
    cleanup->setIcon( ":/icons/delete.png" );
    cleanup->setShortcut( Qt::CTRL | Qt::Key_Delete );

    return cleanup;
}


Cleanup * StdCleanup::clearDirContents( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "Clear Directory C&ontents" ),
				    "rm -rf %d/*",
				    false,
				    true,
				    Cleanup::RefreshThis,
				    true,
				    false,
				    false );
    CHECK_NEW( cleanup );

    return cleanup;
}


#if USE_DEBUG_ACTIONS

Cleanup * StdCleanup::echoargs( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "echoargs" ),
				    "echoargs %p",
				    false,
				    false,
				    Cleanup::NoRefresh,
				    true,
				    true,
				    true );
    CHECK_NEW( cleanup );

    return cleanup;
}


Cleanup * StdCleanup::echoargsMixed( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "Output on stdout and stderr" ),
				    "echoargs_mixed %n one two three four",
				    false,
				    true,
				    Cleanup::NoRefresh,
				    true,
				    true,
				    true );
    CHECK_NEW( cleanup );

    return cleanup;
}


Cleanup * StdCleanup::segfaulter( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "Segfaulter" ),
				    "segfaulter",
				    false,
				    false,
				    Cleanup::NoRefresh,
				    true,
				    true,
				    true );
    CHECK_NEW( cleanup );

    return cleanup;
}


Cleanup * StdCleanup::commandNotFound( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "Nonexistent command" ),
				    "wrglbrmpf",
				    false,
				    false,
				    Cleanup::NoRefresh,
				    true,
				    true,
				    true );
    CHECK_NEW( cleanup );

    return cleanup;
}


Cleanup * StdCleanup::sleepy( QObject * parent )
{
    Cleanup *cleanup = new Cleanup( parent,
				    true,
				    QObject::tr( "Sleepy echoargs" ),
				    "sleep 1; echoargs %p",
				    false,
				    false,
				    Cleanup::NoRefresh,
				    true,
				    true,
				    true );
    CHECK_NEW( cleanup );

    return cleanup;
}


#endif
