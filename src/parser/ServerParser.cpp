#include <sstream>
#include <algorithm>

#include "Logger.hpp"
#include "VirtualServer.hpp"
#include "util/Util.hpp"
#include "exception/ConfigParserException.hpp"
#include "event/Server.hpp"
#include "socket/Tcp.hpp"
#include "parser/LocationParser.hpp"
#include "parser/ServerParser.hpp"

using namespace std;

extern const string	g_webservDir;

map<string, ServerParser::t_setter>	ServerParser::s_serverSetterMap;

void	ServerParser::setServerSetterMap()
{
	s_serverSetterMap["autoindex"] = &ServerParser::setAutoIndex;
	s_serverSetterMap["cgi_pass"] = &ServerParser::setCgiPass;
	s_serverSetterMap["client_max_body_size"] = &ServerParser::setClientMaxBodySize;
	s_serverSetterMap["error_page"] = &ServerParser::setErrorPage;
	s_serverSetterMap["index"] = &ServerParser::setIndex;
	s_serverSetterMap["listen"] = &ServerParser::setListenAddress;
	s_serverSetterMap["location"] = &ServerParser::parseLocation;
	s_serverSetterMap["return"] = &ServerParser::setReturn;
	s_serverSetterMap["root"] = &ServerParser::setRoot;
	s_serverSetterMap["server_name"] = &ServerParser::setServerNames;
}

// constructors & destructor
ServerParser::ServerParser(FileTokenizer& m_tokenizer)
:	m_tokenizer(m_tokenizer)
{
}

ServerParser::~ServerParser()
{
}

// member functions
void
ServerParser::parse(VirtualServer& server)
{
	while (s_serverSetterMap.count(m_tokenizer.peek()) == 1)
	{
		(this->*s_serverSetterMap[m_tokenizer.get()])(server);
	}
	setInheritedAttr(server);
}

void
ServerParser::parseLocation(VirtualServer& server)
{
	string	locationPath = m_tokenizer.get();

	m_tokenizer.eat("{");

	LocationParser	locationParser(m_tokenizer);

	Location	location;

	location.m_path = locationPath;
	locationParser.parse(location);
	server.m_locationTable[locationPath] = location;
	m_tokenizer.eat("}");
}

// setters
void
ServerParser::setIndex(VirtualServer& server)
{
	server.m_index.clear();
	while (m_tokenizer.empty() == false && m_tokenizer.peek() != ";")
	{
		server.m_index.push_back(m_tokenizer.get());
	}
	m_tokenizer.eat(";");
}

void
ServerParser::setServerNames(VirtualServer& server)
{
	server.m_serverNames.clear();
	while (m_tokenizer.empty() == false && m_tokenizer.peek() != ";")
	{
		server.m_serverNames.push_back(m_tokenizer.get());
	}
	m_tokenizer.eat(";");
}

void
ServerParser::setListenAddress(VirtualServer& server)
{
	string				listenField = m_tokenizer.get();
	stringstream		ss;
	string::size_type	colonPos = listenField.find(":");
	uint64_t			addr = 0;
	uint16_t			port = 0;

	if (colonPos != string::npos || count(listenField.begin(), listenField.end(), '.') > 0)
	{
		uint16_t	addrPart = 0;
		char		period;

		ss.str(listenField.substr(0, colonPos));
		for (int i = 0; i < 3; ++i)
		{
			ss >> addrPart >> period;
			if (period != '.' || ss.fail() || addrPart > 255)
				throw ConfigParser::ConfigParserException("invalid listen address");
			addr = (addr << 8) | addrPart;
		}
		ss >> addrPart;
		addr = (addr << 8) | addrPart;
		if (addrPart > 255 || ss.eof() == false || ss.fail() == true)
			throw ConfigParser::ConfigParserException("invalid listen address");
	}

	if (!(colonPos == string::npos && ss.eof() == true))
	{

		if (listenField.rfind(":") != colonPos)
			throw ConfigParser::ConfigParserException("invalid listen address");
		ss.clear();
		ss.str(colonPos != string::npos ? listenField.substr(colonPos + 1, string::npos) : listenField);
		ss >> port;
		if (ss.eof() == false || ss.fail() == true)
			throw ConfigParser::ConfigParserException("invalid listen address");
	}
	server.m_listen = GET_SOCKADDR_IN(addr, port);
	server.m_addrKey.setAddrKey(addr, port);
	LOG(Logger::DEBUG, "addr/port : %s", getFormattedAddress(addr, port).c_str());
	m_tokenizer.eat(";");
}

void
ServerParser::setRoot(VirtualServer& server)
{
	string	root = m_tokenizer.get();

	server.m_root = root[0] == '/' ? root : g_webservDir + root;
	m_tokenizer.eat(";");
}

void
ServerParser::setErrorPage(VirtualServer& server)
{
	vector<string> token;

	while (m_tokenizer.empty() == false && m_tokenizer.peek() != ";")
	{
		token.push_back(m_tokenizer.get());
	}
	for (size_t i = 0; i < token.size() - 1; i++)
	{
		server.m_errorPageTable[Util::toInt(token[i])] = token.back();
	}
	m_tokenizer.eat(";");
}

void
ServerParser::setClientMaxBodySize(VirtualServer& server)
{
	server.m_clientMaxBodySize = Util::toInt(m_tokenizer.get());
	m_tokenizer.eat(";");
}

void
ServerParser::setAutoIndex(VirtualServer& server)
{
	string	token = m_tokenizer.get();
	if (token == "on")
		server.m_autoindex = true;
	else if (token == "off")
		server.m_autoindex = false;
	else
		throw ConfigParser::ConfigParserException("invalid value \"" + token + "\" in \"autoindex\" directive, it must be \"on\" or \"off\".");

	m_tokenizer.eat(";");
}

void
ServerParser::setCgiPass(VirtualServer& server)
{
	vector<string>	token;
	string			cgiPath;

	while (m_tokenizer.empty() == false && m_tokenizer.peek() != ";")
	{
		token.push_back(m_tokenizer.get());
	}
	cgiPath = token.back();
	cgiPath = cgiPath[0] == '/' ? cgiPath : g_webservDir + cgiPath;
	for (size_t i = 0; i < token.size() - 1; ++i)
		server.m_cgiPass[token[i]] = cgiPath;
	m_tokenizer.eat(";");
}

void
ServerParser::setReturn(VirtualServer& server)
{
	server.m_return = m_tokenizer.get();
	if (Util::checkUrl(server.m_return) == false)
		throw ConfigParser::ConfigParserException("invalid URL in return field");
	m_tokenizer.eat(";");
}

void
ServerParser::setInheritedAttr(VirtualServer& server)
{
	map<string, Location>& locationTable = server.m_locationTable;

	server.m_locationTable["/"];
	map<string, Location>::iterator mapIt = locationTable.begin();
	for (; mapIt != locationTable.end(); ++mapIt)
	{
		Location& location = mapIt->second;

		if (location.m_autoindex == -1)
			location.m_autoindex = server.m_autoindex;
		if (location.m_clientMaxBodySize == -1)
			location.m_clientMaxBodySize = server.m_clientMaxBodySize;
		if (location.m_root == "" && location.m_alias == "")
			location.m_root = server.m_root;
		if (location.m_index.empty())
			location.m_index = server.m_index;
		if (location.m_errorPageTable.empty())
			location.m_errorPageTable = server.m_errorPageTable;
		if (location.m_return.empty())
			location.m_return = server.m_return;
	}
}
