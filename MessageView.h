// MessageView.h

// Declares the MessageView class representing the view widget for log messages





#ifndef MESSAGEVIEW_H
#define MESSAGEVIEW_H





#include <QAbstractItemView>





// fwd:
class QHeaderView;






class MessageView:
	public QAbstractItemView
{
	Q_OBJECT
	typedef QAbstractItemView Super;


public:

	MessageView(QWidget * a_Parent = nullptr);


	/** Sets the specified model to be shown.
	Reconnects the model's signals, updates the viewport. */
	virtual void setModel(QAbstractItemModel * a_Model) override;

	/** Sets the width of the specified header column. */
	void setColumnWidth(int a_Column, int a_Width);

	// QAbstractItemView overrides:
	virtual void scrollTo(const QModelIndex & a_Index, ScrollHint a_ScrollHint = EnsureVisible) override;

protected:

	/** The model being displayed by this widget. */
	QAbstractItemModel * m_CurrentModel;

	/** The column header. */
	QHeaderView * m_Header;

	/** ID of the timer used for async updating. */
	int m_TimerIDUpdate;

	/** ID of the timer used for async column resizing. */
	int m_TimerIDColumnResize;

	/** List of columns that have been resized since the last refresh. */
	QList<int> m_ResizedColumns;

	/** Height of a single row, in pixels. Cached from the FontInfo for better perf. */
	int m_RowHeight;



	/** Sets up a short timer, after which the viewport is updated. */
	void queueUpdate();

	/** Updates the dimensions from the model. */
	void updateDimensions();

	virtual void timerEvent(QTimerEvent * a_Event) override;
	virtual void paintEvent(QPaintEvent * a_Event) override;

	// QAbstractItemView overrides:
	virtual QRect visualRect(const QModelIndex & a_Index) const override;
	virtual QModelIndex indexAt(const QPoint & a_Point) const override;
	virtual QModelIndex moveCursor(QAbstractItemView::CursorAction a_CursorAction, Qt::KeyboardModifiers a_KeyboardModifiers) override;
	virtual int horizontalOffset(void) const override;
	virtual int verticalOffset(void) const override;
	virtual bool isIndexHidden(const QModelIndex & a_Index) const override;
	virtual void setSelection(const QRect & a_Rect, QItemSelectionModel::SelectionFlags a_Flags) override;
	virtual QRegion visualRegionForSelection(const QItemSelection & a_Selection) const override;
	virtual void updateGeometries() override;
	virtual void scrollContentsBy(int a_Dx, int a_Dy) override;

	/** Process all pending column resizes (in m_ResizedColumns). */
	void syncResizeColumns();

protected slots:

	/** Emitted by the model after rows have been inserted. */
	void modelRowsInserted(QModelIndex, int, int);

	/** Emitted by the model after rows have been removed. */
	void modelRowsRemoved (QModelIndex, int, int);

	/** Emitted by m_Header when its section is resized. */
	void columnResized(int a_Column, int a_OldWidth, int a_NewWidth);

};





#endif // MESSAGEVIEW_H
