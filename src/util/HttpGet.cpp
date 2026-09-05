// B173 - a126cpp portable HttpGet, commit 92974b6b71780cb86d6f35d1291ccb794d034bd2.
// Local socket startup/types and the existing Betacraft proxy route are PC integration.
#include "util/HttpGet.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace HttpGet
{

#ifdef _WIN32
typedef SOCKET SocketHandle;
static const SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;

struct SocketRuntime
{
	int result;
	SocketRuntime()
	{
		WSADATA data;
		result = WSAStartup(MAKEWORD(2, 2), &data);
	}
	~SocketRuntime()
	{
		if (result == 0)
			WSACleanup();
	}
};
#else
typedef int SocketHandle;
static const SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

// Enough for a skin sheet many times over; a server that sends more than this
// is misbehaving and the read is abandoned rather than growing without bound.
static const size_t MAX_BODY_BYTES = 8u * 1024u * 1024u;

static void closeHandle(SocketHandle handle)
{
#ifdef _WIN32
	::closesocket(handle);
#else
	::close(handle);
#endif
}

struct Url
{
	std::string host;
	std::string path;
	int_t port = 80;
};

static bool parseUrl(const std::string &url, Url &out, std::string &error)
{
	const size_t schemeEnd = url.find("://");
	if (schemeEnd == std::string::npos)
	{
		error = "malformed url";
		return false;
	}
	if (url.compare(0, schemeEnd, "http") != 0)
	{
		error = "unsupported scheme (only http)";
		return false;
	}

	const std::string remainder = url.substr(schemeEnd + 3);
	const size_t pathStart = remainder.find('/');
	const std::string authority = remainder.substr(0, pathStart);
	out.path = (pathStart == std::string::npos) ? "/" : remainder.substr(pathStart);

	const size_t portSeparator = authority.find(':');
	if (portSeparator == std::string::npos)
	{
		out.host = authority;
	}
	else
	{
		out.host = authority.substr(0, portSeparator);
		out.port = std::atoi(authority.c_str() + portSeparator + 1);
		if (out.port <= 0 || out.port > 65535)
		{
			error = "bad port";
			return false;
		}
	}

	if (out.host.empty())
	{
		error = "empty host";
		return false;
	}
	return true;
}

static bool connectTo(const std::string &host, int_t port, int_t timeoutSeconds,
	SocketHandle &out, std::string &error)
{
	// getaddrinfo rather than gethostbyname: it is the portable spelling and
	// libnx implements it.
	struct addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	const std::string service = std::to_string(port);
	struct addrinfo *candidates = nullptr;
	if (::getaddrinfo(host.c_str(), service.c_str(), &hints, &candidates) != 0 ||
		candidates == nullptr)
	{
		error = "dns lookup failed for " + host;
		return false;
	}

	SocketHandle handle = INVALID_SOCKET_HANDLE;
	for (const struct addrinfo *candidate = candidates; candidate != nullptr;
		candidate = candidate->ai_next)
	{
		handle = ::socket(candidate->ai_family, candidate->ai_socktype,
			candidate->ai_protocol);
		if (handle == INVALID_SOCKET_HANDLE)
			continue;

		// Receive/send timeouts, so a silent host cannot wedge the calling
		// thread forever.
#ifdef _WIN32
		const DWORD milliseconds = static_cast<DWORD>(timeoutSeconds) * 1000;
		::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO,
			reinterpret_cast<const char *>(&milliseconds), sizeof(milliseconds));
		::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO,
			reinterpret_cast<const char *>(&milliseconds), sizeof(milliseconds));
#else
		struct timeval timeout = {};
		timeout.tv_sec = timeoutSeconds;
		::setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		::setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

		if (::connect(handle, candidate->ai_addr,
			static_cast<int>(candidate->ai_addrlen)) == 0)
		{
			break;
		}

		closeHandle(handle);
		handle = INVALID_SOCKET_HANDLE;
	}

	::freeaddrinfo(candidates);

	if (handle == INVALID_SOCKET_HANDLE)
	{
		error = "connect failed to " + host;
		return false;
	}

	out = handle;
	return true;
}

static bool sendAll(SocketHandle handle, const std::string &data)
{
	size_t sent = 0;
	while (sent < data.size())
	{
		const int written = ::send(handle, data.data() + sent,
			static_cast<int>(data.size() - sent), 0);
		if (written <= 0)
			return false;
		sent += static_cast<size_t>(written);
	}
	return true;
}

// Waits for the socket to become readable. SO_RCVTIMEO alone is not portable
// enough to rely on: the Switch BSD service returns ETIMEDOUT from a blocking
// recv even when a response is pending, while poll reports readability
// correctly. Polling first also gives a real timeout on stacks that ignore
// SO_RCVTIMEO entirely.
static bool waitReadable(SocketHandle handle, int_t timeoutSeconds)
{
	const int milliseconds = static_cast<int>(timeoutSeconds) * 1000;
#ifdef _WIN32
	WSAPOLLFD waiting = {};
	waiting.fd = handle;
	waiting.events = POLLRDNORM;
	return WSAPoll(&waiting, 1, milliseconds) > 0;
#else
	struct pollfd waiting = {};
	waiting.fd = handle;
	waiting.events = POLLIN;
	return ::poll(&waiting, 1, milliseconds) > 0;
#endif
}

// One recv, appended to out. False once the peer closes, errors or stalls.
static bool receiveSome(SocketHandle handle, std::string &out, int_t timeoutSeconds)
{
	if (!waitReadable(handle, timeoutSeconds))
		return false;

	char buffer[8192];
	// Winsock's recv takes an int length, so the cast is required there and
	// harmless on POSIX.
	const int read = ::recv(handle, buffer, static_cast<int>(sizeof(buffer)), 0);
	if (read <= 0)
		return false;
	out.append(buffer, static_cast<size_t>(read));
	return out.size() <= MAX_BODY_BYTES;
}

enum class ChunkState
{
	Incomplete,
	Complete,
	Malformed
};

// Decodes as much of a chunked body as is present. Servers commonly ignore
// "Connection: close", so the terminating zero-length chunk is the only
// reliable end-of-body signal.
static ChunkState decodeChunked(const std::string &encoded, std::string &out)
{
	out.clear();

	size_t offset = 0;
	for (;;)
	{
		const size_t lineEnd = encoded.find("\r\n", offset);
		if (lineEnd == std::string::npos)
			return ChunkState::Incomplete;

		char *parseEnd = nullptr;
		const unsigned long size = std::strtoul(
			encoded.substr(offset, lineEnd - offset).c_str(), &parseEnd, 16);
		if (parseEnd == nullptr || parseEnd == encoded.c_str())
			return ChunkState::Malformed;

		offset = lineEnd + 2;
		if (size == 0)
			return ChunkState::Complete;

		if (offset + size + 2 > encoded.size())
			return ChunkState::Incomplete;

		out.append(encoded, offset, size);
		offset += size + 2; // skip the chunk's trailing CRLF
	}
}

static std::string toLower(const std::string &value)
{
	std::string lowered = value;
	for (char &character : lowered)
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	return lowered;
}

static int_t parseStatus(const std::string &statusLine)
{
	// "HTTP/1.1 200 OK"
	const size_t firstSpace = statusLine.find(' ');
	if (firstSpace == std::string::npos)
		return 0;
	return static_cast<int_t>(std::atoi(statusLine.c_str() + firstSpace + 1));
}

Response fetch(const std::string &url, int_t timeoutSeconds, const std::string &proxyHost, int_t proxyPort)
{
	Response response;

#ifdef _WIN32
	static SocketRuntime sockets;
	if (sockets.result != 0)
	{
		response.error = "WSAStartup failed: " + std::to_string(sockets.result);
		return response;
	}
#endif

	Url parsed;
	if (!parseUrl(url, parsed, response.error))
		return response;

	SocketHandle handle = INVALID_SOCKET_HANDLE;
	if (!connectTo(proxyHost.empty() ? parsed.host : proxyHost,
		proxyHost.empty() ? parsed.port : proxyPort, timeoutSeconds, handle, response.error))
		return response;

	// RFC 7230 requires the port in Host whenever it is not the scheme default,
	// otherwise a name-based virtual host on a non-standard port never matches.
	const std::string hostHeader = (parsed.port == 80)
		? parsed.host
		: parsed.host + ":" + std::to_string(parsed.port);
	const std::string &target = proxyHost.empty() ? parsed.path : url;

	const std::string request =
		"GET " + target + " HTTP/1.1\r\n"
		"Host: " + hostHeader + "\r\n"
		"User-Agent: a126cpp\r\n"
		"Accept: */*\r\n"
		"Connection: close\r\n"
		"\r\n";

	if (!sendAll(handle, request))
	{
		closeHandle(handle);
		response.error = "send failed";
		return response;
	}

	// Headers first, so the body length is known before reading it.
	std::string raw;
	size_t headerEnd = std::string::npos;
	while ((headerEnd = raw.find("\r\n\r\n")) == std::string::npos)
	{
		if (!receiveSome(handle, raw, timeoutSeconds))
			break;
	}

	if (headerEnd == std::string::npos)
	{
		closeHandle(handle);
		response.error = raw.empty() ? "no response" : "headers truncated";
		return response;
	}

	const std::string head = raw.substr(0, headerEnd);
	const std::string loweredHead = toLower(head);
	response.status = parseStatus(head.substr(0, head.find("\r\n")));

	const bool chunked =
		loweredHead.find("transfer-encoding: chunked") != std::string::npos;

	size_t contentLength = 0;
	bool haveContentLength = false;
	const size_t lengthHeader = loweredHead.find("content-length:");
	if (!chunked && lengthHeader != std::string::npos)
	{
		contentLength = std::strtoul(
			loweredHead.c_str() + lengthHeader + std::strlen("content-length:"),
			nullptr, 10);
		haveContentLength = true;
	}

	std::string body = raw.substr(headerEnd + 4);

	if (chunked)
	{
		// Keep reading until the zero-length chunk arrives.
		std::string decoded;
		ChunkState state = decodeChunked(body, decoded);
		while (state == ChunkState::Incomplete)
		{
			if (!receiveSome(handle, body, timeoutSeconds))
				break;
			state = decodeChunked(body, decoded);
		}

		closeHandle(handle);

		if (state != ChunkState::Complete)
		{
			response.error = (state == ChunkState::Malformed)
				? "malformed chunked body"
				: "chunked body truncated";
			return response;
		}
		body = std::move(decoded);
	}
	else if (haveContentLength)
	{
		while (body.size() < contentLength)
		{
			if (!receiveSome(handle, body, timeoutSeconds))
				break;
		}
		closeHandle(handle);

		if (body.size() < contentLength)
		{
			response.error = "body truncated";
			return response;
		}
		body.resize(contentLength);
	}
	else
	{
		// Neither framing header: HTTP/1.0 style, body ends at close.
		while (receiveSome(handle, body, timeoutSeconds))
		{
		}
		closeHandle(handle);
	}

	response.body.assign(body.begin(), body.end());
	return response;
}

}
