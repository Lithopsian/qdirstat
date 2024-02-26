/*
 *   File name: DotEntry.cpp
 *   Summary:	Support classes for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "DotEntry.h"
#include "DirTree.h"
#include "Exception.h"
#include "Logger.h"


using namespace QDirStat;


DotEntry::DotEntry( DirTree * tree,
		    DirInfo * parent )
    : DirInfo( parent, tree, dotEntryName() )
{
    if ( parent )
    {
	_device = parent->device();
	_mode	= parent->mode();
	_uid	= parent->uid();
	_gid	= parent->gid();
    }
}


void DotEntry::insertChild( FileInfo * newChild )
{
    CHECK_PTR( newChild );

    // Whatever is added here is added directly to this node; a dot entry
    // cannot have a dot entry itself.

    newChild->setNext( _firstChild );
    _firstChild = newChild;
    newChild->setParent( this );	// make sure the parent pointer is correct

    childAdded( newChild );		// update summaries
}


DirReadState DotEntry::readState() const
{
    if ( _parent )
	return _parent->readState();
    else // This should never happen
	return _readState;
}
