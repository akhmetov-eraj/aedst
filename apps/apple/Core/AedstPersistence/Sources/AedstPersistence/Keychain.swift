import AedstCore
import Foundation
import Security

/// Minimal generic-password Keychain wrapper — the one place `SecItem*`
/// calls happen. Everything else (tokens, biometric-gated secrets) goes
/// through `KeychainTokenStore` rather than calling this directly.
public struct Keychain: Sendable {
    private let service: String
    private let logger = AedstLogger(category: "keychain")

    public init(service: String = "com.aedst.apple") {
        self.service = service
    }

    /// Writes `value` for `key`. A failed write (locked device, unavailable
    /// Keychain) is logged rather than thrown — callers (`KeychainTokenStore`)
    /// treat a token as absent either way, but the failure should be visible
    /// in diagnostics instead of silently vanishing.
    public func set(_ value: String, for key: String) {
        let data = Data(value.utf8)
        var query = baseQuery(for: key)
        query[kSecValueData as String] = data

        SecItemDelete(query as CFDictionary)
        let status = SecItemAdd(query as CFDictionary, nil)
        if status != errSecSuccess {
            logger.error("Keychain write failed for key '\(key)' (OSStatus \(status))")
        }
    }

    public func get(_ key: String) -> String? {
        var query = baseQuery(for: key)
        query[kSecReturnData as String] = true
        query[kSecMatchLimit as String] = kSecMatchLimitOne

        var result: AnyObject?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        guard status == errSecSuccess, let data = result as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }

    public func remove(_ key: String) {
        SecItemDelete(baseQuery(for: key) as CFDictionary)
    }

    private func baseQuery(for key: String) -> [String: Any] {
        [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: key,
            kSecAttrAccessible as String: kSecAttrAccessibleAfterFirstUnlock,
        ]
    }
}
