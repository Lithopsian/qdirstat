/*
 *   File name: SizeColDelegate.h
 *   Summary:	DirTreeView delegate for the size column
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */

#ifndef SizeColDelegate_h
#define SizeColDelegate_h


#include <QStyledItemDelegate>
#include <QTreeView>

#include "DirTreeModel.h"


namespace QDirStat
{
    class DirTreeModel;

    /**
     * Item delegate for the size column in the DirTreeView.
     *
     * This class can handle different font attributes and colors.
     **/
    class SizeColDelegate: public QStyledItemDelegate
    {
	Q_OBJECT

    public:

        /**
         * Constructor.
         **/
	SizeColDelegate( QTreeView * treeView ):
	    QStyledItemDelegate { treeView }
//	    _treeView { treeView },
//	    _model { 0 }
	{}

        /**
         * Destructor.
         **/
        virtual ~SizeColDelegate() {}

	/**
	 * Paint one cell in the view.
	 * Inherited from QStyledItemDelegate.
	 **/
	void paint( QPainter		       * painter,
		    const QStyleOptionViewItem & option,
		    const QModelIndex	       & index ) const Q_DECL_OVERRIDE;

	/**
	 * Return a size hint for one cell in the view.
	 * Inherited from QStyledItemDelegate.
	 **/
	QSize sizeHint( const QStyleOptionViewItem & option,
			const QModelIndex	   & index) const Q_DECL_OVERRIDE;

    protected:

        /**
         * Get the model from a model index and store it in _model.
         **/
//        void ensureModel( const QModelIndex & index ) const;

        /**
         * Get the model from a model index and store it in _model.
         **/
//        static bool isDelegateItem( FileInfo * item );

        /**
         * Determine the color to use for the highlighted (allocated) porition
	 * of the delegate text size string.  This is based on the actual background
	 * colour of the cell, to account for both dark themes and whether the item
	 * is selected.
         **/
	static QColor highlightedText( const QStyleOptionViewItem & option, bool sparseFile );


        //
        // Data members
        //

//        QTreeView *             _treeView;
//        mutable DirTreeModel *  _model;

    };  // class SizeColDelegate

}       // namespace QDirStat

#endif  // SizeColDelegate_h
