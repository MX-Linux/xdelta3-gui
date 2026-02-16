#include "cmd.h"

#include <QDebug>
#include <QEventLoop>

Cmd::Cmd(QObject *parent)
    : QProcess(parent)
{
    connect(this, &Cmd::readyReadStandardOutput, [this] { emit outputAvailable(readAllStandardOutput()); });
    connect(this, &Cmd::readyReadStandardError, [this] { emit errorAvailable(readAllStandardError()); });
    connect(this, &Cmd::outputAvailable, [this](const QString &out) { out_buffer += out; });
    connect(this, &Cmd::errorAvailable, [this](const QString &out) { out_buffer += out; });
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Cmd::done);
}

bool Cmd::run(const QString &cmd, Output output)
{
    QString out;
    return run(cmd, &out, output);
}

QString Cmd::getCmdOut(const QString &cmd, Output output)
{
    QString out;
    run(cmd, &out, output);
    return out;
}

bool Cmd::run(const QString &cmd, QString *out, Output output)
{
    out_buffer.clear();
    if (state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << program() << arguments();
        return false;
    }
    if (output == Verbose) {
        qDebug().noquote() << cmd;
    }
    QEventLoop loop;
    connect(this, &Cmd::done, &loop, &QEventLoop::quit);
    start("/bin/bash", {"-c", cmd});
    loop.exec();
    *out = out_buffer.trimmed();
    return (exitStatus() == QProcess::NormalExit && exitCode() == 0);
}

bool Cmd::run(const QString &program, const QStringList &args, QString *out, Output output)
{
    out_buffer.clear();
    if (state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << this->program() << arguments();
        return false;
    }
    if (output == Verbose) {
        qDebug().noquote() << program << args;
    }
    QEventLoop loop;
    connect(this, &Cmd::done, &loop, &QEventLoop::quit);
    start(program, args);
    loop.exec();
    *out = out_buffer.trimmed();
    return (exitStatus() == QProcess::NormalExit && exitCode() == 0);
}
