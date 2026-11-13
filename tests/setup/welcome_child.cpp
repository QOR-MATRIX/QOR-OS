// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include <QCoreApplication>
#include <QFile>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QFile file(qEnvironmentVariable("QOR_SETUP_TEST_PROBE"));
    if (!file.open(QIODevice::Append))
        return 2;
    const QByteArray data = app.arguments().mid(1).join(u' ').toUtf8() + '\n' + qgetenv("QOR_REDUCED_MOTION") + '\n'
                            + qgetenv("QOR_INCREASE_CONTRAST") + '\n';
    return file.write(data) == data.size() ? 0 : 3;
}
