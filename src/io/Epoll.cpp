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
	static const uint32_t	filterTable[3] = {EPOLLIN, EPOLLOUT, EPOLLERR};
	Event	newEvent;
  int   epollOp = 0;
  int   newEpollFilter = 0;
  int   prevEpollFilter = 0;

	::memset(&newEvent, 0, sizeof(newEvent));
	newEvent.data.ptr = object;
	for (int count = 0, bitmask = 0x1; count < 3; ++count, bitmask <<= 1)
	{
		if (filter & bitmask)
			newEpollFilter |= filterTable[count];
    if (object->m_filter & bitmask)
      prevEpollFilter |= filterTable[count];
	}
	switch (op)
	{
		case OP_ADD:
			object->m_filter = filter;
      epollOp = EPOLL_CTL_ADD;
			break;
		case OP_DELETE:
			object->m_filter &= ~filter;
      if (object->m_filter == 0)
      {
        epollOp = EPOLL_CTL_DEL;
      }
      else
      {
        newEpollFilter = ~newEpollFilter & prevEpollFilter;
        epollOp = EPOLL_CTL_MOD;
      }
      break;
		case OP_MODIFY:
			object->m_filter = filter;
      epollOp = EPOLL_CTL_MOD;
			break;
	}
  newEvent.events = newEpollFilter;
  LOG(DEBUG, "[%d] epoll op = %d, filter = %d", object->m_fd, epollOp, newEvent.events);
	if (epoll_ctl(m_epoll, epollOp, fd, &newEvent) < 0)
		throw std::runtime_error("epoll_ctl() fail");
}

int
Epoll::pollWork()
{
	const int	maxEvent = 64;
	int			count = 0;

	m_eventList.resize(maxEvent);
	count = epoll_wait(m_epoll, m_eventList.data(), m_eventList.size(), 100000);
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

    Logger::log(Logger::DEBUG, "[%d] event filter = %d", object->m_fd, event.events);
		if (TEST_BITMASK(event.events, EPOLLIN) || TEST_BITMASK(event.events, EPOLLHUP))
		{
      event.events &= ~EPOLLIN;
//	object->m_filter = FILT_READ;
			status |= object->handleReadEvent();
		}
		if (TEST_BITMASK(event.events, EPOLLOUT))
		{
      event.events &= ~EPOLLOUT;
//	object->m_filter = FILT_WRITE;
			status |= object->handleWriteEvent();
		}
		if (TEST_BITMASK(event.events, EPOLLERR))
		{
      event.events &= ~EPOLLERR;
//	object->m_filter = FILT_ERROR;
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

