#pragma once

#include <QProcess>

class Cmd : public QProcess
{
    Q_OBJECT
public:
    enum Output { Verbose, Quiet };

    explicit Cmd(QObject *parent = nullptr);
    void runAsync(const QString &program, const QStringList &args, Output output = Verbose);

    // Environment for xdelta3 children. XDELTA is stripped because xdelta3
    // reads extra options from it (e.g. -c would redirect output to stdout).
    static QProcessEnvironment xdeltaEnvironment();

signals:
    void commandFinished(bool success, const QString &output);

private:
    QString out_buffer;
};
