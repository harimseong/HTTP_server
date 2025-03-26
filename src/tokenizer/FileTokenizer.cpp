#include <sstream>
#include <iostream>

#include "Logger.hpp"
#include "util/Util.hpp"
#include "parser/ConfigParser.hpp"
#include "exception/FileTokenizerException.hpp"
#include "tokenizer/FileTokenizer.hpp"

using namespace std;

const std::string	FileTokenizer::s_eot = "";

// constructors & destructor
FileTokenizer::FileTokenizer()
{
}

FileTokenizer::~FileTokenizer()
{
}

string
FileTokenizer::peek()
{
	if (empty() == true)
		return s_eot;
	return (m_tokenArr[m_idx].first);
}

string
FileTokenizer::get()
{
	if (empty() == true)
		throw FileTokenizerException("consumed every tokens");
	return (m_tokenArr[m_idx++].first);
}

void
FileTokenizer::eat(const string& target)
{
	string	temp = peek();

	if (get() != target)
		throw FileTokenizerException("unexpected token: '" + temp + "'");
}

unsigned int
FileTokenizer::size()
{
	return m_tokenArr.size();
}

bool
FileTokenizer::empty() const
{
	return m_idx >= m_tokenArr.size();
}

string
FileTokenizer::getErrorLog(const char* message)
{
	int		errorLineNumber = m_tokenArr[m_idx - 1].second;
	string	errorLine;
	string	errorLog;

	m_fstream.clear();
	m_fstream.seekg(ios_base::beg);
	cout << message << '\n';
	for (int i = 1; i < errorLineNumber - 2; ++i)
		getline(m_fstream, errorLine);

	if (errorLineNumber >= 3)
	{
		getline(m_fstream, errorLine);
		errorLine = "line " + Util::toString(errorLineNumber - 2) + ": " + errorLine;
		errorLog += errorLine + "\n";
	}
	if (errorLineNumber >= 2)
	{
		getline(m_fstream, errorLine);
		errorLine = "line " + Util::toString(errorLineNumber - 1) + ": " + errorLine;
		errorLog += errorLine + "\n";
	}
	getline(m_fstream, errorLine);
	errorLine = "line " + Util::toString(errorLineNumber) + ": " + errorLine;
	errorLog += errorLine + "  <<\n";
	return errorLog;
}

void
FileTokenizer::tokenize(const std::string& chunk, int lineNumber)
{
	for (string::size_type start = 0, end = 0; end < chunk.size(); ++end)
	{
		if (chunk[end] == ';' || chunk[end] == '{' || chunk[end] == '}')
		{
			if (start != end)
			{
				m_tokenArr.push_back(make_pair(chunk.substr(start, end - start), lineNumber));
			}
			m_tokenArr.push_back(make_pair(string(1, chunk[end]), lineNumber));
			start = end + 1;
		}
		else if (chunk[end] == '\0' && start != end)
		{
			m_tokenArr.push_back(make_pair(chunk.substr(start, end - start), lineNumber));
		}
	}
}

void
FileTokenizer::init(const string& path)
{
	string		line;

	m_fstream.open(path.data());
	for (int lineNumber = 1; m_fstream.good() == true; ++lineNumber)
	{
		if (std::getline(m_fstream, line).good() == false)
			break;
		if (line == "")
			continue;

		stringstream	ss(line);
		string			chunk;

		while (ss.good())
		{
			chunk.clear();
			ss >> chunk;
			if (chunk.size() == 0 || chunk[0] == '#')
				break;
			chunk.push_back('\0');
			tokenize(chunk, lineNumber);
		}
	}
	if (m_fstream.eof() == false && m_fstream.fail())
		throw FileTokenizerException("file read error");
	m_idx = 0;
}

void
FileTokenizer::printTokens() const
{
	for (unsigned int i = 0; i < m_tokenArr.size(); ++i)
		cout << "\'" << m_tokenArr[i].first << "'\n";
}

void
FileTokenizer::printTokensByLine() const
{
	int		line = 1;

	cout << "line 1: ";
	for (unsigned int i = 0; i < m_tokenArr.size(); ++i)
	{
		if (line != m_tokenArr[i].second)
		{
			++line;
			cout << "\nline " + Util::toString(line) + ": ";
		}
		cout << m_tokenArr[i].first << ' ';
	}
	cout << "\n";
}
