// SessionMessagesModel.h

// Declares the SessionMessagesModel class representing the QT model for displaying log messages





#ifndef SESSIONMESSAGESMODEL_H
#define SESSIONMESSAGESMODEL_H





#include <memory>
#include <set>
#include <QAbstractTableModel>
#include "LogFile.h"





// fwd:
class Session;
typedef std::shared_ptr<Session> SessionPtr;





class SessionMessagesModel:
	public QAbstractTableModel
{
	Q_OBJECT

public:

	/** Column indices, must match labels in headerData(). */
	enum
	{
		colDateTime = 0,
		colLogLevel = 1,
		colThreadID = 2,
		colSource   = 3,
		colModule   = 4,
		colText     = 5,

		colMax,  // Used for column count
	};

	/** Custom data roles returnable by data(). */
	enum
	{
		ItemRoleMessagePtr = Qt::UserRole + 1,
		ItemRoleLogFilePtr,
	};


	explicit SessionMessagesModel(SessionPtr a_Session);

	// QAbstractTableModel overrides:
	virtual int rowCount(const QModelIndex & a_Parent) const override;
	virtual int columnCount(const QModelIndex & a_Parent) const override;
	virtual QVariant data(const QModelIndex & a_Index, int a_Role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int a_Section, Qt::Orientation a_Orientation, int a_Role) const override;

	/** Enables or disables the specified log file.
	Messages from a disabled log file do not show in the model. */
	void setLogFileEnabled(LogFile * a_LogFile, bool a_IsEnabled);

	/** Sets the string on which to filter. */
	void setFilterString(const QString & a_FilterString);

	/** Returns true if the model is being filtered by m_FilterString. */
	bool isFilteringByString() const { return !m_FilterString.empty(); }

	/** Sets whether the specified LogLevel should be shown or not. */
	void setLogLevelFilter(LogFile::LogLevel a_LogLevel, bool a_ShouldShow);

	/** Returns whether the specified LogLevel is shown. */
	bool isLogLevelShown(LogFile::LogLevel a_LogLevel) const;


protected slots:

	/** Emitted by m_Session when a new logfile is added to it. */
	void sessionLogFileAdded(LogFilePtr a_LogFile);


protected:
	class MessageSorter;

	/** Represents a single row in the model. */
	struct MessageRow
	{
		LogFile * m_LogFile;
		size_t m_MessageIndex;
	};
	typedef std::vector<MessageRow> MessageRows;


	/** The session represented by this model. */
	SessionPtr m_Session;

	/** Set of LogFiles that are currently disabled for display. */
	std::set<const LogFile *> m_DisabledLogFiles;

	/** Individual logfile messages, sorted by their datetime.
	Order in this vector directly indicates the order in the view. */
	MessageRows m_MessageRows;

	/** If non-empty, only items containing the specified string will be shown. */
	std::string m_FilterString;

	/** Specifies the case sensitivity of m_FilterString. */
	Qt::CaseSensitivity m_FilterCaseSensitive;

	/** Indicates which LogLevels are hidden. */
	std::set<LogFile::LogLevel> m_LogLevelHidden;


	/** Returns true if the specified LogFile is enabled for display. */
	bool isLogFileEnabled(LogFile * a_LogFile) const;

	/** Returns the string representation of the specified LogLevel. */
	static QString logLevelToString(LogFile::LogLevel a_LogLevel);

	/** Inserts all messages from the specified logfile into the model.
	Insert-sorts the messages into m_MessageRows. Emits appropriate model's item insertion signals. */
	void insertLogFileMessages(LogFile * a_LogFile);

	/** Removes all mesasges originating in the specified logfile from the model.
	Removes the messages from m_MessageRows, emits appropriate model's item deletion signals. */
	void deleteLogFileMessages(LogFile * a_LogFile);

	/** Returns true if the specified log message should be shown (is not filtered). */
	bool shouldShowMessage(LogFile * a_LogFile, int a_MsgIndex) const;

	/** Re-evaluates the filter for all messages, inserting and deleting rows as necessary.
	Filter in this context is the m_FilterString, m_LogLevelHidden and m_DisabledLogFiles combo. */
	void reFilter();

	/** Returns true if the specified message passes the filter.
	Filter in this context is the m_FilterString, m_LogLevelHidden and m_DisabledLogFiles combo. */
	bool shouldShowMessage(const LogFile::Message & a_Message, const LogFile * a_LogFile);
};





#endif // SESSIONMESSAGESMODEL_H
