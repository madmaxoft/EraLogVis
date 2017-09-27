// LogFile.cpp

// Implements the LogFile class representing a single log file's contents





#include "LogFile.h"
#include <assert.h>
#include <QFileInfo>
#include "Exceptions.h"





LogFile::LogFile(const QString & a_FileName,
	const QString & a_InnerFileName,
	SourceType a_SourceType,
	const QString & a_SourceIdentifier,
	std::string && a_CompleteText
):
	m_FileName(a_FileName),
	m_InnerFileName(a_InnerFileName),
	m_SourceType(a_SourceType),
	m_SourceIdentifier(a_SourceIdentifier),
	m_CompleteText(std::move(a_CompleteText))
{
	constructDisplayName();
}




void LogFile::addMessage(
	QDateTime && a_DateTime,
	LogLevel a_LogLevel,
	const std::string & a_Module,
	quint64 a_ThreadID,
	size_t a_TextStart, size_t a_TextLength
)
{
	assert(a_TextStart + a_TextLength <= m_CompleteText.size());
	auto moduleIdx = moduleToIdentifier(a_Module);
	m_Messages.emplace_back(
		std::move(a_DateTime),
		a_LogLevel,
		moduleIdx,
		a_ThreadID,
		a_TextStart, a_TextLength
	);
}




bool LogFile::appendContinuationToLastMessage(size_t a_AddLength)
{
	if (m_Messages.empty())
	{
		return false;
	}
	m_Messages.back().m_TextLength += a_AddLength;
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





std::string LogFile::identifierToModule(int a_ModuleIdentifier) const
{
	auto itr = m_IdentifierToModule.find(a_ModuleIdentifier);
	if (itr == m_IdentifierToModule.end())
	{
		return std::string();
	}
	return itr->second;
}





QString LogFile::getMessageText(const Message & a_Message) const
{
	assert(a_Message.m_TextStart + a_Message.m_TextLength <= m_CompleteText.size());
	return QString::fromUtf8(
		m_CompleteText.c_str() + a_Message.m_TextStart,
		static_cast<int>(a_Message.m_TextLength)
	);
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
	// Specific unique modules present in the log indicate the source type:
	auto end = m_ModuleToIdentifier.end();
	if (m_ModuleToIdentifier.find("CMultiProxyToMultiAgentConnectorModule") != end)
	{
		return SourceType::stMultiProxy;
	}
	if (m_ModuleToIdentifier.find("CVAHConnectorModule") != end)
	{
		return SourceType::stMultiAgent;
	}
	if (m_ModuleToIdentifier.find("CMDMConnectorModule") != end)
	{
		return SourceType::stMultiAgent;
	}
	if (m_ModuleToIdentifier.find("CSystemConnectorModule") != end)
	{
		return SourceType::stAgent;
	}

	// TODO: Based on the filenames

	// Not recognized:
	return SourceType::stUnknown;
}





int LogFile::moduleToIdentifier(const std::string & a_ModuleName)
{
	auto itr = m_ModuleToIdentifier.find(a_ModuleName);
	if (itr != m_ModuleToIdentifier.end())
	{
		// Already known, return the found identifier:
		return itr->second;
	}

	// Not found, add it to both maps:
	auto identifier = static_cast<int>(m_ModuleToIdentifier.size());
	m_ModuleToIdentifier[a_ModuleName] = identifier;
	m_IdentifierToModule[identifier] = a_ModuleName;
	return identifier;
}




