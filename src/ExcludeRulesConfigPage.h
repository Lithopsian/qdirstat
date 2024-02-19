/*
 *   File name: ExcludeRulesConfigPage.h
 *   Summary:	QDirStat configuration dialog classes
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#ifndef ExcludeRulesConfigPage_h
#define ExcludeRulesConfigPage_h

#include "ListEditor.h"
#include "ui_exclude-rules-config-page.h"


namespace QDirStat
{
	class ExcludeRule;

    /**
     * Configuration page (tab) for exclude rules:
     * Edit, add, delete, and reorder exclude rules.
     **/
    class ExcludeRulesConfigPage: public ListEditor
    {
	Q_OBJECT

    public:

	ExcludeRulesConfigPage( QWidget * parent = 0 );
	virtual ~ExcludeRulesConfigPage();

    protected slots:
	/**
	 * Create a new list item.  Overload of ListEditor::add() to allow
	 * detection of new insertions so that focus can be put in the only
	 * sensible place.
	 **/
	virtual void add() Q_DECL_OVERRIDE;

	/**
	 * Populate the widgets.
	 **/
	void setup();

	/**
	 * Write changes back to the settings.
	 **/
	void applyChanges();

	/**
	 * Abandon changes and revert everything to the original settings.
	 **/
	void discardChanges();

	/**
	 * Notification that the user changed the "Pattern" field of the
	 * current exclude rule.
	 **/
	void patternChanged( const QString & newPattern );

    protected:

        /**
         * Enable or disable the widgets to edit an exclude rule.
         **/
        void enableEditRuleWidgets( bool enable );

	/**
	 * Fill the exclude rule list widget from the ExcludeRules.
	 *
	 * Implemented from ListEditor.
	 **/
	virtual void fillListWidget() Q_DECL_OVERRIDE;

	/**
	 * Save the contents of the widgets to the specified value.
	 *
	 * Implemented from ListEditor.
	 **/
	virtual void save( void * value ) Q_DECL_OVERRIDE;

	/**
	 * Load the content of the widgets from the specified value.
	 *
	 * Implemented from ListEditor.
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
	 * This is called when the 'Remove' button is clicked and the user
	 * confirms the confirmation pop-up.
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
	 * Return the message for the 'really delete?' message for the current
	 * item ('value'). If this returns an empty string, the item cannot be
	 * deleted.
	 *
	 * Implemented from ListEditor.
	 **/
//	virtual QString deleteConfirmationMessage( void * value ) Q_DECL_OVERRIDE;

	/**
	 * Clean up the working list of rules when the dialog is being closed.
	 **/
//	void finished( int result );

	/**
	 * Comparison operator for two exclude rules.
	 **/
	static bool equal ( const ExcludeRule * rule1, const ExcludeRule * rule2 );


	//
	// Data members
	//

	Ui::ExcludeRulesConfigPage * _ui;

    };	// class ExcludeRulesConfigPage

}	// namespace QDirStat

#endif	// ExcludeRulesConfigPage_h
