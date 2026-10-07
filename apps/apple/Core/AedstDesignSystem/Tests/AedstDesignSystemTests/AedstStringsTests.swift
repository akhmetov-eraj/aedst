import Testing
@testable import AedstDesignSystem

@Suite("AedstStrings")
struct AedstStringsTests {
    @Test("every accessibility string resolves from the catalog (not the raw key)")
    func resolves() {
        let all = [
            AedstStrings.checkbox, AedstStrings.checked, AedstStrings.unchecked, AedstStrings.close,
            AedstStrings.oneTimePasscode, AedstStrings.empty, AedstStrings.previousPage, AedstStrings.nextPage,
            AedstStrings.progress, AedstStrings.select, AedstStrings.slider, AedstStrings.switchControl,
            AedstStrings.on, AedstStrings.off,
        ]
        for text in all {
            #expect(!text.isEmpty)
            #expect(!text.hasPrefix("ds"), "unresolved localization key: \(text)")
        }
    }

    @Test("formatted strings interpolate their arguments")
    func formats() {
        #expect(AedstStrings.chartSummary(7).contains("7"))
        #expect(AedstStrings.percent(42).contains("42"))
        #expect(AedstStrings.errorPrefix("Required").contains("Required"))
        #expect(AedstStrings.expands("Billing").contains("Billing"))
        #expect(AedstStrings.collapses("Billing").contains("Billing"))
    }
}
