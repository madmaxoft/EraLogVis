#ifndef MAINWINDOW_H
#define MAINWINDOW_H





#include <memory>
#include <QMainWindow>
#include "BackgroundParser.h"





// fwd:
class Session;
class SessionSourcesModel;
class SessionMessagesModel;
class QStandardItem;
typedef std::shared_ptr<Session> SessionPtr;





namespace Ui
{
	class MainWindow;
}





class MainWindow:
	public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget * a_Parent = nullptr);

	~MainWindow();

	BackgroundParser & getBackgroundParser() { return m_BackgroundParser; }

private:
	std::unique_ptr<Ui::MainWindow> m_UI;

	/** The log file parser. */
	BackgroundParser m_BackgroundParser;


	/** Connects all UI signals for this window. */
	void connectSignals();

public slots:
	/** Displays the UI to choose a file, then opens that file. */
	void openFile();

	/** Opens the specified file (appends to current session). */
	void openFile(const QString & a_FileName);

	/** Displays the UI to choose a folder, then opens all files in that folder. */
	void openFolder();

	/** Opens the Find messages dialog, selects the next message containing m_FindText. */
	void findMessages();

	/** Selects the next message (relative to current selection) containing m_FindText. */
	void findNextMessage();

	/** Opens the Filter messages dialog for filtering messages, or clears the current message filter (toggle). */
	void filterMessages(bool a_StartFiltering);


protected slots:

	/** Emitted by m_SourcesModel when one of its items is changed. */
	void sourceItemChanged(QStandardItem * a_Item);

	/** Emitted by FileParser when the file cannot be recognized. */
	void failedToRecognizeFile(const QString & a_Details);

	/** Emitted by FileParser when there's an error while parsing. */
	void parseFailed(const QString & a_Details);

	/** Emitted by BackgroundParser when it parses an entire file. */
	void finishedParsingFile(LogFilePtr a_Data
	);

	/** Emitted when a LogLevel action is toggled, modifies the filter.
	Uses sender() to recognize which loglevel to toggle - therefore protected. */
	void logLevelToggled(bool a_IsChecked);


protected:

	/** The session that is being serviced by this window. */
	SessionPtr m_Session;

	/** The model used by tvSources to display the LogFile list. */
	std::shared_ptr<SessionSourcesModel> m_SourcesModel;

	/** The model used by lvMessages to display the log messages. */
	std::shared_ptr<SessionMessagesModel> m_MessagesModel;

	/** The text that has been used in Find the last time.
	Used for FindNext functionality in findMessage() and findNextMessage(). */
	QString m_FindText;


	/** Returns the filenames of all log files in the specified folder (recursive). */
	QStringList getFolderLogFiles(const QString & a_FolderPath);
};





#endif // MAINWINDOW_H
