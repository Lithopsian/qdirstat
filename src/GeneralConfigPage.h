/*
 *   File name: GeneralConfigPage.h
 *   Summary:	QDirStat configuration dialog classes
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#ifndef GeneralConfigPage_h
#define GeneralConfigPage_h

#include "ui_general-config-page.h"


namespace QDirStat
{
    class GeneralConfigPage: public QWidget
    {
        Q_OBJECT

    public:

        /**
         * Constructor.
         **/
        GeneralConfigPage( QWidget * parent = 0 );

        /**
         * Destructor.
         **/
        virtual ~GeneralConfigPage();


    public slots:

	/**
	 * Populate the widgets from the values held in MainWindow and DirTreeModel.
	 **/
	void setup();

	/**
	 * Apply changes to the settings.  The values are sent directly to MainWindow
	 * and DirTreeModel, the two classes that use the settings on this page.
	 **/
	void applyChanges();

	/**
	 * Abandon changes.  Currently a no-op since all changes are local until applied.
	 **/
	void discardChanges();


    protected:

	//
	// Data members
	//

	Ui::GeneralConfigPage * _ui;

    }; // class GeneralConfigPage
}

#endif // GeneralConfigPage_h
