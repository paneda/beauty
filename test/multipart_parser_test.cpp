#include <catch2/catch_test_macros.hpp>

#include "multipart_parser.hpp"
#include "request.hpp"

using namespace beauty;

namespace {

std::vector<char> convertToCharVec(const std::string &s) {
    std::vector<char> ret(s.begin(), s.end());
    return ret;
}

struct Fixture {
    Fixture(size_t maxContentSize) : parser_(content_) {
        content_.reserve(maxContentSize);
    }
    bool parseHeader(const Request &req) {
        return parser_.parseHeader(req);
    }
    MultiPartParser::result_type parse(const Request &req,
                                       std::vector<char> &content,
                                       std::deque<MultiPartParser::ContentPart> &parts) {
        return parser_.parse(req, content, parts);
    }

    void flush(std::vector<char> &content, std::deque<MultiPartParser::ContentPart> &parts) {
        parser_.flush(content, parts);
    }

    const std::deque<MultiPartParser::ContentPart> &peakLastPart() const {
        return parser_.peakLastPart();
    }

    std::vector<char> content_;
    MultiPartParser parser_;
};

}  // namespace

TEST_CASE("parse header", "[multipart_parser]") {
    Fixture fixture(1024);
    std::vector<char> body;  // not used in tests
    Request request(body);

    SECTION("should return OK when boundary is last") {
        request.headers_.push_back({"From", "user@example.com"});
        request.headers_.push_back(
            {"Content-Type",
             "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
        request.headers_.push_back({"Content-Length", "281"});
        REQUIRE(fixture.parseHeader(request));
    }
    SECTION("should return OK when boundary is first") {
        request.headers_.push_back({"From", "user@example.com"});
        request.headers_.push_back(
            {"Content-Type",
             "boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE; multipart/form-data;"});
        request.headers_.push_back({"Content-Length", "281"});
        REQUIRE(fixture.parseHeader(request));
    }
}

TEST_CASE("parse single part content", "[multipart_parser]") {
    Fixture fixture(1024);
    std::vector<char> body;  // not used in tests
    Request request(body);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "281"});
    REQUIRE(fixture.parseHeader(request));  // make sure boundary is set

    const std::string contentStr =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Content-Disposition: form-data; name=\"file1\"; filename=\"testfile01.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "This body is a bit tricky as it contains some ------WebKitFormBoundary chars, but not "
        "all,.\n"
        "\r\n------WebKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    std::vector<char> content = convertToCharVec(contentStr);

    SECTION("should return done for single parts") {
        // getting back the initial empty parts
        std::deque<MultiPartParser::ContentPart> parts;
        REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::done);
        REQUIRE(parts.size() == 0);

        // result==done so call flush
        fixture.flush(content, parts);
        REQUIRE(parts.size() == 1);
        REQUIRE(parts[0].filename_ == "testfile01.txt");
        REQUIRE(parts[0].foundStart_);
        REQUIRE(*parts[0].start_ == 'T');
        REQUIRE(parts[0].foundEnd_);
        REQUIRE(*(parts[0].end_ - 2) == '.');
    }
}

TEST_CASE("parse multi-part content", "[multipart_parser]") {
    Fixture fixture(1024);
    std::vector<char> body;  // not used in tests
    Request request(body);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "358"});
    REQUIRE(fixture.parseHeader(request));  // make sure boundary is set

    const std::string contentStr =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Content-Disposition: form-data; name=\"file1\"; filename=\"testfile01.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "First part.\n"
        "\r\n"
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Content-Disposition: form-data; name=\"file1\"; filename=\"testfile02.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Second part!\n"
        "\r\n------WebKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    std::vector<char> content = convertToCharVec(contentStr);

    SECTION("should return done for multiple parts") {
        std::deque<MultiPartParser::ContentPart> parts;
        // getting back the initial empty parts
        REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::done);
        REQUIRE(parts.size() == 0);

        // result==done so call flush
        fixture.flush(content, parts);
        REQUIRE(parts.size() == 2);
        REQUIRE(parts[0].filename_ == "testfile01.txt");
        REQUIRE(parts[0].foundStart_);
        REQUIRE(*parts[0].start_ == 'F');
        REQUIRE(parts[0].foundEnd_);
        REQUIRE(*(parts[0].end_ - 2) == '.');

        REQUIRE(parts[1].filename_ == "testfile02.txt");
        REQUIRE(parts[0].foundStart_);
        REQUIRE(*parts[1].start_ == 'S');
        REQUIRE(parts[0].foundEnd_);
        REQUIRE(*(parts[1].end_ - 2) == '!');
    }
}

TEST_CASE("parse until start of content", "[multipart_parser]") {
    Fixture fixture(1024);
    std::vector<char> body;  // not used in tests
    Request request(body);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type",
         "multipart/form-data; boundary=--------------------------567026409988538820744572"});
    request.headers_.push_back({"Content-Length", "224"});
    REQUIRE(fixture.parseHeader(request));  // make sure boundary is set

    const std::string contentStr1 =
        "----------------------------567026409988538820744572\r\nContent-Disposition: form-data; "
        "name=\"file1\"; filename=\"firstpart.txt\"\r\nContent-Type: text/plain\r\n\r\n";

    const std::string contentStr2 =
        "First part.\n\r\n----------------------------567026409988538820744572--\r\n";

    SECTION("should return start and end not found for first part") {
        std::deque<MultiPartParser::ContentPart> parts;
        // getting back the initial empty parts
        std::vector<char> content = convertToCharVec(contentStr1);
        REQUIRE(fixture.parse(request, content, parts) ==
                MultiPartParser::result_type::indeterminate);
        REQUIRE(parts.size() == 0);

        content = convertToCharVec(contentStr2);
        REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::done);
        REQUIRE(parts.size() == 1);
        REQUIRE(parts[0].filename_ == "firstpart.txt");
        REQUIRE(!parts[0].foundStart_);
        REQUIRE(!parts[0].foundEnd_);
        REQUIRE(parts[0].headerOnly_);

        // result == done so call flush
        fixture.flush(content, parts);
        REQUIRE(parts.size() == 1);
        REQUIRE(parts[0].filename_ == "");
        REQUIRE(parts[0].foundStart_);
        REQUIRE(parts[0].foundEnd_);
        REQUIRE(!parts[0].headerOnly_);
        REQUIRE(*parts[0].start_ == 'F');
        REQUIRE(*(parts[0].end_ - 2) == '.');
    }
}

TEST_CASE("parse empty content", "[multipart_parser]") {
    Fixture fixture(1024);
    std::vector<char> body;  // not used in tests
    Request request(body);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "184"});
    REQUIRE(fixture.parseHeader(request));  // make sure boundary is set

    std::vector<char> content;

    SECTION("should return indeterminate") {
        std::deque<MultiPartParser::ContentPart> parts;
        // getting back the initial empty parts
        REQUIRE(fixture.parse(request, content, parts) ==
                MultiPartParser::result_type::indeterminate);
        REQUIRE(parts.size() == 0);

        // result==done so call flush
        fixture.flush(content, parts);
        REQUIRE(parts.size() == 0);
    }
}

TEST_CASE("parse empty part content", "[multipart_parser]") {
    Fixture fixture(1024);
    std::vector<char> body;  // not used in tests
    Request request(body);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "184"});
    REQUIRE(fixture.parseHeader(request));  // make sure boundary is set

    const std::string contentStr =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\n"
        "Content-Disposition: form-data; name=\"file1\"; filename=\"empty.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "\r\n"
        "------WebKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    std::vector<char> content = convertToCharVec(contentStr);

    SECTION("should return size = 0") {
        std::deque<MultiPartParser::ContentPart> parts;
        // getting back the initial empty parts
        REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::done);
        REQUIRE(parts.size() == 0);

        // result==done so call flush
        fixture.flush(content, parts);
        REQUIRE(parts.size() == 1);
        REQUIRE(parts[0].filename_ == "empty.txt");
        REQUIRE(parts[0].foundStart_);
        REQUIRE(parts[0].foundEnd_);
        REQUIRE(parts[0].start_ == parts[0].end_);
    }
}

TEST_CASE("content start and end in consecutive buffers", "[multipart_parser]") {
    Fixture fixture(1024);
    std::vector<char> body;  // not used in tests
    Request request(body);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "281"});
    REQUIRE(fixture.parseHeader(request));  // make sure boundary is set

    const std::string contentStr1 =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\nContent-Disposition: form-data; "
        "name=\"file1\"; filename=\"testfile01.txt\"\r\nContent-Type: text/plain\r\n\r\nThis bo";
    const std::string contentStr2 =
        "dy is a bit tricky as it contains some ------WebKitFormBoundary chars, but not "
        "all.\n\r\n------WebKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    std::deque<MultiPartParser::ContentPart> parts;

    // getting back the initial empty parts
    std::vector<char> content = convertToCharVec(contentStr1);
    REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::indeterminate);
    REQUIRE(parts.size() == 0);

    // peaking the last part..
    const std::deque<MultiPartParser::ContentPart> &peakParts = fixture.peakLastPart();
    REQUIRE(peakParts.size() == 1);
    REQUIRE(peakParts[0].filename_ == "testfile01.txt");
    REQUIRE(peakParts[0].foundStart_);
    REQUIRE(*peakParts[0].start_ == 'T');
    REQUIRE(!peakParts[0].foundEnd_);
    REQUIRE(*(peakParts[0].end_ - 1) == 'o');

    // ..that is actually delivered in the next parse call
    content = convertToCharVec(contentStr2);
    REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::done);
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0].filename_ == "testfile01.txt");
    REQUIRE(parts[0].foundStart_);
    REQUIRE(*parts[0].start_ == 'T');
    REQUIRE(!parts[0].foundEnd_);
    REQUIRE(*(parts[0].end_ - 1) == 'o');

    // result==done so call flush
    fixture.flush(content, parts);
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0].filename_ == "");
    REQUIRE(!parts[0].foundStart_);
    REQUIRE(*parts[0].start_ == 'd');
    REQUIRE(parts[0].foundEnd_);
    REQUIRE(*(parts[0].end_ - 2) == '.');
}

TEST_CASE("content end in next to last body part", "[multipart_parser]") {
    Fixture fixture(1024);
    std::vector<char> body;  // not used in tests
    Request request(body);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "336"});
    REQUIRE(fixture.parseHeader(request));  // make sure boundary is set

    const std::string contentStr1 =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\nContent-Disposition: form-data; n";
    const std::string contentStr2 =
        "ame=\"file1\"; filename=\"testfile01.txt\"\r\nContent-Type: text/plain\r\n\r\nThis bo";
    const std::string contentStr3 =
        "dy is a bit tricky as it contains some ------WebKitFormBoundary chars, but ";
    const std::string contentStr4 =
        "not all. Note that the closing boundary comes in the last part.\n\r\n------We";
    const std::string contentStr5 = "bKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    std::deque<MultiPartParser::ContentPart> parts;

    // getting back the initial empty parts
    std::vector<char> content = convertToCharVec(contentStr1);
    REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::indeterminate);
    REQUIRE(parts.size() == 0);

    // Note: Whenever no start or end is found, the multipart parser assumes
    // that we're in the middle of a large file and creates a part with
    // start/end to match the entire request.content_ buffer. However as this
    // use case uses very small contentStr's (75 chars), this situation happens
    // here for the first contentStr1. For real use cases it must be
    // a requirement that if receiving any body data in the first request, we
    // at least get all the first multipart headers.
    // So here we "overlook" this and get parts.size() == 1, but really it
    // should be 0. But to get '0', the code would need more logic that in
    // practise doesn't make much sense.
    content = convertToCharVec(contentStr2);
    REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::indeterminate);
    REQUIRE(parts.size() == 1);

    content = convertToCharVec(contentStr3);
    REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::indeterminate);
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0].filename_ == "testfile01.txt");
    REQUIRE(parts[0].foundStart_);
    REQUIRE(*parts[0].start_ == 'T');
    REQUIRE(!parts[0].foundEnd_);
    REQUIRE(*(parts[0].end_ - 1) == 'o');

    content = convertToCharVec(contentStr4);
    REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::indeterminate);
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0].filename_ == "");
    REQUIRE(!parts[0].foundStart_);
    REQUIRE(*parts[0].start_ == 'd');
    REQUIRE(!parts[0].foundEnd_);
    REQUIRE(*(parts[0].end_ - 1) == ' ');

    content = convertToCharVec(contentStr5);
    REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::done);
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0].filename_ == "");
    REQUIRE(!parts[0].foundStart_);
    REQUIRE(*parts[0].start_ == 'n');
    REQUIRE(parts[0].foundEnd_);
    REQUIRE(*(parts[0].end_ - 2) == '.');

    // result==done so call flush
    fixture.flush(content, parts);
    REQUIRE(parts.size() == 0);
}

TEST_CASE("content end in previous body part and last part contain content", "[multipart_parser]") {
    Fixture fixture(1024);
    std::vector<char> body;  // not used in tests
    Request request(body);
    request.headers_.push_back({"From", "user@example.com"});
    request.headers_.push_back(
        {"Content-Type", "multipart/form-data; boundary=----WebKitFormBoundarylSu7ajtLodoq9XHE"});
    request.headers_.push_back({"Content-Length", "336"});
    REQUIRE(fixture.parseHeader(request));  // make sure boundary is set

    const std::string contentStr1 =
        "------WebKitFormBoundarylSu7ajtLodoq9XHE\r\nContent-Disposition: form-data; "
        "name=\"file1\"; filename=\"testfile01.txt\"\r\nContent-Type: text/plain\r\n\r\nFirst "
        "part.\n\r\n------We"
        "bKitFormBoundarylSu7ajtLodoq9XHE\r\nContent-Disposition: form-data; name=\"file2\"; "
        "filename=\"testfile02.txt\"\r\nContent-Type: text/plain\r\n\r\nSecond "
        "part!\n\r\n------WebKitFor";
    const std::string contentStr2 =
        "mBoundarylSu7ajtLodoq9XHE\r\nContent-Disposition: form-data; name=\"file3\"; "
        "filename=\"testfile03.txt\"\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "Third part!\n"
        "\r\n------WebKitFormBoundarylSu7ajtLodoq9XHE--\r\n";
    std::deque<MultiPartParser::ContentPart> parts;

    // getting back the initial empty parts
    std::vector<char> content = convertToCharVec(contentStr1);
    REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::indeterminate);
    REQUIRE(parts.size() == 0);

    content = convertToCharVec(contentStr2);
    REQUIRE(fixture.parse(request, content, parts) == MultiPartParser::result_type::done);
    REQUIRE(parts.size() == 2);
    REQUIRE(parts[0].filename_ == "testfile01.txt");
    REQUIRE(parts[0].foundStart_);
    REQUIRE(*parts[0].start_ == 'F');
    REQUIRE(parts[0].foundEnd_);
    REQUIRE(*(parts[0].end_ - 2) == '.');
    REQUIRE(parts[1].filename_ == "testfile02.txt");
    REQUIRE(*parts[1].start_ == 'S');
    REQUIRE(*(parts[1].end_ - 2) == '!');

    // result==done so call flush
    fixture.flush(content, parts);
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0].filename_ == "testfile03.txt");
    REQUIRE(parts[0].foundStart_);
    REQUIRE(*parts[0].start_ == 'T');
    REQUIRE(parts[0].foundEnd_);
    REQUIRE(*(parts[0].end_ - 2) == '!');
}
