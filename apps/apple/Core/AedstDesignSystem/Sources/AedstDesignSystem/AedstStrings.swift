import Foundation

/// Localized strings the design system itself needs (accessibility labels,
/// values and hints baked into components). Catalog: `Resources/Localizable.xcstrings`
/// (en, ru, ar, tg, uz). Feature copy stays in each feature's own catalog.
enum AedstStrings {
    static func errorPrefix(_ message: String) -> String {
        String(format: String(localized: "dsErrorPrefix", bundle: .module), message)
    }

    static func chartSummary(_ pointCount: Int) -> String {
        String(format: String(localized: "dsChartSummary", bundle: .module), pointCount)
    }

    static func percent(_ value: Int) -> String {
        String(format: String(localized: "dsPercent", bundle: .module), value)
    }

    static func collapses(_ title: String) -> String {
        String(format: String(localized: "dsCollapses", bundle: .module), title)
    }

    static func expands(_ title: String) -> String {
        String(format: String(localized: "dsExpands", bundle: .module), title)
    }

    static var errorTitle: String { String(localized: "dsErrorTitle", bundle: .module) }
    static var ok: String { String(localized: "dsOK", bundle: .module) }
    static var retry: String { String(localized: "dsRetry", bundle: .module) }
    static var loading: String { String(localized: "dsLoading", bundle: .module) }
    static var imageUnavailable: String { String(localized: "dsImageUnavailable", bundle: .module) }
    static var checkbox: String { String(localized: "dsCheckbox", bundle: .module) }
    static var checked: String { String(localized: "dsChecked", bundle: .module) }
    static var unchecked: String { String(localized: "dsUnchecked", bundle: .module) }
    static var close: String { String(localized: "dsClose", bundle: .module) }
    static var oneTimePasscode: String { String(localized: "dsOneTimePasscode", bundle: .module) }
    static var empty: String { String(localized: "dsEmpty", bundle: .module) }
    static var previousPage: String { String(localized: "dsPreviousPage", bundle: .module) }
    static var nextPage: String { String(localized: "dsNextPage", bundle: .module) }
    static var progress: String { String(localized: "dsProgress", bundle: .module) }
    static var select: String { String(localized: "dsSelect", bundle: .module) }
    static var slider: String { String(localized: "dsSlider", bundle: .module) }
    static var switchControl: String { String(localized: "dsSwitch", bundle: .module) }
    static var on: String { String(localized: "dsOn", bundle: .module) }
    static var off: String { String(localized: "dsOff", bundle: .module) }
}

/// Design-system strings other packages may reuse (localized in this package's catalog).
public enum AedstCommonStrings {
    /// "Try again".
    public static var retry: String { AedstStrings.retry }
    /// "OK".
    public static var ok: String { AedstStrings.ok }
    /// "Loading".
    public static var loading: String { AedstStrings.loading }
    /// "Image unavailable".
    public static var imageUnavailable: String { AedstStrings.imageUnavailable }
    /// "Something went wrong".
    public static var errorTitle: String { AedstStrings.errorTitle }
}
