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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>
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
    void cmdDone();
    void cmdStart();
    void updateBar();
    void onSelectFile(QLineEdit *lineEdit);
    void onSelectDir();

private:
    Ui::MainWindow *ui;
    Cmd cmd;
    QProgressBar *bar {};
    QProgressDialog *progress {};
    QSettings settings;
    QTimer timer;
    QElapsedTimer elapsedTimer;

    void applyPatch();
    void centerWindow();
    void checkAllinfo();
    bool checkFile(const QString &fileName);
    void createPatch();
    void setConnections();
    void setPatchName();
    void setProgressDialog();
    static QString findCommonPrefix(const QString &str1, const QString &str2);
};
#endif // MAINWINDOW_H
