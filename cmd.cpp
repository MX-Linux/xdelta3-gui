#include "cmd.h"

#include <QDebug>

Cmd::Cmd(QObject *parent)
    : QProcess(parent)
{
    connect(this, &Cmd::readyReadStandardOutput, [this] { out_buffer += readAllStandardOutput(); });
    connect(this, &Cmd::readyReadStandardError, [this] { out_buffer += readAllStandardError(); });
    connect(this, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this](int exitCode, QProcess::ExitStatus exitStatus) {
        bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
        emit commandFinished(success, out_buffer.trimmed());
    });
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
