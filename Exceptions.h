// Exceptions.h

// Declares the exception types used by the project





#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H





class EException:
	public std::runtime_error
{
	typedef std::runtime_error Super;

public:
	/** The filename where the exception was thrown. */
	std::string m_FileName;

	/** The line number where the exception was thrown. */
	int m_Line;

	explicit EException(const char * a_FileName, int a_Line):
		Super("EException"),
		m_FileName(a_FileName),
		m_Line(a_Line)
	{
	}
};





class EIndexOutOfBounds:
	public EException
{
	typedef EException Super;

public:
	/** The number of items that could be indexed.
	The maximum index is one less than this. */
	size_t m_NumItems;

	/** The actual index used that caused this exception. */
	size_t m_UsedIndex;

	explicit EIndexOutOfBounds(const char * a_FileName, int a_Line, size_t a_NumItems, size_t a_UsedIndex):
		Super(a_FileName, a_Line),
		m_NumItems(a_NumItems),
		m_UsedIndex(a_UsedIndex)
	{
	}
};





#endif // EXCEPTIONS_H
