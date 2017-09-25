// StreamUngzipper.cpp

// Implements the StreamUngzipper class representing a stream filter that ungzips data passing through it





#include "StreamUngzipper.h"





StreamUngzipper::StreamUngzipper(QIODevice & a_ParentStream):
	m_ParentStream(a_ParentStream),
	m_IsEOF(false)
{
	memset(&m_ZlibStream, 0, sizeof(m_ZlibStream));
	auto res = inflateInit2(&m_ZlibStream, 31);  // Force GZIP decoding
	if (res != Z_OK)
	{
		qDebug("%s: uncompression initialization failed: %d (\"%s\").", __FUNCTION__, res, m_ZlibStream.msg);
		m_IsEOF = true;
	}

	fillBuffer();
}





StreamUngzipper::~StreamUngzipper()
{
	inflateEnd(&m_ZlibStream);
}





qint64 StreamUngzipper::readData(char * a_Data, qint64 a_MaxLen)
{
	qint64 cur = 0;
	m_ZlibStream.avail_out = static_cast<uInt>(a_MaxLen);
	m_ZlibStream.next_out = reinterpret_cast<Bytef *>(a_Data);
	while (m_ZlibStream.avail_out > 0)
	{
		// If there's no data in the buffer, try to re-fill it:
		if (m_ZlibStream.avail_in == 0)
		{
			if (m_IsEOF)
			{
				return (cur == 0) ? -1 : cur;  // If no bytes could be read, indicate EOF upwards
			}
			fillBuffer();
			if (m_ZlibStream.avail_in == 0)
			{
				m_IsEOF = true;
				return -1;  // No more bytes here
			}
		}

		// Decompress data from m_Buffer into a_Data:
		auto prevAvailOut = m_ZlibStream.avail_out;
		auto res = inflate(&m_ZlibStream, Z_SYNC_FLUSH);
		switch (res)
		{
			case Z_OK: break;
			case Z_STREAM_END:
			{
				return a_MaxLen - m_ZlibStream.avail_out;
			}
			default:
			{
				m_IsEOF = true;
				return -1;
			}
		}
		if (prevAvailOut == m_ZlibStream.avail_out)
		{
			// No data was decompressed, bail out with an error to prevent an endless loop:
			return -1;
		}
	}  // while (more data to read)
	return a_MaxLen;
}





qint64 StreamUngzipper::writeData(const char * a_Data, qint64 a_MaxLen)
{
	Q_UNUSED(a_Data);
	Q_UNUSED(a_MaxLen);

	// Writing is not supported
	return -1;
}





void StreamUngzipper::fillBuffer()
{
	Q_ASSERT(m_ZlibStream.avail_in == 0);  // Is the buffer empty?

	if (m_IsEOF)
	{
		return;
	}

	// Read more data from m_ParentStream into m_Buffer:
	m_ZlibStream.next_in = reinterpret_cast<Bytef *>(m_Buffer);
	auto numRead = m_ParentStream.read(m_Buffer, sizeof(m_Buffer));
	if (numRead < 0)
	{
		// No more data in the stream
		m_IsEOF = true;
		m_ZlibStream.avail_in = 0;
	}
	else
	{
		m_ZlibStream.avail_in = static_cast<uInt>(numRead);
	}
}



