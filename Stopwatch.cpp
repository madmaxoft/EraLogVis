// Stopwatch.cpp

// Implements the Stopwatch class that measures the time between its constructor and destructor calls





#include "Stopwatch.h"





Stopwatch::Stopwatch(const QString & a_DisplayName):
	m_DisplayName(a_DisplayName),
	m_StartTime(std::chrono::high_resolution_clock::now())
{
}





Stopwatch::~Stopwatch()
{
	auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_StartTime).count();
	qDebug("%s took %.3f seconds", m_DisplayName.toUtf8().data(), static_cast<double>(dur) / 1000);
}
