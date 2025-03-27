#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>

#include "Logger.hpp"
#include "ServerManager.hpp"
#include "exception/HttpErrorHandler.hpp"
#include "http/RequestHandler.hpp"
#include "io/Buffer.hpp"
#include "util/Util.hpp"
#include "AResponder.hpp"
#include "event/Cgi.hpp"

#ifndef TEMP_DIR
# define TEMP_DIR "/tmp/"
#endif

#define FILE_BUFFER_SIZE (8192)

extern const std::string	g_tempDir = TEMP_DIR;

using namespace std;

// constructors & destructor
AResponder::AResponder(RequestHandler& requestHandler)
:	m_requestHandler(requestHandler),
	m_request(m_requestHandler.m_request),
	m_sendBuffer(m_requestHandler.m_sendBuffer),
	m_recvBuffer(m_requestHandler.m_recvBuffer),
	m_responseStatus(RES_HEADER),
	m_dataSize(-1),
	m_totalContentLentgh(0)
{
	if (m_request.m_headerFieldsMap.count("TRANSFER-ENCODING") == 1
		&& m_request.m_headerFieldsMap["TRANSFER-ENCODING"].back() == "chunked")
		m_recvContentFunc = &AResponder::receiveContentChunked;
	else
		m_recvContentFunc = &AResponder::receiveContentNormal;

	if (m_request.m_isCgi == true)
		m_procContentFunc = &AResponder::writeToBuffer;
	else
		m_procContentFunc = &AResponder::writeToFile;
	m_cgi = NULL;
	m_totalrecvBufferSize = 0;
}

AResponder::~AResponder()
{
	delete m_cgi;
}

// operators
AResponder&
AResponder::operator=(const AResponder& aMethod)
{
	(void)aMethod;
	return *this;
}

void
AResponder::respond() try
{
	if (m_request.m_method != RequestHandler::HEAD
		&& m_request.m_method != RequestHandler::GET
		&& m_request.m_status >= 300)
		throw (m_request.m_status);
	respondWork();
}
catch (int errorStatusCode)
{
	string readBody;

	LOG(DEBUG, "%d error while responding", errorStatusCode);
	m_request.m_status = errorStatusCode;
	respondStatusLine(errorStatusCode);
	respondHeader();
	m_request.m_file.clear();
	m_request.m_path = getErrorPage(readBody);
	readFile(readBody);
	respondBody(readBody);
	endResponse();
}

string
AResponder::getErrorPage(string& readBody)
{
	map<int, string>*	error_page;
	string				root;
	string				filePath = "";

	if (m_request.m_locationBlock != NULL)
	{
		error_page = &m_request.m_locationBlock->m_errorPageTable;
		root = m_request.m_locationBlock->m_root;
		if(error_page->count(m_request.m_status) != 0)
		{
			filePath = root + (*error_page)[m_request.m_status];
		}
	}
	if (Util::checkFileStat(filePath.c_str()) == false)
	{
		readBody = RequestHandler::makeErrorPage(m_request.m_status);
	}
	return (filePath);
}

// member functions
void
AResponder::readFile(std::string& readBody)
{
	struct stat		fileStatus;
	std::ifstream	file;
	std::string		fileBuffer;
	std::string 	filePath = m_request.m_path + m_request.m_file;

	if (readBody.empty() == false)
		return;
	file.open(filePath.c_str());
	stat(filePath.c_str(), &fileStatus);
	readBody.clear();
	readBody.reserve(fileStatus.st_size);
	while (!file.eof())
	{
		fileBuffer.resize(FILE_BUFFER_SIZE);
		file.read(&fileBuffer[0], FILE_BUFFER_SIZE);
		fileBuffer.resize(file.gcount());
		readBody += fileBuffer;
	}
	file.close();
}

void
AResponder::openFile()
{
	openFile(m_request.m_path + m_request.m_file);
}

void
AResponder::openFile(const string& path)
{
	m_fileFd = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (m_fileFd < 0)
		throw 500;
}

int
AResponder::writeToFile(size_t writeSize)
{
	return (write(m_fileFd, m_recvBuffer.data(), writeSize));
}

int
AResponder::writeToBuffer(size_t writeSize)
{
	m_buffer.append(m_recvBuffer.data(), writeSize);
	return (writeSize);
}

bool
AResponder::checkFileExists(const std::string& filePath)
{
	struct stat buffer;
	int			exist;

	exist = stat(filePath.c_str(), &buffer);
	if (exist == 0 && ((buffer.st_mode & S_IFMT) == S_IFREG))
		return (true);
	return (false);
}

bool
AResponder::checkDirExists(const std::string& filePath)
{
	struct stat buffer;
	int			exist;

	exist = stat(filePath.c_str(), &buffer);
	if (exist == 0 && ((buffer.st_mode & S_IFMT) == S_IFDIR))
		return (true);
	return (false);
}

void
AResponder::endResponse()
{
	m_requestHandler.m_parser.m_readStatus = HttpRequestParser::FINISHED;
	if (m_request.m_status >= 300)
		m_requestHandler.m_parser.m_readStatus = HttpRequestParser::ERROR;
}

void
AResponder::respondHeader()
{
	if (m_request.m_status == 301)
	{
		m_sendBuffer.append("Location: " + m_request.m_locationBlock->m_return);
		m_sendBuffer.append(g_CRLF);
		LOG(DEBUG, "response header");
		Logger::log(Logger::DEBUG, m_sendBuffer);
		return;
	}
	m_sendBuffer.append("Server: webserv/2.0");
	m_sendBuffer.append(g_CRLF);
	m_sendBuffer.append("Date: " + Util::getDate("%a, %d %b %Y %X %Z"));
	m_sendBuffer.append(g_CRLF);
	m_sendBuffer.append("Content-Type: " + m_requestHandler.findContentType(m_request.m_file));
	m_sendBuffer.append(g_CRLF);
	if (m_request.m_status == 405)
	{
		m_sendBuffer.append("Allow:" + RequestHandler::methodToString(m_request.m_locationBlock->m_limitExcept));
		m_sendBuffer.append(g_CRLF);
	}
	if (m_request.m_status >= 300)
	{
		m_sendBuffer.append("Connection: close");
		m_sendBuffer.append(g_CRLF);
	}
	else if (m_request.m_headerFieldsMap.count("CONNECTION") > 0)
	{
		m_sendBuffer.append("Connection: " + m_request.m_headerFieldsMap["CONNECTION"][0]);
		m_sendBuffer.append(g_CRLF);
	}
	if (m_request.m_status == 201)
	{
		m_sendBuffer.append("Location: " + m_request.m_path + m_request.m_file);
		m_sendBuffer.append(g_CRLF);
	}
	LOG(DEBUG, "response header");
	Logger::log(Logger::DEBUG, m_sendBuffer);
}

void
AResponder::respondBody(const string& readBody)
{
	m_sendBuffer.append("Content-Length: ");
	m_sendBuffer.append(Util::toString(readBody.size()));
	m_sendBuffer.append(g_CRLF);
	m_sendBuffer.append(g_CRLF);
	m_sendBuffer.append(readBody);
}

int
AResponder::sendContentNormal()
{
	return 0;
}

int
AResponder::sendContentChunked()
{
	return 0;
}

string
AResponder::parseChunkSize()
{
	string hex;
	size_t crlfPos = m_recvBuffer.find(g_CRLF);

	if (crlfPos == string::npos)
		return ("");
	if (crlfPos == 0)
	{
		m_recvBuffer.erase(0, 2);
		return (parseChunkSize());
	}
	hex = m_recvBuffer.substr(0, crlfPos);
	m_recvBuffer.erase(0, crlfPos + 2);
	return (hex);
}

int
AResponder::receiveContentChunked()
{
	while (m_recvBuffer.size() != 0)
	{
		if (m_dataSize == -1)
		{
			m_dataSize = Util::hexToDecimal(parseChunkSize());
		}
		if (m_totalContentLentgh > m_request.m_locationBlock->m_clientMaxBodySize)
		{
			LOG(DEBUG, "[%d] content length sum = %d, max content size = %d", m_requestHandler.m_socket->m_fd,
				m_totalContentLentgh, m_request.m_locationBlock->m_clientMaxBodySize);
			throw (413);
		}
		if (m_dataSize == 0 && m_recvBuffer.size() == 2)
		{
			m_buffer.status(Buffer::BUF_EOF);
			m_recvBuffer.clear();
			return (1);
		}
		if (m_dataSize > 0 && m_dataSize + 2 <= static_cast<int>(m_recvBuffer.size()))
		{
			m_totalContentLentgh += m_dataSize;
			(this->*m_procContentFunc)(m_dataSize);
			m_recvBuffer.erase(0, m_dataSize + 2);
			m_dataSize = -1;
		}
		else if (m_dataSize > 0 && m_dataSize >= static_cast<int>(m_recvBuffer.size()))
		{
			m_totalContentLentgh += m_recvBuffer.size();
			(this->*m_procContentFunc)(m_recvBuffer.size());
			m_dataSize -= m_recvBuffer.size();
			m_recvBuffer.clear();
			if (m_dataSize == 0)
				m_dataSize = -1;
		}
		else
			return (0);
	}
	return (0);
}

int
AResponder::receiveContentNormal()
{
	if (m_dataSize == -1)
	{
		if (m_request.m_headerFieldsMap.count("CONTENT-LENGTH") == 0)
		{
			m_dataSize = 0;
		}
		else
		{
			m_dataSize = Util::toInt(m_request.m_headerFieldsMap["CONTENT-LENGTH"][0]);
		}
		if (m_dataSize > m_request.m_locationBlock->m_clientMaxBodySize)
		{
			LOG(DEBUG, "[%d] content length sum = %d, max content size = %d", m_requestHandler.m_socket->m_fd,
				m_totalContentLentgh, m_request.m_locationBlock->m_clientMaxBodySize);
			throw (413);
		}
	}
	if (m_dataSize == 0)
	{
		m_buffer.status(Buffer::BUF_EOF);
		return 1;
	}

	int	count = (this->*m_procContentFunc)(m_recvBuffer.size());
	if (count == -1)
		return 0;
	m_dataSize -= count;
	m_recvBuffer.erase(0, count);
	if (m_dataSize == 0)
	{
		m_buffer.status(Buffer::BUF_EOF);
		m_recvBuffer.clear();
		return 1;
	}
	return (0);
}

void
AResponder::respondStatusLine(int statusCode)
{
	m_sendBuffer.append(g_httpVersion);
	m_sendBuffer.append(" ");
	m_sendBuffer.append(Util::toString(statusCode));
	m_sendBuffer.append(" ");
	m_sendBuffer.append(HttpErrorHandler::getErrorMessage(statusCode));
	m_sendBuffer.append(g_CRLF);
}
