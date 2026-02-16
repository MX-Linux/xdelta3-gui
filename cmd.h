#pragma once

#include <QProcess>

class QTextStream;

class Cmd : public QProcess
{
    Q_OBJECT
public:
    enum Output { Verbose, Quiet };

    explicit Cmd(QObject *parent = nullptr);
    bool run(const QString &cmd, Output output = Verbose);
    bool run(const QString &cmd, QString *out, Output output = Verbose);
    bool run(const QString &program, const QStringList &args, QString *out, Output output = Verbose);
    [[nodiscard]] QString getCmdOut(const QString &cmd, Output output = Verbose);

signals:
    void done();
    void errorAvailable(const QString &err);
    void outputAvailable(const QString &out);

private:
    QString out_buffer;
};
