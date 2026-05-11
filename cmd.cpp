#include "cmd.h"

#include <QDebug>

Cmd::Cmd(QObject *parent)
    : QProcess(parent)
{
    connect(this, &Cmd::readyReadStandardOutput, [this] {
        QString out = readAllStandardOutput();
        out_buffer += out;
        emit outputAvailable(out);
    });
    connect(this, &Cmd::readyReadStandardError, [this] {
        QString err = readAllStandardError();
        out_buffer += err;
        emit errorAvailable(err);
    });
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this](int exitCode, QProcess::ExitStatus exitStatus) {
        bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
        emit commandFinished(success, out_buffer.trimmed());
    });
}

void Cmd::runAsync(const QString &program, const QStringList &args, Output output)
{
    out_buffer.clear();
    if (state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << this->program() << arguments();
        return;
    }
    if (output == Verbose) {
        qDebug().noquote() << program << args;
    }
    start(program, args);
}

void Cmd::runAsync(const QString &cmd, Output output)
{
    out_buffer.clear();
    if (state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << program() << arguments();
        return;
    }
    if (output == Verbose) {
        qDebug().noquote() << cmd;
    }
    start("/bin/bash", {"-c", cmd});
}

bool Cmd::run(const QString &cmd, QString *out, Output output)
{
    QProcess p;
    if (output == Verbose) {
        qDebug().noquote() << cmd;
    }
    p.start("/bin/bash", {"-c", cmd});
    if (!p.waitForFinished(5000)) {
        return false;
    }
    *out = p.readAllStandardOutput().trimmed();
    return (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);
}
