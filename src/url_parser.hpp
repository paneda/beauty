#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <string>

namespace http {
namespace server {

class UrlParser {
   public:
    UrlParser() : valid_(false) {}

    explicit UrlParser(const std::string &url) : valid_(true) {
        parse(url);
    }

    bool parse(const std::string &str) {
        url_ = Url();
        parse_(str);

        return isValid();
    }

    bool isValid() const {
        return valid_;
    }

    std::string scheme() const {
        assert(isValid());
        return url_.scheme_;
    }

    std::string username() const {
        assert(isValid());
        return url_.username_;
    }

    std::string password() const {
        assert(isValid());
        return url_.password_;
    }

    std::string hostname() const {
        assert(isValid());
        return url_.hostname_;
    }

    std::string port() const {
        assert(isValid());
        return url_.port_;
    }

    std::string path() const {
        assert(isValid());
        return url_.path_;
    }

    std::string query() const {
        assert(isValid());
        return url_.query_;
    }

    std::string fragment() const {
        assert(isValid());
        return url_.fragment_;
    }

    uint16_t httpPort() const {
        const uint16_t defaultHttpPort = 80;
        const uint16_t defaultHttpsPort = 443;

        assert(isValid());

        if (url_.port_.empty()) {
            if (scheme() == "https")
                return defaultHttpsPort;
            else
                return defaultHttpPort;
        } else {
            return url_.integerPort_;
        }
    }

   private:
    bool isUnreserved(char ch) const {
        if (isalnum(ch))
            return true;

        switch (ch) {
            case '-':
            case '.':
            case '_':
            case '~':
                return true;
        }

        return false;
    }

    void parse_(const std::string &str) {
        enum {
            scheme,
            slash_after_scheme1,
            slash_after_scheme_2,
            username_or_hostname,
            password,
            hostname,
            ipv6_host_name,
            port_or_password,
            port,
            path,
            query,
            fragment
        } state = scheme;

        std::string usernameOrHostname;
        std::string portOrPassword;

        valid_ = true;
        url_.path_ = "/";
        url_.integerPort_ = 0;

        for (size_t i = 0; i < str.size() && valid_; ++i) {
            char ch = str[i];

            switch (state) {
                case scheme:
                    if (isalnum(ch) || ch == '+' || ch == '-' || ch == '.') {
                        url_.scheme_ += ch;
                    } else if (ch == ':') {
                        state = slash_after_scheme1;
                    } else {
                        valid_ = false;
                        url_ = Url();
                    }
                    break;
                case slash_after_scheme1:
                    if (ch == '/') {
                        state = slash_after_scheme_2;
                    } else if (isalnum(ch)) {
                        usernameOrHostname = ch;
                        state = username_or_hostname;
                    } else {
                        valid_ = false;
                        url_ = Url();
                    }
                    break;
                case slash_after_scheme_2:
                    if (ch == '/') {
                        state = username_or_hostname;
                    } else {
                        valid_ = false;
                        url_ = Url();
                    }
                    break;
                case username_or_hostname:
                    if (isUnreserved(ch) || ch == '%') {
                        usernameOrHostname += ch;
                    } else if (ch == ':') {
                        state = port_or_password;
                    } else if (ch == '@') {
                        state = hostname;
                        std::swap(url_.username_, usernameOrHostname);
                    } else if (ch == '/') {
                        state = path;
                        std::swap(url_.hostname_, usernameOrHostname);
                    } else {
                        valid_ = false;
                        url_ = Url();
                    }
                    break;
                case password:
                    if (isalnum(ch) || ch == '%') {
                        url_.password_ += ch;
                    } else if (ch == '@') {
                        state = hostname;
                    } else {
                        valid_ = false;
                        url_ = Url();
                    }
                    break;
                case hostname:
                    if (ch == '[' && url_.hostname_.empty()) {
                        state = ipv6_host_name;
                    } else if (isUnreserved(ch) || ch == '%') {
                        url_.hostname_ += ch;
                    } else if (ch == ':') {
                        state = port;
                    } else if (ch == '/') {
                        state = path;
                    } else {
                        valid_ = false;
                        url_ = Url();
                    }
                    break;
                case ipv6_host_name:
                    abort();  // TODO
                case port_or_password:
                    if (isdigit(ch)) {
                        portOrPassword += ch;
                    } else if (ch == '/') {
                        std::swap(url_.hostname_, usernameOrHostname);
                        std::swap(url_.port_, portOrPassword);
                        url_.integerPort_ = atoi(url_.port_.c_str());
                        state = path;
                    } else if (isalnum(ch) || ch == '%') {
                        std::swap(url_.username_, usernameOrHostname);
                        std::swap(url_.password_, portOrPassword);
                        url_.password_ += ch;
                        state = password;
                    } else {
                        valid_ = false;
                        url_ = Url();
                    }
                    break;
                case port:
                    if (isdigit(ch)) {
                        portOrPassword += ch;
                    } else if (ch == '/') {
                        std::swap(url_.port_, portOrPassword);
                        url_.integerPort_ = atoi(url_.port_.c_str());
                        state = path;
                    } else {
                        valid_ = false;
                        url_ = Url();
                    }
                    break;
                case path:
                    if (ch == '#') {
                        state = fragment;
                    } else if (ch == '?') {
                        state = query;
                    } else {
                        url_.path_ += ch;
                    }
                    break;
                case query:
                    if (ch == '#') {
                        state = fragment;
                    } else if (ch == '?') {
                        state = query;
                    } else {
                        url_.query_ += ch;
                    }
                    break;
                case fragment:
                    url_.fragment_ += ch;
                    break;
            }
        }

        assert(portOrPassword.empty());

        if (!usernameOrHostname.empty()) {
            url_.hostname_ = usernameOrHostname;
        }
    }

    bool valid_;

    struct Url {
        Url() : integerPort_(0) {}

        std::string scheme_;
        std::string username_;
        std::string password_;
        std::string hostname_;
        std::string port_;
        std::string path_;
        std::string query_;
        std::string fragment_;
        uint16_t integerPort_;
    } url_;
};

}  // namespace server
}  // namespace http

