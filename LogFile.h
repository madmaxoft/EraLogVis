// LogFile.h

// Declares the LogFile class representing a single log file's contents





#ifndef LOGFILE_H
#define LOGFILE_H





#include <vector>
#include <memory>
#include <QDateTime>





class LogFile
{
public:
	/** Type of the source that produced the log. */
	enum class SourceType
	{
		stMDMVAH,
		stMultiProxy,
		stMultiAgent,
		stAgent,
		stUnknown,
	};


	/** Log level of an individual message in the log. */
	enum class LogLevel
	{
		llFatal,
		llCritical,
		llError,
		llWarning,
		llInformation,
		llDebug,
		llTrace,
		llStatus,

		llUnknown,  // Used when a parser cannot decipher the loglevel indicator
	};


	/** Representation of a single line in the log. */
	struct Message
	{
		QDateTime m_DateTime;
		LogLevel m_LogLevel;
		std::string m_Module;
		quint64 m_ThreadID;
		std::string m_Text;

		explicit Message(
			QDateTime && a_DateTime,
			LogLevel a_LogLevel,
			std::string && a_Module,
			quint64 a_ThreadID,
			std::string && a_Text
		):
			m_DateTime(std::move(a_DateTime)),
			m_LogLevel(a_LogLevel),
			m_Module(std::move(a_Module)),
			m_ThreadID(a_ThreadID),
			m_Text(std::move(a_Text))
		{
		}
	};


	/** Constructs a new object with the specified filename, empty inner filename and unknown type. */
	explicit LogFile(const QString & a_FileName);

	/** Constructs a new object with the specified filename and inner filename, and unknown type. */
	explicit LogFile(const QString & a_FileName, const QString & a_InnerFileName);

	/** Constructs a new object with the specified everything. */
	explicit LogFile(
		const QString & a_FileName,
		const QString & a_InnerFileName,
		SourceType a_SourceType,
		const QString & a_SourceIdentifier = ""
	);

	/** Adds a new message to the storage.
	The message is expected to logically belong after the last message already present. */
	void addMessage(Message && a_Message);

	/** Adds a new message to the storage.
	The message is expected to logically belong after the last message already present. */
	void addMessage(QDateTime && a_DateTime,
		LogLevel a_LogLevel,
		std::string && a_Module,
		quint64 a_ThreadID,
		std::string && a_Text
	);

	/** Appends the specified text to the last message's text.
	Returns true on success, false if there is no message. */
	bool appendContinuationToLastMessage(std::string && a_Text);

	/** Returns the message at the specified index.
	Throws EIndexOutOfBounds if index is invalid. */
	const Message & getMessageByIndex(size_t a_Index) const;

	/** Returns the number of messages currently stored. */
	size_t messageCount(void) const { return m_Messages.size(); }

	/** Returns the (read-only) messages contained within. */
	const std::vector<Message> & messages(void) const { return m_Messages; }

	/** Tries to identify the source type and identifier based on filenames and messages already present.
	Returns true if the source was identified, false if not. */
	void tryIdentifySource(void);

	/** Returns the display name, used in the logfile lists. */
	const QString & displayName(void) const { return m_DisplayName; }

	SourceType sourceType(void) const { return m_SourceType; }

	const QString & sourceIdentifier(void) const { return m_SourceIdentifier; }

	/** Comparison between two logfiles, allows sorting by logfile sourcetype and identifier. */
	bool operator < (const LogFile & a_Other) const;

protected:
	/** Name of the file from which the log data was read.
	Always a disk file. */
	QString m_FileName;

	/** If m_FileName is a name of a multi-file archive, this is the name of the file in the archive.
	If not from an archive, this is empty. */
	QString m_InnerFileName;

	/** The name that should be displayed for the log file in the logfile list. */
	QString m_DisplayName;

	/** Type of the source that produced the log. */
	SourceType m_SourceType;

	/** Identifier of the source that produced the log.
	Used especially for MultiAgent to distinguish multiple instances. */
	QString m_SourceIdentifier;

	/** The individual log messages in the log file.
	Sorted by their original order in the file (m_DateTime). */
	std::vector<Message> m_Messages;


	/** Sets the DisplayName based on the FileName and InnerFileName. */
	void constructDisplayName();

	/** Attempts to identify the SourceType based on the filenames and messages already present. */
	SourceType tryIdentifySourceType(void) const;
};

typedef std::shared_ptr<LogFile> LogFilePtr;





#endif // LOGFILE_H
