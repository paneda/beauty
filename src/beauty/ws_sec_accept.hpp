#pragma once
#include <string>

namespace beauty {

class IRandom;

// Compute the value of the Sec-WebSocket-Accept response header from the value
// of the Sec-WebSocket-Key request header (server side, RFC6455 section 1.3).
std::string computeWsSecAccept(const char *key);

// Generate a new random Sec-WebSocket-Key header value (client side, RFC6455
// section 4.1): 16 random bytes, base64 encoded into a 24 character string.
std::string generateWsSecKey(IRandom &random);

// Verify a server's Sec-WebSocket-Accept response header against the
// Sec-WebSocket-Key that was sent by the client (client side). Leading and
// trailing whitespace in the accept header is ignored.
bool verifyWsSecAccept(const std::string &key, const std::string &acceptHeader);

}  // namespace beauty
