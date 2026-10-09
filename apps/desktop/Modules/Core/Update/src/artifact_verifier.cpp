#include "aedst/update/artifact_verifier.hpp"

#include "aedst/update/manifest.hpp"
#include "hex.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace aedst::update {
namespace {

using core::Error;
using core::fail;

constexpr std::size_t kChunkBytes = 64U * 1024U;
constexpr std::size_t kSha256Bytes = 32;

struct MdCtxDeleter {
    void operator()(EVP_MD_CTX* ctx) const noexcept { EVP_MD_CTX_free(ctx); }
};

} // namespace

bool constantTimeEquals(std::span<const std::uint8_t> lhs, std::span<const std::uint8_t> rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    if (lhs.empty()) {
        return true;
    }
    return CRYPTO_memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

core::VoidOutcome ArtifactVerifier::verifyFile(const std::filesystem::path& file, const ExpectedArtifact& expected) {
    if (expected.size == 0 || expected.size > kMaxArtifactBytes) {
        return fail(Error::invalidInput("artifact: expected size is out of range"));
    }
    const auto expectedDigest = detail::decodeHex(expected.sha256);
    if (expected.sha256.size() != 64 || !expectedDigest || expectedDigest->size() != kSha256Bytes) {
        return fail(Error::invalidInput("artifact: expected sha256 must be 64 hex characters"));
    }

    std::error_code ec;
    const std::uintmax_t actualSize = std::filesystem::file_size(file, ec);
    if (ec) {
        return fail(Error::storage("artifact: cannot stat file: " + ec.message()));
    }
    if (actualSize != expected.size) {
        return fail(Error::invalidInput("artifact: size does not match the manifest"));
    }

    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return fail(Error::storage("artifact: cannot open file"));
    }
    const std::unique_ptr<EVP_MD_CTX, MdCtxDeleter> ctx{EVP_MD_CTX_new()};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        return fail(Error::unknown("artifact: cannot initialise SHA-256"));
    }

    std::vector<char> buffer(kChunkBytes);
    std::uint64_t total = 0;
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = static_cast<std::size_t>(in.gcount());
        if (count == 0) {
            break;
        }
        total += count;
        if (total > expected.size) {
            return fail(Error::invalidInput("artifact: file is larger than the manifest allows"));
        }
        if (EVP_DigestUpdate(ctx.get(), buffer.data(), count) != 1) {
            return fail(Error::unknown("artifact: SHA-256 update failed"));
        }
    }
    if (in.bad()) {
        return fail(Error::storage("artifact: read error"));
    }
    if (total != expected.size) {
        return fail(Error::invalidInput("artifact: file is shorter than the manifest says"));
    }

    std::array<std::uint8_t, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLength = 0;
    if (EVP_DigestFinal_ex(ctx.get(), digest.data(), &digestLength) != 1 || digestLength != kSha256Bytes) {
        return fail(Error::unknown("artifact: SHA-256 finalisation failed"));
    }
    if (!constantTimeEquals(std::span<const std::uint8_t>{digest.data(), digestLength}, *expectedDigest)) {
        return fail(Error::unauthenticated("artifact: SHA-256 does not match the manifest"));
    }
    return core::ok();
}

} // namespace aedst::update
