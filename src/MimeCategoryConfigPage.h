/*
 *   File name: MimeCategoryConfigPage.h
 *   Summary:	QDirStat configuration dialog classes
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#ifndef MimeCategoryConfigPage_h
#define MimeCategoryConfigPage_h

#include "ListEditor.h"
#include "ui_mime-category-config-page.h"


namespace QDirStat

{
    class ConfigDialog;

    /**
     * Configuration page (tab) for MimeCategories:
     * Edit, add, delete categories in the MimeCategorizer.
     * A working set of new MimeCategory objects is copied from the
     * live categories and used to populate the list widget.
     **/
    class MimeCategoryConfigPage: public ListEditor
    {
	Q_OBJECT

    public:

	MimeCategoryConfigPage( ConfigDialog * parent );
	virtual ~MimeCategoryConfigPage();


    protected slots:

	/**
	 * Write changes back to the settings.
	 **/
	void applyChanges();

	/**
	 * Abandon changes and revert everything to the original settings.
	 **/
//	void discardChanges();

	/**
	 * Create a new list item.  Overload of ListEditor::add() to allow
	 * detection of new insertions for sorting and setting focus when
	 * new items are added.
	 *
	 * A sorted QListWidget does not behave well with items that have the
	 * same sort key, in this case the category name.  So the category list
	 * is configured to be unsorted and is then sorted explicitly whenever
	 * a sort key changes, including inserting a new category.
	 **/
	virtual void add() Q_DECL_OVERRIDE;

	/**
	 * Notification that the user changed the "Name" field of the
	 * current category.
	 **/
	void nameChanged( const QString & newName );

	/**
	 * Notification that the user changed the "Color" field of the
	 * current category.
	 **/
	void categoryColorChanged( const QString & newColor );

	/**
	 * Open a color dialog and let the user pick a color for the current
	 * category.
	 **/
	void pickCategoryColor();

	/**
	 * Notification that the user changed the fixed tile color.
	 **/
	void tileColorChanged( const QString & newColor );

	/**
	 * Open a color dialog and let the user pick a fixed color for
	 * the tiles.
	 **/
	void pickTileColor();

	/**
	 * Set the other widgets when the cushion shading checkbox is changed.
	 **/
	void cushionShadingChanged( int state );

	/**
	 * Updates the treemapView when something changes in the configuration.
	 **/
	void configChanged();

	/**
	 * The category list has been resized.  Adjust the colour shading.
	 **/
	virtual void resizeEvent( QResizeEvent * ) Q_DECL_OVERRIDE { adjustShadingWidth(); }

	/**
	 * The category list has been shown.  Adjust the colour shading.
	 **/
	virtual void showEvent( QShowEvent * ) Q_DECL_OVERRIDE { adjustShadingWidth(); }

	/**
	 * The pane splitter has moved, meaning the list has resized without
	 * a resize event.  Adjust the sgading width.
	 **/
	void splitterMoved( int, int ) { adjustShadingWidth(); }

	/**
	 * Process the action to toggle the colour previews.
	 **/
	void colourPreviewsTriggered( bool );

	/**
	 * Process the action to add a new category.
	 **/
	void addTriggered( bool checked );

	/**
	 * Process the action to remove a category.
	 **/
	void removeTriggered( bool checked );


    protected:

	/**
	 * Populate the widgets.
	 **/
	void setup();

	/**
	 * Fill the category list widget from the category collection.
	 *
	 * Implemented from ListEditor.
	 **/
	virtual void fillListWidget() Q_DECL_OVERRIDE;

	/**
	 * The category list has been shown.  Adjust the colour shading.
	 **/
	void adjustShadingWidth();

	/**
	 * Set the background shading of a list item.
	 **/
	void setBackground( QListWidgetItem * item );

	/**
	 * Save the patterns from the dialog to the specified category.
	 * The name and colour are handled in real-time as they are edited.
	 **/
	virtual void save( void * value ) Q_DECL_OVERRIDE;

	/**
	 * Load the fields from the specified category into the dialog.
	 **/
	virtual void load( void * value ) Q_DECL_OVERRIDE;

	/**
	 * Create a new value with default values.
	 * This is called when the 'Add' button is clicked.
	 *
	 * Implemented from ListEditor.
	 **/
	virtual void * createValue() Q_DECL_OVERRIDE;

	/**
	 * Remove a value from the internal list and delete it.
	 *
	 * This is called when the 'Remove' button is clicked.
	 *
	 * Implemented from ListEditor.
	 **/
	virtual void removeValue( void * value );

	/**
	 * Return a text for the list item of 'value'.
	 *
	 * Implemented from ListEditor.
	 **/

	virtual QString valueText( void * value ) Q_DECL_OVERRIDE;

	/**
	 * Signal handler for a change in the list widget current item.
	 *
	 * Implemented from ListEditor.
	 **/
	virtual void currentItemChanged( QListWidgetItem * current, QListWidgetItem * previous) Q_DECL_OVERRIDE;

	/**
	 * Update actions to match the current item properties.
	 *
	 * Implemented from ListEditor.
	 **/
	virtual void updateActions() Q_DECL_OVERRIDE;

	/**
	 * Set the remove button, name, and patterns enabled or disabled,
	 * based on the name of the current category item.
	 *
	 * Called by currentItemChanged() and updateActions().
	 **/
	void setActions( const QListWidgetItem * currentItem );

	/**
	 * Convert 'patternList' into a newline-separated string and set it as
	 * text of 'textEdit'.
	 **/
	void setPatternList( QPlainTextEdit * textEdit, const QStringList & patternList );

	/**
	 * Add demo content to the tremap view.
	 **/
	void populateTreemapView();

	/**
	 * Handle a right click.
	 **/
	virtual void contextMenuEvent( QContextMenuEvent * event ) Q_DECL_OVERRIDE;


	// Data

	Ui::MimeCategoryConfigPage * _ui;
	DirTree			   * _dirTree;

	bool _dirty;

    };	// class MimeCategoryConfigPage

}	// namespace QDirStat

#endif	// MimeCategoryConfigPage_h
