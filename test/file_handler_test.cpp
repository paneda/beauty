#include "file_handler.hpp"

#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <numeric>

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
    FileHandler fh;

    SECTION("should open file idempotent") {
        REQUIRE(fh.openFile(0, "testfile.bin"));
        REQUIRE(fh.openFile(0, "testfile.bin"));
        REQUIRE(fh.openFile(0, "testfile.bin"));
    }
    SECTION("should close file idempotent") {
        fh.openFile(0, "testfile.bin");
        fh.closeFile(0);
        fh.closeFile(0);
        fh.closeFile(0);
    }
    SECTION("should provide correct size") {
        fh.openFile(0, "testfile.bin");
        REQUIRE(fh.getFileSize(0) == arr.size() * typeSize);
    }
    SECTION("should read chunks") {
        fh.openFile(0, "testfile.bin");
        std::vector<uint32_t> readData(10);

        fh.readFile(0, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fh.readFile(0, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
    SECTION("should allow parallell reads") {
        fh.openFile(0, "testfile.bin");
        std::vector<uint32_t> readData(10);

        fh.readFile(0, (char*)readData.data(), readData.size() * typeSize);

        fh.openFile(1, "testfile.bin");

        fh.readFile(0, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);

        fh.readFile(1, (char*)readData.data(), readData.size() * typeSize);
        expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fh.closeFile(0);

        fh.readFile(1, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
}

TEST_CASE("mock_file_handler.cpp", "[file_handler]") {
    std::vector<uint32_t> arr(100);
    size_t typeSize = sizeof(decltype(arr)::value_type);
    std::iota(arr.begin(), arr.end(), 0);
    MockFileHandler fh;
    fh.createMockFile(100 * typeSize);
    fh.createMockFile(100 * typeSize);

    SECTION("should open file") {
        // FileHandler fh;
        REQUIRE(fh.openFile(0, "testfile.bin"));
    }
    SECTION("should provide correct size") {
        // FileHandler fh;
        fh.openFile(0, "testfile.bin");
        REQUIRE(fh.getFileSize(0) == arr.size() * typeSize);
    }
    SECTION("should read chunks") {
        // FileHandler fh;
        fh.openFile(0, "testfile.bin");
        std::vector<uint32_t> readData(10);

        fh.readFile(0, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fh.readFile(0, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
    SECTION("should allow parallell reads") {
        fh.openFile(0, "testfile.bin");
        std::vector<uint32_t> readData(10);

        fh.readFile(0, (char*)readData.data(), readData.size() * typeSize);

        fh.openFile(1, "testfile.bin");

        fh.readFile(0, (char*)readData.data(), readData.size() * typeSize);
        std::vector<uint32_t> expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);

        fh.readFile(1, (char*)readData.data(), readData.size() * typeSize);
        expected = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        REQUIRE(readData == expected);

        fh.closeFile(0);

        fh.readFile(1, (char*)readData.data(), readData.size() * typeSize);
        expected = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
        REQUIRE(readData == expected);
    }
}
