/*
 *   File name: HeaderTweaker.cpp
 *   Summary:	Helper class for DirTreeView
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QMenu>
#include <QAction>

#include "HeaderTweaker.h"
#include "DirTreeView.h"
#include "Settings.h"
#include "Logger.h"
#include "Exception.h"
//#include "SignalBlocker.h"

using namespace QDirStat;


HeaderTweaker::HeaderTweaker( QHeaderView * header, DirTreeView * parent ):
    QObject ( parent ),
    _treeView { parent },
    _header { header },
    _currentSection { -1 },
    _currentLayout { 0 }
{
    CHECK_PTR( _header );

//    _header->setSortIndicator( NameCol, Qt::AscendingOrder );
//    _header->setStretchLastSection( false );
    _header->setContextMenuPolicy( Qt::CustomContextMenu );

//    setAllColumnsAutoSize( true );
    createActions();
    createColumnLayouts();

    connect( _header, &QHeaderView::sectionCountChanged,
	     this,    &HeaderTweaker::initHeader );

    connect( _header, &QHeaderView::customContextMenuRequested,
	     this,    &HeaderTweaker::contextMenu );
}


HeaderTweaker::~HeaderTweaker()
{
    if ( _currentLayout )
	saveLayout( _currentLayout );

    writeSettings();

    qDeleteAll( _layouts );
}


void HeaderTweaker::initHeader()
{
    // Initialize stuff when the header actually has sections: It's constructed
    // empty. It is only populated when the tree view model requests header
    // data from the data model.

    // logDebug() << "Header count: " << _header->count() << Qt::endl;
    readSettings();
}


void HeaderTweaker::createColumnLayout( const QString & layoutName)
{
    ColumnLayout * layout = new ColumnLayout( layoutName );
    CHECK_PTR( layout );
    _layouts[ layoutName ] = layout;
}


void HeaderTweaker::createColumnLayouts()
{
    // Layout L1: Short
    createColumnLayout( "L1" );

    // L2: Classic QDirStat Style
    createColumnLayout( "L2" );

    // L3: Full
    createColumnLayout( "L3" );
}


QAction * HeaderTweaker::createAction( const QString & title, void( HeaderTweaker::*slot )( void ) )
{
    QAction * action = new QAction( title, this );
    CHECK_NEW( action );
    connect( action, &QAction::triggered, this, slot );

    return action;
}


void HeaderTweaker::createActions()
{
    _actionHideCurrentCol =
	createAction( tr( "&Hide" ), &HeaderTweaker::hideCurrentCol );

    _actionShowAllHiddenColumns =
	createAction( tr( "Show &All Hidden Columns" ), &HeaderTweaker::showAllHiddenColumns );

    _actionAllColumnsAutoSize =
	createAction( tr( "Auto &Size" ), &HeaderTweaker::setAllColumnsAutoSize );

    _actionAllColumnsInteractiveSize =
	createAction( tr( "&Interactive Size" ), &HeaderTweaker::setAllColumnsInteractiveSize );

    _actionResetToDefaults =
	createAction( tr( "&Reset to Defaults" ), &HeaderTweaker::resetToDefaults );

    _actionAutoSizeCurrentCol =
	createAction( tr( "A&uto Size" ), &HeaderTweaker::autoSizeCurrentCol );
    _actionAutoSizeCurrentCol->setCheckable( true );
}


void HeaderTweaker::updateActions( int section )
{
    _actionHideCurrentCol->setEnabled( section != 0 );

//    SignalBlocker sigBlocker( _actionAutoSizeCurrentCol ); // this doesn't fire triggered()
    _actionAutoSizeCurrentCol->setChecked( autoSizeCol( section ) );
}


void HeaderTweaker::contextMenu( const QPoint & pos )
{
    _currentSection = _header->logicalIndexAt( pos );
    QString colName = this->colName( _currentSection );
    updateActions( _currentSection );

    QMenu menu;
//    menu.addAction( tr( "Column \"%1\"" ).arg( colName ) );
//    menu.addSeparator();
    menu.addAction( _actionAutoSizeCurrentCol );
    menu.addAction( _actionHideCurrentCol     );
    _actionHideCurrentCol->setText( QString( "&Hide \"%1\"" ).arg( colName ) );
    menu.addSeparator();
    menu.addMenu( createHiddenColMenu( &menu ) );

    QMenu allColMenu( tr( "&All Columns" ) );
    menu.addMenu( &allColMenu );
    allColMenu.addAction( _actionAllColumnsAutoSize );
    allColMenu.addAction( _actionAllColumnsInteractiveSize );
    allColMenu.addAction( _actionResetToDefaults );

    menu.exec( _header->mapToGlobal( pos ) );
}


QMenu * HeaderTweaker::createHiddenColMenu( QWidget * parent )
{
    int actionCount = 0;
    QMenu * hiddenColMenu = new QMenu( tr( "Hi&dden Columns" ), parent );

    for ( int section = 0; section < _header->count(); ++section )
    {
	if ( _header->isSectionHidden( section ) )
	{
	    const QString text = tr( "Show Column \"%1\"" ).arg( this->colName( section ) );
	    QAction * showAction = createAction( text, &HeaderTweaker::showHiddenCol );
	    showAction->setData( section );
	    hiddenColMenu->addAction( showAction );
	    ++actionCount;
	}
    }

    if ( actionCount == 0 )
    {
	hiddenColMenu->setEnabled( false );
    }
    else if ( actionCount > 1 )
    {
	hiddenColMenu->addSeparator();
	hiddenColMenu->addAction( _actionShowAllHiddenColumns );
    }

    return hiddenColMenu;
}


QString HeaderTweaker::colName( int section ) const
{
    const DataColumn col = DataColumns::instance()->reverseMappedCol( static_cast<DataColumn>( section ) );
    const QString name = _treeView->model()->headerData( col, Qt::Horizontal, Qt::DisplayRole ).toString();
    if ( col == UndefinedCol )
	logError() << "No column at section " << section << Qt::endl;

    return name;
}


void HeaderTweaker::hideCurrentCol()
{
    if ( _currentSection >= 0 )
    {
	logDebug() << "Hiding column \"" << colName( _currentSection ) << "\"" << Qt::endl;
	_header->setSectionHidden( _currentSection, true );
    }

    _currentSection = -1;
}


void HeaderTweaker::autoSizeCurrentCol()
{
    if ( _currentSection >= 0 )
    {
	setResizeMode( _currentSection,
		       _actionAutoSizeCurrentCol->isChecked() ?
		       QHeaderView::ResizeToContents :
		       QHeaderView::Interactive );
    }
    else
	logWarning() << "No current section" << Qt::endl;

    _currentSection = -1;
}


void HeaderTweaker::setAllColumnsAutoSize( bool autoSize )
{
    const QHeaderView::ResizeMode resizeMode =
	autoSize ? QHeaderView::ResizeToContents : QHeaderView::Interactive;

    for ( int section = 0; section < _header->count(); ++section )
	setResizeMode( section, resizeMode );
}


void HeaderTweaker::setAllColumnsAutoSize()
{
    setAllColumnsAutoSize( true );
}


void HeaderTweaker::setAllColumnsInteractiveSize()
{
    setAllColumnsAutoSize( false );
}


void HeaderTweaker::showHiddenCol()
{
    QAction * action = qobject_cast<QAction *>( sender() );

    if ( ! action )
    {
	logError() << "Wrong sender type: " << sender()->metaObject()->className() << Qt::endl;
	return;
    }

    if ( action->data().isValid() )
    {
	const int section = action->data().toInt();
	if ( section >= 0 && section < _header->count() )
	{
	    logDebug() << "Showing column \"" << colName( section ) << "\"" << Qt::endl;
	    _header->setSectionHidden( section, false );
	}
	else
	    logError() << "Section index out of range: " << section << Qt::endl;
    }
    else
    {
	logError() << "No data() set for this QAction" << Qt::endl;
    }
}


void HeaderTweaker::showAllHiddenColumns()
{
    logDebug() << "Showing all columns" << Qt::endl;

    for ( int section = 0; section < _header->count(); ++section )
    {
	if ( _header->isSectionHidden( section ) )
	    _header->setSectionHidden( section, false );
    }
}


void HeaderTweaker::resetToDefaults()
{
    if ( _currentLayout )
    {
	_currentLayout->columns = _currentLayout->defaultColumns();
	applyLayout( _currentLayout );
    }
}


void HeaderTweaker::setColumnOrder( const DataColumnList & columns )
{
    DataColumnList colOrderList = columns;
    addMissingColumns( colOrderList );

    int visualIndex = 0;

    for ( DataColumn col : colOrderList )
    {
	if ( visualIndex < _header->count() )
	{
	    // logDebug() << "Moving " << col << " to position " << visualIndex << Qt::endl;
	    _header->moveSection( _header->visualIndex( col ), visualIndex++ );
	}
	else
	    logWarning() << "More columns than header sections" << Qt::endl;
    }
}


void HeaderTweaker::readSettings()
{
    Settings settings;
    settings.beginGroup( "TreeViewColumns" );

    //
    // Set all column widths that are specified
    //

    for ( int section = 0; section < _header->count(); ++section )
    {
	const DataColumn col	= static_cast<DataColumn>( section );
	const QString colName	= DataColumns::toString( col );

	const int width = settings.value( "Width_" + colName, -1 ).toInt();

	if ( width > 0 )
	{
	    setResizeMode( section, QHeaderView::Interactive );
	    _header->resizeSection( section, width );
	}
	else
	{
	    setResizeMode( section, QHeaderView::ResizeToContents );
	}
    }

    settings.endGroup();

    for ( ColumnLayout * layout : qAsConst( _layouts ) )
	readLayoutSettings( layout );
}


void HeaderTweaker::readLayoutSettings( ColumnLayout * layout )
{
    CHECK_PTR( layout );

    Settings settings;
    settings.beginGroup( "TreeViewLayout_" + layout->name );

    QStringList strList = settings.value( "Columns" ).toStringList();
    layout->columns = DataColumns::fromStringList( strList );

    fixupLayout( layout );

    settings.endGroup();
}


void HeaderTweaker::writeSettings()
{
    Settings settings;
    settings.beginGroup( "TreeViewColumns" );

    // Remove any leftovers from old config file versions
    settings.remove( "" ); // Remove all keys in this settings group

    // Save column widths
    for ( int visualIndex = 0; visualIndex < _header->count(); ++visualIndex )
    {
	const int logicalIndex = _header->logicalIndex( visualIndex );
	const DataColumn col	 = static_cast<DataColumn>( logicalIndex );
	const QString widthKey = "Width_" + DataColumns::toString( col );

	if ( autoSizeCol( logicalIndex ) )
	    settings.setValue( widthKey, "auto" );
	else
	    settings.setValue( widthKey, _header->sectionSize( logicalIndex ) );
    }

    settings.endGroup();

    // Save column layouts
    for ( ColumnLayout * layout : qAsConst( _layouts ) )
	writeLayoutSettings( layout );
}


void HeaderTweaker::writeLayoutSettings( ColumnLayout * layout )
{
    CHECK_PTR( layout );

    Settings settings;
    settings.beginGroup( "TreeViewLayout_" + layout->name );
    settings.setValue( "Columns", DataColumns::toStringList( layout->columns ) );
    settings.endGroup();
}


void HeaderTweaker::setColumnVisibility( const DataColumnList & columns )
{
    for ( int section = 0; section < _header->count(); ++section )
    {
	const DataColumn col = static_cast<DataColumn>( section );
	_header->setSectionHidden( DataColumns::toViewCol( section ), !columns.contains( col ) );
    }
}


void HeaderTweaker::addMissingColumns( DataColumnList & colList )
{
    for ( const DataColumn col : DataColumns::instance()->defaultColumns() )
    {
	if ( ! colList.contains( col ) )
	     colList << col;
    }
}


void HeaderTweaker::changeLayout( const QString & name )
{
    if ( ! _layouts.contains( name ) )
    {
	logError() << "No layout " << name << Qt::endl;
	return;
    }

    // logDebug() << "Changing to layout " << name << Qt::endl;

    if ( _currentLayout )
	saveLayout( _currentLayout );

    _currentLayout = _layouts[ name ];
    applyLayout( _currentLayout );
}


void HeaderTweaker::saveLayout( ColumnLayout * layout )
{
    CHECK_PTR( layout );

    layout->columns.clear();

    for ( int visualIndex = 0; visualIndex < _header->count(); ++visualIndex )
    {
	const int logicalIndex = _header->logicalIndex( visualIndex );
	const DataColumn col   = static_cast<DataColumn>( logicalIndex );

	if ( ! _header->isSectionHidden( logicalIndex ) )
	    layout->columns << col;
    }
}


void HeaderTweaker::applyLayout( ColumnLayout * layout )
{
    CHECK_PTR( layout );

    fixupLayout( layout );
    setColumnOrder( layout->columns );
    setColumnVisibility( layout->columns );
}


void HeaderTweaker::fixupLayout( ColumnLayout * layout )
{
    CHECK_PTR( layout );

    if ( layout->columns.isEmpty() )
    {
	logDebug() << "Falling back to default visible columns" << Qt::endl;
	layout->columns = layout->defaultColumns();
    }

    DataColumns::ensureNameColFirst( layout->columns );
}


void HeaderTweaker::setResizeMode( int section, QHeaderView::ResizeMode resizeMode )
{
    _header->setSectionResizeMode( section, resizeMode );
}


void HeaderTweaker::resizeToContents( QHeaderView * header )
{
    for ( int col = 0; col < header->count(); ++col )
	header->setSectionResizeMode( col, QHeaderView::ResizeToContents );
}



DataColumnList ColumnLayout::defaultColumns( const QString & layoutName )
{
    if ( layoutName == "L1" )
	return { NameCol,
                 PercentBarCol,
                 PercentNumCol,
                 SizeCol,
                 LatestMTimeCol
	       };

    if ( layoutName == "L2" )
	return { NameCol,
		 PercentBarCol,
		 PercentNumCol,
		 SizeCol,
		 TotalItemsCol,
		 TotalFilesCol,
		 TotalSubDirsCol,
		 LatestMTimeCol
	       };

    return DataColumns::instance()->allColumns();
}
