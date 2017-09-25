// main.cpp

// Implements the main entrypoint to the app





#include "MainWindow.h"
#include <QApplication>





int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MainWindow w;
	w.showMaximized();

	// Command line:
	// EraLogVis -f <folder1> -f <folder2> <file1> <file2> -f <folder3> ...
	auto & backgroundParser = w.getBackgroundParser();
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-f") == 0)
		{
			if (i < argc - 1)
			{
				backgroundParser.addFolder(QString::fromUtf8(argv[i + 1]));
			}
			i += 1;
			continue;
		}
		backgroundParser.addFile(QString::fromUtf8(argv[i]));
	}

	return a.exec();
}
