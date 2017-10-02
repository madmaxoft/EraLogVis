// SessionMessagesModel.cpp

// Implements the SessionMessagesModel class representing the QT model for displaying log messages





#include "SessionMessagesModel.h"
#include <QBrush>
#include <QDebug>
#include "Session.h"
#include "LogFile.h"
#include "Stopwatch.h"





/** Returns true if the "First" message should go in front of "Second". */
static bool isMessageEarlier(
	const LogFile::Message & a_FirstMsg,
	const LogFile & a_FirstFile,
	const LogFile::Message & a_SecondMsg,
	const LogFile & a_SecondFile
)
{
	return (
		(a_FirstMsg.m_DateTime < a_SecondMsg.m_DateTime) ||
		(
			(a_FirstMsg.m_DateTime == a_SecondMsg.m_DateTime) &&
			(a_FirstFile < a_SecondFile)
		)
	);
}





/** Incrementally reports all messages from the specified Session in the sorted order. */
class SessionMessagesModel::MessageSorter
{
public:
	MessageSorter(Session & a_Session):
		m_LogFiles(a_Session.logFiles()),
		// m_LogFilesMessages(initLogFilesMessagesVector()),
		m_NumLogFiles(a_Session.logFiles().size())
	{
		m_Indices.resize(a_Session.logFiles().size());
		for (auto & i: m_Indices)
		{
			i = 0;
		}
	}

	/** Returns the next message in the sorted order.
	If there's no message to return, returns {nullptr, 0} */
	SessionMessagesModel::MessageRow getNextMessage()
	{
		LogFile * f = nullptr;
		size_t idx = 0;
		size_t toIncrement = 0;
		for (size_t i = 0; i < m_NumLogFiles; ++i)
		{
			if (m_Indices[i] >= m_LogFiles[i]->messages().size())
			{
				// This LogFile is exhausted
				continue;
			}
			if (
				(f == nullptr) ||
				(isMessageEarlier(
					f->messages()[idx], *f,
					m_LogFiles[i]->messages()[m_Indices[i]], *m_LogFiles[i]
				))
			)
			{
				f = m_LogFiles[i].get();
				idx = m_Indices[i];
				toIncrement = i;
			}
		}
		if (f != nullptr)
		{
			m_Indices[toIncrement] += 1;
		}
		return MessageRow{f, idx};
	}


protected:

	/** The LogFile vector from m_Session. */
	const std::vector<LogFilePtr> & m_LogFiles;

	/** The messages() objects of each of m_LogFiles, in the same order. */
	// const std::vector<const std::vector<LogFile::Message> &> m_LogFilesMessages;

	/** Number of LogFilePtr instances in m_LogFiles. */
	size_t m_NumLogFiles;

	/** Per-LogFile indices for the next message in each file to consider.
	m_Session.logFiles()[i].messages()[m_Indices[i]] */
	std::vector<size_t> m_Indices;


	#if 0
	/** Creates the data that is stored in m_LogFilesMessages by the constructor. */
	std::vector<const std::vector<LogFile::Message> &> initLogFilesMessagesVector(
		const std::vector<LogFilePtr> & a_LogFiles
	)
	{
		std::vector<const std::vector<LogFile::Message> &> res;
		for (const auto & lf: a_LogFiles)
		{
			res.push_back(lf->messages());
		}
		return res;
	}
	#endif
};





////////////////////////////////////////////////////////////////////////////////
// SessionMessagesModel:

SessionMessagesModel::SessionMessagesModel(SessionPtr a_Session):
	m_Session(a_Session)
{
	connect(a_Session.get(), SIGNAL(logFileAdded(LogFilePtr)), this, SLOT(sessionLogFileAdded(LogFilePtr)));
}





int SessionMessagesModel::rowCount(const QModelIndex & a_Parent) const
{
	Q_UNUSED(a_Parent);
	Q_ASSERT(m_Session != nullptr);

	return static_cast<int>(m_MessageRows.size());
}





int SessionMessagesModel::columnCount(const QModelIndex & a_Parent) const
{
	Q_UNUSED(a_Parent);

	return colMax;
}





QVariant SessionMessagesModel::data(const QModelIndex & a_Index, int a_Role) const
{
	if ((a_Index.row() < 0) || (a_Index.row() >= static_cast<int>(m_MessageRows.size())))
	{
		qDebug() << QString("Requesting data for non-existent row %1 (out of %2).").arg(a_Index.row()).arg(m_MessageRows.size());
		return QVariant();
	}

	switch (a_Role)
	{
		case Qt::DisplayRole:
		{
			const auto & row = m_MessageRows[a_Index.row()];
			if (row.m_LogFile == nullptr)
			{
				// Happens while removing items from the model with the m_MessageRows resized to max
				return QVariant();
			}
			const auto & logFile = *(row.m_LogFile);
			const auto & msg = logFile.messages()[row.m_MessageIndex];
			static const QString dateTimeFormat = "yyyy-MM-dd HH:mm:ss";
			static const QString formatSingle = "%1";
			switch (a_Index.column())
			{
				case colDateTime: return msg.m_DateTime.toString(dateTimeFormat);
				case colLogLevel: return logLevelToString(msg.m_LogLevel);
				case colThreadID: return formatSingle.arg(msg.m_ThreadID);
				case colModule:   return moduleIdentifierToString(logFile, msg.m_ModuleIdentifier);
				case colText:     return logFile.getMessageText(msg);
				case colSource:
				{
					static const QString strAgent = "Agent";
					static const QString strMDMVAH = "MDM / VAH";
					static const QString strMultiAgent = "MA: %1";
					static const QString strMultiProxy = "MultiProxy";
					static const QString strUnknown = "?";
					switch (row.m_LogFile->sourceType())
					{
						case LogFile::SourceType::stAgent:      return strAgent;
						case LogFile::SourceType::stMDMVAH:     return strMDMVAH;
						case LogFile::SourceType::stMultiAgent: return strMultiAgent.arg(row.m_LogFile->sourceIdentifier());
						case LogFile::SourceType::stMultiProxy: return strMultiProxy;
						case LogFile::SourceType::stUnknown:    return strUnknown;
					}
					break;
				}
			}  // switch (column)
			break;
		}  // case Qt::DisplayRole

		case Qt::BackgroundRole:
		{
			auto lf = m_MessageRows[a_Index.row()].m_LogFile;
			if (lf == nullptr)
			{
				// Happens while removing items from the model with the m_MessageRows resized to max
				return QVariant();
			}
			switch (lf->sourceType())
			{
				case LogFile::SourceType::stAgent:      return QBrush(QColor(0xffffdf));
				case LogFile::SourceType::stMDMVAH:     return QBrush(QColor(0xffffff));
				case LogFile::SourceType::stMultiAgent: return QBrush(QColor(0xdfffdf));
				case LogFile::SourceType::stMultiProxy: return QBrush(QColor(0xdfffff));
				case LogFile::SourceType::stUnknown:    return QBrush(QColor(0xffdfdf));
			}
			break;
		}  // case Qt::BackgroundRole

		case ItemRoleMessagePtr:
		{
			const auto & row = m_MessageRows[a_Index.row()];
			const auto * msg = &(row.m_LogFile->messages()[row.m_MessageIndex]);
			return QVariant(reinterpret_cast<qulonglong>(msg));
			break;
		}

		case ItemRoleLogFilePtr:
		{
			auto lf = m_MessageRows[a_Index.row()].m_LogFile;
			return QVariant(reinterpret_cast<qulonglong>(lf));
			break;
		}
	}
	return QVariant();
}





QVariant SessionMessagesModel::headerData(int a_Section, Qt::Orientation a_Orientation, int a_Role) const
{
	if (a_Orientation != Qt::Horizontal)
	{
		return QVariant();
	}
	switch (a_Role)
	{
		case Qt::DisplayRole:
		{
			static const QString headerStrings[] =
			{
				"DateTime",
				"Lvl",
				"ThreadID",
				"Source",
				"Module",
				"Message",
			};
			if ((a_Section >= 0) && (a_Section < (sizeof(headerStrings) / sizeof(headerStrings[0]))))
			{
				return headerStrings[a_Section];
			}
			break;
		}  // case Qt::DisplayRole
	}  // switch (a_Role)
	return QVariant();
}





void SessionMessagesModel::setLogFileEnabled(LogFile * a_LogFile, bool a_IsEnabled)
{
	auto wasEnabled = isLogFileEnabled(a_LogFile);
	if (wasEnabled && !a_IsEnabled)
	{
		// The log file is to be disabled, remove its messages from the model:
		m_DisabledLogFiles.insert(a_LogFile);
		deleteLogFileMessages(a_LogFile);
		return;
	}

	if (!wasEnabled && a_IsEnabled)
	{
		// The log file is to be enabled, insert its messages to the model:
		m_DisabledLogFiles.erase(a_LogFile);
		insertLogFileMessages(a_LogFile);
		return;
	}
}





void SessionMessagesModel::setFilterString(const QString & a_FilterString)
{
	auto requestedFilterStringBA = a_FilterString.toUtf8();
	auto requestedFilterString = std::string(requestedFilterStringBA.data(), static_cast<size_t>(requestedFilterStringBA.size()));
	if (m_FilterString == requestedFilterString)
	{
		// Same filter string, NOP
		return;
	}
	m_FilterString = requestedFilterString;
	reFilter();
}





void SessionMessagesModel::setLogLevelFilter(LogFile::LogLevel a_LogLevel, bool a_ShouldShow)
{
	if (a_ShouldShow)
	{
		if (m_LogLevelHidden.find(a_LogLevel) == m_LogLevelHidden.end())
		{
			// Already shown, NOP
			return;
		}
		m_LogLevelHidden.erase(a_LogLevel);
		reFilter();
	}
	else
	{
		if (m_LogLevelHidden.find(a_LogLevel) != m_LogLevelHidden.end())
		{
			// Already hidden, NOP
			return;
		}
		m_LogLevelHidden.insert(a_LogLevel);
		reFilter();
	}
}





void SessionMessagesModel::sessionLogFileAdded(LogFilePtr a_LogFile)
{
	insertLogFileMessages(a_LogFile.get());
}





bool SessionMessagesModel::isLogFileEnabled(LogFile * a_LogFile) const
{
	auto itr = m_DisabledLogFiles.find(a_LogFile);
	return (itr == m_DisabledLogFiles.end());
}





QString SessionMessagesModel::logLevelToString(LogFile::LogLevel a_LogLevel)
{
	switch (a_LogLevel)
	{
		case LogFile::LogLevel::llFatal:       return "F";
		case LogFile::LogLevel::llCritical:    return "C";
		case LogFile::LogLevel::llError:       return "E";
		case LogFile::LogLevel::llWarning:     return "W";
		case LogFile::LogLevel::llInformation: return "I";
		case LogFile::LogLevel::llDebug:       return "d";
		case LogFile::LogLevel::llTrace:       return "t";
		case LogFile::LogLevel::llStatus:      return "S";
		default:
		{
			return "?";
		}
	}
}





void SessionMessagesModel::insertLogFileMessages(LogFile * a_LogFile)
{
	Stopwatch sw("Inserting LogFile messages into SessionMessagesModel");
	MessageRows origRows;
	std::swap(origRows, m_MessageRows);
	size_t origSize = origRows.size();
	size_t insSize = a_LogFile->messages().size();
	size_t newSize = origSize + insSize;
	m_MessageRows.resize(newSize);
	size_t origIdx = 0, insIdx = 0;
	QModelIndex parentIndex;
	bool isCoalescingInserts = false;
	int lastNonInsertIdx = 0;
	for (size_t i = 0; i < newSize; ++i)
	{
		if (origIdx >= origSize)
		{
			// All the original rows have been exhausted, only the new rows remain
			if (isCoalescingInserts)
			{
				beginInsertRows(parentIndex, lastNonInsertIdx, static_cast<int>(i - 1));
				endInsertRows();
				isCoalescingInserts = false;
			}
			beginInsertRows(parentIndex, static_cast<int>(i), static_cast<int>(newSize));
			for (size_t j = i; j < newSize; ++j)
			{
				m_MessageRows[j].m_LogFile = a_LogFile;
				m_MessageRows[j].m_MessageIndex = insIdx;
				insIdx += 1;
			}
			endInsertRows();
			return;
		}

		if (insIdx >= insSize)
		{
			// All of the new rows have been exhausted, only the original rows remain
			if (isCoalescingInserts)
			{
				beginInsertRows(parentIndex, lastNonInsertIdx, static_cast<int>(i - 1));
				endInsertRows();
				isCoalescingInserts = false;
			}
			for (size_t j = i; j < newSize; ++j)
			{
				m_MessageRows[j] = origRows[origIdx];
				origIdx += 1;
			}
			return;
		}

		// There are rows in both streams, pick the one with a smaller datetime
		const auto & msgOrig = origRows[origIdx].m_LogFile->messages()[origRows[origIdx].m_MessageIndex];
		const auto & msgIns  = a_LogFile->messages()[insIdx];
		if (isMessageEarlier(msgOrig, *origRows[origIdx].m_LogFile, msgIns, *a_LogFile))
		{
			// Orig should go in front of Ins
			if (isCoalescingInserts)
			{
				beginInsertRows(parentIndex, lastNonInsertIdx, static_cast<int>(i - 1));
				endInsertRows();
				isCoalescingInserts = false;
			}
			m_MessageRows[i] = origRows[origIdx];
			origIdx += 1;
		}
		else
		{
			// Ins should go in front of Orig
			if (!isCoalescingInserts)
			{
				lastNonInsertIdx = static_cast<int>(i);
				isCoalescingInserts = true;
			}
			m_MessageRows[i].m_LogFile = a_LogFile;
			m_MessageRows[i].m_MessageIndex = insIdx;
			insIdx += 1;
		}
	}  // for i
}





void SessionMessagesModel::deleteLogFileMessages(LogFile * a_LogFile)
{
	Stopwatch sw("Deleting LogFile messages from SessionMessagesModel");
	size_t idx = 0;
	size_t origSize = m_MessageRows.size();
	QModelIndex parentIndex;
	size_t lastChangeIdx = 0;
	size_t numDeleted;
	bool lastIsDeleting = false;
	for (size_t i = 0; i < origSize; ++i)
	{
		if (m_MessageRows[i].m_LogFile == a_LogFile)
		{
			if (!lastIsDeleting)
			{
				lastChangeIdx = idx;
				lastIsDeleting = true;
				numDeleted = 0;
			}
			else
			{
				numDeleted += 1;
			}
		}
		else
		{
			if (lastIsDeleting)
			{
				// We were deleting up to now, coalescing the row deletion notifications
				beginRemoveRows(parentIndex, static_cast<int>(lastChangeIdx), static_cast<int>(lastChangeIdx + numDeleted));
				endRemoveRows();
				lastIsDeleting = false;
			}
			m_MessageRows[idx] = m_MessageRows[i];
			idx += 1;
		}
	}
	if (lastIsDeleting)
	{
		beginRemoveRows(parentIndex, static_cast<int>(lastChangeIdx), static_cast<int>(lastChangeIdx + numDeleted));
		endRemoveRows();
	}
	m_MessageRows.resize(idx);
}





void SessionMessagesModel::reFilter()
{
	// Re-create the m_MessageRows based on current filter settings
	// Coalesce insertions and removals for better performance
	Stopwatch sw("Refiltering");
	MessageSorter sorter(*m_Session);
	std::vector<MessageRow> oldRows;  // Remember the current rows
	std::swap(oldRows, m_MessageRows);
	m_MessageRows.resize(m_Session->getMessageCount());
	size_t oldIdx = 0;  // Index into oldRows[] for the next row to process
	size_t newIdx = 0;  // Index into m_MessageRows[] for the next row to assign
	QModelIndex parent;
	enum
	{
		sKeeping,
		sInserting,
		sDeleting,
	} state = sKeeping;
	int numCoalesced = 0;
	for (auto msg = sorter.getNextMessage(); msg.m_LogFile != nullptr; msg = sorter.getNextMessage())
	{
		const auto & m = msg.m_LogFile->messages()[msg.m_MessageIndex];
		if (shouldShowMessage(*msg.m_LogFile, m))
		{
			m_MessageRows[newIdx] = msg;
			newIdx += 1;
			if ((oldIdx < oldRows.size()) && (msg.m_LogFile == oldRows[oldIdx].m_LogFile))
			{
				// Keeping an old row:
				oldIdx += 1;
				switch (state)
				{
					case sKeeping: break;
					case sDeleting:
					{
						beginRemoveRows(parent, static_cast<int>(newIdx - 1), static_cast<int>(newIdx + numCoalesced - 1));
						endRemoveRows();
						numCoalesced = 0;
						break;
					}
					case sInserting:
					{
						beginInsertRows(parent, static_cast<int>(newIdx - numCoalesced), static_cast<int>(newIdx));
						endInsertRows();
						numCoalesced = 0;
						break;
					}
				}  // switch (state)
				state = sKeeping;
			}
			else
			{
				// Inserting a new row:
				switch (state)
				{
					case sKeeping:
					{
						numCoalesced = 0;
						break;
					}
					case sDeleting:
					{
						beginRemoveRows(parent, static_cast<int>(newIdx - 1), static_cast<int>(newIdx + numCoalesced - 1));
						endRemoveRows();
						state = sInserting;
						numCoalesced = 0;
					}
					case sInserting:
					{
						numCoalesced += 1;
						break;
					}
				}  // switch (state)
				state = sInserting;
			}
		}
		else
		{
			if ((oldIdx < oldRows.size()) && (msg.m_LogFile == oldRows[oldIdx].m_LogFile))
			{
				// Removing an existing row:
				oldIdx += 1;
				switch (state)
				{
					case sKeeping:
					{
						numCoalesced = 0;
						break;
					}
					case sDeleting:
					{
						numCoalesced += 1;
						break;
					}
					case sInserting:
					{
						beginInsertRows(parent, static_cast<int>(newIdx - numCoalesced), static_cast<int>(newIdx));
						endInsertRows();
						numCoalesced = 0;
						break;
					}
				}  // switch (state)
				state = sDeleting;
			}
			else
			{
				// Ignoring a row:
				// Nothing needed
			}
		}
	}  // for msg: sorter.getNextMessage
	switch (state)
	{
		case sKeeping: break;  // Nothing needed
		case sInserting:
		{
			beginInsertRows(parent, static_cast<int>(newIdx - numCoalesced), static_cast<int>(newIdx));
			endInsertRows();
			break;
		}
		case sDeleting:
		{
			if (newIdx > 0)
			{
				beginRemoveRows(parent, static_cast<int>(newIdx - 1), static_cast<int>(newIdx + numCoalesced - 1));
			}
			else
			{
				beginRemoveRows(parent, static_cast<int>(newIdx), static_cast<int>(newIdx + numCoalesced));
			}
			endRemoveRows();
			break;
		}
	}
	m_MessageRows.resize(newIdx);
}





bool SessionMessagesModel::shouldShowMessage(const LogFile & a_LogFile, const LogFile::Message & a_Message) const
{
	// Check the LogFile against the set of disabled ones:
	if (m_DisabledLogFiles.find(&a_LogFile) != m_DisabledLogFiles.end())
	{
		return false;
	}

	// Check the LogLevel against the set of disabled ones:
	if (m_LogLevelHidden.find(a_Message.m_LogLevel) != m_LogLevelHidden.end())
	{
		return false;
	}

	/*
	// TODO: Check m_FilterString:
	if (!m_FilterString.empty())
	{
		if (a_LogFile.getMessageTextString(a_Message).find(m_FilterString))
		{
			// Doesn't match m_FilterString, discard:
			return false;
		}
	}
	*/

	return true;
}





QString SessionMessagesModel::moduleIdentifierToString(const LogFile & a_LogFile, int a_ModuleIdentifier) const
{
	auto moduleName = a_LogFile.identifierToModule(a_ModuleIdentifier);
	return QString::fromStdString(moduleName);
}





