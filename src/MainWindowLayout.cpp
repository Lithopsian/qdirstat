/*
 *   File name: MainWindowLayout.cpp
 *   Summary:	QDirStat main window layout-related functions
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#include <QActionGroup>

#include "MainWindow.h"
#include "QDirStatApp.h"
#include "HeaderTweaker.h"
#include "Settings.h"
#include "Exception.h"
#include "Logger.h"

using namespace QDirStat;


QString MainWindow::layoutName( const QAction * action ) const
{
    if ( action == _ui->actionLayout1 ) return "L1";
    if ( action == _ui->actionLayout2 ) return "L2";
    if ( action == _ui->actionLayout3 ) return "L3";
    return QString();
}


QAction * MainWindow::layoutAction( const QString & layoutName ) const
{
    if ( layoutName == "L1" ) return _ui->actionLayout1;
    if ( layoutName == "L2" ) return _ui->actionLayout2;
    if ( layoutName == "L3" ) return _ui->actionLayout3;
    return 0;
}


QString MainWindow::currentLayoutName() const
{
    return layoutName( _layoutActionGroup->checkedAction() );
}


void MainWindow::initLayouts( const QString & currentLayoutName )
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
    initLayout( "L1", currentLayoutName );
    initLayout( "L2", currentLayoutName );
    initLayout( "L3", currentLayoutName );
}


void MainWindow::initLayout( const QString & layoutName, const QString & currentLayoutName )
{
    readLayoutSetting( layoutName );

    QAction * action = layoutAction( layoutName );
    _layoutActionGroup->addAction( action );

    if ( layoutName == currentLayoutName )
    {
	action->setChecked( true );
	changeLayout( layoutName ); // setChecked() doesn't fire triggered() and it isn't connected yet anyway
    }
}


void MainWindow::changeLayoutSlot()
{
    // Get the layout to use from data() from the QAction that sent the signal.
    const QAction * action   = qobject_cast<QAction *>( sender() );
    changeLayout( layoutName( action ) );
}


void MainWindow::changeLayout( const QString & name )
{
    //logDebug() << "Changing to layout " << name << Qt::endl;

    _ui->dirTreeView->headerTweaker()->changeLayout( name );

    QAction * action = layoutAction( name );
    if ( action )
	_ui->actionShowDetailsPanel->setChecked( action->data().toBool() );
    else
	logError() << "No layout " << name << Qt::endl;
}


void MainWindow::updateLayout( bool detailsPanelVisible )
{
    _ui->fileDetailsPanel->setVisible( detailsPanelVisible );
    _layoutActionGroup->checkedAction()->setData( detailsPanelVisible );
}


void MainWindow::readLayoutSetting( const QString & layoutName )
{
    Settings settings;
    settings.beginGroup( "TreeViewLayout_" + layoutName );
    layoutAction( layoutName )->setData( settings.value( "ShowDetailsPanel", false ).toBool() );
    settings.endGroup();
}


void MainWindow::writeLayoutSetting( const QAction * action )
{
    Settings settings;
    settings.beginGroup( "TreeViewLayout_" + layoutName( action ) );
    settings.setValue( "ShowDetailsPanel", action->data().toBool() );
    settings.endGroup();
}


void MainWindow::writeLayoutSettings()
{
    writeLayoutSetting( _ui->actionLayout1 );
    writeLayoutSetting( _ui->actionLayout2 );
    writeLayoutSetting( _ui->actionLayout3 );
}

