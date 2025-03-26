#include <unistd.h>
#include <stdexcept>
#include <errno.h>

#include "Logger.hpp"
#include "event/EventObject.hpp"
#include "Epoll.hpp"

Epoll::Epoll(Epoll const& epoll)
{
	(void)epoll;
	throw std::runtime_error("deleted function Epoll::Epoll(Epoll const&) usage");
}

Epoll&
Epoll::operator=(Epoll const& epoll)
{
	(void)epoll;
	throw std::runtime_error("deleted function Epoll::operator=(Epoll const&) usage");
	return *this;
}

Epoll::Epoll()
{
	m_epoll = epoll_create1(EPOLL_CLOEXEC);
	if (m_epoll < 0)
		throw std::runtime_error("epoll_create1() fail");
}

Epoll::~Epoll()
{
	close(m_epoll);
}

void
Epoll::addWork(int fd, e_operation op, int filter, EventObject* object)
{
	static const int	operationTable[3] = {EPOLL_CTL_ADD, EPOLL_CTL_DEL, EPOLL_CTL_MOD};
	static const uint32_t	filterTable[3] = {EPOLLIN, EPOLLOUT, EPOLLERR};
	Event	newEvent;

	::memset(&newEvent, 0, sizeof(newEvent));
	newEvent.data.ptr = object;
	for (int count = 0, bitmask = 0x1; count < 3; ++count, bitmask <<= 1)
	{
		if (filter & bitmask)
			newEvent.events |= filterTable[count];
	}
	switch (op)
	{
		case OP_ADD:
			object->m_filter = filter;
			break;
		case OP_DELETE:
			object->m_filter = 0;
      break;
		case OP_MODIFY:
			object->m_filter = filter;
			break;
	}
	if (epoll_ctl(m_epoll, operationTable[op - 1], fd, &newEvent) < 0)
		throw std::runtime_error("epoll_ctl() fail");
}

int
Epoll::pollWork()
{
	const int	maxEvent = 64;
	int			count = 0;

	m_eventList.resize(maxEvent);
	count = epoll_wait(m_epoll, m_eventList.data(), m_eventList.size(), 1000);
	if (count < 0)
		throw std::runtime_error("epoll() error");
	m_eventList.resize(count);

/*	if (m_eventList.size() > 0)
	{
		LOG(DEBUG, "%d events polled", m_eventList.size());
	}
*/


	for (size_t i = 0; i < m_eventList.size(); ++i)
	{
		Event&			event = m_eventList[i];
		EventObject*	object = reinterpret_cast<EventObject*>(event.data.ptr);
		int				status = STAT_NORMAL;

    LOG(Logger::DEBUG, "[%d] op=%d", object->m_fd, event.events);
		if (TEST_BITMASK(event.events, EPOLLERR))
			object->m_eventStatus = EventObject::EVENT_EOF;

		if (TEST_BITMASK(event.events, EPOLLIN))
		{
      event.events &= ~EPOLLIN;
			object->m_filter = FILT_READ;
			status |= object->handleReadEvent();
		}
		if (TEST_BITMASK(event.events, EPOLLOUT))
		{
      event.events &= ~EPOLLOUT;
			object->m_filter = FILT_WRITE;
			status |= object->handleWriteEvent();
		}
		if (TEST_BITMASK(event.events, EPOLLERR))
		{
      event.events &= ~EPOLLERR;
			object->m_filter = FILT_ERROR;
			status |= object->handleErrorEvent();
		}
    if (event.events != 0) {
      LOG(WARNING, "[%d] EPOLL event(s) = %d is not handled.", object->m_fd, event.events);
    }
		if (TEST_BITMASK(status, STAT_END) == true)
		{
			delete object;
		}
	}
	return 0;
}

