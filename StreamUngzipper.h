// StreamUngzipper.cpp

// Declares the StreamUngzipper class representing a stream filter that ungzips data passing through it





#ifndef STREAMUNGZIPPER_H
#define STREAMUNGZIPPER_H





#include <QIODevice>
#include <QtZlib/zlib.h>





/** IO device that reads data from another IO device and un-gzips it. */
class StreamUngzipper:
	public QIODevice
{
public:
	StreamUngzipper(QIODevice & a_ParentStream);

	~StreamUngzipper();

protected:
	/** The stream from which the compressed data is read. */
	QIODevice & m_ParentStream;

	/** Buffer for the data read from m_ParentStream waiting for decompression.
	m_ZlibStream.next_in points into this buffer and m_ZlibStream.avail_in indicates
	the number of bytes still available for decompression.
	fillBuffer() reads more data from m_ParentStream into this buffer. */
	char m_Buffer[64000];

	/** Flag that indicates an EOF on the parent stream. */
	bool m_IsEOF;

	/** Opaque Zlib data used for decompression. */
	z_stream m_ZlibStream;


	// QIODevice overrides:
	virtual qint64 readData(char * a_Data, qint64 a_MaxLen) override;
	virtual qint64 writeData(const char * a_Data, qint64 a_MaxLen) override;

	/** Fills m_Buffer with more data to decompress, if possible. */
	void fillBuffer();
};





#endif // STREAMUNGZIPPER_H
