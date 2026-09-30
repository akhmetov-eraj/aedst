#include "aedst/update/url_validation.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace aedst::update {
namespace {

TEST(UrlValidationTest, AcceptsPlainHttpsUrls) {
    const std::vector<std::string> valid{
        "https://aedst.com",
        "https://aedst.com/",
        "https://downloads.aedst.com/v1/aedst-setup.exe",
        "HTTPS://Example.COM/path?q=1&r=%20x#frag",
        "https://example.com:8443/x",
        "https://a-b.example.co.uk/x",
        "https://127.0.0.1/x",
    };
    for (const std::string& url : valid) {
        EXPECT_TRUE(isSecureHttpsUrl(url)) << url;
    }
}

TEST(UrlValidationTest, RejectsEverythingElse) {
    const std::vector<std::string> invalid{
        "",
        "https://",
        "http://aedst.com",
        "ftp://aedst.com",
        "//aedst.com",
        "aedst.com",
        "javascript:alert(1)",
        "file:///etc/passwd",
        "https://user@aedst.com",
        "https://user:pass@aedst.com/x",
        "https://aedst.com@evil.example/x",
        "https://[::1]/x",
        "https://exam ple.com",
        "https://aedst.com/a b",
        "https://aedst.com\\evil",
        "https://aedst.com/\"",
        "https://-bad.example.com",
        "https://bad-.example.com",
        "https://a..b.com",
        "https://.example.com",
        "https://example.com.",
        "https://example.com:0/",
        "https://example.com:65536/",
        "https://example.com:port/",
        "https://example.com:/",
        "https://exa_mple.com",
        "https://\xC3\xA9xample.com",
        std::string{"https://example.com/\x01"},
        std::string{"https://example.com/\n"},
        "https://example.com/" + std::string(2100, 'a'),
    };
    for (const std::string& url : invalid) {
        EXPECT_FALSE(isSecureHttpsUrl(url)) << url;
    }
}

} // namespace
} // namespace aedst::update
