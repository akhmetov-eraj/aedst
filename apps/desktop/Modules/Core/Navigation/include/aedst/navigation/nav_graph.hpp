#pragma once

#include "aedst/core/logger.hpp"

#include <QUrl>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace aedst::navigation {

/// Route name -> QML view. Each feature's `register<Feature>Destinations()`
/// adds its entries; only App/ calls all of them, so only App/ sees the full graph.
class NavGraph final {
public:
    void registerDestination(std::string_view routeName, QUrl view) {
        auto [it, inserted] = destinations_.insert_or_assign(std::string(routeName), std::move(view));
        if (!inserted) {
            core::AedstLogger::warn("NavGraph: destination for '{}' registered twice; last one wins", it->first);
        }
    }

    [[nodiscard]] std::optional<QUrl> resolve(std::string_view routeName) const {
        if (const auto it = destinations_.find(routeName); it != destinations_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    [[nodiscard]] bool contains(std::string_view routeName) const { return destinations_.contains(routeName); }

private:
    std::map<std::string, QUrl, std::less<>> destinations_;
};

} // namespace aedst::navigation
