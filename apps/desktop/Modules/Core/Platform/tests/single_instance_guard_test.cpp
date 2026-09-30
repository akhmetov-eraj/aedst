#include "aedst/platform/single_instance_guard.hpp"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

namespace aedst::platform {

class SingleInstanceGuardTest : public QObject {
    Q_OBJECT

private slots:
    void sanitizeArgumentsDropsUnsafeEntries();
    void firstIsPrimarySecondIsForwarded();
    void lockIsReleasedWhenPrimaryIsDestroyed();
};

void SingleInstanceGuardTest::sanitizeArgumentsDropsUnsafeEntries() {
    QStringList input;
    input << QStringLiteral("aedst://tasks") << QString{} << QStringLiteral("bad\nline")
          << QString(3000, QLatin1Char('x'));
    for (int i = 0; i < 40; ++i) {
        input << QStringLiteral("arg%1").arg(i);
    }
    const QStringList out = SingleInstanceGuard::sanitizeArguments(input);
    QCOMPARE(out.size(), 16);
    QCOMPARE(out.first(), QStringLiteral("aedst://tasks"));
    QCOMPARE(out.at(1), QStringLiteral("arg0"));
}

void SingleInstanceGuardTest::firstIsPrimarySecondIsForwarded() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    SingleInstanceGuard primary{QStringLiteral("com.aedst.test"), dir.path()};
    QCOMPARE(primary.acquire({}), SingleInstanceGuard::Role::Primary);
    QSignalSpy spy(&primary, &SingleInstanceGuard::activationRequested);

    // Same process, separate guard: the lock is held, so it must act as a
    // secondary and deliver its arguments over the local socket.
    SingleInstanceGuard secondary{QStringLiteral("com.aedst.test"), dir.path()};
    QCOMPARE(secondary.acquire({QStringLiteral("aedst://tasks/42"), QStringLiteral("--flag")}),
             SingleInstanceGuard::Role::Secondary);

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);
    const QStringList received = spy.first().first().toStringList();
    QCOMPARE(received, (QStringList{QStringLiteral("aedst://tasks/42"), QStringLiteral("--flag")}));
}

void SingleInstanceGuardTest::lockIsReleasedWhenPrimaryIsDestroyed() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    {
        SingleInstanceGuard first{QStringLiteral("com.aedst.test"), dir.path()};
        QCOMPARE(first.acquire({}), SingleInstanceGuard::Role::Primary);
    }
    SingleInstanceGuard next{QStringLiteral("com.aedst.test"), dir.path()};
    QCOMPARE(next.acquire({}), SingleInstanceGuard::Role::Primary);
}

} // namespace aedst::platform

QTEST_GUILESS_MAIN(aedst::platform::SingleInstanceGuardTest)
#include "single_instance_guard_test.moc"
