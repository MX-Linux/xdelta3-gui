#pragma once

#include <QProcess>

class QTextStream;

class Cmd : public QProcess
{
    Q_OBJECT
public:
    explicit Cmd(QObject *parent = nullptr);
    bool run(const QString &cmd, bool quiet = false);
    bool run(const QString &cmd, QString *output, bool quiet = false);
    [[nodiscard]] QString getCmdOut(const QString &cmd, bool quiet = false);

signals:
    void finished();
    void errorAvailable(const QString &err);
    void outputAvailable(const QString &out);

private:
    QString out_buffer;
};

#endif // CMD_H
