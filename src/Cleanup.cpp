/*
 *   File name: Cleanup.cpp
 *   Summary:	QDirStat classes to reclaim disk space
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QApplication>
#include <QRegularExpression>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFileInfo>

#include "Cleanup.h"
#include "DirTree.h"
#include "DirInfo.h"
#include "OutputWindow.h"
#include "Logger.h"
#include "Exception.h"


#define SIMULATE_COMMAND	1
#define WAIT_TIMEOUT_MILLISEC	30000

using namespace QDirStat;


Cleanup::Cleanup( QObject            * parent,
		  bool                 active,
		  QString              title,
		  QString              command,
		  bool                 recurse,
		  bool                 askForConfirmation,
		  RefreshPolicy        refreshPolicy,
		  bool                 worksForDir,
		  bool                 worksForFile,
		  bool                 worksForDotEntry,
		  OutputWindowPolicy   outputWindowPolicy,
		  int                  outputWindowTimeout,
		  bool                 outputWindowAutoClose,
		  QString              shell,
		  QString              iconName ):
    QAction ( title, parent ),
    _active { active },
    _title { title },
    _command { command },
    _iconName { iconName },
    _recurse { recurse },
    _askForConfirmation { askForConfirmation },
    _refreshPolicy { refreshPolicy },
    _worksForDir { worksForDir },
    _worksForFile { worksForFile },
    _worksForDotEntry { worksForDotEntry },
    _outputWindowPolicy { outputWindowPolicy },
    _outputWindowTimeout { outputWindowTimeout },
    _outputWindowAutoClose { outputWindowAutoClose },
    _shell { shell }
{
    QAction::setEnabled( true );
}


Cleanup::Cleanup( const Cleanup * other ):
    Cleanup ( 0, other->_active, other->_title, other->_command,
              other->_recurse, other->_askForConfirmation, other->_refreshPolicy,
              other->_worksForDir, other->_worksForFile, other->_worksForDotEntry,
              other->_outputWindowPolicy, other->_outputWindowTimeout, other->_outputWindowAutoClose,
              other->_shell, other->_iconName )
{
//    setIcon ( other->iconName() ); // not currently in the config dialog
    setShortcut( other->shortcut() );
}


void Cleanup::setTitle( const QString &title )
{
    _title = title;
    QAction::setText( _title );
}


void Cleanup::setIcon( const QString & iconName )
{
    _iconName = iconName;
    QAction::setIcon( QPixmap( _iconName ) );
}


bool Cleanup::worksFor( FileInfo *item ) const
{
    if ( ! _active || ! item )
	return false;

    if ( item->isPseudoDir() )
	return worksForDotEntry();

    if ( item->isDir() )
	return worksForDir();

    return worksForFile();
}


void Cleanup::execute( FileInfo *item, OutputWindow * outputWindow )
{
    if ( worksFor( item ) )
    {
	executeRecursive( item, outputWindow );

        // Refreshing the tree based on the cleanup's refresh policy is now
        // handled completely in CleanupCollection::execute() to be safer
        // against segfaults due to pointers becoming invalid due to their
        // parents having been deleted during refreshing.
    }
}


void Cleanup::executeRecursive( FileInfo *item, OutputWindow * outputWindow )
{
    if ( worksFor( item ) )
    {
	if ( _recurse )
	{
	    // Recurse into all subdirectories.

	    FileInfo * subdir = item->firstChild();

	    while ( subdir )
	    {
		if ( subdir->isDir() )
		{
		    /**
		     * Recursively execute in this subdirectory, but only if it
		     * really is a directory: File children might have been
		     * reparented to the directory (normally, they reside in
		     * the dot entry) if there are no real subdirectories on
		     * this directory level.
		     **/
		    executeRecursive( subdir, outputWindow );
		}

		subdir = subdir->next();
	    }
	}

	// Perform cleanup for this directory.
	runCommand( item, _command, outputWindow );
    }
}


const QString Cleanup::itemDir( const FileInfo *item ) const
{
    QString dir = item->path();

    if ( ! item->isDir() && ! item->isPseudoDir() )
    {
	dir.replace( QRegularExpression( "/[^/]*$" ), "" );
    }

    return dir;
}


QString Cleanup::expandVariables( const FileInfo * item,
				  const QString	 & unexpanded ) const
{
    QString expanded = expandDesktopSpecificApps( unexpanded );
    QString dirName = "";

    if ( item->isDir() )
	dirName = item->path();
    else if ( item->parent() )
	dirName = item->parent()->path();

    expanded.replace( "%p", quoted( escaped( item->path() ) ) );
    expanded.replace( "%n", quoted( escaped( item->name() ) ) );

    if ( ! dirName.isEmpty() )
	expanded.replace( "%d", quoted( escaped( dirName ) ) );

    // logDebug() << "Expanded: \"" << expanded << "\"" << Qt::endl;
    return expanded;
}


QString Cleanup::chooseShell( OutputWindow * outputWindow ) const
{
    QString errMsg;
    QString shell = this->shell();

    if ( ! shell.isEmpty() )
    {
	logDebug() << "Using custom shell " << shell << Qt::endl;

	if ( ! isExecutable( shell ) )
	{
	    errMsg = tr( "ERROR: Shell %1 is not executable" ).arg( shell );
	    shell = defaultShell();

	    if ( ! shell.isEmpty() )
		errMsg += "\n" + tr( "Using fallback %1" ).arg( shell );
	}
    }

    if ( shell.isEmpty() )
    {
	shell = defaultShell();
	logDebug() << "No custom shell configured - using " << shell << Qt::endl;
    }

    if ( ! errMsg.isEmpty() )
    {
	outputWindow->show(); // Show error regardless of user settings
	outputWindow->addStderr( errMsg );
    }

    return shell;
}


void Cleanup::runCommand( const FileInfo * item,
			  const QString	 & command,
			  OutputWindow	 * outputWindow ) const
{
    const QString shell = chooseShell( outputWindow );

    if ( shell.isEmpty() )
    {
	outputWindow->show(); // Regardless of user settings
	outputWindow->addStderr( tr( "No usable shell - aborting cleanup action" ) );
	logError() << "ERROR: No usable shell" << Qt::endl;
	return;
    }

    QString cleanupCommand( expandVariables( item, command ));
    QProcess * process = new QProcess( parent() );
    CHECK_NEW( process );

    process->setProgram( shell );
    process->setArguments( { "-c", cleanupCommand } );
    process->setWorkingDirectory( itemDir( item ) );
    // logDebug() << "New process \"" << process << Qt::endl;

    outputWindow->addProcess( process );

    // The CleanupCollection will take care about refreshing if this is
    // configured for this cleanup.
}


SettingsEnumMapping Cleanup::refreshPolicyMapping()
{
    static SettingsEnumMapping mapping;

    mapping[ NoRefresh	   ] = "NoRefresh";
    mapping[ RefreshThis   ] = "RefreshThis";
    mapping[ RefreshParent ] = "RefreshParent";
    mapping[ AssumeDeleted ] = "AssumeDeleted";

    return mapping;
}


SettingsEnumMapping Cleanup::outputWindowPolicyMapping()
{
    static SettingsEnumMapping mapping;

    mapping[ ShowAlways	       ] = "ShowAlways";
    mapping[ ShowIfErrorOutput ] = "ShowIfErrorOutput";
    mapping[ ShowAfterTimeout  ] = "ShowAfterTimeout";
    mapping[ ShowNever	       ] = "ShowNever";

    return mapping;
}


bool Cleanup::isExecutable( const QString & programName )
{
    if ( programName.isEmpty() )
	return false;

    QFileInfo fileInfo( programName );
    return fileInfo.isExecutable();
}


const QString & Cleanup::loginShell()
{
    static bool cached = false;
    static QString shell;

    if ( ! cached )
    {
	cached = true;
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	shell = env.value( "SHELL", "" );

	if ( ! isExecutable( shell ) )
	{
	    logError() << "ERROR: Shell \"" << shell << "\" is not executable" << Qt::endl;
	    shell = "";
	}
    }

    return shell;
}


const QStringList & Cleanup::defaultShells()
{
    static bool cached = false;
    static QStringList shells;

    if ( ! cached )
    {
	cached = true;
	const QStringList candidates = { loginShell(), "/bin/bash", "/bin/sh" };

	for ( const QString & shell : candidates )
	{
	    if ( isExecutable( shell ) )
		 shells << shell;
	    else if ( ! shell.isEmpty() )
	    {
		logWarning() << "Shell " << shell << " is not executable" << Qt::endl;
	    }
	}

	if ( ! shells.isEmpty() )
	    logDebug() << "Default shell: " << shells.first() << Qt::endl;
    }

    if ( shells.isEmpty() )
	logError() << "ERROR: No usable shell" << Qt::endl;

    return shells;
}


const QMap<QString, QString> & Cleanup::desktopSpecificApps()
{
    static QMap<QString, QString> apps;

    if ( apps.isEmpty() )
    {
	QString desktop = QString::fromUtf8( qgetenv( "QDIRSTAT_DESKTOP" ) );

	if ( desktop.isEmpty() )
	     desktop = QString::fromUtf8( qgetenv( "XDG_CURRENT_DESKTOP" ) );
	else
	{
	    logDebug() << "Overriding $XDG_CURRENT_DESKTOP with $QDIRSTAT_DESKTOP (\""
		       << desktop << "\")" << Qt::endl;
	}

	if ( desktop.isEmpty() )
	{
	    logWarning() << "$XDG_CURRENT_DESKTOP is not set - using fallback apps" << Qt::endl;
	    apps = fallbackApps();
	}
	else
	{
	    logInfo() << "Detected desktop \"" << desktop << "\"" << Qt::endl;
	    desktop = desktop.toLower();

	    if ( desktop == "kde" )
	    {
		// KDE konsole misbehaves in every way possible:
		//
		// It cannot be started in the background from a cleanup action,
		// it will terminate when QDirStat terminates,
		// and it doesn't give a shit about its current working directory.
		//
		// After having wasted four hours to get that thing to cooperate,
		// I simply don't care any more: The other terminals will get
		// the & added here rather than in the cleanup command line
		// where it would be appropriate. All this just because KDE
		// konsole doesn't comply with any standards whatsoever.

		apps[ "%terminal"    ] = "konsole --workdir %d";

                // Using xdg-open to enable using konqueror if configured,
                // falling back to dolphin otherwise.

		apps[ "%filemanager" ] = "xdg-open %d";
	    }
	    else if ( desktop == "gnome" ||
		      desktop == "unity"   )
	    {
		apps[ "%terminal"    ] = "gnome-terminal &";
		apps[ "%filemanager" ] = "nautilus";
	    }
	    else if ( desktop == "xfce" )
	    {
		apps[ "%terminal"    ] = "xfce4-terminal &";
		apps[ "%filemanager" ] = "thunar";
	    }
	    else if ( desktop == "lxde" )
	    {
		apps[ "%terminal"    ] = "lxterminal &";
		apps[ "%filemanager" ] = "pcmanfm";
	    }
	    else if ( desktop == "enlightenment" )
	    {
		apps[ "%terminal"    ] = "eterm &";
		apps[ "%filemanager" ] = "xdg-open";
	    }

	    if ( apps.isEmpty() )
	    {
		logWarning() << "No mapping available for this desktop - using fallback apps" << Qt::endl;
		apps = fallbackApps();
	    }
	}

	for ( auto it = apps.constBegin(); it != apps.constEnd(); ++it )
	    logInfo() << it.key() << " => \"" << it.value() << "\"" << Qt::endl;
    }

    return apps;
}


const QMap<QString, QString> & Cleanup::fallbackApps()
{
    static QMap<QString, QString> apps;

    if ( apps.isEmpty() )
    {
#ifdef Q_OS_MAC
	apps[ "%terminal"    ] = "open -a Terminal.app .";
	apps[ "%filemanager" ] = "open";
#else
	apps[ "%terminal"    ] = "xterm";
	apps[ "%filemanager" ] = "xdg-open";
#endif
    }

    return apps;
}


QString Cleanup::expandDesktopSpecificApps( const QString & unexpanded ) const
{
    QString expanded = unexpanded;
    const QMap<QString, QString> & apps = desktopSpecificApps();

    for ( auto it = apps.constBegin(); it != apps.constEnd(); ++it )
	expanded.replace( it.key(), it.value() );

    return expanded;
}
