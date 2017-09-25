// SessionSourcesModel.cpp

// Implements the SessionSourcesModel class representing the tree-view item model for the session's sources (individual log files)





#include "SessionSourcesModel.h"
#include "Session.h"
#include "LogFile.h"





SessionSourcesModel::SessionSourcesModel(SessionPtr a_Session):
	m_Session(a_Session)
{
	// Create the base items:
	m_RootItemMDMVAH     = new QStandardItem(tr("MDM / VAH"));
	m_RootItemMultiAgent = new QStandardItem(tr("MultiAgent"));
	m_RootItemMultiProxy = new QStandardItem(tr("MultiProxy"));
	m_RootItemAgent      = new QStandardItem(tr("Agent"));
	m_RootItemUnknown    = new QStandardItem(tr("unknown"));
	appendRow(m_RootItemMDMVAH);
	appendRow(m_RootItemMultiAgent);
	appendRow(m_RootItemMultiProxy);
	appendRow(m_RootItemAgent);
	appendRow(m_RootItemUnknown);

	// Create items for existing logfiles in the session:
	for (const auto lf: a_Session->logFiles())
	{
		addLogFile(lf);
	}

	// Connect the signals from session:
	connect(a_Session.get(), SIGNAL(logFileAdded(LogFilePtr)), this, SLOT(sessionLogFileAdded(LogFilePtr)));
}





void SessionSourcesModel::sessionLogFileAdded(LogFilePtr a_LogFile)
{
	addLogFile(a_LogFile);
}





void SessionSourcesModel::addLogFile(LogFilePtr a_LogFile)
{
	// Create the item:
	auto item = new QStandardItem(a_LogFile->displayName());
	item->setCheckable(true);
	item->setCheckState(Qt::Checked);
	item->setData(QVariant::fromValue(reinterpret_cast<void *>(a_LogFile.get())), ItemRoleLogFilePtr);

	// Add to the correct parent:
	auto parent = getLogFileParentItem(a_LogFile.get());
	if (parent != nullptr)
	{
		// Insert the new item at the sorted position:
		auto numChildren = parent->rowCount();
		auto myText = item->text();
		for (int i = 0; i < numChildren; ++i)
		{
			auto ch = parent->child(i);
			if (ch->text() > myText)
			{
				parent->insertRow(i, item);
				return;
			}
		}
		parent->appendRow(item);
	}
}





QStandardItem * SessionSourcesModel::getLogFileParentItem(LogFile * a_LogFile)
{
	switch (a_LogFile->sourceType())
	{
		case LogFile::SourceType::stAgent:      return m_RootItemAgent;
		case LogFile::SourceType::stMDMVAH:     return m_RootItemMDMVAH;
		case LogFile::SourceType::stMultiProxy: return m_RootItemMultiProxy;
		case LogFile::SourceType::stUnknown:    return m_RootItemUnknown;
		case LogFile::SourceType::stMultiAgent:
		{
			// Get the per-UUID subitem of the MultiAgent subtree:
			auto itr = m_MultiAgentUUIDItems.find(a_LogFile->sourceIdentifier());
			if (itr == m_MultiAgentUUIDItems.end())
			{
				// This UUID is new, create an item for it:
				auto item = new QStandardItem(a_LogFile->sourceIdentifier());
				m_MultiAgentUUIDItems[a_LogFile->sourceIdentifier()] = item;
				return item;
			}
			return itr->second;
		}
	}
	Q_ASSERT(!"Unknown LogFile's sourceType");
	return nullptr;
}




