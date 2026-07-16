#include "network/Java6Http.h"

#include <cctype>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "httplib.h"

namespace
{

bool equalsIgnoreCaseAscii(const std::string &left, const char *right)
{
	const size_t rightLength = std::strlen(right);
	if (left.size() != rightLength)
		return false;
	for (size_t i = 0; i < left.size(); ++i)
	{
		if (std::tolower(static_cast<unsigned char>(left[i])) !=
			std::tolower(static_cast<unsigned char>(right[i])))
			return false;
	}
	return true;
}

struct HttpUrl
{
	std::string host;
	int port = 80;
	std::string path;
	std::string query;
	bool hasQuery = false;
};

constexpr bool isJava6RedirectStatus(int status)
{
	return status >= 300 && status <= 307 && status != 304 && status != 306;
}

constexpr bool isJava6RedirectLimitReached(int redirects)
{
	return redirects >= 20;
}

static_assert(isJava6RedirectStatus(300) && isJava6RedirectStatus(301) &&
	isJava6RedirectStatus(302) && isJava6RedirectStatus(303) &&
	isJava6RedirectStatus(305) && isJava6RedirectStatus(307),
	"Java 6 redirect statuses must be followed");
static_assert(!isJava6RedirectStatus(299) && !isJava6RedirectStatus(304) &&
	!isJava6RedirectStatus(306) && !isJava6RedirectStatus(308),
	"Non-Java 6 redirect statuses must not be followed");
static_assert(!isJava6RedirectLimitReached(19) && isJava6RedirectLimitReached(20),
	"Java 6 permits at most nineteen followed redirect responses");

std::string trimJavaUrlSpec(const std::string &value)
{
	size_t start = 0;
	size_t end = value.size();
	while (start < end && static_cast<unsigned char>(value[start]) <= ' ')
		++start;
	while (end > start && static_cast<unsigned char>(value[end - 1]) <= ' ')
		--end;
	std::string result = value.substr(start, end - start);
	if (result.size() >= 4 &&
		std::tolower(static_cast<unsigned char>(result[0])) == 'u' &&
		std::tolower(static_cast<unsigned char>(result[1])) == 'r' &&
		std::tolower(static_cast<unsigned char>(result[2])) == 'l' && result[3] == ':')
		result.erase(0, 4);
	return result;
}

size_t javaProtocolEnd(const std::string &value)
{
	if (!value.empty() && value[0] == '#')
		return std::string::npos;
	for (size_t i = 0; i < value.size() && value[i] != '/'; ++i)
	{
		if (value[i] != ':')
			continue;
		if (i == 0 || !std::isalpha(static_cast<unsigned char>(value[0])))
			return std::string::npos;
		for (size_t j = 1; j < i; ++j)
		{
			const unsigned char character = static_cast<unsigned char>(value[j]);
			if (!std::isalnum(character) && character != '.' && character != '+' && character != '-')
				return std::string::npos;
		}
		return i;
	}
	return std::string::npos;
}

int parseHttpPort(const std::string &value)
{
	if (value.empty())
		return 80;
	size_t digit = value[0] == '+' || value[0] == '-' ? 1 : 0;
	if (digit == value.size())
		throw std::runtime_error("java.net.MalformedURLException: Invalid port number");
	for (; digit < value.size(); ++digit)
	{
		if (value[digit] < '0' || value[digit] > '9')
			throw std::runtime_error("java.net.MalformedURLException: Invalid port number");
	}
	size_t used = 0;
	int port;
	try
	{
		port = std::stoi(value, &used);
	}
	catch (const std::exception &)
	{
		throw std::runtime_error("java.net.MalformedURLException: Invalid port number");
	}
	if (used != value.size() || port < -1)
		throw std::runtime_error("java.net.MalformedURLException: Invalid port number");
	return port == -1 ? 80 : port;
}

void parseHttpAuthority(const std::string &authority, HttpUrl &url)
{
	const size_t at = authority.find('@');
	if (at != std::string::npos && at != authority.rfind('@'))
		throw std::runtime_error("java.net.MalformedURLException: Invalid authority field: " + authority);
	const std::string hostAndPort = at == std::string::npos ? authority : authority.substr(at + 1);
	url.port = 80;
	if (!hostAndPort.empty() && hostAndPort[0] == '[')
	{
		const size_t close = hostAndPort.find(']');
		if (close == std::string::npos)
			throw std::runtime_error("java.net.MalformedURLException: Invalid authority field: " + authority);
		url.host = hostAndPort.substr(1, close - 1);
		if (close + 1 < hostAndPort.size())
		{
			if (hostAndPort[close + 1] != ':')
				throw std::runtime_error("java.net.MalformedURLException: Invalid authority field: " + authority);
			url.port = parseHttpPort(hostAndPort.substr(close + 2));
		}
	}
	else
	{
		const size_t colon = hostAndPort.find(':');
		url.host = hostAndPort.substr(0, colon);
		if (colon != std::string::npos)
			url.port = parseHttpPort(hostAndPort.substr(colon + 1));
	}
}

void normalizeJavaRelativePath(std::string &path)
{
	size_t index;
	while ((index = path.find("/./")) != std::string::npos)
		path = path.substr(0, index) + path.substr(index + 2);

	index = 0;
	while ((index = path.find("/../", index)) != std::string::npos)
	{
		const size_t previous = index == 0 ? std::string::npos : path.rfind('/', index - 1);
		if (index > 0 && previous != std::string::npos &&
			path.find("/../", previous) != 0)
		{
			path = path.substr(0, previous) + path.substr(index + 3);
			index = 0;
		}
		else
			index += 3;
	}
	while (path.size() >= 3 && path.compare(path.size() - 3, 3, "/..") == 0)
	{
		const size_t first = path.find("/..");
		const size_t previous = first == 0 ? std::string::npos : path.rfind('/', first - 1);
		if (previous == std::string::npos)
			break;
		path = path.substr(0, previous + 1);
	}
	if (path.size() > 2 && path.compare(0, 2, "./") == 0)
		path.erase(0, 2);
	if (path.size() >= 2 && path.compare(path.size() - 2, 2, "/.") == 0)
		path.erase(path.size() - 1);
}

bool resolveJava6Redirect(const HttpUrl &base, const std::string &location, HttpUrl &url)
{
	std::string spec = trimJavaUrlSpec(location);
	const size_t protocolEnd = javaProtocolEnd(spec);
	const bool absolute = protocolEnd != std::string::npos;
	bool sameProtocol = true;
	if (absolute)
	{
		const std::string protocol = spec.substr(0, protocolEnd);
		sameProtocol = equalsIgnoreCaseAscii(protocol, "http");
		if (!sameProtocol && !equalsIgnoreCaseAscii(protocol, "https") &&
			!equalsIgnoreCaseAscii(protocol, "file") && !equalsIgnoreCaseAscii(protocol, "ftp") &&
			!equalsIgnoreCaseAscii(protocol, "gopher") && !equalsIgnoreCaseAscii(protocol, "jar") &&
			!equalsIgnoreCaseAscii(protocol, "mailto") && !equalsIgnoreCaseAscii(protocol, "netdoc"))
			throw std::runtime_error("java.net.MalformedURLException: unknown protocol: " + protocol);
		spec.erase(0, protocolEnd + 1);
	}

	const size_t fragment = spec.find('#');
	if (fragment != std::string::npos)
		spec.erase(fragment);
	if (!absolute && spec.empty())
	{
		url = base;
		return true;
	}

	const size_t query = spec.find('?');
	const bool hasQuery = query != std::string::npos;
	const std::string path = spec.substr(0, query);
	url = absolute ? HttpUrl{} : base;
	url.hasQuery = hasQuery;
	url.query = hasQuery ? spec.substr(query + 1) : "";

	if (path.compare(0, 2, "//") == 0 && path.compare(0, 4, "////") != 0)
	{
		const size_t slash = path.find('/', 2);
		const std::string authority = path.substr(2, slash == std::string::npos ?
			std::string::npos : slash - 2);
		parseHttpAuthority(authority, url);
		if (absolute || !authority.empty() || slash != std::string::npos)
			url.path = slash == std::string::npos ? "" : path.substr(slash);
	}
	else if (absolute)
	{
		url.host.clear();
		url.path = path;
	}
	else if (!path.empty() && path[0] == '/')
		url.path = path;
	else if (!path.empty())
	{
		const bool relativePath = !url.path.empty();
		const size_t slash = url.path.rfind('/');
		url.path = (slash == std::string::npos ? "/" : url.path.substr(0, slash + 1)) + path;
		if (relativePath)
			normalizeJavaRelativePath(url.path);
	}
	else if (hasQuery)
	{
		const size_t slash = url.path.rfind('/');
		url.path = url.path.substr(0, slash == std::string::npos ? 0 : slash) + "/";
	}
	return sameProtocol;
}

std::string requestTarget(const HttpUrl &url)
{
	std::string result = url.path.empty() ? "/" : url.path;
	if (url.hasQuery)
		result += "?" + url.query;
	return result;
}

std::string externalForm(const HttpUrl &url)
{
	const bool ipv6 = url.host.find(':') != std::string::npos;
	std::string result = "http://" + (ipv6 ? "[" + url.host + "]" : url.host);
	if (url.port != 80)
		result += ":" + std::to_string(url.port);
	return result + requestTarget(url);
}

std::string java6HttpGet(HttpUrl url)
{
	int redirects = 0;
	bool useProxy = false;
	std::string proxyHost;
	int proxyPort = 80;
	while (true)
	{
		if (url.host.empty())
			throw std::runtime_error("java.io.IOException: Invalid HTTP host");

		httplib::Client client(url.host, url.port);
		if (useProxy)
			client.set_proxy(proxyHost, proxyPort);
		auto response = client.Get(requestTarget(url).c_str());
		if (!response)
			throw std::runtime_error("java.io.IOException: " + httplib::to_string(response.error()));

		const size_t locationCount = response->get_header_value_count("Location");
		if (isJava6RedirectStatus(response->status) && locationCount > 0)
		{
			HttpUrl redirect;
			if (resolveJava6Redirect(url,
				response->get_header_value("Location", "", locationCount - 1), redirect))
			{
				if (isJava6RedirectLimitReached(++redirects))
					throw std::runtime_error("java.net.ProtocolException: Server redirected too many times (" +
						std::to_string(redirects) + ")");
				if (response->status == 305)
				{
					proxyHost = redirect.host;
					proxyPort = redirect.port;
					useProxy = true;
				}
				else
				{
					url = std::move(redirect);
					useProxy = false;
				}
				continue;
			}
		}

		if (response->status >= 400)
		{
			if (response->status == 404 || response->status == 410)
				throw std::runtime_error("java.io.FileNotFoundException: " + externalForm(url));
			throw std::runtime_error("java.io.IOException: Server returned HTTP response code: " +
				std::to_string(response->status) + " for URL: " + externalForm(url));
		}
		return response->body;
	}
}

}

namespace Java6Http
{

std::string joinServer(const std::string &user, const std::string &sessionId,
	const std::string &serverId)
{
	HttpUrl url;
	url.host = "www.minecraft.net";
	url.path = "/game/joinserver.jsp";
	url.query = "user=" + user + "&sessionId=" + sessionId + "&serverId=" + serverId;
	url.hasQuery = true;
	return java6HttpGet(std::move(url));
}

bool smokeTest()
{
	HttpUrl base;
	base.host = "www.minecraft.net";
	base.path = "/game/joinserver.jsp";
	base.query = "old=1";
	base.hasQuery = true;
	HttpUrl result;

	bool ok = resolveJava6Redirect(base, "../auth/./check?next=1#ignored", result);
	ok = ok && result.host == base.host && result.port == 80 &&
		result.path == "/auth/check" && result.hasQuery && result.query == "next=1";

	ok = ok && resolveJava6Redirect(base, "//auth.example:8080/ok", result);
	ok = ok && result.host == "auth.example" && result.port == 8080 &&
		result.path == "/ok" && !result.hasQuery;

	ok = ok && resolveJava6Redirect(base, "?next=2", result);
	ok = ok && result.host == base.host && result.path == "/game/" &&
		result.hasQuery && result.query == "next=2";

	ok = ok && resolveJava6Redirect(base, "", result);
	ok = ok && result.host == base.host && result.path == base.path &&
		result.hasQuery && result.query == base.query;

	ok = ok && resolveJava6Redirect(base, "//", result);
	ok = ok && result.host.empty() && result.path == base.path && !result.hasQuery;

	HttpUrl root = base;
	root.path.clear();
	ok = ok && resolveJava6Redirect(root, "a/../b", result);
	ok = ok && result.path == "/a/../b";

	ok = ok && !resolveJava6Redirect(base, "https://auth.example/ok", result);
	try
	{
		resolveJava6Redirect(base, "https://auth.example: bad/ok", result);
		ok = false;
	}
	catch (const std::runtime_error &) {}
	try
	{
		resolveJava6Redirect(base, "unknown://auth.example/ok", result);
		ok = false;
	}
	catch (const std::runtime_error &) {}
	try
	{
		resolveJava6Redirect(base, "//one@two@auth.example/ok", result);
		ok = false;
	}
	catch (const std::runtime_error &) {}
	return ok;
}

}
