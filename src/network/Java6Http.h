#pragma once

#include <string>

namespace Java6Http
{

std::string joinServer(const std::string &user, const std::string &sessionId,
	const std::string &serverId);
bool smokeTest();

}
