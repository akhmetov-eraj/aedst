#include "key_file.hpp"

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace aedst::persistence::detail {
namespace {

/// Owns one retained CoreFoundation object.
class CfOwned final {
public:
    explicit CfOwned(CFTypeRef ref = nullptr) noexcept : ref_(ref) {}
    CfOwned(const CfOwned&) = delete;
    CfOwned& operator=(const CfOwned&) = delete;
    CfOwned(CfOwned&&) = delete;
    CfOwned& operator=(CfOwned&&) = delete;
    ~CfOwned() {
        if (ref_ != nullptr) {
            CFRelease(ref_);
        }
    }

    [[nodiscard]] CFTypeRef get() const noexcept { return ref_; }

private:
    CFTypeRef ref_;
};

/// Mutable CFDictionary query/attribute set.
class Dictionary final {
public:
    Dictionary()
        : dict_(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks)) {}
    Dictionary(const Dictionary&) = delete;
    Dictionary& operator=(const Dictionary&) = delete;
    Dictionary(Dictionary&&) = delete;
    Dictionary& operator=(Dictionary&&) = delete;
    ~Dictionary() {
        if (dict_ != nullptr) {
            CFRelease(dict_);
        }
    }

    void set(CFStringRef key, CFTypeRef value) { CFDictionarySetValue(dict_, key, value); }
    [[nodiscard]] CFDictionaryRef get() const noexcept { return dict_; }

private:
    CFMutableDictionaryRef dict_;
};

void identify(Dictionary& query) {
    query.set(kSecClass, kSecClassGenericPassword);
    query.set(kSecAttrService, CFSTR("com.aedst.Desktop"));
    query.set(kSecAttrAccount, CFSTR("master-key"));
}

core::Error keychainError(const char* what, OSStatus status) {
    return core::Error::storage(std::string("Keychain ") + what + " failed (OSStatus " + std::to_string(status) + ")");
}

class KeychainBackend final : public KeyringBackend {
public:
    [[nodiscard]] std::string_view name() const noexcept override { return "keychain"; }

    [[nodiscard]] core::Outcome<std::optional<SecretBytes>> lookup() override {
        Dictionary query;
        identify(query);
        query.set(kSecReturnData, kCFBooleanTrue);
        query.set(kSecMatchLimit, kSecMatchLimitOne);

        CFTypeRef result = nullptr;
        const OSStatus status = SecItemCopyMatching(query.get(), &result);
        const CfOwned owned(result);
        if (status == errSecItemNotFound) {
            return std::optional<SecretBytes>{};
        }
        if (status != errSecSuccess || result == nullptr) {
            return core::fail(keychainError("lookup", status));
        }

        const auto data = static_cast<CFDataRef>(result);
        const auto length = static_cast<std::size_t>(CFDataGetLength(data));
        const std::uint8_t* bytes = CFDataGetBytePtr(data);
        std::vector<std::uint8_t> key(bytes, bytes + length);
        return std::optional<SecretBytes>{SecretBytes{std::move(key)}};
    }

    [[nodiscard]] core::VoidOutcome store(std::span<const std::uint8_t> key) override {
        const CfOwned data(CFDataCreate(kCFAllocatorDefault, key.data(), static_cast<CFIndex>(key.size())));
        if (data.get() == nullptr) {
            return core::fail(core::Error::storage("CFDataCreate failed"));
        }

        Dictionary add;
        identify(add);
        add.set(kSecValueData, data.get());
        // Readable after the first unlock following boot; never synced or
        // migrated to another device via backups.
        add.set(kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly);
        OSStatus status = SecItemAdd(add.get(), nullptr);

        if (status == errSecDuplicateItem) {
            Dictionary match;
            identify(match);
            Dictionary changes;
            changes.set(kSecValueData, data.get());
            status = SecItemUpdate(match.get(), changes.get());
        }
        if (status != errSecSuccess) {
            return core::fail(keychainError("store", status));
        }
        return core::ok();
    }
};

} // namespace

std::unique_ptr<KeyringBackend> makeKeychainBackend() {
    return std::make_unique<KeychainBackend>();
}

} // namespace aedst::persistence::detail

#else

namespace aedst::persistence::detail {

std::unique_ptr<KeyringBackend> makeKeychainBackend() {
    return nullptr;
}

} // namespace aedst::persistence::detail

#endif
