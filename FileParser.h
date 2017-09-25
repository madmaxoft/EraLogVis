// FileParser.h

// Declares the FileParser class representing the parser that creates LogFile objects out of disk files





#ifndef FILEPARSER_H
#define FILEPARSER_H





#include <functional>
#include <atomic>

#include <QObject>
#include "Session.h"





// fwd:
class QIODevice;





/** Parses a single file into the in-memory representation.
Note that a single disk file may result in multiple LogFile representations (zipped multiple files).
Usage: construct an instance, connect to its signals and then call parse(). The signals are emitted during
the execution of the parse() function. */
class FileParser:
	public QObject
{
	Q_OBJECT
	typedef QObject Super;


public:

	/** Creates a new instance of the parser.
	a_ShouldAbort is a shared variable that indicates whether the parsing should be aborted (from another thread). */
	FileParser(std::atomic<bool> & a_ShouldAbort);

	/** Parses the specified file and emits the signals relevant to the parsing. */
	void parse(const QString & a_FileName);

signals:

	/** Emitted when there is an error while parsing. */
	void parseFailed(const QString & a_ErrorText);

	/** Emitted when the data format is not recognized. */
	void failedToRecognize(const QString & a_Details);

	/** Emitted after a single file (out of possibly a multi-file archive) has been parsed successfully. */
	void finishedParsingFile(LogFilePtr a_Data);

	/** Emitted after all files have been processed. */
	void parsedAllFiles();


protected:

	friend class PlainTextMDMVAHParser;  // Needs access to m_ShouldAbort and reportFileParsed()
	friend class PlainTextEraParser;     // Needs access to m_ShouldAbort and reportFileParsed()


	/** When set to true (by another thread), parsing will be aborted at the nearest opportunity. */
	std::atomic<bool> & m_ShouldAbort;

	/** Name of the disk file that is currently being parsed. */
	QString m_FileName;

	/** Inner file name (inside an archive), if applicable, for the currently parsed data stream. */
	QString m_InnerFileName;

	/** Source identification (if available) for the currently parsed data stream.
	May be obtained from the file path. */
	QString m_SourceIdentification;


	/** Attempts to detect the format of the data in the sample (first N bytes of the file).
	Returns the handler to use for the file, nullptr if not known. */
	std::function<bool(QIODevice &)> getFormatHandler(const QByteArray & a_InitialData);

	/** Parses the specified data stream into the specified Session.
	Returns true on success, false on failure. */
	bool parseStream(QIODevice & a_Stream);

	/** Parses the specified GZIP data stream into the specified Session.
	Returns true on success, false on failure. */
	bool parseGZipStream(QIODevice & a_DataStream);

	/** Parses the specified plaintext data stream in MDM/VAH format into the specified Session.
	Returns true on success, false on failure. */
	bool parseTextStreamMDMVAH(QIODevice & a_DataStream);

	/** Parses the specified plaintext data stream in ERA format into the specified Session.
	Returns true on success, false on failure. */
	bool parseTextStreamEra(QIODevice & a_DataStream);

	/** Emits the finishedParsingFile signal with the specified log file data and the stored metadata. */
	void reportFileParsed(LogFilePtr a_LogFile);
};





#endif // FILEPARSER_H
