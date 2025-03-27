#include <fcntl.h>

#include "Logger.hpp"
#include "VirtualServer.hpp"
#include "ServerManager.hpp"
#include "event/Cgi.hpp"
#include "http/AutoIndex.hpp"
#include "responder/GetResponder.hpp"

using namespace std;

extern string	g_tempDir;

// constructors & destructor
GetResponder::GetResponder(RequestHandler& requestHandler)
:	AResponder(requestHandler)
{
}

GetResponder::~GetResponder()
{
}

// operators
GetResponder&
GetResponder::operator=(const GetResponder& getMethod)
{
	(void)getMethod;
	return *this;
}

void
GetResponder::respondWork()
{
	string readBody;

	switch (m_responseStatus)
	{
		case RES_HEADER:
			if (isAutoIndex() && m_request.m_status == 404)
				m_request.m_status = 200;
			else if (m_request.m_status >= 300)
				throw (m_request.m_status);
			else if (m_request.m_isCgi == true)
			{
				constructCgi();
				break;
			}
			respondStatusLine(m_request.m_status);
			respondHeader();
			m_responseStatus = RES_CONTENT; // fall through
		case RES_CONTENT:
			if (isAutoIndex())
				readBody = AutoIndex::autoIndex(m_request.m_path, m_request.m_uri);
			readFile(readBody);
			respondBody(readBody);
			m_responseStatus = RES_DONE; // fall through
		case RES_DONE:
			endResponse();
			break;
		default:
			;
	}
}

void
GetResponder::constructCgi()
{
	int	pipeSet[2];

	pipe(pipeSet);

	Cgi*	cgi = new Cgi(pipeSet, m_requestHandler);

	ServerManager::registerEvent(pipeSet[0], Cgi::IoEventPoller::OP_ADD, Cgi::IoEventPoller::FILT_READ, cgi);
	cgi->initEnv(m_request);
	cgi->executeCgi(pipeSet);
	m_responseStatus = RES_DONE;
}

bool
GetResponder::isAutoIndex()
{
	if (m_request.m_file == "" && m_request.m_locationBlock != NULL)
	{
		return (m_request.m_locationBlock->m_autoindex);
	}
	return (false);
}
