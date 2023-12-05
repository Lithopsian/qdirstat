/*
 *   File name: DpkgPkgManager.cpp
 *   Summary:	Dpkg package manager support for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#include <QFileInfo>
#include <QDir>

#include "DpkgPkgManager.h"
#include "PkgFileListCache.h"
#include "Logger.h"
#include "Exception.h"

#define LOG_COMMANDS	true
#define LOG_OUTPUT	false

#include "SysUtil.h"


using namespace QDirStat;

using SysUtil::runCommand;
using SysUtil::tryRunCommand;
using SysUtil::haveCommand;


bool DpkgPkgManager::isPrimaryPkgManager()
{
    return tryRunCommand( "/usr/bin/dpkg -S /usr/bin/dpkg", QRegExp( "^dpkg:.*" ) );
}


bool DpkgPkgManager::isAvailable()
{
    return haveCommand( "/usr/bin/dpkg" );
}


QString DpkgPkgManager::owningPkg( const QString & path )
{
   // Try first with the full (possibly symlinked) path
    const QFileInfo fileInfo( path );
    int exitCode = -1;
    QString output = runCommand( "/usr/bin/dpkg", QStringList() << "-S" << path, &exitCode );
    if ( exitCode == 0 )
    {
	const QString package = searchOwningPkg( path, output );
	if ( !package.isEmpty() )
	    return package;
    }

    // Search again just by filename in case part of the directory path is symlinked
    // (this may produce a lot of rows)
    const QString filename = fileInfo.fileName();
    exitCode = -1;
    output = runCommand( "/usr/bin/dpkg", QStringList() << "-S" << filename, &exitCode );
    if ( exitCode != 0 )
	return "";

    return searchOwningPkg( path, output );
}


QString DpkgPkgManager::searchOwningPkg( const QString & path, const QString & output )
{
    const QStringList lines = output.trimmed().split( "\n" );
    for ( QStringList::const_iterator line = lines.begin(); line != lines.end(); ++line )
    {
	if ( (*line).isEmpty() )
	    continue;

	// For diversions, the line "diversion by ... from: ..." gives the current owning package
	// A line "diversion by ... to ..." should immediately follow and indicates a path that is
	// the divert target.  The file will not exist unless the diverted file has been renamed;
	// in this case the owning package of this file can only be found by another query against
	// the file path as shown in the "diversion ... from" line.
	// Thankfully very rare!
	if ( isDiversion( *line ) )
	{
	    if ( !isDiversionFrom( *line ) )
		// something wrong, just skip it and hope
		continue;

	    // Need to take this first path and package to compare with the third one
	    // to see if the file really belongs to that package
	    QStringList fields = line->split( ": " );
	    if ( fields.size() != 2 )
		continue;

	    const QString path1 = fields.last();
	    const QString divertPkg = fields.first().split( " " ).at( 2 );

	    // the next line should contain the path where this file now resides
	    // (or would reside if it hasn't been diverted yet)
	    if ( ++line == lines.end() )
		return "";

	    if ( !isDiversionTo( *line ) )
		continue;

	    fields = line->split( ": " );
	    if ( fields.size() != 2 )
		continue;

	    const QString path2Resolved = resolvePath( fields.last() );

	    if ( path2Resolved == path )
		// the renamed file is our file, have to do another query to get the package:
		// dpkg -S against the pathname from the diversion by ... from line
		return originalOwningPkg( path1 );

	    const bool localDiversion = isLocalDiversion( *line );

	    // and the line after that might give the package and the original file path
	    if ( ++line == lines.end() )
		return "";

	    fields = line->split( ": " );
	    if ( fields.size() != 2 )
		continue;

	    QString packages = fields.first();

	    // the from/to pair for the renamed file is followed by an unrelated entry
	    const QString path1Resolved = resolvePath ( path1 );
	    const QString path3Resolved = resolvePath( fields.last() );
	    if ( path1Resolved != path3Resolved )
	    {
		// ... so start parsing again normally from the third line
		--line;
		continue;
	    }

	    // if this is a local diversion, give up at this point because there is no owning package
	    if ( localDiversion )
		continue;

	    // if the package from the diversion by ... from line is also in the third line
	    if ( packages.contains( divertPkg ) )
	    {
		// ... and the resolved path matches the input
		if ( path == path1Resolved )
		    // ... then return the diverting package from the first line
		    return divertPkg;
	    }
	    else
	    {
		// ... and the resolved path matches the input
		if ( path == path2Resolved )
		    // ... then return the package that owned this file pre-divert
		    return packages.split( ": " ).first();
	    }

	    // wrong diversion triplet, carry on, skipping the third line
	    continue;
	}

	// Just a normal package: path line
	const QStringList packages = line->split( ": " );
	if ( packages.size() != 2 )
	    continue;

	// resolve any symlinks in the package path
	const QFileInfo pkgFileInfo( packages.last() );
	const QString pkgPathInfo = pkgFileInfo.path();
	const QString pkgRealpath = QFileInfo( pkgPathInfo ).canonicalFilePath();
	const QString pkgFilename = pkgFileInfo.fileName();
	if ( !pkgRealpath.isEmpty() && !pkgFilename.isNull() )
	{
	    //logDebug() << " " << pkgRealpath << endl;
	    // Is this the exact file we were looking for?
	    if ( pkgRealpath + "/" + pkgFilename == path )
		return packages.first();
	}
    }

    return "";
}


QString DpkgPkgManager::originalOwningPkg( const QString & path )
{
    // Search with the filename from the (potentially symlinked) path
    // ... looking for exactly three lines matching:
    // diversion by other-package from: path
    // diversion by other-package to: renamed-path
    // package: path

    int exitCode = -1;
    const QFileInfo fileInfo( path );
    const QString filename = fileInfo.fileName();
    QString output = runCommand( "/usr/bin/dpkg", QStringList() << "-S" << path, &exitCode );
    if ( exitCode != 0 )
	return "";

    const QString pathResolved = resolvePath( path );

    const QStringList lines = output.trimmed().split( "\n" );
    QStringList::const_iterator line = lines.begin();
    while ( line != lines.end() )
    {
	while ( line != lines.end() && !isDiversionFrom( *line ) )
	    ++line;

	if ( ++line != lines.end() && isDiversionTo( *line ) )
	{
	    if ( ++line != lines.end() )
	    {
		const QStringList fields = (*line).split( ": " );
		if ( fields.size() == 2 && resolvePath( fields.last() ) == pathResolved )
		    return fields.first().split( ", " ).first();
	    }
	}

	++line;
    }

    return "";
}


PkgInfoList DpkgPkgManager::installedPkg()
{
    int exitCode = -1;
    QString output = runCommand( "/usr/bin/dpkg-query",
				 QStringList()
				 << "--show"
				 << "--showformat=${Package} | ${Version} | ${Architecture} | ${Status}\n",
				 &exitCode );

    PkgInfoList pkgList;

    if ( exitCode == 0 )
	pkgList = parsePkgList( output );

    return pkgList;
}


PkgInfoList DpkgPkgManager::parsePkgList( const QString & output )
{
    PkgInfoList pkgList;

    foreach ( const QString & line, output.split( "\n" ) )
    {
	if ( ! line.isEmpty() )
	{
	    QStringList fields = line.split( " | ", QString::KeepEmptyParts );

	    if ( fields.size() != 4 )
		logError() << "Invalid dpkg-query output: \"" << line << "\n" << endl;
	    else
	    {
		QString name	= fields.takeFirst();
		QString version = fields.takeFirst();
		QString arch	= fields.takeFirst();
		QString status	= fields.takeFirst();

		if ( status == "install ok installed" ||
                     status == "hold ok installed"       )
		{
		    PkgInfo * pkg = new PkgInfo( name, version, arch, this );
		    CHECK_NEW( pkg );

		    pkgList << pkg;
		}
		else
		{
		    // logDebug() << "Ignoring " << line << endl;
		}
	    }
	}
    }

    return pkgList;
}


QStringList DpkgPkgManager::parseFileList( const QString & output )
{
    QStringList fileList;
    QStringList lines = output.split( "\n" );

    foreach ( const QString & line, lines )
    {
	if ( isDivertedBy( line ) )
	{
	    // previous line referred to a file that has been diverted to a different location
	    fileList.removeLast();

	    // this line contains the new location for the file from this package
	    const QStringList fields = line.split( ": " );
	    QString divertedFile = fields.last();
	    if ( fields.size() == 2 && !divertedFile.isEmpty() )
		fileList << resolvePath(divertedFile);
	}
	else if ( line != "/." && ! isPackageDivert( line ) )
	    fileList << resolvePath(line);
    }

    return fileList;
}


QString DpkgPkgManager::resolvePath( const QString & pathname )
{
    const QFileInfo fileInfo( pathname );

    // a directory that is a symlink in root (eg. /lib) is resolved
    if ( fileInfo.dir().isRoot() && fileInfo.isDir() ) // first test doesn't access the filesystem
    {
	const QString realpath = fileInfo.canonicalFilePath();
	//logDebug() << pathname << " " << realpath << endl;
	return realpath.isEmpty() ? pathname : realpath;
    }

    // in all other cases, only symlinks in the parent path are resolved
    const QString filename = fileInfo.fileName();
    const QString pathInfo = fileInfo.path();
    const QString realpath = QFileInfo( pathInfo ).canonicalFilePath();
    //logDebug() << pathname << " " << realpath << " " << filename << endl;
    return ( realpath != pathInfo && !realpath.isEmpty() ) ? realpath + "/" + filename : pathname;
}


QString DpkgPkgManager::queryName( PkgInfo * pkg )
{
    CHECK_PTR( pkg );

    QString name = pkg->baseName();

#if 0
    if ( pkg->isMultiVersion() )
	name += "_" + pkg->version();

    if ( pkg->isMultiArch() )
	name += ":" + pkg->arch();
#endif

    if ( pkg->arch() != "all" )
	name += ":" + pkg->arch();

    return name;
}


PkgFileListCache * DpkgPkgManager::createFileListCache( PkgFileListCache::LookupType lookupType )
{
    int exitCode = -1;
    QString output = runCommand( "/usr/bin/dpkg", QStringList() << "-S" << "*", &exitCode );

    if ( exitCode != 0 )
	return 0;

    QStringList lines = output.split( "\n" );
    output.clear(); // Free all that text ASAP
    logDebug() << lines.size() << " output lines" << endl;

    PkgFileListCache * cache = new PkgFileListCache( this, lookupType );
    CHECK_NEW( cache );

    // Sample output:
    //
    //	   zip: /usr/bin/zip
    //	   zlib1g-dev:amd64: /usr/include/zlib.h
    //	   zlib1g:i386, zlib1g:amd64: /usr/share/doc/zlib1g
    for ( QStringList::const_iterator line = lines.begin(); line != lines.end(); ++line )
    {
	if ( line->isEmpty() )
	    continue;

	QString pathname;
	QString packages;

	if ( isDiversion( *line ) )
	{
	    // For diversions, the line "diversion by ... from: ..." gives the current owning package.
	    // Normal lines for files that have been diverted should be ignored because that file will
	    // have been renamed to the path shown in the "diversion by ... to ..." line.
	    // The original file may not exist (see glx-diversions) or may now be owned by a different
	    // package. The new owning package is shown by another query against the file path as shown
	    // in the "diversion ... from" line.
	    // Thankfully very rare!
	    if ( !isDiversionFrom( *line ) )
		// something wrong, just skip it and hope
		continue;

	    // Need to take this first path and package to compare with the last one
	    // to see if the file really belongs to that package
	    QStringList fields = line->split( ": " );
	    QString path1 = fields.last();
	    QString divertPkg = fields.first().split( " " ).at( 2 );

	    // the next line should contain the path where this file now resides
	    ++line;
	    if ( !isDiversionTo( *line ) )
		continue;

	    fields = line->split( ": " );
	    if ( fields.size() != 2 )
		continue;

	    QString path2 = resolvePath( fields.last() );

	    // and the one after that might give the package and the original file path
	    ++line;
	    fields = line->split( ": " );
	    if ( fields.size() != 2 )
		continue;

	    packages = fields.first();

	    // the from/to pair for the renamed file is followed by an unrelated entry
	    QString path3 = fields.last();
	    if ( path1 != path3 )
	    {
		// we start again, could be a normal entry of another diversion line
		--line;
		continue;
	    }
	    else if ( packages.contains( divertPkg ) )
	    {
		// associate original file with diverting package
		packages = divertPkg;
		pathname = resolvePath( path1 );
	    }
	    else
	    {
		// associate renamed file with its original package
		pathname = resolvePath( path2 );
	    }
	}
	else
	{
	    QStringList fields = line->split( ": " );
	    if ( fields.size() != 2 )
	    {
		logError() << "Unexpected file list line: \"" << *line << "\"" << endl;
		continue;
	    }

	    pathname = resolvePath( fields.takeLast() );
	    packages = fields.first();
	}

	if ( pathname.isEmpty() || pathname == "/." )
	    continue;

	foreach ( const QString & pkgName, packages.split( ", " ) )
	    if ( ! pkgName.isEmpty() )
		cache->add( pkgName, pathname );
    }

    logDebug() << "file list cache finished." << endl;

    return cache;
}

