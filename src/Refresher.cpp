/*
 *   File name: Refresher.h
 *   Summary:	Helper class to refresh a number of subtrees
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "Refresher.h"
#include "DirTree.h"
#include "DirInfo.h"
#include "FileInfoSet.h"
#include "Logger.h"

using namespace QDirStat;



void Refresher::refresh()
{
    if ( ! _items.isEmpty() && _tree )
    {
	logDebug() << "Refreshing " << _items.size() << " items" << Qt::endl;

	_tree->refresh( _items );
    }
    else
    {
	logWarning() << "No items to refresh" << Qt::endl;
    }

    this->deleteLater();
}


FileInfoSet Refresher::parents( const FileInfoSet children )
{
    FileInfoSet parents;

    foreach ( FileInfo * child, children )
    {
	if ( child && child->parent() )
        {
            FileInfo * parent = child->parent();

            if ( parent->isPseudoDir() )
                parent = parent->parent();

            if ( parent )
                parents << parent;
        }
    }

    return parents.normalized();
}

