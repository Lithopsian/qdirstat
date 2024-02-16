/*
 *   File name: MainWindowLayout.cpp
 *   Summary:	QDirStat main window layout-related functions
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include "MainWindow.h"
#include "QDirStatApp.h"
#include "HeaderTweaker.h"
#include "Settings.h"
#include "Exception.h"
#include "Logger.h"

using namespace QDirStat;


void MainWindow::initLayouts()
{
    // Qt Designer does not support QActionGroups; it was there for Qt 3, but
    // they dropped that feature for Qt 4/5.
    _layoutActionGroup = new QActionGroup( this );
    CHECK_NEW( _layoutActionGroup );

    // Notice that the column layouts are handled in the HeaderTweaker and its
    // ColumnLayout helper class; see also HeaderTweaker.h and .cpp.
    //
    // The layout names "L1", "L2", "L3" here are important: They need to match
    // the names in the HeaderTweaker.
    initLayout( "L1", _ui->actionLayout1 );
    initLayout( "L2", _ui->actionLayout2 );
    initLayout( "L3", _ui->actionLayout3 );
}


void MainWindow::initLayout( const QString & layoutName, QAction * action )
{
    TreeLayout * layout = new TreeLayout( layoutName );
    CHECK_NEW( layout );

    _layouts[ layoutName ] = layout;

    _layoutActionGroup->addAction( action );
    action->setData( layoutName );
    if ( layoutName == currentLayoutName() )
	action->setChecked( true );
}


void MainWindow::changeLayoutSlot()
{
    // Get the layout to use from data() from the QAction that sent the signal.
    QAction * action   = qobject_cast<QAction *>( sender() );
    const QString layoutName = action && action->data().isValid() ? action->data().toString() : "L2";
    changeLayout( layoutName );
}


void MainWindow::changeLayout( const QString & name )
{
    logDebug() << "Changing to layout " << name << Qt::endl;

    _ui->dirTreeView->headerTweaker()->changeLayout( name );

    if ( _currentLayout )
	saveLayout( _currentLayout );

    if ( _layouts.contains( name ) )
    {
	_currentLayout = _layouts[ name ];
	applyLayout( _currentLayout );
    }
    else
    {
	logError() << "No layout " << name << Qt::endl;
    }
}


void MainWindow::saveLayout( TreeLayout * layout )
{
    CHECK_PTR( layout );

    layout->_showDetailsPanel = _ui->actionShowDetailsPanel->isChecked();
}


void MainWindow::applyLayout( const TreeLayout * layout )
{
    CHECK_PTR( layout );

    _ui->actionShowDetailsPanel->setChecked( layout->_showDetailsPanel );
}


void MainWindow::readLayoutSettings( TreeLayout * layout )
{
    CHECK_PTR( layout );

    Settings settings;
    settings.beginGroup( QString( "TreeViewLayout_%1" ).arg( layout->_name ) );
    layout->_showDetailsPanel = settings.value( "ShowDetailsPanel", layout->_showDetailsPanel ).toBool();
    settings.endGroup();
}


void MainWindow::writeLayoutSettings( const TreeLayout * layout )
{
    CHECK_PTR( layout );

    Settings settings;
    settings.beginGroup( QString( "TreeViewLayout_%1" ).arg( layout->_name ) );
    settings.setValue( "ShowDetailsPanel", layout->_showDetailsPanel );
    settings.endGroup();
}

