/**********************************************************************
 *
 **********************************************************************
 * Copyright (C) 2023 MX Authors
 *
 * Authors: Adrian <adrian@mxlinux.org>
 *          MX Linux <http://mxlinux.org>
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package. If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/
#pragma once

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QTimer>

#include "cmd.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(const QString &patchFile = "", QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void cmdFinished(bool success, const QString &output);
    void cmdStart();
    void updateBar();
    void updateTaskbar(int percent, bool visible);
    void onSelectFile(QLineEdit *lineEdit, const QString &filter);
    void onSelectDir();

private:
    enum class Operation { None, CreatePatch, ApplyPatch };
    Operation currentOp = Operation::None;
    quint64 operationId = 0; // bumped per run, identifies a specific operation
    bool cancelled = false;
    QString outputFinalPath; // where the result should ultimately live
    QString outputTempPath;  // file xdelta3 actually writes to (promoted on success)

    Ui::MainWindow *ui;
    Cmd cmd;
    QSettings settings {QApplication::applicationName()};
    QTimer timer;
    QElapsedTimer elapsedTimer;
    qint64 etaMs = -1;
    int etaTick = 0;
    QString etaSizeStr;
    qint64 targetFileSize = -1;

    void applyPatch();
    void centerWindow();
    void checkAllinfo();
    bool checkFile(const QString &fileName);
    void createPatch();
    void handleProgressOutput();
    void setConnections();
    void setPatchName();
    void setOutputName();
    QString dirSettingsKey(QLineEdit *lineEdit) const;
    static QString makeTempPath(const QString &finalPath);
    static QString formatElapsedTime(qint64 ms);
    static QString formatFileSize(qint64 bytes);

    QProcess *progressProcess {};
    int lastProg = -1;
    QString lastStatsLine;
};
