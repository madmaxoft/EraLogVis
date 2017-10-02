// FileParser.cpp

// Implements the FileParser class representing the parser that creates LogFile objects out of disk files





#include "FileParser.h"

#include <assert.h>
#include <QFile>
#include <QtDebug>

#ifdef _MSC_VER
	// When compiling in MSVC on Windows, use Qt-provided zlib (there's no system-zlib)
	#include <QtZlib/zlib.h>
#else
	// Use system-zlib everywhere else:
	#include <zlib.h>
#endif

#include "Session.h"
#include "LogFile.h"
#include "Stopwatch.h"
#include "Exceptions.h"





/** Reads all the data from the QIODevice into a std::string. */
static std::string readWholeStream(QIODevice & a_DataStream)
{
	std::string res;
	auto size = a_DataStream.size();
	res.resize(static_cast<size_t>(size));
	if (a_DataStream.read(const_cast<char *>(res.data()), size) != size)
	{
		throw EFileReadError(__FILE__, __LINE__);
	}
	return res;
}





/** Passes the specified data into ungzip, returns everything decompressed.
Returns an empty string on failure. */
static std::string ungzipString(const void * a_Data, size_t a_DataSize)
{
	char buf[30000];
	std::string res;
	res.reserve(a_DataSize);

	// Init the ZLIB ungzipper:
	z_stream zlibStream;
	memset(&zlibStream, 0, sizeof(zlibStream));
	auto zr = inflateInit2(&zlibStream, 31);  // Force GZIP decoding
	if (zr != Z_OK)
	{
		qDebug("%s: uncompression initialization failed: %d (\"%s\").", __FUNCTION__, zr, zlibStream.msg);
		return std::string();
	}
	zlibStream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(a_Data));
	zlibStream.avail_in = static_cast<uInt>(a_DataSize);
	while (true)
	{
		zlibStream.next_out = reinterpret_cast<Bytef *>(buf);
		zlibStream.avail_out = sizeof(buf);
		zr = inflate(&zlibStream, Z_SYNC_FLUSH);
		switch (zr)
		{
			case Z_OK:
			{
				// Decompressed a chunk, append it to the output data:
				res.append(buf, sizeof(buf) - zlibStream.avail_out);
				break;
			}
			case Z_STREAM_END:
			{
				// Decompressed all data, append any leftovers and return the whole thing:
				res.append(buf, sizeof(buf) - zlibStream.avail_out);
				inflateEnd(&zlibStream);
				return res;
			}
			default:
			{
				// Error
				qDebug("%s: uncompression failed: %d (\"%s\").", __FUNCTION__, zr, zlibStream.msg);
				inflateEnd(&zlibStream);
				return std::string();
			}
		}
	}  // while (true)
	// Should not get here, the loop above only has a return, no break
	assert(!"Invalid code path");
	return std::string();
}





////////////////////////////////////////////////////////////////////////////////
// PlainTextMDMVAHParser:

/** Parses log data stream formatted in an MDM / VAH format. */
class PlainTextMDMVAHParser
{
public:
	PlainTextMDMVAHParser(
		FileParser & a_FileParser,
		const QString & a_FileName,
		const QString & a_InnerFileName,
		std::string && a_CompleteText
	):
		m_FileParser(a_FileParser),
		m_LogFile(new LogFile(
			a_FileName, a_InnerFileName, LogFile::SourceType::stMDMVAH, "", std::move(a_CompleteText)
		))
	{
	}


	/** Parses the contents stored in m_LogFile into messages
	Returns true on success, false on failure. */
	bool parseContents()
	{
		resetAfterLine();
		const auto & completeText = m_LogFile->getCompleteText();
		if (!parseBuf(completeText.c_str(), completeText.size()))
		{
			return false;
		}
		emit m_FileParser.finishedParsingFile(m_LogFile);
		return true;
	}


protected:

	/** Internal state of the parser. */
	enum
	{
		sYear,
		sMonth,
		sDay,
		sHour,
		sMinute,
		sSecond,
		sLogLevel,
		sThreadID,
		sMessage,
		sContinuation,
	} m_State;

	/** The FileParser instance that has created this helper.
	Used for detecting abortion and to report the parsed files. */
	FileParser & m_FileParser;

	/** The LogFile into which the messages are added. */
	LogFilePtr m_LogFile;

	// Stored state of the parser for the current line:
	int m_CurrentYear, m_CurrentMonth, m_CurrentDay, m_CurrentHour, m_CurrentMinute, m_CurrentSecond;
	quint64 m_CurrentThreadID;
	LogFile::LogLevel m_CurrentLogLevel;
	bool m_HasJustFinishedLine;


	/** Translates the loglevel indicator character into the internal loglevel value. */
	LogFile::LogLevel logLevelFromChar(char a_Indicator)
	{
		switch (a_Indicator)
		{
			case 'f':
			case 'F':
			{
				return LogFile::LogLevel::llFatal;
			}
			case 'c':
			case 'C':
			{
				return LogFile::LogLevel::llCritical;
			}
			case 'e':
			case 'E':
			{
				return LogFile::LogLevel::llError;
			}
			case 'w':
			case 'W':
			{
				return LogFile::LogLevel::llWarning;
			}
			case 'i':
			case 'I':
			{
				return LogFile::LogLevel::llInformation;
			}
			case 'd':
			case 'D':
			{
				return LogFile::LogLevel::llDebug;
			}
			case 't':
			case 'T':
			{
				return LogFile::LogLevel::llTrace;
			}
			case 's':
			case 'S':
			{
				return LogFile::LogLevel::llStatus;
			}
			default:
			{
				return LogFile::LogLevel::llUnknown;
			}
		}
	}



	/** Resets the parser state after parsing a line. */
	void resetAfterLine()
	{
		m_CurrentYear = 0;
		m_CurrentMonth = 0;
		m_CurrentDay = 0;
		m_CurrentHour = 0;
		m_CurrentMinute = 0;
		m_CurrentSecond = 0;
		m_CurrentThreadID = 0;
		m_State = sYear;
		m_HasJustFinishedLine = true;
	}



	/** Processes the specified logfile contents buffer. */
	bool parseBuf(const char * a_Buf, size_t a_Length)
	{
		int linesBeforeAbortCheck = 1000;
		size_t lastEOL = 0;
		size_t lastContinuationBegin = 0;
		size_t lastMessageBegin = 0;
		for (size_t i = 0; i < a_Length; ++i)
		{
			auto ch = a_Buf[i];
			if ((ch == '\n') || (ch == '\r'))
			{
				if (linesBeforeAbortCheck > 0)
				{
					linesBeforeAbortCheck -= 1;
				}
				else
				{
					linesBeforeAbortCheck = 1000;
					if (m_FileParser.m_ShouldAbort.load())  // This check is a bit expensive, don't do it too often
					{
						return false;
					}
				}
				if (m_HasJustFinishedLine)
				{
					// This was a CRLF, skip
					m_HasJustFinishedLine = false;
					continue;
				}
				// Process end-of-line - either add message or add continuation:
				if (m_State == sMessage)
				{
					// We have parsed a full message, add it now:
					QDateTime dt(
						QDate(m_CurrentYear, m_CurrentMonth, m_CurrentDay),
						QTime(m_CurrentHour, m_CurrentMinute, m_CurrentSecond)
					);
					m_LogFile->addMessage(
						std::move(dt),
						m_CurrentLogLevel,
						std::string(),
						m_CurrentThreadID,
						lastMessageBegin, i - lastMessageBegin
					);
				}
				else
				{
					// We have a continuation:
					if (!m_LogFile->appendContinuationToLastMessage(i - lastContinuationBegin))
					{
						return false;
					}
				}
				lastEOL = i;
				resetAfterLine();
				continue;
			}
			m_HasJustFinishedLine = false;

			switch (m_State)
			{
				case sYear:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentYear = m_CurrentYear * 10 + ch - '0';
					}
					else
					{
						if (ch == '-')
						{
							m_State = sMonth;
						}
						else
						{
							lastContinuationBegin = lastEOL;
							m_State = sContinuation;
						}
					}
					break;
				}  // case sYear

				case sMonth:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentMonth = m_CurrentMonth * 10 + ch - '0';
					}
					else
					{
						m_State = sDay;
					}
					break;
				}  // case sMonth

				case sDay:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentDay = m_CurrentDay * 10 + ch - '0';
					}
					else
					{
						m_State = sHour;
					}
					break;
				}  // case sDay

				case sHour:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentHour = m_CurrentHour * 10 + ch - '0';
					}
					else
					{
						m_State = sMinute;
					}
					break;
				}  // case sHour

				case sMinute:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentMinute = m_CurrentMinute * 10 + ch - '0';
					}
					else
					{
						m_State = sSecond;
					}
					break;
				}  // case sMinute

				case sSecond:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentSecond = m_CurrentSecond * 10 + ch - '0';
					}
					else
					{
						m_State = sLogLevel;
					}
					break;
				}  // case sSecond

				case sLogLevel:
				{
					if (ch == '[')
					{
						m_State = sThreadID;
					}
					else if (ch == ' ')
					{
						// Do nothing
					}
					else
					{
						m_CurrentLogLevel = logLevelFromChar(ch);
					}
					break;
				}  // sLogLevel

				case sThreadID:
				{
					if (ch == ']')
					{
						// Do nothing yet
					}
					else if (ch == ' ')
					{
						lastMessageBegin = i;
						m_State = sMessage;
					}
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentThreadID = m_CurrentThreadID * 10 + static_cast<quint64>(ch - '0');
					}
				}  // sThreadID

				case sMessage: break;  // Nothing needed here
				case sContinuation: break;  // Nothing needed here
			}  // swith (ch)
		}  // for i - a_Buf[]

		// Ran out of data to parse, add anything left over:
		switch (m_State)
		{
			case sMessage:
			{
				QDateTime dt(
					QDate(m_CurrentYear, m_CurrentMonth, m_CurrentDay),
					QTime(m_CurrentHour, m_CurrentMinute, m_CurrentSecond)
				);
				m_LogFile->addMessage(
					std::move(dt),
					m_CurrentLogLevel,
					std::string(),
					m_CurrentThreadID,
					lastMessageBegin, a_Length - lastMessageBegin
				);
				break;
			}
			case sContinuation:
			{
				if (!m_LogFile->appendContinuationToLastMessage(a_Length - lastMessageBegin))
				{
					return false;
				}
				break;
			}
			default:
			{
				break;
			}
		}
		return true;
	}
};





////////////////////////////////////////////////////////////////////////////////
// PlainTextEraParser:

/** Parses log data stream formatted in an ERA format. */
class PlainTextEraParser
{
public:
	PlainTextEraParser(
		FileParser & a_FileParser,
		const QString & a_FileName,
		const QString & a_InnerFileName,
		const QString & a_SourceIdentification,
		std::string && a_CompleteText
	):
		m_FileParser(a_FileParser),
		m_LogFile(new LogFile(
			a_FileName, a_InnerFileName,
			LogFile::SourceType::stUnknown, a_SourceIdentification,
			std::move(a_CompleteText)
		))
	{
	}


	/** Parses m_LogFile's m_CompleteText into messages within that LogFile.
	Returns true on success, false on failure. */
	bool parseContents()
	{
		resetAfterLine();
		const auto & completeText = m_LogFile->getCompleteText();
		if (!processBuf(completeText.c_str(), completeText.size()))
		{
			return false;
		}
		m_LogFile->tryIdentifySource();
		emit m_FileParser.finishedParsingFile(m_LogFile);
		return true;
	}


protected:

	/** Internal state of the parser. */
	enum
	{
		sYear,
		sMonth,
		sDay,
		sHour,
		sMinute,
		sSecond,
		sLogLevel,
		sLogLevelIgnore,  // Second and next characters from the LogLevel
		sComponent,
		sComponentIgnore,  // Whitespace and symbols after Component, before ThreadID
		sThreadID,
		sMessage,
		sContinuation,
	} m_State;

	/** The FileParser instance that has created this helper.
	Used for detecting abortion and to report the parsed files. */
	FileParser & m_FileParser;

	/** The LogFile into which the messages are added. */
	LogFilePtr m_LogFile;

	// Stored state of the parser for the current line:
	int m_CurrentYear, m_CurrentMonth, m_CurrentDay, m_CurrentHour, m_CurrentMinute, m_CurrentSecond;
	quint64 m_CurrentThreadID;
	LogFile::LogLevel m_CurrentLogLevel;
	std::string m_CurrentComponent;



	/** Resets the parser state after parsing a line. */
	void resetAfterLine()
	{
		m_CurrentYear = 0;
		m_CurrentMonth = 0;
		m_CurrentDay = 0;
		m_CurrentHour = 0;
		m_CurrentMinute = 0;
		m_CurrentSecond = 0;
		m_CurrentThreadID = 0;
		m_State = sYear;
	}



	/** Translates the loglevel indicator character into the internal loglevel value. */
	LogFile::LogLevel logLevelFromChar(char a_Indicator)
	{
		switch (a_Indicator)
		{
			case 'f':
			case 'F':
			{
				return LogFile::LogLevel::llFatal;
			}
			case 'c':
			case 'C':
			{
				return LogFile::LogLevel::llCritical;
			}
			case 'e':
			case 'E':
			{
				return LogFile::LogLevel::llError;
			}
			case 'w':
			case 'W':
			{
				return LogFile::LogLevel::llWarning;
			}
			case 'i':
			case 'I':
			{
				return LogFile::LogLevel::llInformation;
			}
			case 'd':
			case 'D':
			{
				return LogFile::LogLevel::llDebug;
			}
			case 't':
			case 'T':
			{
				return LogFile::LogLevel::llTrace;
			}
			case 's':
			case 'S':
			{
				return LogFile::LogLevel::llStatus;
			}
			default:
			{
				return LogFile::LogLevel::llUnknown;
			}
		}
	}


	/** Converts a hex number character to its value.
	Returns true on success, false if the input is not a valid hex number char. */
	static bool toHexCharValue(char a_HexChar, quint64 & a_OutValue)
	{
		switch (a_HexChar)
		{
			case '0': a_OutValue =  0; return true;
			case '1': a_OutValue =  1; return true;
			case '2': a_OutValue =  2; return true;
			case '3': a_OutValue =  3; return true;
			case '4': a_OutValue =  4; return true;
			case '5': a_OutValue =  5; return true;
			case '6': a_OutValue =  6; return true;
			case '7': a_OutValue =  7; return true;
			case '8': a_OutValue =  8; return true;
			case '9': a_OutValue =  9; return true;
			case 'a': a_OutValue = 10; return true;
			case 'b': a_OutValue = 11; return true;
			case 'c': a_OutValue = 12; return true;
			case 'd': a_OutValue = 13; return true;
			case 'e': a_OutValue = 14; return true;
			case 'f': a_OutValue = 15; return true;
			case 'A': a_OutValue = 10; return true;
			case 'B': a_OutValue = 11; return true;
			case 'C': a_OutValue = 12; return true;
			case 'D': a_OutValue = 13; return true;
			case 'E': a_OutValue = 14; return true;
			case 'F': a_OutValue = 15; return true;
		}
		return false;
	}


	/** Processes a single line from the input.
	Returns true if the line is processed successfully, false on failure. */
	bool processBuf(const char * a_Buf, size_t a_Length)
	{
		/*
		Typical line:
		2017-01-13 04:55:53 Debug: CReplicationModule [Thread 7f43937fe700]: CStepTx: Remote peer signalized...
		*/
		int linesBeforeAbortCheck = 1000;
		size_t lastEOL = 0;
		size_t lastMessageBegin = 0;
		size_t lastComponentBegin = 0;
		bool hasJustFinishedLine = true;
		for (size_t i = 0; i < a_Length; ++i)
		{
			auto ch = a_Buf[i];
			if ((ch == '\n') || (ch == '\r'))
			{
				if (hasJustFinishedLine)
				{
					// This was a CRLF, skip
					hasJustFinishedLine = false;
					continue;
				}

				// Check whether an abort is requested from the UI thread:
				if (linesBeforeAbortCheck > 0)
				{
					linesBeforeAbortCheck -= 1;
				}
				else
				{
					linesBeforeAbortCheck = 1000;
					if (m_FileParser.m_ShouldAbort.load())  // This check is a bit expensive, don't do it too often
					{
						return false;
					}
				}

				// Process end-of-line - either add message or add continuation:
				if (m_State == sMessage)
				{
					// We have parsed a full message, add it now:
					QDateTime dt(
						QDate(m_CurrentYear, m_CurrentMonth, m_CurrentDay),
						QTime(m_CurrentHour, m_CurrentMinute, m_CurrentSecond)
					);
					m_LogFile->addMessage(
						std::move(dt),
						m_CurrentLogLevel,
						std::move(m_CurrentComponent),
						m_CurrentThreadID,
						lastMessageBegin, i - lastMessageBegin
					);
				}
				else
				{
					// We have a continuation:
					if (!m_LogFile->appendContinuationToLastMessage(i - lastEOL))
					{
						return false;
					}
				}
				lastEOL = i;
				resetAfterLine();
				hasJustFinishedLine = true;
				continue;
			}
			hasJustFinishedLine = false;

			switch (m_State)
			{
				case sYear:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentYear = m_CurrentYear * 10 + ch - '0';
					}
					else
					{
						if (ch == '-')
						{
							m_State = sMonth;
						}
						else
						{
							m_State = sContinuation;
						}
					}
					break;
				}  // case sYear

				case sMonth:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentMonth = m_CurrentMonth * 10 + ch - '0';
					}
					else
					{
						m_State = sDay;
					}
					break;
				}  // case sMonth

				case sDay:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentDay = m_CurrentDay * 10 + ch - '0';
					}
					else
					{
						m_State = sHour;
					}
					break;
				}  // case sDay

				case sHour:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentHour = m_CurrentHour * 10 + ch - '0';
					}
					else
					{
						m_State = sMinute;
					}
					break;
				}  // case sHour

				case sMinute:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentMinute = m_CurrentMinute * 10 + ch - '0';
					}
					else
					{
						m_State = sSecond;
					}
					break;
				}  // case sMinute

				case sSecond:
				{
					if ((ch >= '0') && (ch <= '9'))
					{
						m_CurrentSecond = m_CurrentSecond * 10 + ch - '0';
					}
					else
					{
						m_State = sLogLevel;
					}
					break;
				}  // case sSecond

				case sLogLevel:
				{
					m_CurrentLogLevel = logLevelFromChar(ch);
					m_State = sLogLevelIgnore;
					break;
				}  // sLogLevel

				case sLogLevelIgnore:
				{
					if (ch == ' ')
					{
						lastComponentBegin = i + 1;
						m_State = sComponent;
					}
					break;
				}  // sLogLevelIgnore

				case sComponent:
				{
					if (ch == ' ')
					{
						m_CurrentComponent.assign(a_Buf + lastComponentBegin, i - lastComponentBegin);
						m_State = sComponentIgnore;
					}
					break;
				}  // sComponent

				case sComponentIgnore:
				{
					if (ch == ' ')
					{
						m_State = sThreadID;
					}
					break;
				}  // sComponentIgnore

				case sThreadID:
				{
					quint64 v;
					if (ch == ']')
					{
						// Do nothing yet
					}
					else if (ch == ' ')
					{
						lastMessageBegin = i + 1;
						m_State = sMessage;
					}
					else if (toHexCharValue(ch, v))
					{
						m_CurrentThreadID = m_CurrentThreadID * 16 + v;
					}
				}  // sThreadID

				case sMessage: break;  // Nothing needed here
				case sContinuation: break;  // Nothing needed here
			}  // swith (ch)
		}  // for i - a_Buf[]

		// Ran out of data to parse, append any leftovers:
		switch (m_State)
		{
			case sMessage:
			{
				QDateTime dt(
					QDate(m_CurrentYear, m_CurrentMonth, m_CurrentDay),
					QTime(m_CurrentHour, m_CurrentMinute, m_CurrentSecond)
				);
				m_LogFile->addMessage(
					std::move(dt),
					m_CurrentLogLevel,
					std::move(m_CurrentComponent),
					m_CurrentThreadID,
					lastMessageBegin, a_Length - lastMessageBegin
				);
			}
			case sContinuation:
			{
				// We have a continuation:
				if (!m_LogFile->appendContinuationToLastMessage(a_Length - lastEOL))
				{
					return false;
				}
				break;
			}
			default:
			{
				break;
			}
		}
		return true;
	}
};





////////////////////////////////////////////////////////////////////////////////
// FileParser:

FileParser::FileParser(std::atomic<bool> & a_ShouldAbort):
	m_ShouldAbort(a_ShouldAbort)
{
}




void FileParser::parse(const QString & a_FileName)
{
	m_FileName = a_FileName;
	m_InnerFileName.clear();
	m_SourceIdentification.clear();
	QFile f(a_FileName);
	if (!f.open(QFile::ReadOnly))
	{
		emit parseFailed(tr("Cannot open file %1 for reading").arg(a_FileName));
	}
	else
	{
		auto contents = readWholeStream(f);
		parseContents(std::move(contents));
	}
	emit parsedAllFiles();
}





bool FileParser::parseContents(std::string && a_Contents)
{
	// Try to recognize format based on the initial data in the stream:
	auto handler = getFormatHandler(a_Contents);
	if (!handler)
	{
		return false;
	}
	return handler(std::move(a_Contents));
}





std::function<bool (std::string &&)> FileParser::getFormatHandler(const std::string & a_Contents)
{
	if (a_Contents.size() < 2)
	{
		emit failedToRecognize(tr("Not enough data present in the file"));
		return nullptr;
	}

	// Test for GZIP header:
	if ((a_Contents[0] == 0x1f) && (static_cast<unsigned char>(a_Contents[1]) == 0x8b))
	{
		return [this](std::string && a_HContents)
		{
			return this->parseGZipContents(std::move(a_HContents));
		};
	}

	// Test if most characters within the first 1000 bytes are plain letters / CR / LF / SP / HT
	size_t numPlainText = 0;
	auto size = std::min<size_t>(a_Contents.size(), 1000);
	for (size_t i = 0; i < size; i++)
	{
		auto v = a_Contents[i];
		if (
			(v == 0x09) ||  // HT
			(v == 0x0a) ||  // LF
			(v == 0x0d) ||  // CR
			((v >= 32) && (v < 127))
		)
		{
			numPlainText = numPlainText + 1;
		}
	}
	if (size - numPlainText < size / 50)  // Less than 2 % "weird" characters
	{
		// Check that the first line starts with a date
		static const QString format = "yyyy-MM-dd HH:mm:ss";
		auto dateTimeString = QString::fromStdString(a_Contents.substr(0, 19));
		QDateTime dateTime = QDateTime::fromString(dateTimeString, format);
		if (dateTime.isValid())
		{
			// Decide between the MDM/VAH format and ERA format
			// VAH format always has a space as the 21st character
			if (a_Contents[21] == ' ')
			{
				return [this](std::string && a_HContents)
				{
					return this->parseTextContentsMDMVAH(std::move(a_HContents));
				};
			}
			else
			{
				return [this](std::string && a_HContents)
				{
					return this->parseTextContentsEra(std::move(a_HContents));
				};
			}
		}
	}

	emit failedToRecognize(tr("Did not match any known format"));
	return nullptr;
}





bool FileParser::parseGZipContents(std::string && a_Contents)
{
	Stopwatch sw("GZIP + parsing");
	return parseContents(ungzipString(a_Contents.data(), a_Contents.size()));
}





bool FileParser::parseTextContentsMDMVAH(std::string && a_Contents)
{
	PlainTextMDMVAHParser parser(*this, m_FileName, m_InnerFileName, std::move(a_Contents));
	Stopwatch sw("MDM / VAH parsing");
	if (!parser.parseContents())
	{
		emit parseFailed(tr("MDM / VAH log parser error"));
		return false;
	}
	return true;
}





bool FileParser::parseTextContentsEra(std::string && a_Contents)
{
	PlainTextEraParser parser(*this, m_FileName, m_InnerFileName, m_SourceIdentification, std::move(a_Contents));
	Stopwatch sw("ERA parsing");
	if (parser.parseContents())
	{
		return true;
	}
	emit parseFailed(tr("ERA log parser error"));
	return false;
}
