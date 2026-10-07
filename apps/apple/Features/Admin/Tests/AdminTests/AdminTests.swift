import Testing
@testable import Admin

@Suite("Admin")
struct AdminTests {
    @Test("broadcast validation mirrors the proto rules")
    func broadcastValidation() {
        #expect(Broadcast(title: "Maintenance tonight").isValid)
        #expect(!Broadcast(title: "  ").isValid)
        #expect(!Broadcast(title: String(repeating: "x", count: 121)).isValid)
        #expect(Broadcast(title: "Hi", link: "/tasks").isValid)
        #expect(!Broadcast(title: "Hi", link: "https://evil.example").isValid)
        #expect(!Broadcast(title: "Hi", link: "//evil.example").isValid)
    }

    @Test("status 3 is TASK_STATUS_DONE")
    func taskStatus() {
        #expect(AdminTask(id: "1", ownerID: "u", title: "t", status: 3, createdAt: nil).isDone)
    }
}
