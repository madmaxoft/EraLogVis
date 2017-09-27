// Session.cpp

// Implements the Session class representing the whole session - all the log files currently loaded





#include "Session.h"





Session::Session()
{
	// Nothing needed yet
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




