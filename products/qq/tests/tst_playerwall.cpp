// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// The Player's kernel wall from the hostile side: what a game that got native code past QQ's own checks would try. A copy
// of this program asks for a socket by every route there is, raises the wall as the Player does, and asks again. It
// answers with errno, 0 for a socket made. The wall refuses with EACCES, which a kernel lacking a route does not: that
// one answers ENOSYS, so a pass here means the wall stopped it on any machine.

#include "sandbox.h"

#include <QCoreApplication>
#include <QHash>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <cerrno>
#include <cstdio>
#include <linux/io_uring.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

int answer(long result, int error)
{
    if (result < 0)
        return error;
    ::close(int(result));
    return 0;
}

int makeSocket(int family)
{
    const long made = ::socket(family, SOCK_STREAM, 0);
    return answer(made, errno);
}

int makeRing()
{
    io_uring_params params{};
    const long made = ::syscall(SYS_io_uring_setup, 1, &params);
    return answer(made, errno);
}

#if defined(__x86_64__)
int makeSocketBy32BitCall()
{
    long made = 359;  // socket, in the i386 table.
    asm volatile("int $0x80"
                 : "+a"(made)
                 : "b"(AF_INET), "c"(SOCK_STREAM), "d"(0)
                 : "r8", "r9", "r10", "r11", "cc", "memory");
    return made < 0 ? answer(-1, int(-made)) : answer(made, 0);
}

int makeSocketByX32Call()
{
    const long made = ::syscall(0x40000000L + SYS_socket, AF_INET, SOCK_STREAM, 0);
    return answer(made, errno);
}
#endif

void askEveryRoute(const char *when)
{
    std::printf("%s inet %d\n", when, makeSocket(AF_INET));
    std::printf("%s inet6 %d\n", when, makeSocket(AF_INET6));
    // How a process in WSL reaches Windows' file server (tools/ci/isolated-linux-runner.sh).
    std::printf("%s vsock %d\n", when, makeSocket(AF_VSOCK));
    std::printf("%s local %d\n", when, makeSocket(AF_UNIX));
    std::printf("%s ring %d\n", when, makeRing());
#if defined(__x86_64__)
    std::printf("%s call32 %d\n", when, makeSocketBy32BitCall());
    std::printf("%s callx32 %d\n", when, makeSocketByX32Call());
#endif
}

int behindTheWall(const char *package)
{
    askEveryRoute("before");
    const QString standing = qq::raisePlayerWall({QString::fromLocal8Bit(package), {}, {}});
    std::printf("standing %s\n", qPrintable(standing));
    askEveryRoute("after");
    return 0;
}

}  // namespace

class PlayerWallTest : public QObject
{
    Q_OBJECT

private slots:
    void noRouteToTheNetwork();
};

void PlayerWallTest::noRouteToTheNetwork()
{
    QTemporaryDir package;
    QVERIFY(package.isValid());

    QProcess copy;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("QQ_PLAYER_SANDBOX"));
    copy.setProcessEnvironment(environment);
    copy.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--behind-the-wall"), package.path()});
    QVERIFY(copy.waitForFinished(30000));
    const QByteArray output = copy.readAllStandardOutput();
    QVERIFY2(copy.exitStatus() == QProcess::NormalExit && copy.exitCode() == 0, output.constData());

    QHash<QByteArray, int> answers;
    QByteArray standing;
    for (const QByteArray &line : output.split('\n')) {
        const QList<QByteArray> words = line.trimmed().split(' ');
        if (words.size() == 3)
            answers.insert(words[0] + ' ' + words[1], words[2].toInt());
        else if (words.size() == 2 && words[0] == "standing")
            standing = words[1];
    }
    QVERIFY2(standing.contains("seccomp"), output.constData());

    // Without these the rest would pass on a process that could make nothing at all.
    QCOMPARE(answers.value("before inet", -1), 0);
    QCOMPARE(answers.value("before local", -1), 0);

    QCOMPARE(answers.value("after inet", -1), EACCES);
    QCOMPARE(answers.value("after inet6", -1), EACCES);
    QCOMPARE(answers.value("after vsock", -1), EACCES);
    QCOMPARE(answers.value("after ring", -1), EACCES);
#if defined(__x86_64__)
    QCOMPARE(answers.value("after call32", -1), EACCES);
    QCOMPARE(answers.value("after callx32", -1), EACCES);
#endif
    // What the display and sound need stays.
    QCOMPARE(answers.value("after local", -1), 0);

    qInfo("Open on this machine before the wall: ring %s, 32-bit call %s, x32 call %s.",
          answers.value("before ring", -1) == 0 ? "yes" : "no",
          answers.value("before call32", -1) == 0 ? "yes" : "no",
          answers.value("before callx32", -1) == 0 ? "yes" : "no");
}

int main(int argc, char *argv[])
{
    if (argc == 3 && qstrcmp(argv[1], "--behind-the-wall") == 0)
        return behindTheWall(argv[2]);
    QCoreApplication app(argc, argv);
    PlayerWallTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_playerwall.moc"
