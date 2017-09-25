// BackgroundParser.cpp

// Implements the BackgroundParser class representing the background thread pool for parsing files / folders.





#include "BackgroundParser.h"
#include <QThread>
#include <QDir>
#include <QDebug>
#include "FileParser.h"





////////////////////////////////////////////////////////////////////////////////
/** Task executed inside BackgroundParser to parse a single file. */
class FileParseTask:
	public QRunnable
{
public:
	FileParseTask(BackgroundParser & a_BackgroundParser, const QString & a_FileName):
		m_FileName(a_FileName),
		m_BackgroundParser(a_BackgroundParser)
	{
	}


	virtual void run()
	{
		FileParser parser(m_BackgroundParser.m_ShouldAbort);
		QObject::connect(&parser, &FileParser::finishedParsingFile, &m_BackgroundParser, &BackgroundParser::finishedParsingFile);
		parser.parse(m_FileName);
	}


protected:

	QString m_FileName;
	BackgroundParser & m_BackgroundParser;
};





////////////////////////////////////////////////////////////////////////////////
/** Task executed inside BackgroundParser to parse a folder. */
class FolderParseTask:
	public QRunnable
{
public:
	FolderParseTask(BackgroundParser & a_BackgroundParser, const QString & a_FolderPath):
		m_FolderPath(a_FolderPath),
		m_BackgroundParser(a_BackgroundParser)
	{
	}


	virtual void run() override
	{
		addFolderLogFiles(m_FolderPath);
	}


	void addFolderLogFiles(const QString & a_FolderPath)
	{
		static QStringList logFileNameFilters;
		if (logFileNameFilters.isEmpty())
		{
			logFileNameFilters << "*.log" << "*.gz" << "*.txt";
		}

		QDir folder(a_FolderPath);
		QStringList res;
		for (const auto & fileName: folder.entryList(logFileNameFilters, QDir::Files | QDir::Hidden | QDir::System))
		{
			m_BackgroundParser.addFile(a_FolderPath + "/" + fileName);
		}
		for (const auto & folderPath: folder.entryList(QStringList(), QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
		{
			addFolderLogFiles(a_FolderPath + "/" + folderPath);
		}
	}


protected:

	QString m_FolderPath;
	BackgroundParser & m_BackgroundParser;
};





////////////////////////////////////////////////////////////////////////////////
// BackgroundParser:

BackgroundParser::BackgroundParser():
	Super(nullptr)
{
}





BackgroundParser::~BackgroundParser()
{
	qDebug() << "Aborting all parsers";
	m_ThreadPool.clear();
	m_ShouldAbort.store(true);
}





void BackgroundParser::addFile(const QString & a_FileName)
{
	m_ThreadPool.start(new FileParseTask(*this, a_FileName));
}





void BackgroundParser::addFolder(const QString & a_FolderPath)
{
	m_ThreadPool.start(new FolderParseTask(*this, a_FolderPath));
}
