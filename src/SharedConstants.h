#pragma once

#include "java/String.h"
#include "ClientTarget.h"

namespace SharedConstants
{

static const jstring VERSION_STRING = ClientTarget::isAlphaPlace() ? u"Alpha v1.2.6" : u"Beta 1.7.3";
extern const int NETWORK_PROTOCOL_VERSION;
extern const int maxChatLength;
extern const jstring acceptableLetters;

}
