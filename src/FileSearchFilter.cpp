/*
 *   File name: FileSearchFilter.h
 *   Summary:	Package manager Support classes for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#include "FileSearchFilter.h"
#include "Logger.h"
#include "Exception.h"


using namespace QDirStat;


FileSearchFilter::FileSearchFilter():
    FileSearchFilter { 0, "", Auto, true }
{
}

FileSearchFilter::FileSearchFilter( DirInfo *       subtree,
                                    const QString & pattern,
                                    FilterMode      filterMode,
                                    bool            caseSensitive ):
    SearchFilter( pattern,
                  filterMode,
                  Contains, // defaultFilterMode
                  caseSensitive ),  // case-insensitive
    _subtree( subtree ),
    _findFiles( true ),
    _findDirs( true ),
    _findSymLinks( true ),
    _findPkg( true )
{
}
