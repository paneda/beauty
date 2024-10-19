#pragma once
#include <string>

namespace beauty {

class WsSecAccept {
   public:
    WsSecAccept();
    WsSecAccept(const WsSecAccept &) = delete;
    WsSecAccept &operator=(const WsSecAccept &) = delete;

    std::string compute(const char *key);
};

}  // namespace beauty
