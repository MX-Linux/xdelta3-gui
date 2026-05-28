#pragma once

#include <QProcess>

class Cmd : public QProcess
{
    Q_OBJECT
public:
    enum Output { Verbose, Quiet };

    explicit Cmd(QObject *parent = nullptr);
    void runAsync(const QString &program, const QStringList &args, Output output = Verbose);

signals:
    void commandFinished(bool success, const QString &output);

private:
    QString out_buffer;
};
