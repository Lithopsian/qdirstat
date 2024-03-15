/*
 *   File name: FileInfoIterator.h
 *   Summary:	Support classes for QDirStat - DirTree iterators
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#ifndef FileInfoIterator_h
#define FileInfoIterator_h


#include <QList>
#include "FileInfo.h"


namespace QDirStat
{
    typedef FileInfoList::const_iterator FileInfoListPos;

    /**
     * Iterator class for children of a FileInfo object. For optimum
     * performance, this iterator class does NOT return children in any
     * specific sort order.
     *
     * Sample usage:
     *
     *	  FileInfoIterator it( node );
     *
     *	  while ( *it )
     *	  {
     *	     logDebug() << *it << ":\t" << (*it)->totalSize() << Qt::endl;
     *	     ++it;
     *	  }
     *
     * This will output the URL (path+name) and the total size of each (direct)
     * subdirectory child and each (direct) file child of 'node'.
     *
     * Notice: This does not recurse into subdirectories, and the dot entry is
     * treated just like a subdirectory!
     *
     * @short (unsorted) iterator for FileInfo children.
     **/
    class FileInfoIterator
    {
    public:
	/**
	 * Constructor: Initialize an iterator object to iterate over the
	 * children of 'parent' (unsorted!). The dot entry is treated as a
	 * subdirectory.  Construction is delegated to the protected
	 * constructor with callNext hard-coded to true.
	 **/
	FileInfoIterator( const FileInfo * parent ) :
	    FileInfoIterator ( parent, true )
	{}

    protected:
	/**
	 * Alternate constructor to be called from derived classes: Those can
	 * choose not to call next() in the constructor.
	 **/
	FileInfoIterator( const FileInfo * parent,
			  bool             callNext );

    public:

	/**
	 * Return the current child object or 0 if there is no more.
	 * Same as operator*() .
	 **/
	FileInfo * current() const { return _current; }

	/**
	 * Return the current child object or 0 if there is no more.
	 * Same as current().
	 **/
	FileInfo * operator*() const { return current(); }

	/**
	 * Advance to the next child. Same as operator++().
	 **/
	void next();

	/**
	 * Advance to the next child. Same as next().
	 **/
	void operator++() { next(); }

	/**
	 * Return the number of items that will be processed.
	 * This is an expensive operation.
	 **/
//	int count();


    protected:

	const FileInfo	* _parent;
	FileInfo	* _current;
	bool		  _directChildrenProcessed;
	bool		 _dotEntryProcessed;

    };	// class FileInfoIterator


    class FileInfoSortedBySizeIterator
    {
    public:

	/**
	 * Constructor. Children below 'minSize' will be ignored by this
	 * iterator.
	 **/
	FileInfoSortedBySizeIterator( FileInfo		* parent,
				      FileSize		( *itemTotalSize )( FileInfo * ) = nullptr,
				      Qt::SortOrder	sortOrder = Qt::DescendingOrder );

	/**
	 * Return the current child object or 0 if there is no more.
	 * Same as operator*() .
	 **/
	FileInfo * current() const { return _currentIt == _sortedChildren.cend() ? 0 : *_currentIt; }

	/**
	 * Return the current child object or 0 if there is no more.
	 * Same as current().
	 **/
	FileInfo * operator*() const { return current(); }

	/**
	 * Advance to the next child. Same as operator++().
	 **/
	void next() { if ( _currentIt != _sortedChildren.cend() ) ++_currentIt; }

	/**
	 * Advance to the next child. Same as next().
	 **/
	void operator++() { next(); }

	/**
	 * Return the total size of the children to be iterated, calculated
	 * using the optional function passed to the constructor.  This is mainly
	 * to avoid TreemapTile having to iterate all the children again.
	 **/
	FileSize totalSize() const { return _totalSize; }

	/**
	 * Functions for "bookmarking" a position in the children that can be returned
	 * to at a later point.  This allows TreemapTile to iterate ahead to identify
	 * tiles to form a row in the squarified layout, but then go back to the original
	 * position to start layout out the row.
	 **/
	FileInfoListPos currentPos() const { return _currentIt; }
	void setPos( FileInfoListPos pos ) { _currentIt = pos; }

    protected:

	FileInfoList     _sortedChildren;
	FileInfoListPos  _currentIt;
	FileSize         _totalSize;
    };

} // namespace QDirStat


#endif // ifndef FileInfoIterator_h

