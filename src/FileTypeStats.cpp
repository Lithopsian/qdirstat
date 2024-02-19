/*
 *   File name: FileTypeStats.cpp
 *   Summary:	Statistics classes for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "FileTypeStats.h"
#include "DirTree.h"
#include "FileInfoIterator.h"
#include "MimeCategorizer.h"
#include "FormatUtil.h"
#include "Logger.h"
#include "Exception.h"

using namespace QDirStat;


FileTypeStats::FileTypeStats( QObject  * parent ):
    QObject( parent ),
    _totalSize( 0LL )
{
    _mimeCategorizer = MimeCategorizer::instance();
    CHECK_PTR( _mimeCategorizer );

    _otherCategory = new MimeCategory( tr( "Other" ) );
    CHECK_NEW( _otherCategory );
}


FileTypeStats::~FileTypeStats()
{
    clear();
    delete _otherCategory;
}


void FileTypeStats::clear()
{
    _suffixSum.clear();
    _suffixCount.clear();
    _categorySum.clear();
    _categoryCount.clear();
    _totalSize = 0LL;
}


void FileTypeStats::calc( FileInfo * subtree )
{
    clear();

    if ( subtree && subtree->checkMagicNumber() )
    {
        collect( subtree );
        _totalSize = subtree->totalSize();
        removeCruft();
        removeEmpty();
        sanityCheck();
    }

    emit calcFinished();
}


void FileTypeStats::collect( FileInfo * dir )
{
    if ( ! dir )
	return;

    FileInfoIterator it( dir );

    while ( *it )
    {
	FileInfo * item = *it;

	if ( item->hasChildren() )
	{
	    collect( item );
	}
	else if ( item->isFile() )
	{
	    QString suffix;

	    // First attempt: Try the MIME categorizer.
	    //
	    // If it knows the file's suffix, it can much easier find the
	    // correct one in case there are multiple to choose from, for
	    // example ".tar.bz2", not ".bz2" for a bzipped tarball. But on
	    // Linux systems, having multiple dots in filenames is very common,
	    // e.g. in .deb or .rpm packages, so the longest possible suffix is
	    // not always the useful one (because it might contain version
	    // numbers and all kinds of irrelevant information).
	    //
	    // The suffixes the MIME categorizer knows are carefully
	    // hand-crafted, so if it knows anything about a suffix, it's the
	    // best choice.

	    const MimeCategory * category = _mimeCategorizer->category( item, &suffix );

	    if ( category )
            {
                addCategorySum( category, item );

		if ( suffix.isEmpty() )
                    addNonSuffixRuleSum( category, item );
                else
                    addSuffixSum( suffix, category, item );
	    }
            else // ! category
            {
                addCategorySum( _otherCategory, item );

                if ( suffix.isEmpty() )
                {
                    if ( item->name().contains( '.' ) && ! item->name().startsWith( '.' ) )
                    {
                        // Fall back to the last (i.e. the shortest) suffix if the
                        // MIME categorizer didn't know it: Use section -1 (the
                        // last one, ignoring any trailing '.' separator).
                        //
                        // The downside is that this would not find a ".tar.bz",
                        // but just the ".bz" for a compressed tarball. But it's
                        // much better than getting a ".eab7d88df-git.deb" rather
                        // than a ".deb".

                        suffix = item->name().section( '.', -1 );
                    }
                }

                suffix = suffix.toLower();

                if ( suffix.isEmpty() )
                    suffix = NO_SUFFIX;

		addSuffixSum( suffix, _otherCategory, item );
            }

	    // Disregard symlinks, block devices and other special files
	}

	++it;
    }
}


void FileTypeStats::addCategorySum( const MimeCategory * category, const FileInfo * item )
{
    _categorySum[ category ] += item->size();
    ++_categoryCount[ category ];
}


void FileTypeStats::addNonSuffixRuleSum( const MimeCategory * category, const FileInfo * item )
{
    _categoryNonSuffixRuleSum[ category ] += item->size();
    ++_categoryNonSuffixRuleCount[ category ];
}


void FileTypeStats::addSuffixSum( const QString & suffix, const MimeCategory * category, const FileInfo * item )
{
    const MapCategory mapCategory = { suffix, category };
    _suffixSum[ mapCategory ] += item->size();
    ++_suffixCount[ mapCategory ];
}


void FileTypeStats::removeCruft()
{
    // Make sure those two already exist to avoid confusing the iterator
    // (QMap::operator[] auto-inserts with default ctor if not already there)

    const MapCategory cruftMapCategory = { NO_SUFFIX, _otherCategory };
    _suffixSum	[ cruftMapCategory ] += 0LL;
    _suffixCount[ cruftMapCategory ] += 0;

    FileSize totalMergedSum   = 0LL;
    int	     totalMergedCount = 0;

    QStringList cruft;
    StringIntMap::iterator it = _suffixCount.begin();

    while ( it != _suffixCount.end() )
    {
	const QString suffix = it.key().first;
	const MimeCategory * category = it.key().second;

	if ( isCruft( suffix, category ) )
	{
	    const MapCategory mapCategory = { suffix, category };

	    _suffixSum	[ cruftMapCategory ] += _suffixSum  [ mapCategory ];
	    _suffixCount[ cruftMapCategory ] += _suffixCount[ mapCategory ];

	    totalMergedSum   += _suffixSum  [ mapCategory ];
	    totalMergedCount += _suffixCount[ mapCategory ];

	    it = _suffixCount.erase( it );
	    _suffixSum.remove( { suffix, category } );

	    cruft << "*." + suffix;
	}
	else
	{
	    ++it;
	}
    }

#if 1
    logDebug() << "Merged " << cruft.size() << " suffixes to <NO SUFFIX>: "
	       << cruft.join( ", " ) << Qt::endl;
#endif
    logDebug() << "Merged: " << totalMergedCount << " files "
	       << "(" << formatSize( totalMergedSum ) << ")"
	       << Qt::endl;
}


void FileTypeStats::removeEmpty()
{
    StringIntMap::iterator it = _suffixCount.begin();

    while ( it != _suffixCount.end() )
    {
	const int	count  = it.value();
	const bool	remove = count == 0;

	const QString suffix = it.key().first;
	const MimeCategory * category = it.key().second;

	if ( remove )
	{
	    logDebug() << "Removing empty suffix *." << suffix << Qt::endl;
	    it = _suffixCount.erase( it );
	    _suffixSum.remove( { suffix, category } );
	}
	else
	{
	    ++it;
	}
    }
}


bool FileTypeStats::isCruft( const QString & suffix, const MimeCategory * category ) const
{
    // Unknown category should all have been marked in _otherCategory already
    if ( suffix == NO_SUFFIX || category != _otherCategory )
	return false;

    if ( suffix.contains( ' ' ) )
	return true;

    const int letters = suffix.count( QRegularExpression( "[a-zA-Z]" ) );
    if ( letters == 0 )
	return true;

    // The most common case: 3-letter suffix
    const int len = suffix.size();
    if ( len == 3 && letters == 3 )
	return false;

    const int count = _suffixCount[ { suffix, category } ];
    if ( len > 6 && count < len )
	return true;

    // Forget long suffixes with mostly non-letters
    const float lettersPercent = len > 0 ? (100.0 * letters) / len : 0.0;
    if ( lettersPercent < 70.0 && count < len )
	return true;

    return false;
}


void FileTypeStats::sanityCheck()
{
    FileSize categoryTotal = 0LL;

    foreach ( const FileSize sum, _categorySum )
	categoryTotal += sum;

    const FileSize missing = totalSize() - categoryTotal;

    logDebug() << "Unaccounted in categories: " << formatSize( missing )
	       << " of " << formatSize( totalSize() )
	       << " (" << QString::number( percentage( missing ), 'f', 2 ) << "%)"
	       << Qt::endl;
}
