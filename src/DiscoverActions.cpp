/*
 *   File name: DiscoverActions.cpp
 *   Summary:	Actions for QDirStat
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "DiscoverActions.h"
#include "BusyPopup.h"
#include "DirInfo.h"
#include "DirTree.h"
#include "FileSearchFilter.h"
#include "FormatUtil.h"
#include "LocateFilesWindow.h"
#include "QDirStatApp.h"
#include "TreeWalker.h"
//#include "Logger.h"
//#include "Exception.h"

using namespace QDirStat;


void DiscoverActions::discoverLargestFiles()
{
    discoverFiles( new QDirStat::LargestFilesTreeWalker(), tr( "Largest files in %1" ) );
    _locateFilesWindow->sortByColumn( LocateListSizeCol, Qt::DescendingOrder );
}


void DiscoverActions::discoverNewestFiles()
{
    discoverFiles( new QDirStat::NewFilesTreeWalker(), tr( "Newest files in %1" ) );
    _locateFilesWindow->sortByColumn( LocateListMTimeCol, Qt::DescendingOrder );
}


void DiscoverActions::discoverOldestFiles()
{
    discoverFiles( new QDirStat::OldFilesTreeWalker(), tr( "Oldest files in %1" ) );
    _locateFilesWindow->sortByColumn( LocateListMTimeCol, Qt::AscendingOrder );
}


void DiscoverActions::discoverHardLinkedFiles()
{
    discoverFiles( new QDirStat::HardLinkedFilesTreeWalker(), tr( "Files with multiple hard links in %1" ) );
    _locateFilesWindow->sortByColumn( LocateListPathCol, Qt::AscendingOrder );
}


void DiscoverActions::discoverBrokenSymLinks()
{
    BusyPopup msg( tr( "Checking symlinks..." ), app()->findMainWindow() );
    discoverFiles( new QDirStat::BrokenSymLinksTreeWalker(), tr( "Broken symbolic links in %1" ) );
    _locateFilesWindow->sortByColumn( LocateListPathCol, Qt::AscendingOrder );
}


void DiscoverActions::discoverSparseFiles()
{
    discoverFiles( new QDirStat::SparseFilesTreeWalker(), tr( "Sparse files in %1" ) );
    _locateFilesWindow->sortByColumn( LocateListSizeCol, Qt::DescendingOrder );
}


void DiscoverActions::discoverFilesFromYear( const QString & path, short year )
{
    const QString headingText = tr( "Files from %1 in %2" ).arg( year );
    discoverFiles( new QDirStat::FilesFromYearTreeWalker( year ), headingText, path );
    _locateFilesWindow->sortByColumn( LocateListMTimeCol, Qt::DescendingOrder );
}


void DiscoverActions::discoverFilesFromMonth( const QString & path, short year, short month )
{
    const QString headingText = tr( "Files from %1 %2 in %3" ).arg( monthAbbreviation( month ) ).arg( year);
    discoverFiles( new QDirStat::FilesFromMonthTreeWalker( year, month ), headingText, path );
    _locateFilesWindow->sortByColumn( LocateListMTimeCol, Qt::DescendingOrder );
}


void DiscoverActions::discoverFiles( TreeWalker *    treeWalker,
                                     const QString & headingText,
                                     const QString & path         )
{
    ensureLocateFilesWindow( treeWalker );
    FileInfo * sel = 0;

    if ( !path.isEmpty() )
    {
        sel = app()->dirTree()->locate( path,
                                        true ); // findPseudoDirs
    }

    if ( !sel )
        sel = app()->selectedDirInfoOrRoot();

    if ( sel )
    {
        if ( !headingText.isEmpty() )
            _locateFilesWindow->setHeading( headingText.arg( sel->url() ) );

        _locateFilesWindow->populate( sel );
        _locateFilesWindow->show();
    }
}


void DiscoverActions::ensureLocateFilesWindow( TreeWalker * treeWalker )
{
    if ( _locateFilesWindow )
        _locateFilesWindow->setTreeWalker( treeWalker );
    else
	// This deletes itself when the user closes it. The associated QPointer
	// keeps track of that and sets the pointer to 0 when it happens.
	_locateFilesWindow = new LocateFilesWindow( treeWalker,
                                                    app()->findMainWindow() ); // parent
}


void DiscoverActions::findFiles( const FileSearchFilter & filter )
{
    ensureLocateFilesWindow( new FindFilesTreeWalker( filter ) );
    FileInfo * sel = filter.subtree();

    if ( ! sel )
        sel = app()->selectedDirInfoOrRoot();

    if ( sel )
    {
        _locateFilesWindow->setHeading( tr( "Search results for '%1'" ).arg( filter.pattern() ) );
        _locateFilesWindow->populate( sel );
        _locateFilesWindow->show();
    }
}
