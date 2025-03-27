#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>

#include "Logger.hpp"
#include "ServerManager.hpp"
#include "exception/HttpErrorHandler.hpp"
#include "parser/HttpRequestParser.hpp"
#include "responder/Responder.hpp"
#include "http/FindLocation.hpp"
#include "http/RequestHandler.hpp"

#define CHECK_PERMISSION(mode, mask) (((mode) & (mask)) == (mask))

using namespace	std;

const char*	g_httpVersion = "HTTP/1.1";

static size_t g_totalReceivedBytes = 0;

map<string, uint16_t>	RequestHandler::s_methodConvertTable;
map<uint16_t, string>	RequestHandler::s_methodRConvertTable;
std::map<std::string, std::string>	RequestHandler::s_extensionTypeTable;


RequestHandler::RequestHandler(const RequestHandler& requestHandler)
:	m_parser(m_recvBuffer),
	m_socket(NULL),
	m_responder(NULL)
{
	(void)requestHandler;
}

RequestHandler&
RequestHandler::operator=(const RequestHandler& request)
{
	(void)request;
	return *this;
}

// constructors & destructor
RequestHandler::RequestHandler(const Socket<Tcp>& socket)
:	m_parser(m_recvBuffer),
	m_socket(&socket),
	m_responder(NULL)
{
	m_recvBuffer.setFd(m_socket->m_fd);
	m_sendBuffer.setFd(m_socket->m_fd);
}

RequestHandler::~RequestHandler()
{
}

int
RequestHandler::receiveRequest()
{
	int		count;
	int		receiveStatus = RECV_NORMAL;

	if (m_sendBuffer.size() != 0)
		return RECV_SKIPPED;

	count = m_recvBuffer.receive(m_socket->m_fd);
	if (count < 0)
		return RECV_SKIPPED;
	else if (count == 0)
		return RECV_END;
  g_totalReceivedBytes += count;
  Logger::log(Logger::DEBUG, "[%d] receiveRequest() count = %d, totalCount = %zu",
      m_socket->m_fd, count, g_totalReceivedBytes);
	switch (m_parser.m_readStatus)
	{
		case HttpRequestParser::REQUEST_LINE_METHOD: // fall through
		case HttpRequestParser::REQUEST_LINE: // fall through
		case HttpRequestParser::HEADER_FIELDS:
			m_parser.parse(m_request);
			if (m_parser.m_readStatus <= HttpRequestParser::HEADER_FIELDS)
				break; // fall through
		case HttpRequestParser::HEADER_FIELDS_END:
			createResponseHeader();
			receiveStatus = RECV_EVENT;
			if (m_parser.m_readStatus == HttpRequestParser::HEADER_FIELDS_END)
				break; // fall through
		case HttpRequestParser::CONTENT:
			m_responder->respond();
			if (m_parser.m_readStatus == HttpRequestParser::CONTENT)
				break; // fall through
		case HttpRequestParser::FINISHED:
			delete m_responder;
			if (m_parser.m_readStatus != HttpRequestParser::ERROR)
			{
				resetStates();
				break;
			}
		default:
			;
	}
	return receiveStatus;
}

void
RequestHandler::createResponseHeader()
{
	int&			statusCode = m_request.m_status;

	if (statusCode < 400)
		checkRequestMessage();
	if (statusCode >= 400)
	{
		m_request.m_isCgi = false;
	}
	switch (m_request.m_method)
	{
		case GET:
			m_responder = new GetResponder(*this);
			break;
		case HEAD:
			m_responder = new HeadResponder(*this);
			break;
		case POST:
			m_responder = new PostResponder(*this);
			break;
		case PUT:
			m_responder = new PutResponder(*this);
			break;
		case DELETE:
			m_responder = new DeleteResponder(*this);
			break;
		default:
			UPDATE_REQUEST_ERROR(statusCode, 405);
			m_responder = new GetResponder(*this);
	}
	LOG(DEBUG, "[%d] status code = %d", m_socket->m_fd, statusCode);
	LOG(DEBUG, "request header");
	Logger::log(Logger::DEBUG, m_request);
	m_parser.m_readStatus = HttpRequestParser::CONTENT;
}

void
RequestHandler::checkRequestMessage()
{
	FindLocation	findLocation;
	VirtualServer*	virtualServer;

	checkStatusLine();
	checkHeaderFields();
	virtualServer = resolveVirtualServer(m_request.m_headerFieldsMap["HOST"][0]);
	m_request.m_virtualServer = virtualServer;
	m_request.m_locationBlock = &virtualServer->m_locationTable["/"];
	findLocation.saveRealPath(m_request, virtualServer->m_locationTable, virtualServer);
	checkIsCgi();
	checkAllowedMethod(m_request.m_locationBlock->m_limitExcept);
	if (m_request.m_file != "")
		checkResourceStatus();
	checkRedirection();
}

void
RequestHandler::checkRedirection()
{
	if (!m_request.m_locationBlock->m_return.empty())
	{
		LOG(INFO, "root location block %s", m_request.m_locationBlock->m_return.c_str());
		m_request.m_status = 301;
	}
}

void
RequestHandler::checkIsCgi()
{
	m_request.m_isCgi = false;
	if (m_request.m_file != "" && (m_request.m_method == GET || m_request.m_method == POST))
	{
		string ext = "";
		if (m_request.m_file.rfind(".") != string::npos)
		{
			ext = m_request.m_file.substr(m_request.m_file.rfind("."));
			if (m_request.m_virtualServer->m_cgiPass.count(ext) == true)
			{
				m_request.m_cgi = m_request.m_virtualServer->m_cgiPass[ext];
				m_request.m_isCgi = m_request.m_method == RequestHandler::GET && ext == ".bla" ? false : true;
			}
		}
		if (m_request.m_isCgi == true && (m_request.m_method == POST || m_request.m_method == PUT))
			m_request.m_status = 200;
	}
}

void
RequestHandler::checkStatusLine()
{
	if (m_request.m_protocol != "HTTP/1.1")
		UPDATE_REQUEST_ERROR(m_request.m_status, 505);
}

void
RequestHandler::checkHeaderFields()
{
	size_t	pos;

	if (m_request.m_headerFieldsMap.count("HOST") == 0)
	{
		m_request.m_headerFieldsMap["HOST"].push_back("");
		return;
	}
	pos = m_request.m_headerFieldsMap["HOST"][0].find(":");
	if (pos != string::npos)
	{
		m_request.m_headerFieldsMap["HOST"][0].erase(pos);
	}
}

VirtualServer*
RequestHandler::resolveVirtualServer(const string& host)
{
	Tcp::SocketAddr	addr = m_socket->getAddress();
	AddrKey			addrKey;
	VirtualServer*	server;

	addrKey.setAddrKey(ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port));
	if (ServerManager::s_virtualServerTable.count(addrKey) == 0)
		addrKey.value &= 0xffff00000000;
	if (ServerManager::s_virtualServerTable[addrKey].count(host) == 0)
		server = ServerManager::s_virtualServerTable[addrKey]["."];
	else
		server = ServerManager::s_virtualServerTable[addrKey][host];
	return server;
}

void
RequestHandler::checkAllowedMethod(uint16_t allowed)
{
	if (!(m_request.m_method & allowed))
	{
		if (m_request.m_status == 404)
			m_request.m_status = 405;
		UPDATE_REQUEST_ERROR(m_request.m_status, 405);
	}
}

void
RequestHandler::checkResourceStatus()
{
	struct stat	status;
	int			statusCode = 0;
	unsigned int	permission;

	switch (m_request.m_method)
	{
		case GET: // fall through
		case HEAD:
			permission = R_OK; break;
		case POST:
			if (m_request.m_isCgi == true)
				return;
			permission = R_OK; break;
		case PUT:
			return;
		case DELETE:
			permission = W_OK; break;
	}
	if (stat((m_request.m_path + m_request.m_file).c_str(), &status) == 0 && S_ISREG(status.st_mode)
		&& access((m_request.m_path + m_request.m_file).c_str(), permission) == 0)
		return;

	switch (errno)
	{
		case EACCES:
			statusCode = 403; break;
		case ENOENT: // fall through
		case ENOTDIR:
			statusCode = 404; break;
		case ENAMETOOLONG:
			statusCode = 414; break;
		default:
			statusCode = 500; break;
	}
	UPDATE_REQUEST_ERROR(m_request.m_status, statusCode);
}

std::string
RequestHandler::findContentType(std::string& content)
{
	std::string extension;

	if (content.find('.') != std::string::npos)
	{
		extension = content.substr(content.find('.') + 1);
		if (s_extensionTypeTable.count(extension) == 1)
			return s_extensionTypeTable[extension];
	}
	extension = "text/html";
	return (extension);
}

void
RequestHandler::resetStates()
{
	m_request.m_headerFieldsMap.clear();
	m_request = Request();
	m_parser.m_readStatus = HttpRequestParser::REQUEST_LINE_METHOD;
}

int
RequestHandler::sendResponse() try
{
	int		count = m_sendBuffer.send(m_socket->m_fd);

	if (count == 0 && (m_parser.m_readStatus == HttpRequestParser::REQUEST_LINE_METHOD || m_parser.m_readStatus == HttpRequestParser::ERROR))
	{
		m_sendBuffer.status(Buffer::BUF_EOF);
		return m_parser.m_readStatus == HttpRequestParser::ERROR ? SEND_ERROR : SEND_END;
	}
  Logger::log(Logger::DEBUG, "[%d] sendResponse() count = %d, readStatus = %d", m_socket->m_fd, count, m_parser.m_readStatus);
	return SEND_NORMAL;
}
catch (runtime_error& e)
{
	LOG(ERROR, "%s", e.what());
	return SEND_ERROR;
}

std::ostream&
operator<<(std::ostream& os, const Request& request)
{
	HeaderFieldsMap::const_iterator mapIt;

	os << "reqeust info\n";
	os << "status line\n";
	os << "\tmethod : " << RequestHandler::s_methodRConvertTable[request.m_method] << '\n';
	os << "\ttarget : " << request.m_uri << '\n';
	os << "\tprotocol : " << request.m_protocol << '\n';
	os << "header field";
	for (mapIt = request.m_headerFieldsMap.begin();
			mapIt != request.m_headerFieldsMap.end(); mapIt++)
	{
		os << "\n\t" << mapIt->first << " : ";
		vector<string>::const_iterator vecIt = mapIt->second.begin();
		for (; vecIt != mapIt->second.end(); vecIt++)
			os << *vecIt << " ";
	}
	os << endl;
	return (os);
}

void
RequestHandler::setMethodConvertTable()
{
	s_methodConvertTable["GET"] = RequestHandler::GET;
	s_methodConvertTable["HEAD"] = RequestHandler::HEAD;
	s_methodConvertTable["POST"] = RequestHandler::POST;
	s_methodConvertTable["PUT"] = RequestHandler::PUT;
	s_methodConvertTable["DELETE"] = RequestHandler::DELETE;
	s_methodConvertTable["OPTION"] = RequestHandler::OPTION;
	s_methodConvertTable["TRACE"] = RequestHandler::TRACE;
	s_methodConvertTable["CONNECT"] = RequestHandler::CONNECT;

	s_methodRConvertTable[RequestHandler::GET] = "GET";
	s_methodRConvertTable[RequestHandler::HEAD] = "HEAD";
	s_methodRConvertTable[RequestHandler::POST] = "POST";
	s_methodRConvertTable[RequestHandler::PUT] = "PUT";
	s_methodRConvertTable[RequestHandler::DELETE] = "DELETE";
	s_methodRConvertTable[RequestHandler::OPTION] = "OPTION";
	s_methodRConvertTable[RequestHandler::TRACE] = "TRACE";
	s_methodRConvertTable[RequestHandler::CONNECT] = "CONNECT";
}

void
RequestHandler::initExtensionList()
{
	s_extensionTypeTable["html"] = "text/html";
	s_extensionTypeTable["htm"] = "text/html";
	s_extensionTypeTable["shtml"] = "text/html";
	s_extensionTypeTable["css"] = "text/css";
	s_extensionTypeTable["xml"] = "text/xml";
	s_extensionTypeTable["gif"] = "image/gif";
	s_extensionTypeTable["jpeg"] = "image/gif";
	s_extensionTypeTable["jpg"] = "image/jpeg";
	s_extensionTypeTable["txt"] = "text/plain";
	s_extensionTypeTable["png"] = "image/png";
	s_extensionTypeTable["ico"] = "image/x-icon";
	s_extensionTypeTable["bmp"] = "image/x-ms-bmp";
	s_extensionTypeTable["svg"] = "image/x-ms-bmp";
	s_extensionTypeTable["webp"] = "image/webp";
	s_extensionTypeTable["mp4"] = "video/mp4";
	s_extensionTypeTable["mpeg"] = "video/mp4";
	s_extensionTypeTable["mpg"] = "video/mpeg";
	s_extensionTypeTable["avi"] = "video/x-msvideo";
	s_extensionTypeTable["js"] = "application/javascript";
	s_extensionTypeTable["woff"] = "application/font-woff";
	s_extensionTypeTable["json"] = "application/json";
	s_extensionTypeTable["doc"] = "application/msword";
	s_extensionTypeTable["pdf"] = "application/pdf";
	s_extensionTypeTable["xls"] = "application/vnd.ms-excel";
	s_extensionTypeTable["rar"] = "application/x-rar-compressed";
	s_extensionTypeTable["zip"] = "application/zip";
	s_extensionTypeTable["7z"] = "application/x-7z-compressed";
	s_extensionTypeTable["bin"] = "application/zip";
	s_extensionTypeTable["exe"] = "application/zip";
	s_extensionTypeTable["mp3"] = "audio/mpeg";
	s_extensionTypeTable["ogg"] = "audio/ogg";
	s_extensionTypeTable["m4a"] = "audio/x-m4a";
}

string
RequestHandler::makeErrorPage(int status)
{
	string buf;
	string statusStr = Util::toString(status);

	buf =
	"<!DOCTYPE html>\n"
	"<html>\n"
	"	<head>\n"
	"<meta charset=\"utf-8\">\n"
	"		<title>" + statusStr + "</title>\n"
	"	</head>\n"
	"	<body>\n"
	"		<h1>"
	+ statusStr + " " + HttpErrorHandler::getErrorMessage(status) +
	"		</h1>\n"
	"	</body>\n"
	"</html>\n";

	return buf;
}

std::string
RequestHandler::methodToString(uint16_t allowed)
{
	string methodString;
	if (allowed & RequestHandler::GET)
		methodString += " GET,";
	if (allowed & RequestHandler::HEAD)
		methodString += " HEAD,";
	if (allowed & RequestHandler::POST)
		methodString += " POST,";
	if (allowed & RequestHandler::PUT)
		methodString += " PUT,";
	if (allowed & RequestHandler::DELETE)
		methodString += " DELETE,";
	if (methodString.size() > 0)
		methodString.erase(methodString.end() - 1);
	return (methodString);
}
