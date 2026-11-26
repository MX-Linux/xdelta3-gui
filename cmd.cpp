#include "cmd.h"

#include <QDebug>

Cmd::Cmd(QObject *parent)
    : QProcess(parent)
{
    setProcessEnvironment(xdeltaEnvironment());
    connect(this, &Cmd::readyReadStandardOutput, [this] { out_buffer += readAllStandardOutput(); });
    connect(this, &Cmd::readyReadStandardError, [this] { out_buffer += readAllStandardError(); });
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this](int exitCode, QProcess::ExitStatus exitStatus) {
        bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
        emit commandFinished(success, out_buffer.trimmed());
    });
    // A program that cannot be started never emits finished(), so report the
    // failure here; otherwise the caller's cleanup and UI reset never run.
    // Queued so the caller has returned from runAsync() before it is handled.
    connect(
        this, &QProcess::errorOccurred, this,
        [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                emit commandFinished(false, errorString());
            }
        },
        Qt::QueuedConnection);
}

QProcessEnvironment Cmd::xdeltaEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("XDELTA");
    return env;
}

void Cmd::runAsync(const QString &program, const QStringList &args, Output output)
{
    if (state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << this->program() << arguments();
        return;
    }
    out_buffer.clear();
    if (output == Verbose) {
        qDebug().noquote() << program << args;
    }
    start(program, args);
}
