// SessionSourcesModel.h

// Declares the SessionSourcesModel class representing the tree-view item model for the session's sources (individual log files)





#ifndef SESSIONSOURCESMODEL_H
#define SESSIONSOURCESMODEL_H





#include <memory>
#include <QStandardItemModel>





// fwd:
class LogFile;
class Session;
typedef std::shared_ptr<LogFile> LogFilePtr;
typedef std::shared_ptr<Session> SessionPtr;





class SessionSourcesModel:
	public QStandardItemModel
{
	Q_OBJECT

public:

	enum
	{
		ItemRoleLogFilePtr = Qt::UserRole + 1,
	};

	/** Creates a new model based tied to the specified session.
	Automatically connects the session's signals. */
	SessionSourcesModel(SessionPtr a_Session);

protected:

	// The root items for the log sources:
	QStandardItem * m_RootItemMDMVAH;
	QStandardItem * m_RootItemMultiAgent;
	QStandardItem * m_RootItemMultiProxy;
	QStandardItem * m_RootItemAgent;
	QStandardItem * m_RootItemUnknown;

	/** The per-UUID subitems for MultiAgent logfiles.
	Maps the LogFile's SourceIdentification string to the tree item representing the log group of that source. */
	std::map<QString, QStandardItem *> m_MultiAgentUUIDItems;

	/** The session represented by this model. */
	SessionPtr m_Session;


	/** Adds the specified logfile to the item list. */
	void addLogFile(LogFilePtr a_LogFile);

protected slots:

	/** Triggered when a LogFile is added to the session. */
	void sessionLogFileAdded(LogFilePtr a_LogFile);

	/** Returns the item under which the specified LogFile's item should be nested.
	For MultiAgent logfiles, creates the MultiAgent UUID item, if needed. */
	QStandardItem * getLogFileParentItem(LogFile * a_LogFile);
};





#endif // SESSIONSOURCESMODEL_H
