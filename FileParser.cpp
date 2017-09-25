// FileParser.cpp

// Implements the FileParser class representing the parser that creates LogFile objects out of disk files





#include "FileParser.h"

#include <QFile>
#include <QtDebug>

#include "Session.h"
#include "LogFile.h"
#include "StreamUngzipper.h"
#include "Stopwatch.h"





////////////////////////////////////////////////////////////////////////////////
// PreviewDataStream:

/** Implements a wrapper over QIODevice that reads a portion of the stream for preview,
but then allows the whole stream to be read again. */
template <size_t NumPreviewBytes = 1000>
class PreviewDataStream:
	public QIODevice
{
public:
	PreviewDataStream(QIODevice & a_ParentStream):
		m_ParentStream(a_ParentStream),
		m_PreviewData(a_ParentStream.read(NumPreviewBytes)),
		m_ReadPos(0)
	{
	}


	/** Returns the data read for preview, read-only. */
	const QByteArray & previewData() const { return m_PreviewData; }


protected:

	/** The IO device on top of which this wrapper operates. */
	QIODevice & m_ParentStream;

	/** The data read from m_ParentStream for preview. */
	QByteArray m_PreviewData;

	/** Index into m_PreviewData where the next read operation should begin reading.
	-1 if m_PreviewData should be skipped. */
	int m_ReadPos;



	// QIODevice overrides:
	virtual qint64 readData(char * a_Data, qint64 a_MaxLen) override
	{
		// If there's no data in the m_PreviewData buffer, read directly from the parent:
		if (m_ReadPos < 0)
		{
			return m_ParentStream.read(a_Data, a_MaxLen);
		}

		// Read from the m_PreviewData buffer first, then from the parent:
		auto numToRead = std::min<int>(a_MaxLen, m_PreviewData.size() - m_ReadPos);
		memcpy(a_Data, m_PreviewData.data() + m_ReadPos, numToRead);
		m_ReadPos += numToRead;
		if (m_ReadPos >= m_PreviewData.size())
		{
			m_ReadPos = -1;
		}
		if (a_MaxLen == numToRead)
		{
			return a_MaxLen;
		}
		auto numReadAfter = readData(a_Data + numToRead, a_MaxLen - numToRead);
		if (numReadAfter < 0)
		{
			// Failed to read anything past the preview data, return just the preview data:
			return numToRead;
		}
		return numReadAfter + numToRead;
	}

	virtual qint64 writeData(const char * a_Data, qint64 a_MaxLen) override
	{
		Q_UNUSED(a_Data);
		Q_UNUSED(a_MaxLen);
		Q_ASSERT(!"Writing not supported");
		return -1;
	}
};





////////////////////////////////////////////////////////////////////////////////
// PlainTextMDMVAHParser:

/** Parses log data stream formatted in an MDM / VAH format. */
class PlainTextMDMVAHParser
{
public:
	PlainTextMDMVAHParser(
		FileParser & a_FileParser,
		const QString & a_FileName,
		const QString & a_InnerFileName
	):
		m_FileParser(a_FileParser),
		m_LogFile(new LogFile(a_FileName, a_InnerFileName, LogFile::SourceType::stMDMVAH, ""))
	{
	}


	/** Parses the specified stream into the LogFile object contained within self.
	Returns true on success, false on failure. */
	bool parseStream(QIODevice & a_DataStream)
	{
		// Parse the data by fixed-size buffers:
		resetAfterLine();
		while (!m_FileParser.m_ShouldAbort.load())
		{
			char buf[30000];
			auto bufLen = a_DataStream.read(buf, sizeof(buf));
			if (bufLen <= 0)
			{
				break;
			}
			if (!processBuf(buf, static_cast<int>(bufLen)))
			{
				return false;
			}
		}
		processBuf("\n", 1);  // Push an empty line through the parser to flush
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

	/** The FileParser isntance that has created this helper.
	Used for detecting abortion and to report the parsed files. */
	FileParser & m_FileParser;

	/** The LogFile into which the messages are added. */
	LogFilePtr m_LogFile;

	// Stored state of the parser for the current line:
	int m_CurrentYear, m_CurrentMonth, m_CurrentDay, m_CurrentHour, m_CurrentMinute, m_CurrentSecond;
	quint64 m_CurrentThreadID;
	LogFile::LogLevel m_CurrentLogLevel;
	bool m_HasJustFinishedLine;

	// Data remembered from a previous buffer:
	QByteArray m_BufferedMessage;
	QByteArray m_BufferedContinuation;


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
		m_BufferedMessage.clear();
		m_BufferedContinuation.clear();
		m_State = sYear;
		m_HasJustFinishedLine = true;
	}



	/** Processes a single line from the input.
	Returns true if the line is processed successfully, false on failure. */
	bool processBuf(const char * a_Buf, int a_Length)
	{
		int lastEOL = 0;
		int lastContinuationBegin = 0;
		int lastMessageBegin = 0;
		for (int i = 0; i < a_Length; ++i)
		{
			auto ch = a_Buf[i];
			if ((ch == '\n') || (ch == '\r'))
			{
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
					std::string msg;
					if (m_BufferedMessage.isEmpty())
					{
						// The whole message is in a_Buf, no need to copy it
						if (i > lastMessageBegin)
						{
							msg.assign(a_Buf + lastMessageBegin, i - lastMessageBegin);
						}
					}
					else
					{
						// Part of the message is buffered from previous buf, copy the rest and decode:
						if (i > 0)
						{
							m_BufferedMessage.append(a_Buf, i);
						}
						msg = m_BufferedMessage.toStdString();
						m_BufferedMessage.clear();
					}
					m_LogFile->addMessage(std::move(dt), m_CurrentLogLevel, std::string(), m_CurrentThreadID, std::move(msg));
				}
				else
				{
					// We have a continuation:
					std::string msg;
					if (m_BufferedContinuation.isEmpty())
					{
						// The whole message is in a_Buf, no need to copy it
						if (i - 1 > lastContinuationBegin)
						{
							msg.assign(a_Buf + lastContinuationBegin, i - lastContinuationBegin - 1);
						}
					}
					else
					{
						// Part of the message is buffered from previous buf, copy the rest and decode:
						if (i > 0)
						{
							m_BufferedContinuation.append(a_Buf, i);
						}
						msg = m_BufferedContinuation.toStdString();
						m_BufferedContinuation.clear();
					}
					if (!m_LogFile->appendContinuationToLastMessage(std::move(msg)))
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

		// Ran out of data to parse, buffer anything important for the next buffer-parse:
		switch (m_State)
		{
			case sMessage:
			{
				m_BufferedMessage.append(a_Buf + lastMessageBegin, a_Length - lastMessageBegin);
				break;
			}
			case sContinuation:
			{
				m_BufferedContinuation.append(a_Buf + lastContinuationBegin, a_Length - lastContinuationBegin);
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
		const QString & a_SourceIdentification
	):
		m_FileParser(a_FileParser),
		m_LogFile(new LogFile(
			a_FileName, a_InnerFileName, LogFile::SourceType::stUnknown, a_SourceIdentification
		))
	{
	}


	/** Parses the specified stream into the LogFile object contained within self.
	Returns true on success, false on failure. */
	bool parseStream(QIODevice & a_DataStream)
	{
		// Parse the data by fixed-size buffers:
		resetAfterLine();
		while (!m_FileParser.m_ShouldAbort.load())
		{
			char buf[30000];
			auto bufLen = a_DataStream.read(buf, sizeof(buf));
			if (bufLen <= 0)
			{
				break;
			}
			if (!processBuf(buf, static_cast<int>(bufLen)))
			{
				return false;
			}
		}
		processBuf("\n", 1);  // Push an empty line through the parser to flush
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

	/** The FileParser isntance that has created this helper.
	Used for detecting abortion and to report the parsed files. */
	FileParser & m_FileParser;

	/** The LogFile into which the messages are added. */
	LogFilePtr m_LogFile;

	// Stored state of the parser for the current line:
	int m_CurrentYear, m_CurrentMonth, m_CurrentDay, m_CurrentHour, m_CurrentMinute, m_CurrentSecond;
	quint64 m_CurrentThreadID;
	LogFile::LogLevel m_CurrentLogLevel;
	std::string m_CurrentComponent;
	bool m_HasJustFinishedLine;

	// String values buffered between calls to processBuf():
	QByteArray m_BufferedMessage;
	QByteArray m_BufferedContinuation;
	QByteArray m_BufferedComponent;



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
		m_BufferedMessage.clear();
		m_BufferedContinuation.clear();
		m_BufferedComponent.clear();
		m_State = sYear;
		m_HasJustFinishedLine = true;
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
	bool processBuf(const char * a_Buf, int a_Length)
	{
		/*
		Typical line:
		2017-01-13 04:55:53 Debug: CReplicationModule [Thread 7f43937fe700]: CStepTx: Remote peer signalized...
		*/
		int lastEOL = 0;
		int lastMessageBegin = 0;
		int lastComponentBegin = 0;
		for (int i = 0; i < a_Length; ++i)
		{
			auto ch = a_Buf[i];
			if ((ch == '\n') || (ch == '\r'))
			{
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
					std::string msg;
					if (m_BufferedMessage.isEmpty())
					{
						// The whole message is in a_Buf, no need to copy it
						if (i > lastMessageBegin)
						{
							msg.assign(a_Buf + lastMessageBegin, i - lastMessageBegin);
						}
					}
					else
					{
						// Part of the message is buffered from previous buf, copy the rest and decode:
						if (i > 0)
						{
							m_BufferedMessage.append(a_Buf, i);
						}
						msg = m_BufferedMessage.toStdString();
						m_BufferedMessage.clear();
					}
					m_LogFile->addMessage(
						std::move(dt),
						m_CurrentLogLevel,
						std::move(m_CurrentComponent),
						m_CurrentThreadID,
						std::move(msg)
					);
				}
				else
				{
					// We have a continuation:
					std::string msg;
					if (m_BufferedContinuation.isEmpty())
					{
						// The whole message is in a_Buf, no need to copy it
						if (i - 1 > lastEOL)
						{
							msg.assign(a_Buf + lastEOL + 1, i - lastEOL - 1);
						}
					}
					else
					{
						// Part of the message is buffered from previous buf, copy the rest and decode:
						if (i > 0)
						{
							m_BufferedContinuation.append(a_Buf, i);
						}
						msg = m_BufferedContinuation.toStdString();
						m_BufferedContinuation.clear();
					}
					if (!m_LogFile->appendContinuationToLastMessage(std::move(msg)))
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
						m_BufferedComponent.clear();
					}
					break;
				}  // sLogLevelIgnore

				case sComponent:
				{
					if (ch == ' ')
					{
						if (m_BufferedComponent.isEmpty())
						{
							m_CurrentComponent.assign(a_Buf + lastComponentBegin, i - lastComponentBegin);
						}
						else
						{
							m_BufferedComponent.append(a_Buf, i);
							m_CurrentComponent = m_BufferedComponent.toStdString();
							m_BufferedComponent.clear();
						}
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

		// Ran out of data to parse, buffer anything important for the next buffer-parse:
		switch (m_State)
		{
			case sComponent:
			{
				m_BufferedComponent.append(a_Buf + lastComponentBegin, a_Length - lastComponentBegin);
				break;
			}
			case sMessage:
			{
				m_BufferedMessage.append(a_Buf + lastMessageBegin, a_Length - lastMessageBegin);
				break;
			}
			case sContinuation:
			{
				m_BufferedContinuation.append(a_Buf + lastEOL, a_Length - lastEOL);
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
		parseStream(f);
	}
	emit parsedAllFiles();
}





bool FileParser::parseStream(QIODevice & a_Stream)
{
	// Try to recognize format based on the initial data in the stream:
	PreviewDataStream<1000> ds(a_Stream);
	ds.open(QIODevice::ReadOnly);
	auto handler = getFormatHandler(ds.previewData());
	if (!handler)
	{
		return false;
	}
	return handler(ds);
}





std::function<bool(QIODevice &)> FileParser::getFormatHandler(const QByteArray & a_InitialData)
{
	if (a_InitialData.count() < 2)
	{
		emit failedToRecognize(tr("Not enough data present in the file"));
		return nullptr;
	}

	// Test for GZIP header:
	if ((a_InitialData[0] == 0x1f) && (static_cast<unsigned char>(a_InitialData[1]) == 0x8b))
	{
		return [this](QIODevice & a_DataStream)
		{
			return this->parseGZipStream(a_DataStream);
		};
	}

	// Test if most characters are plain letters / CR / LF / SP / HT
	int numPlainText = 0;
	int count = a_InitialData.count();
	for (int i = 0; i < count; i++)
	{
		auto v = a_InitialData.at(i);
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
	if (count - numPlainText < count / 50)  // Less than 2 % "weird" characters
	{
		// Check that the first line starts with a date
		static const QString format = "yyyy-MM-dd HH:mm:ss";
		auto dateTimeString = QString::fromUtf8(a_InitialData.left(19));
		QDateTime dateTime = QDateTime::fromString(dateTimeString, format);
		if (dateTime.isValid())
		{
			// Decide between the MDM/VAH format and ERA format
			// VAH format always has a space as the 21st character
			if (a_InitialData.at(21) == ' ')
			{
				return [this](QIODevice & a_DataStream)
				{
					return this->parseTextStreamMDMVAH(a_DataStream);
				};
			}
			else
			{
				return [this](QIODevice & a_DataStream)
				{
					return this->parseTextStreamEra(a_DataStream);
				};
			}
		}
	}

	emit failedToRecognize(tr("Did not match any known format"));
	return nullptr;
}





bool FileParser::parseGZipStream(QIODevice & a_DataStream)
{
	Stopwatch sw("GZIP + parsing");
	StreamUngzipper ungzipper(a_DataStream);
	ungzipper.open(QIODevice::ReadOnly);
	return parseStream(ungzipper);
}





bool FileParser::parseTextStreamMDMVAH(QIODevice & a_DataStream)
{
	PlainTextMDMVAHParser parser(*this, m_FileName, m_InnerFileName);
	Stopwatch sw("MDM / VAH parsing");
	if (!parser.parseStream(a_DataStream))
	{
		emit parseFailed(tr("MDM / VAH log parser error"));
		return false;
	}
	return true;
}





bool FileParser::parseTextStreamEra(QIODevice & a_DataStream)
{
	PlainTextEraParser parser(*this, m_FileName, m_InnerFileName, m_SourceIdentification);
	Stopwatch sw("ERA parsing");
	if (parser.parseStream(a_DataStream))
	{
		return true;
	}
	emit parseFailed(tr("ERA log parser error"));
	return false;
}
