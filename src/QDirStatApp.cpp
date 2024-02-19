/*
 *   File name: QDirStatApp.cpp
 *   Summary:	QDirStat application class for key objects
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#include <QWidget>
#include <QList>

#include "QDirStatApp.h"
#include "DirTreeModel.h"
#include "DirTree.h"
#include "FileInfoSet.h"
#include "SelectionModel.h"
#include "CleanupCollection.h"
#include "MainWindow.h"
#include "Logger.h"
#include "Exception.h"


using namespace QDirStat;


QDirStatApp * QDirStatApp::instance()
{
    static QDirStatApp _instance;
    return &_instance;
}


DirTree * QDirStatApp::dirTree() const
{
    return _dirTreeModel ? _dirTreeModel->tree() : 0;
}


QDirStatApp::QDirStatApp()
{
    // logDebug() << "Creating app" << Qt::endl;

    _dirTreeModel = new DirTreeModel();
    CHECK_NEW( _dirTreeModel );

    _selectionModel = new SelectionModel( _dirTreeModel );
    CHECK_NEW( _selectionModel );

    _dirTreeModel->setSelectionModel( _selectionModel );

    _cleanupCollection = new CleanupCollection( _selectionModel );
    CHECK_NEW( _cleanupCollection );
}


QDirStatApp::~QDirStatApp()
{
    // logDebug() << "Destroying app" << Qt::endl;

    delete _cleanupCollection;
    delete _selectionModel;
    delete _dirTreeModel;

    // logDebug() << "App destroyed." << Qt::endl;
}


QWidget * QDirStatApp::findMainWindow() const
{
    QWidget * mainWin = 0;
    const QWidgetList toplevel = QApplication::topLevelWidgets();

    for ( QWidgetList::const_iterator it = toplevel.constBegin();
          it != toplevel.constEnd() && ! mainWin;
          ++it )
    {
        mainWin = qobject_cast<MainWindow *>( *it );
    }

    if ( ! mainWin )
        logWarning() << "NULL mainWin for shared instance" << Qt::endl;

    return mainWin;
}


FileInfo * QDirStatApp::root() const
{
    return dirTree() ? dirTree()->firstToplevel() : 0;
}


FileInfo * QDirStatApp::currentItem() const
{
    return _selectionModel->currentItem();
}


FileInfo * QDirStatApp::selectedDirInfo() const
{
    const FileInfoSet selectedItems = _selectionModel->selectedItems();
    FileInfo * sel = selectedItems.first();

    return sel && sel->isDirInfo() ? sel : 0;
}


FileInfo * QDirStatApp::selectedDirInfoOrRoot() const
{
    FileInfo * sel = selectedDirInfo();

    return sel ? sel : root();
}




QDirStatApp * QDirStat::app()
{
    return QDirStatApp::instance();
}
