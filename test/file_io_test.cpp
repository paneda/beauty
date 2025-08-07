#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <numeric>

#include "file_io.hpp"
#include "utils/mock_file_io.hpp"

using namespace beauty;

TEST_CASE("file_io.cpp", "[file_io]") {
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
    FileIO fio("./");
    std::vector<char> body;  // not used in tests
    Request req(body);       // not used in mock
    Reply rep(1024);
    rep.filePath_ = "testfile.bin";

    SECTION("should open file") {
        REQUIRE(fio.openFileForRead("0", req, rep));
    }
    SECTION("should close file idempotent") {
        fio.openFileForRead("0", req, rep);
        fio.closeReadFile("0");
        fio.closeReadFile("0");
        fio.closeReadFile("0");
    }
    SECTION("should provide correct size") {
        size_t fileSize = fio.openFileForRead("0", req, rep);
        REQUIRE(fileSize == arr.size() * typeSize);
    }
    SECTION("should read chunks") {
        fio.openFileForRead("0", req, rep);
        std::vector<uint32_t> readData(10);

        fio.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fio.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
    SECTION("should allow parallell reads") {
        fio.openFileForRead("0", req, rep);
        std::vector<uint32_t> readData(10);

        fio.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);

        fio.openFileForRead("1", req, rep);

        fio.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);

        fio.readFile("1", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fio.closeReadFile("0");

        fio.readFile("1", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
}

TEST_CASE("Reading from MockFileIO", "[file_handler]") {
    std::vector<uint32_t> arr(100);
    size_t typeSize = sizeof(decltype(arr)::value_type);
    std::iota(arr.begin(), arr.end(), 0);
    MockFileIO fio;
    fio.createMockFile(100 * typeSize);
    fio.createMockFile(100 * typeSize);
    std::vector<char> body;
    Request req(body);  // not used in mock
    Reply rep(1024);
    rep.filePath_ = "testfile.bin";

    SECTION("should open file") {
        REQUIRE(fio.openFileForRead("0", req, rep));
    }
    SECTION("should throw if call open() when already opened for read") {
        REQUIRE(fio.openFileForRead("0", req, rep));
        REQUIRE_THROWS_AS(fio.openFileForRead("0", req, rep), std::runtime_error);
    }
    SECTION("should provide correct size") {
        size_t fileSize = fio.openFileForRead("0", req, rep);
        REQUIRE(fileSize == arr.size() * typeSize);
    }
    SECTION("should throw if call read() before opened") {
        std::vector<uint32_t> readData(10);
        REQUIRE_THROWS_AS(
            fio.readFile("0", req, (char*)readData.data(), readData.size() * typeSize),
            std::runtime_error);
    }
    SECTION("should read chunks") {
        fio.openFileForRead("0", req, rep);
        std::vector<uint32_t> readData(10);

        fio.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fio.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
    SECTION("should support parallel reads") {
        fio.openFileForRead("0", req, rep);
        std::vector<uint32_t> readData(10);

        fio.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);

        fio.openFileForRead("1", req, rep);

        fio.readFile("0", req, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);

        fio.readFile("1", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fio.closeReadFile("0");

        fio.readFile("1", req, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
}

TEST_CASE("Writing to MockFileIO", "[file_handler]") {
    MockFileIO fio;
    std::string err;
    std::vector<char> body;
    Request req(body);  // not used in mock
    Reply rep(1024);
    rep.filePath_ = "testfile.bin";

    SECTION("should open file") {
        REQUIRE(fio.openFileForWrite("0", req, rep, err));
    }
    SECTION("should throw if call open() when already opened for write") {
        REQUIRE(fio.openFileForWrite("0", req, rep, err));
        REQUIRE_THROWS_AS(fio.openFileForWrite("0", req, rep, err), std::runtime_error);
    }
    SECTION("should throw if call write() before opened") {
        std::vector<uint32_t> writeData(10);
        std::string err;
        REQUIRE_THROWS_AS(
            fio.writeFile("0", req, (char*)writeData.data(), writeData.size(), false, err),
            std::runtime_error);
    }
    SECTION("should write chunks") {
        std::string err;
        fio.openFileForWrite("0", req, rep, err);
        std::vector<char> writeData1 = {'a', 'b', 'c', 'd', 'e'};

        fio.writeFile("0", req, writeData1.data(), writeData1.size(), false, err);
        std::vector<char> result = fio.getMockWriteFile("0");
        REQUIRE(result == writeData1);

        std::vector<char> writeData2 = {'f', 'g', 'h'};
        fio.writeFile("0", req, writeData2.data(), writeData2.size(), true, err);
        result = fio.getMockWriteFile("0");
        writeData1.insert(writeData1.end(), writeData2.begin(), writeData2.end());
        REQUIRE(result == writeData1);
    }
    SECTION("should support parallel writes") {
        std::string err;
        fio.openFileForWrite("0", req, rep, err);
        fio.openFileForWrite("1", req, rep, err);

        std::vector<char> writeData1 = {'a', 'b', 'c', 'd', 'e'};
        std::vector<char> writeData2 = {'f', 'g', 'h'};

        fio.writeFile("0", req, writeData1.data(), writeData1.size(), true, err);
        fio.writeFile("1", req, writeData2.data(), writeData2.size(), true, err);

        std::vector<char> result = fio.getMockWriteFile("0");
        REQUIRE(result == writeData1);

        result = fio.getMockWriteFile("1");
        REQUIRE(result == writeData2);
    }
}
