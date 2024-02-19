/*
 *   File name: PkgManager.cpp
 *   Summary:	Simple package manager support for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "PkgQuery.h"
#include "PkgManager.h"
#include "DpkgPkgManager.h"
#include "RpmPkgManager.h"
#include "PacManPkgManager.h"
#include "Logger.h"
#include "Exception.h"

#define LOG_COMMANDS	true
#define LOG_OUTPUT	false
#include "SysUtil.h"


#define CACHE_SIZE		5000
#define CACHE_COST		1

#define VERBOSE_PKG_QUERY	0


using namespace QDirStat;

using SysUtil::runCommand;
using SysUtil::tryRunCommand;
using SysUtil::haveCommand;


PkgQuery * PkgQuery::instance()
{
    static PkgQuery _instance;
    return &_instance;
}


PkgQuery::PkgQuery()
{
    _cache.setMaxCost( CACHE_SIZE );
    checkPkgManagers();
}


PkgQuery::~PkgQuery()
{
    qDeleteAll( _pkgManagers );
}


void PkgQuery::checkPkgManagers()
{
    logInfo() << "Checking available supported package managers..." << Qt::endl;

    checkPkgManager( new DpkgPkgManager()   );
    checkPkgManager( new RpmPkgManager()    );
    checkPkgManager( new PacManPkgManager() );

    _pkgManagers += _secondaryPkgManagers;
    _secondaryPkgManagers.clear();

    if ( _pkgManagers.isEmpty() )
        logInfo() << "No supported package manager found." << Qt::endl;
    else
    {
        QStringList available;

        foreach ( const PkgManager * pkgManager, _pkgManagers )
            available << pkgManager->name();

        logInfo() << "Found " << available.join( ", " )  << Qt::endl;
    }
}


void PkgQuery::checkPkgManager( const PkgManager * pkgManager )
{
    CHECK_PTR( pkgManager );

    if ( pkgManager->isPrimaryPkgManager() )
    {
	logInfo() << "Found primary package manager " << pkgManager->name() << Qt::endl;
	_pkgManagers << pkgManager;
    }
    else if ( pkgManager->isAvailable() )
    {
	logInfo() << "Found secondary package manager " << pkgManager->name() << Qt::endl;
	_secondaryPkgManagers << pkgManager;
    }
    else
    {
	delete pkgManager;
    }
}


QString PkgQuery::getOwningPackage( const QString & path )
{
    QString pkg = "";
    QString foundBy;
    bool haveResult = false;

    if ( _cache.contains( path ) )
    {
	haveResult = true;
	foundBy	   = "Cache";
	pkg	   = *( _cache[ path ] );
    }


    if ( ! haveResult )
    {
	foreach ( const PkgManager * pkgManager, _pkgManagers )
	{
	    pkg = pkgManager->owningPkg( path );

	    if ( ! pkg.isEmpty() )
	    {
		haveResult = true;
		foundBy	   = pkgManager->name();
		break;
	    }
	}

	if ( foundBy.isEmpty() )
	    foundBy = "all";

	// Insert package name (even if empty) into the cache
	_cache.insert( path, new QString( pkg ), CACHE_COST );
    }

#if VERBOSE_PKG_QUERY
    if ( pkg.isEmpty() )
	logDebug() << foundBy << ": No package owns " << path << Qt::endl;
    else
	logDebug() << foundBy << ": Package " << pkg << " owns " << path << Qt::endl;
#endif

    return pkg;
}


PkgInfoList PkgQuery::getInstalledPkg() const
{
    PkgInfoList pkgList;

    foreach ( const PkgManager * pkgManager, _pkgManagers )
    {
        pkgList.append( pkgManager->installedPkg() );
    }

    return pkgList;
}


QStringList PkgQuery::getFileList( const PkgInfo * pkg ) const
{
    foreach ( const PkgManager * pkgManager, _pkgManagers )
    {
        const QStringList fileList = pkgManager->fileList( pkg );

        if ( ! fileList.isEmpty() )
            return fileList;
    }

    return QStringList();
}


bool PkgQuery::checkGetInstalledPkgSupport() const
{
    foreach ( const PkgManager * pkgManager, _pkgManagers )
    {
        if ( pkgManager->supportsGetInstalledPkg() )
            return true;
    }

    return false;
}


bool PkgQuery::checkFileListSupport() const
{
    foreach ( const PkgManager * pkgManager, _pkgManagers )
    {
        if ( pkgManager->supportsFileList() )
            return true;
    }

    return false;
}
