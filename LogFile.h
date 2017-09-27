// LogFile.h

// Declares the LogFile class representing a single log file's contents





#ifndef LOGFILE_H
#define LOGFILE_H





#include <vector>
#include <map>
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
		int m_ModuleIdentifier;  // Identifier from LogFile's m_ModuleToIdentifier / m_IdentifierToModule
		quint64 m_ThreadID;
		size_t m_TextStart, m_TextLength;  // Index into LogFile's m_CompleteText

		explicit Message(
			QDateTime && a_DateTime,
			LogLevel a_LogLevel,
			int a_ModuleIdentifier,
			quint64 a_ThreadID,
			size_t a_TextStart, size_t a_TextLength
		):
			m_DateTime(std::move(a_DateTime)),
			m_LogLevel(a_LogLevel),
			m_ModuleIdentifier(a_ModuleIdentifier),
			m_ThreadID(a_ThreadID),
			m_TextStart(a_TextStart),
			m_TextLength(a_TextLength)
		{
		}
	};


	/** Constructs a new object with the specified properties. */
	explicit LogFile(const QString & a_FileName,
		const QString & a_InnerFileName,
		SourceType a_SourceType,
		const QString & a_SourceIdentifier,
		std::string && a_CompleteText
	);

	/** Adds a new message to the storage.
	The message is expected to logically belong after the last message already present. */
	void addMessage(QDateTime && a_DateTime,
		LogLevel a_LogLevel,
		const std::string & a_Module,
		quint64 a_ThreadID,
		size_t a_TextStart,
		size_t a_TextLength
	);

	/** Appends the specified text to the last message's text.
	Returns true on success, false if there is no message. */
	bool appendContinuationToLastMessage(size_t a_AddLength);

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

	/** Converts the module identifier into the module name.
	If no such module is known, returns an empty string. */
	std::string identifierToModule(int a_ModuleIdentifier) const;

	/** Returns the log message text for the specified message. */
	QString getMessageText(const Message & a_Message) const;

	/** Returns the entire unparsed log file data contained within. */
	const std::string & getCompleteText() const { return m_CompleteText; }

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

	/** The complete logfile text. The messages contain indices into this string. */
	std::string m_CompleteText;

	/** The individual log messages in the log file.
	Sorted by their original order in the file (m_DateTime). */
	std::vector<Message> m_Messages;

	/** Map of modules' identifier numbers to module name.
	Each module is assigned a number which represents the module in each log message. */
	std::map<int, std::string> m_IdentifierToModule;

	/** Map of module names to their respective identifier number.
	Each module is assigned a number which represents the module in each log message. */
	std::map<std::string, int> m_ModuleToIdentifier;


	/** Sets the DisplayName based on the FileName and InnerFileName. */
	void constructDisplayName();

	/** Attempts to identify the SourceType based on the filenames and messages already present. */
	SourceType tryIdentifySourceType() const;

	/** Converts the module name into the identifier number.
	If such a module is not yet in the maps, adds it and assigns a new identifier. */
	int moduleToIdentifier(const std::string & a_ModuleName);
};

typedef std::shared_ptr<LogFile> LogFilePtr;





#endif // LOGFILE_H
