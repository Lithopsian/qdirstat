/*
 *   File name: PkgInfo.cpp
 *   Summary:	Support classes for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "PkgInfo.h"
#include "DirTree.h"
#include "FileInfoIterator.h"
#include "Logger.h"


using namespace QDirStat;


QString PkgInfo::url() const
{
    return isPkgUrl( _name ) ? "Pkg:/" : "Pkg:/" + _name;
}


bool PkgInfo::isPkgUrl( const QString & url )
{
    return url.startsWith( "Pkg:" );
}


QString PkgInfo::pkgUrl( const QString & path ) const
{
    if ( isPkgUrl( path ) )
        return path;
    else
        return url() + path;
}


FileInfo * PkgInfo::locate( const QString & path )
{
    QStringList components = path.split( "/", Qt::SkipEmptyParts );

    if ( isPkgUrl( path ) )
    {
        components.removeFirst();       // Remove the leading "Pkg:"

        if ( components.isEmpty() )
            return ( this == _tree->root() ) ? this : 0;

        QString pkgName = components.takeFirst();

        if ( pkgName != _name )
        {
            logError() << "Path " << path << " does not belong to " << this << Qt::endl;
            return nullptr;
        }

        if ( components.isEmpty() )
            return this;
    }

    return locate( this, components );
}


FileInfo * PkgInfo::locate( const QStringList & pathComponents )
{
    return locate( this, pathComponents );
}


FileInfo * PkgInfo::locate( DirInfo *           subtree,
                            const QStringList & pathComponents )
{
    // logDebug() << "Locating /" << pathComponents.join( "/" ) << " in " << subtree << Qt::endl;

    if ( ! subtree || pathComponents.isEmpty() )
        return nullptr;

    QStringList   components = pathComponents;
    const QString wanted     = components.takeFirst();

    FileInfoIterator it( subtree );

    while ( *it )
    {
        // logDebug() << "Checking " << (*it)->name() << " in " << subtree << " for " << wanted << Qt::endl;

        if ( (*it)->name() == wanted )
        {
            if ( components.isEmpty() )
            {
                // logDebug() << "  Found " << *it << Qt::endl;
                return *it;
            }
            else
            {
                if ( ! (*it)->isDirInfo() )
                    return nullptr;
                else
                    return locate( (*it)->toDirInfo(), components );
            }
        }

        ++it;
    }

    return nullptr;
}
