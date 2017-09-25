// MainWindow.cpp

// Implements the MainWindow class representing the main app window





#include "MainWindow.h"
#include <QDebug>
#include <QFileDialog>
#include <QInputDialog>
#include <QSortFilterProxyModel>
#include <QMessageBox>
#include <QString>
#include "ui_MainWindow.h"
#include "Session.h"
#include "SessionSourcesModel.h"
#include "SessionMessagesModel.h"





MainWindow::MainWindow(QWidget * a_Parent):
	QMainWindow(a_Parent),
	m_UI(new Ui::MainWindow)
{
	// Register LogFilePtr so that it can be used in inter-thread signals/slot mechanisms:
	qRegisterMetaType<LogFilePtr>("LogFilePtr");

	m_UI->setupUi(this);

	// Add a decoration to the splitter handle to make it more visible:
	{
		auto handle = m_UI->splitHorz->handle(1);
		auto layout = new QVBoxLayout(handle);
		layout->setSpacing(0);
		layout->setMargin(0);
		auto line = new QFrame(handle);
		line->setFrameShape(QFrame::StyledPanel);
		line->setFrameShadow(QFrame::Sunken);
		layout->addWidget(line);
	}

	// Set the LogLevel action properties, so that they can be identified in logLevelToggled():
	m_UI->actLogLevelFatal->setProperty      ("LogLevel", static_cast<int>(LogFile::LogLevel::llFatal));
	m_UI->actLogLevelCritical->setProperty   ("LogLevel", static_cast<int>(LogFile::LogLevel::llCritical));
	m_UI->actLogLevelError->setProperty      ("LogLevel", static_cast<int>(LogFile::LogLevel::llError));
	m_UI->actLogLevelWarning->setProperty    ("LogLevel", static_cast<int>(LogFile::LogLevel::llWarning));
	m_UI->actLogLevelInformation->setProperty("LogLevel", static_cast<int>(LogFile::LogLevel::llInformation));
	m_UI->actLogLevelDebug->setProperty      ("LogLevel", static_cast<int>(LogFile::LogLevel::llDebug));
	m_UI->actLogLevelTrace->setProperty      ("LogLevel", static_cast<int>(LogFile::LogLevel::llTrace));
	m_UI->actLogLevelStatus->setProperty     ("LogLevel", static_cast<int>(LogFile::LogLevel::llStatus));
	m_UI->actLogLevelUnknown->setProperty    ("LogLevel", static_cast<int>(LogFile::LogLevel::llUnknown));

	connectSignals();

	m_Session = std::make_shared<Session>();
	m_SourcesModel = std::make_shared<SessionSourcesModel>(m_Session);
	m_UI->tvSources->setModel(m_SourcesModel.get());
	connect(
		m_SourcesModel.get(), SIGNAL(itemChanged(QStandardItem *)),
		this, SLOT(sourceItemChanged(QStandardItem *))
	);
	m_UI->tvSources->expandToDepth(1);

	m_MessagesModel = std::make_shared<SessionMessagesModel>(m_Session);
	m_UI->lvMessages->setModel(m_MessagesModel.get());
	m_UI->lvMessages->setColumnWidth(0, 120);
	m_UI->lvMessages->setColumnWidth(1, 25);
	m_UI->lvMessages->setColumnWidth(2, 100);
	m_UI->lvMessages->setColumnWidth(3, 150);
	m_UI->lvMessages->setColumnWidth(4, 150);
}





MainWindow::~MainWindow()
{
	// Nothing needed, but needs to be defined in the CPP file,
	// so that the unique_ptr<> has the full declaration of the Ui::MainWindow class
}





void MainWindow::connectSignals()
{
	connect(m_UI->actFileOpenFile,        SIGNAL(triggered()),   this, SLOT(openFile()));
	connect(m_UI->actFileOpenFolder,      SIGNAL(triggered()),   this, SLOT(openFolder()));
	connect(m_UI->actMessagesFind,        SIGNAL(triggered()),   this, SLOT(findMessages()));
	connect(m_UI->actMessagesFindNext,    SIGNAL(triggered()),   this, SLOT(findNextMessage()));
	connect(m_UI->actMessagesFilter,      SIGNAL(toggled(bool)), this, SLOT(filterMessages(bool)));
	connect(m_UI->actLogLevelFatal,       SIGNAL(toggled(bool)), this, SLOT(logLevelToggled(bool)));
	connect(m_UI->actLogLevelCritical,    SIGNAL(toggled(bool)), this, SLOT(logLevelToggled(bool)));
	connect(m_UI->actLogLevelError,       SIGNAL(toggled(bool)), this, SLOT(logLevelToggled(bool)));
	connect(m_UI->actLogLevelWarning,     SIGNAL(toggled(bool)), this, SLOT(logLevelToggled(bool)));
	connect(m_UI->actLogLevelInformation, SIGNAL(toggled(bool)), this, SLOT(logLevelToggled(bool)));
	connect(m_UI->actLogLevelDebug,       SIGNAL(toggled(bool)), this, SLOT(logLevelToggled(bool)));
	connect(m_UI->actLogLevelTrace,       SIGNAL(toggled(bool)), this, SLOT(logLevelToggled(bool)));
	connect(m_UI->actLogLevelStatus,      SIGNAL(toggled(bool)), this, SLOT(logLevelToggled(bool)));
	connect(m_UI->actLogLevelUnknown,     SIGNAL(toggled(bool)), this, SLOT(logLevelToggled(bool)));
	connect(&m_BackgroundParser,          &BackgroundParser::finishedParsingFile, this, &MainWindow::finishedParsingFile);
}





void MainWindow::openFile()
{
	auto fileNames = QFileDialog::getOpenFileNames(
		nullptr,                          // Parent widget
		tr("Open log files"),             // Title
		QString(),                        // Initial folder
		tr("Log file(*.txt *.log *.gz)")  // Filter
	);
	for (const auto & fileName: fileNames)
	{
		m_BackgroundParser.addFile(fileName);
	}
}





void MainWindow::openFile(const QString & a_FileName)
{
	// Parse the file into a new session:
	m_BackgroundParser.addFile(a_FileName);
}





void MainWindow::openFolder()
{
	auto folderPath = QFileDialog::getExistingDirectory(
		this,
		tr("Open all log files in folder")
	);
	if (!folderPath.isEmpty())
	{
		m_BackgroundParser.addFolder(folderPath);
	}
}





void MainWindow::findMessages()
{
	m_FindText = QInputDialog::getText(this, tr("Find messages"), tr("Find messages"));
	if (!m_FindText.isEmpty())
	{
		findNextMessage();
	}
}





void MainWindow::findNextMessage()
{
	if (m_FindText.isEmpty())
	{
		findMessages();
		return;
	}

	// Get the start row from which to search:
	auto sel = m_UI->lvMessages->selectionModel()->selectedIndexes();
	int lastRow = -1;
	for (const auto & idx: sel)
	{
		if (idx.row() > lastRow)
		{
			lastRow = idx.row();
		}
	}
	m_UI->lvMessages->setFocus(Qt::OtherFocusReason);

	// Search from the next row:
	auto rowCount = m_MessagesModel->rowCount(QModelIndex());
	for (auto i = lastRow + 1; i < rowCount; ++i)
	{
		auto idx = m_MessagesModel->index(i, SessionMessagesModel::colText);
		if (m_MessagesModel->data(idx).toString().contains(m_FindText))
		{
			auto tl = m_MessagesModel->index(i, 0);
			auto br = m_MessagesModel->index(i, SessionMessagesModel::colMax - 1);
			m_UI->lvMessages->selectionModel()->select(QItemSelection(tl, br), QItemSelectionModel::ClearAndSelect);
			m_UI->lvMessages->scrollTo(idx, QAbstractItemView::PositionAtCenter);
			return;
		}
	}

	// Search from the top:
	for (auto i = 0; i < lastRow; ++i)
	{
		auto idx = m_MessagesModel->index(i, SessionMessagesModel::colText);
		if (m_MessagesModel->data(idx).toString().contains(m_FindText))
		{
			auto tl = m_MessagesModel->index(i, 0);
			auto br = m_MessagesModel->index(i, SessionMessagesModel::colMax - 1);
			m_UI->lvMessages->selectionModel()->select(QItemSelection(tl, br), QItemSelectionModel::ClearAndSelect);
			m_UI->lvMessages->scrollTo(idx, QAbstractItemView::PositionAtCenter);
			return;
		}
	}
}





void MainWindow::filterMessages(bool a_StartFiltering)
{
	if (a_StartFiltering)
	{
		auto filterText = QInputDialog::getText(this, tr("Filter messages"), tr("Only show messages containing:"));
		m_MessagesModel->setFilterString(filterText);
	}
	else
	{
		m_MessagesModel->setFilterString(QString());
	}
}





void MainWindow::sourceItemChanged(QStandardItem * a_Item)
{
	// Update the Messages model based on whether this item's source is enabled or not:
	auto logFile = reinterpret_cast<LogFile *>(a_Item->data(SessionSourcesModel::ItemRoleLogFilePtr).value<void *>());
	m_MessagesModel->setLogFileEnabled(logFile, (a_Item->checkState() == Qt::Checked));
}





void MainWindow::failedToRecognizeFile(const QString & a_Details)
{
	QMessageBox::warning(
		this,
		tr("EraLogVis: Failed to parse file"),
		tr("Failed to recognize file:\n%1").arg(a_Details)
	);
}





void MainWindow::parseFailed(const QString & a_Details)
{
	QMessageBox::warning(
		this,
		tr("EraLogVis: Failed to parse file"),
		tr("Failed to parse file:\n%1").arg(a_Details)
	);
}





void MainWindow::finishedParsingFile(LogFilePtr a_Data)
{
	m_Session->appendLogFile(a_Data);
}





void MainWindow::logLevelToggled(bool a_IsChecked)
{
	auto logLevel = static_cast<LogFile::LogLevel>(sender()->property("LogLevel").toInt());
	m_MessagesModel->setLogLevelFilter(logLevel, !a_IsChecked);
}





