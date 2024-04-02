#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <numeric>

#include "file_handler.hpp"
#include "utils/mock_file_handler.hpp"

using namespace http::server;

TEST_CASE("file_handler.cpp", "[file_handler]") {
    // fixture: create a file (if not exist) with uint32_t values
    std::vector<uint32_t> arr(100);
    size_t typeSize = sizeof(decltype(arr)::value_type);
    std::iota(arr.begin(), arr.end(), 0);
    std::ifstream f("testfile.bin");
    if (!f.good()) {
        std::ofstream of("testfile.bin", std::ios::out | std::ios::binary);
        for (uint32_t val : arr) {
            of.write((char*)&val, sizeof(val));
        }
    }
    FileHandler fh("./");
    std::vector<char> body;  // not used in tests
    Request req(body);       // not used in mock
    Reply rep(1024);
    rep.filePath_ = "testfile.bin";

    SECTION("should open file") {
        REQUIRE(fh.openFileForRead("0", req, rep));
    }
    SECTION("should close file idempotent") {
        fh.openFileForRead("0", req, rep);
        fh.closeReadFile("0");
        fh.closeReadFile("0");
        fh.closeReadFile("0");
    }
    SECTION("should provide correct size") {
        size_t fileSize = fh.openFileForRead("0", req, rep);
        REQUIRE(fileSize == arr.size() * typeSize);
    }
    SECTION("should read chunks") {
        fh.openFileForRead("0", req, rep);
        std::vector<uint32_t> readData(10);

        fh.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fh.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
    SECTION("should allow parallell reads") {
        fh.openFileForRead("0", req, rep);
        std::vector<uint32_t> readData(10);

        fh.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);

        fh.openFileForRead("1", req, rep);

        fh.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);

        fh.readFile("1", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fh.closeReadFile("0");

        fh.readFile("1", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
}

TEST_CASE("Reading from MockFileHandler", "[file_handler]") {
    std::vector<uint32_t> arr(100);
    size_t typeSize = sizeof(decltype(arr)::value_type);
    std::iota(arr.begin(), arr.end(), 0);
    MockFileHandler fh;
    fh.createMockFile(100 * typeSize);
    fh.createMockFile(100 * typeSize);
    std::vector<char> body;
    Request req(body);  // not used in mock
    Reply rep(1024);
    rep.filePath_ = "testfile.bin";

    SECTION("should open file") {
        REQUIRE(fh.openFileForRead("0", req, rep));
    }
    SECTION("should throw if call open() when already opened for read") {
        REQUIRE(fh.openFileForRead("0", req, rep));
        REQUIRE_THROWS_AS(fh.openFileForRead("0", req, rep), std::runtime_error);
    }
    SECTION("should provide correct size") {
        size_t fileSize = fh.openFileForRead("0", req, rep);
        REQUIRE(fileSize == arr.size() * typeSize);
    }
    SECTION("should throw if call read() before opened") {
        std::vector<uint32_t> readData(10);
        REQUIRE_THROWS_AS(fh.readFile("0", req, (char*)readData.data(), readData.size() * typeSize),
                          std::runtime_error);
    }
    SECTION("should read chunks") {
        fh.openFileForRead("0", req, rep);
        std::vector<uint32_t> readData(10);

        fh.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fh.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
    SECTION("should support parallel reads") {
        fh.openFileForRead("0", req, rep);
        std::vector<uint32_t> readData(10);

        fh.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);

        fh.openFileForRead("1", req, rep);

        fh.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);

        fh.readFile("1", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fh.closeReadFile("0");

        fh.readFile("1", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
}

TEST_CASE("Writing to MockFileHandler", "[file_handler]") {
    MockFileHandler fh;
    std::string err;
    std::vector<char> body;
    Request req(body);  // not used in mock
    Reply rep(1024);
    rep.filePath_ = "testfile.bin";

    SECTION("should open file") {
        REQUIRE(fh.openFileForWrite("0", req, rep, err));
    }
    SECTION("should throw if call open() when already opened for write") {
        REQUIRE(fh.openFileForWrite("0", req, rep, err));
        REQUIRE_THROWS_AS(fh.openFileForWrite("0", req, rep, err), std::runtime_error);
    }
    SECTION("should throw if call write() before opened") {
        std::vector<uint32_t> writeData(10);
        std::string err;
        REQUIRE_THROWS_AS(
            fh.writeFile("0", req, (char*)writeData.data(), writeData.size(), false, err),
            std::runtime_error);
    }
    SECTION("should write chunks") {
        std::string err;
        fh.openFileForWrite("0", req, rep, err);
        std::vector<char> writeData1 = {'a', 'b', 'c', 'd', 'e'};

        fh.writeFile("0", req, writeData1.data(), writeData1.size(), false, err);
        std::vector<char> result = fh.getMockWriteFile("0");
        REQUIRE(result == writeData1);

        std::vector<char> writeData2 = {'f', 'g', 'h'};
        fh.writeFile("0", req, writeData2.data(), writeData2.size(), true, err);
        result = fh.getMockWriteFile("0");
        writeData1.insert(writeData1.end(), writeData2.begin(), writeData2.end());
        REQUIRE(result == writeData1);
    }
    SECTION("should support parallel writes") {
        std::string err;
        fh.openFileForWrite("0", req, rep, err);
        fh.openFileForWrite("1", req, rep, err);

        std::vector<char> writeData1 = {'a', 'b', 'c', 'd', 'e'};
        std::vector<char> writeData2 = {'f', 'g', 'h'};

        fh.writeFile("0", req, writeData1.data(), writeData1.size(), true, err);
        fh.writeFile("1", req, writeData2.data(), writeData2.size(), true, err);

        std::vector<char> result = fh.getMockWriteFile("0");
        REQUIRE(result == writeData1);

        result = fh.getMockWriteFile("1");
        REQUIRE(result == writeData2);
    }
}
