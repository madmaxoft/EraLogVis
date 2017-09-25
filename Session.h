// Session.h

// Declares the Session class representing the whole session - all the log files currently loaded

#ifndef SESSION_H
#define SESSION_H





#include <memory>
#include <vector>
#include <QObject>
#include "LogFile.h"





/// fwd:
typedef std::shared_ptr<LogFile> LogFilePtr;





class Session:
	public QObject
{
	Q_OBJECT

public:
	Session();

	/** Adds a new log file to the collection. */
	LogFilePtr addLogFile(
		const QString & a_FileName,
		const QString & a_InnerFileName,
		LogFile::SourceType a_SourceType,
		const QString & a_SourceIdentifier = ""
	);

	/** Adds the specified existing log file data to the collection. */
	void appendLogFile(LogFilePtr a_LogFile);

	/** Merges the logfiles from the specified session into this session (shallow-copy m_LogFiles).
	All logfiles are copied, even the "conflicting" ones. */
	void merge(Session & a_Src);

	/** Returns all the log files currently loaded in this session (read-only). */
	const std::vector<LogFilePtr> & logFiles(void) const { return m_LogFiles; }

	/** Returns the sum of all message counts for all the LogFiles. */
	size_t getMessageCount() const;

protected:

	/** All the log files currently loaded, in no specific order. */
	std::vector<LogFilePtr> m_LogFiles;

signals:
	/** Emitted after a new LogFile is added to the list. */
	void logFileAdded(LogFilePtr a_LogFile);
};





#endif // SESSION_H
