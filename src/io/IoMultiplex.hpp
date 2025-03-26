#ifndef IOMULTIPLEX_HPP
#define IOMULTIPLEX_HPP

#include <vector>

#define TEST_BITMASK(a, b) ((a & b) == b)

class	EventObject;

template <typename T>
class	IoMultiplex
{
public:
	enum	e_eventStatus
	{
		STAT_END = 0x1,
		STAT_NORMAL = 0x2,
		STAT_NONBLOCK = 0x4,
		STAT_ERROR = 0x8,
	};

	enum	e_operation
	{
		OP_ADD = 1,
		OP_DELETE = 2,
		OP_MODIFY = 3,
	};

	enum	e_filters
	{
		FILT_NONE = 0x0,
		FILT_READ = 0x1,
		FILT_WRITE = 0x2,
		FILT_ERROR = 0x4
	};

	typedef T					Event;
	typedef enum e_eventStatus	EventStatus;
	typedef std::vector<T>		EventList;

protected:
// constructors & destructor
	IoMultiplex() {};
	~IoMultiplex() {};

public:
// member functions
	void	add(int fd, e_operation op, int filter, EventObject* userData)
	{addWork(fd, op, filter, userData);}

	int		poll() {return pollWork();}

private:
	virtual void	addWork(int fd, e_operation op, int filter, EventObject* userData) = 0;
	virtual int		pollWork() = 0;
};

#endif
