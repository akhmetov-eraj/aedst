#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

// Qt-free on purpose: the language-selection rules are pure functions so they
// can be tested without QLocale/QTranslator. TranslationLoader (Qt) applies them.
namespace aedst::designsystem {

/// Languages every catalog (`*_en.ts`, `*_ru.ts`, `*_ar.ts`, `*_tg.ts`, `*_uz.ts`) ships.
inline const std::vector<std::string>& supportedLanguages() {
    static const std::vector<std::string> kLanguages{"en", "ru", "ar", "tg", "uz"};
    return kLanguages;
}

inline constexpr std::string_view kFallbackLanguage = "en";

/// "ru-RU", "uz_Latn_UZ", "AR" -> "ru", "uz", "ar" (lowercase primary language subtag).
[[nodiscard]] inline std::string primaryLanguage(std::string_view tag) {
    const auto end = tag.find_first_of("-_.@");
    const std::string_view primary = tag.substr(0, end);
    std::string out;
    out.reserve(primary.size());
    for (const char c : primary) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

/// First UI language (in the user's preference order) whose primary subtag is in
/// `available`; `kFallbackLanguage` when none matches or the list is empty.
[[nodiscard]] inline std::string resolveLanguage(const std::vector<std::string>& uiLanguages,
                                                 const std::vector<std::string>& available) {
    for (const auto& tag : uiLanguages) {
        const std::string primary = primaryLanguage(tag);
        if (!primary.empty() && std::find(available.begin(), available.end(), primary) != available.end()) {
            return primary;
        }
    }
    return std::string(kFallbackLanguage);
}

/// Scripts written right-to-left among the supported (and likely future) languages.
[[nodiscard]] inline bool isRightToLeftLanguage(std::string_view tag) {
    static constexpr std::array<std::string_view, 5> kRtl{"ar", "he", "fa", "ur", "ps"};
    const std::string primary = primaryLanguage(tag);
    return std::find(kRtl.begin(), kRtl.end(), primary) != kRtl.end();
}

} // namespace aedst::designsystem
