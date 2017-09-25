// Session.cpp

// Implements the Session class representing the whole session - all the log files currently loaded





#include "Session.h"





Session::Session()
{
	// Nothing needed yet
}





LogFilePtr Session::addLogFile(
	const QString & a_FileName,
	const QString & a_InnerFileName,
	LogFile::SourceType a_SourceType,
	const QString & a_SourceIdentifier
)
{
	auto logFile = std::make_shared<LogFile>(a_FileName, a_InnerFileName, a_SourceType, a_SourceIdentifier);
	m_LogFiles.push_back(logFile);
	emit logFileAdded(logFile);
	return logFile;
}





void Session::appendLogFile(LogFilePtr a_LogFile)
{
	m_LogFiles.push_back(a_LogFile);
	emit logFileAdded(a_LogFile);
}





void Session::merge(Session & a_Src)
{
	for (auto lf: a_Src.m_LogFiles)
	{
		m_LogFiles.push_back(lf);
		emit logFileAdded(lf);
	}
}





size_t Session::getMessageCount() const
{
	size_t res = 0;
	for (const auto & lf: m_LogFiles)
	{
		res += lf->messages().size();
	}
	return res;
}




