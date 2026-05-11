#pragma once

#include <QProcess>

class QTextStream;

class Cmd : public QProcess
{
    Q_OBJECT
public:
    enum Output { Verbose, Quiet };

    explicit Cmd(QObject *parent = nullptr);
    void runAsync(const QString &program, const QStringList &args, Output output = Verbose);
    void runAsync(const QString &cmd, Output output = Verbose);
    static bool run(const QString &cmd, QString *out, Output output = Verbose);
    [[nodiscard]] QString getOutput() const { return out_buffer.trimmed(); }

signals:
    void commandFinished(bool success, const QString &output);
    void errorAvailable(const QString &err);
    void outputAvailable(const QString &out);

private:
    QString out_buffer;
};
