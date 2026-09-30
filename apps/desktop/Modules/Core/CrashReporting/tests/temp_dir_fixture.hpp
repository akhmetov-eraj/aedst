#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

namespace aedst::crash::test_support {

/// Fresh, empty directory per test, removed afterwards.
class TempDirTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::error_code ec;
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        root_ = std::filesystem::temp_directory_path(ec) /
                (std::string{"aedst_crash_"} + info->test_suite_name() + "_" + info->name());
        std::filesystem::remove_all(root_, ec);
        std::filesystem::create_directories(root_, ec);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    static void writeFile(const std::filesystem::path& path, std::string_view content) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    static std::string readFile(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }

    std::filesystem::path root_;
};

} // namespace aedst::crash::test_support
