// Stopwatch.h

// Declares the Stopwatch class that measures the time between its constructor and destructor calls
// The destructor then logs a message with the duration





#ifndef STOPWATCH_H
#define STOPWATCH_H





#include <chrono>
#include <QString>





class Stopwatch
{
public:
	Stopwatch(const QString & a_DisplayName);
	~Stopwatch();

protected:
	QString m_DisplayName;

	std::chrono::high_resolution_clock::time_point m_StartTime;
};





#endif // STOPWATCH_H
