#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>
#include <unistd.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // The shell's executable engine has already closed its result pipes.
    QThread::msleep(250);
    const QByteArray out("AERO7_DETACHED_FIXTURE_STDOUT\n");
    const QByteArray err("AERO7_DETACHED_FIXTURE_STDERR\n");
    if (write(STDOUT_FILENO, out.constData(), out.size()) != out.size()
        || write(STDERR_FILENO, err.constData(), err.size()) != err.size())
        return 4;
    QFile marker(qEnvironmentVariable("AERO7_QA_LAUNCH_MARKER"));
    if (!marker.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        return 5;
    return marker.write(QJsonDocument(QJsonArray::fromStringList(app.arguments().mid(1))).toJson()) > 0 ? 0 : 6;
}
