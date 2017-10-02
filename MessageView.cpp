// MessageView.cpp

// Implements the MessageView class representing the view widget for log messages





#include "MessageView.h"
#include <QHeaderView>
#include <QTimerEvent>
#include <QScrollBar>
#include <QPainter>
#include <QPaintEvent>
#include <QDebug>





MessageView::MessageView(QWidget * a_Parent):
	Super(a_Parent),
	m_CurrentModel(nullptr),
	m_Header(new QHeaderView(Qt::Horizontal, this)),
	m_TimerIDUpdate(0),
	m_TimerIDColumnResize(0),
	m_RowHeight(fontMetrics().lineSpacing() + 1)
{
	m_Header->setParent(this);
	m_Header->setStretchLastSection(true);
	m_Header->setSectionsClickable(false);
	m_Header->setHighlightSections(false);
	connect(m_Header, SIGNAL(sectionResized(int, int, int)),   this, SLOT(columnResized(int, int, int)));
	connect(m_Header, SIGNAL(sectionHandleDoubleClicked(int)), this, SLOT(resizeColumnToContents(int)));
	connect(m_Header, SIGNAL(geometriesChanged()),             this, SLOT(updateGeometries()));
}





void MessageView::setModel(QAbstractItemModel * a_Model)
{
	// Bail out if no change:
	if (a_Model == m_CurrentModel)
	{
		return;
	}

	// Disconnect the old model:
	if (m_CurrentModel)
	{
		disconnect(m_CurrentModel, SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(modelRowsInserted(QModelIndex, int, int)));
		disconnect(m_CurrentModel, SIGNAL(rowsRemoved (QModelIndex, int, int)), this, SLOT(modelRowsRemoved (QModelIndex, int, int)));
	}

	// Connect the new model:
	m_CurrentModel = a_Model;
	if (a_Model)
	{
		connect(a_Model, SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(modelRowsInserted(QModelIndex, int, int)));
		connect(a_Model, SIGNAL(rowsRemoved (QModelIndex, int, int)), this, SLOT(modelRowsRemoved (QModelIndex, int, int)));
	}
	m_Header->setModel(a_Model);

	Super::setModel(a_Model);
}





void MessageView::setColumnWidth(int a_Column, int a_Width)
{
	m_Header->resizeSection(a_Column, a_Width);
}





void MessageView::queueUpdate()
{
	if (m_TimerIDUpdate == 0)
	{
		m_TimerIDUpdate = startTimer(0);
	}
}





void MessageView::updateDimensions()
{
	// Update the scrollbar:
	auto rowCount = (m_CurrentModel == nullptr) ? 0 : m_CurrentModel->rowCount();
	auto vs = verticalScrollBar();
	auto viewportSize = viewport()->size();
	vs->setPageStep(viewportSize.height());
	vs->setRange(0, rowCount * m_RowHeight - viewportSize.height());
}





void MessageView::timerEvent(QTimerEvent * a_Event)
{
	auto timerID =  a_Event->timerId();
	if (timerID == m_TimerIDUpdate)
	{
		killTimer(timerID);
		updateDimensions();
		viewport()->update();
		m_TimerIDUpdate = 0;
	}
	else if (timerID == m_TimerIDColumnResize)
	{
		killTimer(timerID);
		syncResizeColumns();
		m_TimerIDColumnResize = 0;
	}
	Super::timerEvent(a_Event);
}





void MessageView::paintEvent(QPaintEvent * a_Event)
{
	// setup temp variables for the painting
	// const bool rightToLeft = isRightToLeft();
	QStyleOptionViewItem option = viewOptions();

	QPainter painter(viewport());

	// if there's nothing to do, clear the area and return
	if ((m_CurrentModel == nullptr) || (m_CurrentModel->rowCount() == 0))
	{
		return;
	}

	int scrollPos = verticalScrollBar()->value();
	int maxBottom = scrollPos + viewport()->height();
	int maxRight = m_Header->length() - m_Header->offset();

	const QRegion region = a_Event->region();

	for(QRect dirtyArea: region.rects())
	{
		dirtyArea.translate(0, scrollPos);
		dirtyArea.setBottom(qMin(dirtyArea.bottom(), maxBottom));
		dirtyArea.setRight(qMin(dirtyArea.right(), maxRight));

		// get the horizontal start and end visual sections
		int left = m_Header->visualIndexAt(dirtyArea.left());
		int right = m_Header->visualIndexAt(dirtyArea.right());
		if (left == -1) left = 0;
		if (right == -1) right = m_Header->count() - 1;

		// get the vertical start and end visual sections
		int bottom = dirtyArea.bottom() / m_RowHeight;
		int top = dirtyArea.top() / m_RowHeight;
		if (top > bottom)
		{
			continue;
		}

		// Paint each row item
		QModelIndex root;
		auto selModel = selectionModel();
		for (int visualRowIndex = top; visualRowIndex <= bottom; ++visualRowIndex)
		{
			int row = visualRowIndex;
			int rowY = row * m_RowHeight - scrollPos;
			int rowh = m_RowHeight - 1;

			// Paint each column item
			for (int visualColumnIndex = left; visualColumnIndex <= right; ++visualColumnIndex)
			{
				int col = m_Header->logicalIndex(visualColumnIndex);
				if (m_Header->isSectionHidden(col))
				{
					continue;
				}
				int colp = m_Header->sectionViewportPosition(col);
				int colw = m_Header->sectionSize(col) - 1;

				const QModelIndex index = m_CurrentModel->index(row, col, root);
				auto opt = option;
				if (index.isValid())
				{
					style()->drawPrimitive(QStyle::PE_PanelItemViewRow, &opt, &painter, this);
					opt.rect = QRect(colp, rowY, colw, rowh);
					if (selModel && selModel->isSelected(index))
					{
						opt.state |= QStyle::State_Selected;
					}
					itemDelegate(index)->paint(&painter, opt, index);
				}
			}
		}

		/*
		if (showGrid) {
			// Find the bottom right (the last rows/columns might be hidden)
			while (verticalHeader->isSectionHidden(verticalHeader->logicalIndex(bottom))) --bottom;
			QPen old = painter.pen();
			painter.setPen(gridPen);
			// Paint each row
			for (int visualIndex = top; visualIndex <= bottom; ++visualIndex) {
				int row = verticalHeader->logicalIndex(visualIndex);
				if (verticalHeader->isSectionHidden(row))
					continue;
				int rowY = rowViewportPosition(row);
				rowY += offset.y();
				int rowh = rowHeight(row) - gridSize;
				painter.drawLine(dirtyArea.left(), rowY + rowh, dirtyArea.right(), rowY + rowh);
			}

			// Paint each column
			for (int h = left; h <= right; ++h) {
				int col = m_Header->logicalIndex(h);
				if (m_Header->isSectionHidden(col))
					continue;
				int colp = columnViewportPosition(col);
				colp += offset.x();
				if (!rightToLeft)
					colp +=  columnWidth(col) - gridSize;
				painter.drawLine(colp, dirtyArea.top(), colp, dirtyArea.bottom());
			}

			//draw the top & left grid lines if the headers are not visible.
			//We do update this line when subsequent scroll happen (see scrollContentsBy)
			if (m_Header->isHidden() && verticalScrollMode() == ScrollPerItem)
				painter.drawLine(dirtyArea.left(), 0, dirtyArea.right(), 0);
			if (verticalHeader->isHidden() && horizontalScrollMode() == ScrollPerItem)
				painter.drawLine(0, dirtyArea.top(), 0, dirtyArea.bottom());
			painter.setPen(old);
		}
		*/
	}  // for dirtyArea: region
}





QRect MessageView::visualRect(const QModelIndex & a_Index) const
{
	auto scrollPos = verticalScrollBar()->value();
	return QRect(0, a_Index.row() * m_RowHeight - scrollPos, viewport()->width(), m_RowHeight);
}





void MessageView::scrollTo(const QModelIndex & a_Index, QAbstractItemView::ScrollHint a_ScrollHint)
{
	// Adjust vertical position:
	auto viewportHeight = viewport()->height();
	auto verticalPosition = a_Index.row() * m_RowHeight;
	auto scrollPos = verticalScrollBar()->value();

	if (a_ScrollHint == EnsureVisible)
	{
		if ((verticalPosition < scrollPos) || (m_RowHeight > viewportHeight))
		{
			a_ScrollHint = PositionAtTop;
		}
		else if ((verticalPosition + m_RowHeight - scrollPos > viewportHeight))
		{
			a_ScrollHint = PositionAtBottom;
		}
	}
	if (a_ScrollHint == PositionAtTop)
	{
		verticalScrollBar()->setValue(verticalPosition);
	}
	else if (a_ScrollHint == PositionAtBottom)
	{
		verticalScrollBar()->setValue(verticalPosition - viewportHeight + m_RowHeight);
	}
	else if (a_ScrollHint == PositionAtCenter)
	{
		verticalScrollBar()->setValue(verticalPosition - ((viewportHeight - m_RowHeight) / 2));
	}

	update(a_Index);
}





QModelIndex MessageView::indexAt(const QPoint & a_Point) const
{
	if (m_CurrentModel == nullptr)
	{
		return QModelIndex();
	}
	auto row = (a_Point.y() + verticalScrollBar()->value()) / m_RowHeight;
	auto col = m_Header->logicalIndexAt(a_Point.x());
	return m_CurrentModel->index(row, col);
}





QModelIndex MessageView::moveCursor(QAbstractItemView::CursorAction a_CursorAction, Qt::KeyboardModifiers a_KeyboardModifiers)
{
	Q_UNUSED(a_KeyboardModifiers);

	if (m_CurrentModel == nullptr)
	{
		return QModelIndex();
	}
	auto rowCount = m_CurrentModel->rowCount();
	auto colCount = m_CurrentModel->columnCount();

	if ((rowCount == 0) || (colCount == 0))
	{
		// Model is empty
		return QModelIndex();
	}

	// Get the current position:
	auto selModel = selectionModel();
	auto current = (selModel != nullptr) ? selModel->currentIndex() : QModelIndex();
	if (!current.isValid())
	{
		return m_CurrentModel->index(0, 0);
	}
	auto visualRow = current.row();
	auto visualColumn = current.column();

	if (isRightToLeft())
	{
		if (a_CursorAction == MoveLeft)
		{
			a_CursorAction = MoveRight;
		}
		else if (a_CursorAction == MoveRight)
		{
			a_CursorAction = MoveLeft;
		}
	}

	switch (a_CursorAction)
	{
		case MoveUp:
		{
			visualRow = std::max(0, visualRow - 1);
			break;
		}
		case MoveDown:
		{
			visualRow = std::min(rowCount - 1, visualRow + 1);
			break;
		}
		case MovePrevious:
		case MoveLeft:
		case MoveNext:
		case MoveRight:
		{
			// No change in the line-based selection
			break;
		}
		case MoveHome:
		{
			visualRow = 0;
			break;
		}
		case MoveEnd:
		{
			visualRow = rowCount - 1;
			break;
		}
		case MovePageUp:
		{
			auto newRow = std::max(0, visualRow - viewport()->height() / m_RowHeight + 1);
			return m_CurrentModel->index(newRow, current.column());
		}
		case MovePageDown:
		{
			auto newRow = std::min(rowCount - 1, visualRow + viewport()->height() / m_RowHeight - 1);
			return m_CurrentModel->index(newRow, current.column());
		}
	}

	if (!m_CurrentModel->hasIndex(visualRow, visualColumn))
	{
		return QModelIndex();
	}

	return m_CurrentModel->index(visualRow, visualColumn);
}





int MessageView::horizontalOffset(void) const
{
	return m_Header->offset();
}





int MessageView::verticalOffset(void) const
{
	return 0;
}





bool MessageView::isIndexHidden(const QModelIndex & a_Index) const
{
	Q_UNUSED(a_Index);
	return false;
}





void MessageView::setSelection(const QRect & a_Rect, QItemSelectionModel::SelectionFlags a_Flags)
{
	auto scrollPos = verticalScrollBar()->value();
	auto maxCol = (m_CurrentModel == nullptr) ? 0 : m_CurrentModel->columnCount() - 1;
	auto tl = m_CurrentModel->index((a_Rect.top() + scrollPos) / m_RowHeight, 0);
	auto br = m_CurrentModel->index((a_Rect.bottom() + scrollPos) / m_RowHeight, maxCol);
	QItemSelection selection;
	QItemSelectionRange range(tl, br);
	if (!range.isEmpty())
	{
		selection.append(range);
	}
	selectionModel()->select(selection, a_Flags);
}





QRegion MessageView::visualRegionForSelection(const QItemSelection & a_Selection) const
{
	const int gridAdjust = 1;  // showGrid() ? 1 : 0;
	const auto & viewportRect = viewport()->rect();
	QRegion selectionRegion;
	auto scrollPos = verticalScrollBar()->value();
	for (auto range: a_Selection)
	{
		if (!range.isValid())
		{
			continue;
		}

		const int rtop = range.top() * m_RowHeight - scrollPos;
		const int rbottom = (range.bottom() + 1) * m_RowHeight - scrollPos;
		int rleft;
		int rright;
		if (isLeftToRight())
		{
			rleft = m_Header->sectionViewportPosition(range.left());
			rright = m_Header->sectionViewportPosition(range.right()) + m_Header->sectionSize(range.right());
		}
		else
		{
			rleft = m_Header->sectionViewportPosition(range.right());
			rright = m_Header->sectionViewportPosition(range.left()) + m_Header->sectionSize(range.left());
		}
		const QRect rangeRect(QPoint(rleft, rtop), QPoint(rright - 1 - gridAdjust, rbottom - 1 - gridAdjust));
		if (viewportRect.intersects(rangeRect))
		{
			selectionRegion += rangeRect;
		}
	}
	return selectionRegion;
}





void MessageView::updateGeometries()
{
	// Set the viewport margins (non-scrolling areas):
	int height = 0;
	if (!m_Header->isHidden())
	{
		height = qMax(m_Header->minimumHeight(), m_Header->sizeHint().height());
		height = qMin(height, m_Header->maximumHeight());
	}
	setViewportMargins(0, height, 0, 0);

	// Update the header:
	QRect vg = viewport()->geometry();
	int horizontalTop = vg.top() - height;
	m_Header->setGeometry(vg.left(), horizontalTop, vg.width(), height);
	if (m_Header->isHidden())
	{
		QMetaObject::invokeMethod(m_Header, "updateGeometries");
	}

	// Update the vertical scroll bar:
	updateDimensions();

	Super::updateGeometries();
}





void MessageView::scrollContentsBy(int a_Dx, int a_Dy)
{
	viewport()->scroll(a_Dx, a_Dy);
}





void MessageView::syncResizeColumns()
{
	QRect rect;
	int viewportHeight = viewport()->height();
	int viewportWidth = viewport()->width();
	for (int i = m_ResizedColumns.size()-1; i >= 0; --i)
	{
		int column = m_ResizedColumns.at(i);
		int x = m_Header->sectionViewportPosition(column);
		if (isRightToLeft())
		{
			rect |= QRect(0, 0, x + m_Header->sectionSize(column), viewportHeight);
		}
		else
		{
			rect |= QRect(x, 0, viewportWidth - x, viewportHeight);
		}
	}
	viewport()->update(rect.normalized());
	m_ResizedColumns.clear();
}





void MessageView::modelRowsInserted(QModelIndex, int, int)
{
	queueUpdate();
}





void MessageView::modelRowsRemoved (QModelIndex, int, int)
{
	queueUpdate();
}





void MessageView::columnResized(int a_Column, int a_OldWidth, int a_NewWidth)
{
	Q_UNUSED(a_OldWidth);
	Q_UNUSED(a_NewWidth);

	m_ResizedColumns << a_Column;
	if (m_TimerIDColumnResize == 0)
	{
		m_TimerIDColumnResize = startTimer(0);
	}
}




