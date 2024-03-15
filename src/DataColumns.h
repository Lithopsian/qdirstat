    /*
 *   File name: DataColumns.h
 *   Summary:	Data column mapping
 *   License:	GPL V2 - See file LICENSE for details.
 *
 *   Author:	Stefan Hundhammer <Stefan.Hundhammer@gmx.de>
 */


#ifndef DataColumns_h
#define DataColumns_h

#include <QList>
#include <QString>
#include <QStringList>
#include <QTextStream>


namespace QDirStat
{
    /**
     * Data columns for data model, view and sorting.
     **/
    enum DataColumn
    {
	NameCol	       = 0,	// File / directory name
	PercentBarCol,		// Graphical percentage bar
	PercentNumCol,		// Numeric percentage Value
	SizeCol,		// size (subtree or own size for files)
	TotalItemsCol,		// Total number of items    in subtree
	TotalFilesCol,		// Total number of files    in subtree
	TotalSubDirsCol,	// Total number of subdirs  in subtree
	LatestMTimeCol,		// Latest modification time in subtree
        OldestFileMTimeCol,     // mtime of the oldest file in subtree
        UserCol,                // User (owner)
        GroupCol,               // Group
        PermissionsCol,         // Permissions (symbolic; -rwxrxxrwx)
        OctalPermissionsCol,    // Permissions (octal; 0644)
	ReadJobsCol,		// Number of pending read jobs in subtree
	UndefinedCol
    };

    // For use in loops
#define DataColumnBegin NameCol
#define DataColumnEnd	UndefinedCol


    typedef QList<DataColumn> DataColumnList;


    /**
     * Singleton class for data columns.
     *
     * This class keeps track what columns should be displayed and in what
     * order and how to map view columns to model columns and vice versa.
     *
     * The model uses the DataColumn enum internally. For the view, however,
     * the columns may be rearranged, and any column may be omitted (not
     * displayed at all). This class handles that mapping.
     **/
    class DataColumns
    {

	/**
	 * Constructor. This is not meant for general use; use
	 * the static methods instead.
	 **/
	DataColumns();

	/**
	 * Return the singleton instance for this class. This will create the
	 * singleton upon the first call.  Provate, use the static methods.
	 **/
	static DataColumns * instance();

    public:

	/**
	 * Map a view column to the corresponding model column.
	 * (static version)
	 **/
	static DataColumn fromViewCol( int viewCol )
	    { return instance()->mappedCol( static_cast<DataColumn>( viewCol ) ); }

	/**
	 * Map a model column to the corresponding view column.
	 * (static version)
	 **/
	static DataColumn toViewCol( int modelCol )
	    { return instance()->reverseMappedCol( static_cast<DataColumn>( modelCol ) ); }

	/**
	 * Return the number of columns that are currently displayed.
	 **/
	static int colCount()
	    { return instance()->_columns.size(); }

	/**
	 * Set the column order and what columns to display.
	 *
	 * Example:
	 *
	 *   DataColumnList col;
	 *   col << QDirStat::NameCol,
	 *	 << QDirStat::PercentBarCol,
	 *	 << QDirStat::PercentNumCol,
	 *	 << QDirStat::SizeCol;
	 *   DataColumns::setColumns( col );
	 *
	 * NOTICE: If a data view is active, use DirTreeModel::setColumns()
	 * instead (which will call this function in turn) so the view is
	 * correctly notified about this change.
	 *
	 * This will emit a columnsChanged() signal.
	 */
	static void setColumns( const DataColumnList & columns )
	    { instance()->instanceSetColumns( columns ); }

	/**
	 * Return the model columns that are currently being displayed.
	 **/
//	const DataColumnList & columns() const { return _columns; }

	/**
	 * Return the default model columns.
	 **/
	static const DataColumnList defaultColumns();

        /**
         * Return all model columns in default order.
         **/
	static const DataColumnList allColumns() { return defaultColumns(); }

	/**
	 * Convert a column to string.
	 **/
	static QString toString( DataColumn col );

	/**
	 * Convert string to column.
	 **/
	static DataColumn fromString( const QString & str );

	/**
	 * Convert a list of columns to a string list.
	 **/
	static QStringList toStringList( const DataColumnList & colList );

	/**
	 * convert a string list to a list of columns.
	 **/
	static DataColumnList fromStringList( const QStringList & strList );

	/**
	 * Ensure that NameCol at the first position of colList.
	 **/
	static void ensureNameColFirst( DataColumnList & colList );


    public slots:

	/**
	 * Read parameters from the settings file.
	 **/
	void readSettings();

	/**
	 * Write parameters to the settings file.
	 **/
	void writeSettings();


    protected:

	/**
	 * Set the columns.  This is protected, use the static function
	 * setColumns() instead.
	 **/
	void instanceSetColumns( const DataColumnList & columns );

	/**
	 * Map a view column to the corresponding model column.
	 **/
	DataColumn mappedCol( DataColumn viewCol ) const;

	/**
	 * Map a model column to the corresponding view column.
	 **/
	DataColumn reverseMappedCol( DataColumn modelCol ) const;


	// Data members

	DataColumnList _columns;

    };	// class DataColumns


    /**
     * Print a DataColumn in text form to a debug stream.
     **/
    inline QTextStream & operator<< ( QTextStream & stream, DataColumn col )
    {
	stream << DataColumns::toString( col );

	return stream;
    }


    /**
     * Print a DataColumn in text form to a debug stream.
     **/
    inline QTextStream & operator<< ( QTextStream &          stream,
                                      const DataColumnList & colList )
    {
	stream << "[ "
               << DataColumns::toStringList( colList ).join( ", " )
               << " ]";

	return stream;
    }


}	// namespace QDirStat

#endif	// DataColumns_h
