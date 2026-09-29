import Testing
@testable import AedstNavigation

private enum TestRoute: AedstRoute {
    case a, b
}

@Suite("Router")
@MainActor
struct RouterTests {
    @Test("push increases path count")
    func push() {
        let router = Router()
        router.push(TestRoute.a)
        router.push(TestRoute.b)
        #expect(router.path.count == 2)
    }

    @Test("popToRoot empties the path")
    func popToRoot() {
        let router = Router()
        router.push(TestRoute.a)
        router.push(TestRoute.b)
        router.popToRoot()
        #expect(router.path.isEmpty)
    }

    @Test("pop on empty path is a no-op")
    func popOnEmpty() {
        let router = Router()
        router.pop()
        #expect(router.path.isEmpty)
    }
}
