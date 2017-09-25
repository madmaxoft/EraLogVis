// BackgroundParser.h

// Declares the BackgroundParser class representing the background thread pool for parsing files / folders.





#ifndef BACKGROUNDPARSER_H
#define BACKGROUNDPARSER_H





#include <memory>
#include <atomic>

#include <QObject>
#include <QThreadPool>





// fwd:
class LogFile;
typedef std::shared_ptr<LogFile> LogFilePtr;





class BackgroundParser:
	public QObject
{
	Q_OBJECT
	typedef QObject Super;


public:

	BackgroundParser();

	~BackgroundParser();

	/** Adds a file to be parsed in the background. */
	void addFile(const QString & a_FileName);

	/** Adds a folder to be parsed in the background. */
	void addFolder(const QString & a_FolderPath);


protected:

	friend class FileParseTask;  // Needs access to m_ShouldAbort

	/** The threads that do the actual parsing. */
	QThreadPool m_ThreadPool;

	/** Flag that is shared with all the parsers to indicate they should abort parsing. */
	std::atomic<bool> m_ShouldAbort;

signals:

	/** (Re-emitted from FileParser)
	Emitted after a single file (out of possibly a multi-file archive) has been parsed successfully. */
	void finishedParsingFile(LogFilePtr a_Data);

public slots:
};





#endif // BACKGROUNDPARSER_H
