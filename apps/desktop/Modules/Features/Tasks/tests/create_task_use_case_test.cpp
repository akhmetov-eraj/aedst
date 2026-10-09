#include "Data/connect_task_repository.hpp"
#include "Domain/create_task_use_case.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace aedst::feature::tasks {
namespace {

using ::testing::_;

class MockTaskRepository final : public TaskRepository {
public:
    MOCK_METHOD(void, list, (const TaskQuery&, core::Completion<TaskPage>), (override));
    MOCK_METHOD(void, get, (const std::string&, core::Completion<Task>), (override));
    MOCK_METHOD(void, create, (const NewTask&, core::Completion<Task>), (override));
    MOCK_METHOD(void, complete, (const std::string&, core::Completion<Task>), (override));
    MOCK_METHOD(void, reopen, (const std::string&, core::Completion<Task>), (override));
    MOCK_METHOD(void, remove, (const std::string&, core::Completion<void>), (override));
};

TEST(CreateTaskUseCaseTest, RejectsBlankTitlesWithoutCallingTheServer) {
    auto repository = std::make_shared<MockTaskRepository>();
    EXPECT_CALL(*repository, create(_, _)).Times(0);
    CreateTaskUseCase useCase(repository);

    std::optional<core::Outcome<Task>> received;
    useCase.execute(NewTask{.title = "   \t"}, [&](core::Outcome<Task> result) { received = std::move(result); });
    ASSERT_TRUE(received.has_value());
    ASSERT_FALSE(received->has_value());
    EXPECT_EQ(received->error().code, core::ErrorCode::InvalidInput);
}

TEST(CreateTaskUseCaseTest, TrimsAndDefaultsPriorityBeforeCreating) {
    auto repository = std::make_shared<MockTaskRepository>();
    EXPECT_CALL(*repository, create(_, _)).WillOnce([](const NewTask& task, core::Completion<Task> done) {
        EXPECT_EQ(task.title, "Write report");
        EXPECT_EQ(task.priority, TaskPriority::Medium);
        done(Task{.id = "t1", .title = task.title});
    });
    CreateTaskUseCase useCase(repository);
    std::optional<core::Outcome<Task>> received;
    useCase.execute(NewTask{.title = "  Write report  ", .priority = TaskPriority::Unspecified},
                    [&](core::Outcome<Task> result) { received = std::move(result); });
    ASSERT_TRUE(received && received->has_value());
    EXPECT_EQ((*received)->id, "t1");
}

TEST(CreateTaskUseCaseTest, CountsCodePointsNotBytes) {
    NewTask ascii{.title = std::string(200, 'a')};
    EXPECT_FALSE(CreateTaskUseCase::normalizeAndValidate(ascii).has_value());
    // 200 two-byte characters (400 bytes) are still 200 characters.
    std::string twoByte;
    for (int i = 0; i < 200; ++i) {
        twoByte += "\xD0\x96"; // U+0416
    }
    NewTask wide{.title = twoByte};
    EXPECT_FALSE(CreateTaskUseCase::normalizeAndValidate(wide).has_value());
    NewTask tooLong{.title = std::string(201, 'a')};
    EXPECT_TRUE(CreateTaskUseCase::normalizeAndValidate(tooLong).has_value());
}

TEST(TaskTest, OverdueOnlyWhenOpenAndPastDue) {
    const auto now = std::chrono::system_clock::now();
    Task task{.status = TaskStatus::Todo, .dueDate = now - std::chrono::hours{1}};
    EXPECT_TRUE(task.isOverdue(now));
    task.status = TaskStatus::Done;
    EXPECT_FALSE(task.isOverdue(now));
    task.status = TaskStatus::Todo;
    task.dueDate.reset();
    EXPECT_FALSE(task.isOverdue(now));
}

TEST(ConnectTaskRepositoryTest, MapsProtoTaskToDomain) {
    tasksv1::Task proto;
    proto.set_id("t1");
    proto.set_title("Title");
    proto.set_status(tasksv1::TASK_STATUS_DONE);
    proto.set_priority(tasksv1::TASK_PRIORITY_URGENT);
    proto.mutable_due_date()->set_seconds(1'700'000'000);
    proto.set_version(3);

    const Task task = ConnectTaskRepository::toDomain(proto);
    EXPECT_EQ(task.id, "t1");
    EXPECT_EQ(task.status, TaskStatus::Done);
    EXPECT_EQ(task.priority, TaskPriority::Urgent);
    ASSERT_TRUE(task.dueDate.has_value());
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(task.dueDate->time_since_epoch()).count(),
              1'700'000'000);
    EXPECT_FALSE(task.completedAt.has_value());
    EXPECT_EQ(task.version, 3);
}

} // namespace
} // namespace aedst::feature::tasks
