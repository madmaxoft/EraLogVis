// LogFile.cpp

// Implements the LogFile class representing a single log file's contents





#include "LogFile.h"
#include <QFileInfo>
#include "Exceptions.h"





LogFile::LogFile(const QString & a_FileName):
	m_FileName(a_FileName),
	m_SourceType(LogFile::SourceType::stUnknown)
{
	constructDisplayName();
}





LogFile::LogFile(const QString & a_FileName, const QString & a_InnerFileName):
	m_FileName(a_FileName),
	m_InnerFileName(a_InnerFileName),
	m_SourceType(LogFile::SourceType::stUnknown)
{
	constructDisplayName();
}





LogFile::LogFile(
	const QString & a_FileName,
	const QString & a_InnerFileName,
	SourceType a_SourceType,
	const QString & a_SourceIdentifier
):
	m_FileName(a_FileName),
	m_InnerFileName(a_InnerFileName),
	m_SourceType(a_SourceType),
	m_SourceIdentifier(a_SourceIdentifier)
{
	constructDisplayName();
}




void LogFile::addMessage(Message && a_Message)
{
	m_Messages.emplace_back(std::move(a_Message));
}





void LogFile::addMessage(
	QDateTime && a_DateTime,
	LogLevel a_LogLevel,
	std::string && a_Module,
	quint64 a_ThreadID,
	std::string && a_Text
)
{
	m_Messages.emplace_back(
		std::move(a_DateTime),
		a_LogLevel,
		std::move(a_Module),
		a_ThreadID,
		std::move(a_Text)
	);
}




bool LogFile::appendContinuationToLastMessage(std::string && a_Text)
{
	if (m_Messages.empty())
	{
		return false;
	}
	m_Messages.back().m_Text.append(std::move(a_Text));
	return true;
}





const LogFile::Message & LogFile::getMessageByIndex(size_t a_Index) const
{
	if (a_Index >= m_Messages.size())
	{
		throw EIndexOutOfBounds(__FILE__, __LINE__, m_Messages.size(), a_Index);
	}
	return m_Messages[a_Index];
}





void LogFile::tryIdentifySource(void)
{
	// If the source type is not known, try to guess it:
	if (m_SourceType == SourceType::stUnknown)
	{
		m_SourceType = tryIdentifySourceType();
	}
}





bool LogFile::operator < (const LogFile & a_Other) const
{
	// If the SourceType differs, use it as comparison result:
	if (m_SourceType != a_Other.m_SourceType)
	{
		return (m_SourceType < a_Other.m_SourceType);
	}

	// SourceType is the same, use SourceIdentifier for comparison:
	if (m_SourceIdentifier != a_Other.m_SourceIdentifier)
	{
		return (m_SourceIdentifier < a_Other.m_SourceIdentifier);
	}

	// As the last effor, compare filenames:
	if (m_FileName != a_Other.m_FileName)
	{
		return (m_FileName < a_Other.m_FileName);
	}
	return (m_InnerFileName < a_Other.m_InnerFileName);
}





void LogFile::constructDisplayName()
{
	QString fn = m_InnerFileName.isEmpty() ? m_FileName : m_InnerFileName;
	QFileInfo fi(fn);
	m_DisplayName = fi.fileName();
}





LogFile::SourceType LogFile::tryIdentifySourceType() const
{
	// Based on the module names in log messages:
	static const std::string modMultiProxy   = "CMultiProxyToMultiAgentConnectorModule";
	static const std::string modVAHConnector = "CVAHConnectorModule";
	static const std::string modMDMConnector = "CMDMConnectorModule";
	static const std::string modOSConnector  = "CSystemConnectorModule";
	for (const auto & msg: m_Messages)
	{
		if (msg.m_Module == modMultiProxy)
		{
			return SourceType::stMultiProxy;
		}
		if ((msg.m_Module == modVAHConnector) || (msg.m_Module == modMDMConnector))
		{
			return SourceType::stMultiAgent;
		}
		if (msg.m_Module == modOSConnector)
		{
			return SourceType::stAgent;
		}
	}  // for msg - m_Messages[]

	// TODO: Based on the filenames

	// Not recognized:
	return SourceType::stUnknown;
}




