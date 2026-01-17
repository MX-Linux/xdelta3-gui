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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "droplineedit.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QLocale>
#include <QMessageBox>
#include <QScreen>
#include <QTime>

#include <chrono>

using namespace std::chrono_literals;

MainWindow::MainWindow(const QString &patchFile, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setConnections();
    adjustSize();
    const QSize size = this->size();
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
        if (isMaximized()) { // add option to resize if maximized
            resize(size);
            centerWindow();
        }
    }
    ui->tabWidget->setCurrentWidget(ui->tabCreatePatch);
    ui->spinCompressionLevel->setValue(settings.value("compression_level", 4).toInt());
    ui->comboCompression->setCurrentIndex(
        ui->comboCompression->findText((settings.value("secondary_compression", "None").toString())));
    if (QFile::exists(patchFile)) {
        ui->tabWidget->setCurrentWidget(ui->tabApplyPatch);
        ui->textApplyPatch->setText(patchFile);
    }
}

MainWindow::~MainWindow()
{
    settings.setValue("geometry", saveGeometry());
    settings.setValue("compression_level", ui->spinCompressionLevel->cleanText());
    settings.setValue("secondary_compression", ui->comboCompression->currentText());
    delete ui;
}

void MainWindow::centerWindow()
{
    auto screenGeometry = QApplication::primaryScreen()->geometry();
    auto x = (screenGeometry.width() - width()) / 2;
    auto y = (screenGeometry.height() - height()) / 2;
    move(x, y);
}

void MainWindow::cmdStart()
{
    bar->setValue(0);
    progress->setLabelText("");
    if (!timer.isActive()) {
        timer.start(1s);
    }
    setCursor(QCursor(Qt::BusyCursor));
}

void MainWindow::cmdDone()
{
    timer.stop();
    setCursor(QCursor(Qt::ArrowCursor));
    bar->setValue(bar->maximum());
    progress->close();
}

void MainWindow::onSelectFile(QLineEdit *lineEdit)
{
    QString filter = lineEdit->objectName() != "textApplyPatch"
                         ? tr("ISO Files (*.iso);;All Files (*)", "kinds of files to choose")
                         : tr("ISO Files (*.xdelta3);;All Files (*)", "kinds of files to choose");
    // Choosing a file
    QString selected
        = QFileDialog::getOpenFileName(this, tr("Select file", "choose a file"), QDir::currentPath(), filter);
    if (checkFile(selected)) {
        lineEdit->setText(selected);
        QDir::setCurrent(QFileInfo(selected).absolutePath());
    } else {
        checkAllinfo();
        return;
    }
    if (lineEdit->objectName() == "textTarget") {
        setPatchName();
    }
    checkAllinfo();
}

void MainWindow::onSelectDir()
{
    QString name = ui->tabWidget->currentWidget() == ui->tabCreatePatch ? QFileInfo(ui->textPatch->text()).fileName()
                                                                        : QFileInfo(ui->textOutput->text()).fileName();
    QString path = QFileDialog::getExistingDirectory(
        this, tr("Select directory to place the file in", "select a target directory"), QDir::currentPath());
    if (path.isEmpty()) {
        return;
    }

    if (QFile::exists(path)) {
        if (ui->tabWidget->currentWidget() == ui->tabCreatePatch) {
            ui->textPatch->setText(path + "/" + name);
        } else {
            ui->textOutput->setText(path + "/" + name);
        }
    } else {
        QMessageBox::warning(this, tr("Path not found", "warning about not finding the target directory"),
                             tr("Please select directory again.", "message to try again"));
    }
    checkAllinfo();
}

void MainWindow::applyPatch()
{
    if (!ui->textOutput->text().isEmpty() && QFileInfo(ui->textOutput->text()).baseName().isEmpty()) {
        ui->pushApplyPatch->setDisabled(true);
        QMessageBox::warning(this, tr("Error"), tr("Please enter a name for the output file."));
        return;
    }

    QString output;
    if (!ui->textOutput->text().isEmpty()) {
        output = " \"" + ui->textOutput->text() + '"';
    }

    if (QFileInfo(ui->textOutput->text()).isFile()) {
        if (QMessageBox::No
            == QMessageBox::question(
                this, tr("File exists"),
                tr("Output file exists, do you want to overwrite?", "warning about overwritting an existing file"))) {
            return;
        }
    }
    progress->show();
    elapsedTimer.restart();
    QString cmdout;
    bool res = cmd.run("xdelta3 -f decode -s \"" + ui->textInput->text() + "\" \"" + ui->textApplyPatch->text() + '"'
                            + output,
                        &cmdout);
    QString elapsedStr = formatElapsedTime(elapsedTimer.elapsed());
    QString location = output.isEmpty() ? QFileInfo(ui->textInput->text()).absolutePath().remove("\"")
                                        : QFileInfo(output).absolutePath().remove("\"");

    if (res) {
        QMessageBox::information(
            this, tr("Success", "information that file was successfully written"),
            tr("File was successfuly written to '%1' directory.", "information that file was successfully written")
                    .arg(location)
                + '\n'
                + tr("Took %1 to patch the file.", "elapsed time, leave %1 untranslated").arg(elapsedStr));
    } else {

        QMessageBox::critical(
            this, tr("Error"),
            tr("Error: Could not write the file.", "information that there was an error creating the file") + "\n\n"
                + cmdout);
    }
}

void MainWindow::checkAllinfo()
{
    if (ui->tabWidget->currentWidget() == ui->tabCreatePatch) {
        bool sourceSet = !ui->textSource->text().isEmpty();
        bool targetSet = !ui->textTarget->text().isEmpty();
        bool patchSet = !ui->textPatch->text().isEmpty();
        ui->pushCreatePatch->setDisabled(!sourceSet || !targetSet || !patchSet);
        if (sourceSet && targetSet && QFileInfo(ui->textPatch->text()).fileName().isEmpty()) {
            ui->pushCreatePatch->setDisabled(true);
            QMessageBox::warning(this, tr("Error"),
                                 tr("Please enter a name for the delta file.", "name of delta file being created"));
        }
    } else {
        ui->pushApplyPatch->setDisabled(ui->textInput->text().isEmpty() || ui->textApplyPatch->text().isEmpty());
    }
}

bool MainWindow::checkFile(const QString &fileName)
{
    if (fileName.isEmpty()) {
        return false;
    }
    if (!QFileInfo(fileName).isFile()) {
        QMessageBox::warning(
            this, tr("File not found", "warning about file not found"),
            tr("File '%1' found or not a file, please double-check the input.", "warning about file not found")
                .arg(fileName));
        return false;
    } else {
        return true;
    }
}

void MainWindow::createPatch()
{
    QString force;
    if (QFileInfo(ui->textPatch->text()).isFile()) {
        if (QMessageBox::No
            == QMessageBox::question(
                this, tr("File exists", "warning about overwritting file"),
                tr("Delta file exists, do you want to overwrite?", "warning about overwritting file"))) {
            return;
        } else {
            force = "-f ";
        }
    }
    progress->show();
    elapsedTimer.restart();
    QString cmdout;
    bool res = cmd.run("xdelta3 " + force + "encode -" + ui->spinCompressionLevel->cleanText() + " -S "
                            + ui->comboCompression->currentText().toLower() + " -s \"" + ui->textSource->text() + "\" \""
                            + ui->textTarget->text() + "\" \"" + ui->textPatch->text() + '"',
                        &cmdout);
    QString elapsedStr = formatElapsedTime(elapsedTimer.elapsed());
    if (res) {
        QString patchPath = QFileInfo(ui->textPatch->text()).absoluteFilePath().remove('"');
        QString patchSize = formatFileSize(QFileInfo(patchPath).size());
        QMessageBox::information(this, tr("Success", "information on file written succesfully"),
                                  tr("File '%1' was successfuly written.", "information on file written succesfully")
                                          .arg(patchPath)
                                      + '\n'
                                      + tr("Patch size: %1", "size of the created patch file").arg(patchSize)
                                      + '\n'
                                      + tr("Took %1 to create the patch.", "elapsed time, leave %1 untranslated")
                                            .arg(elapsedStr));
    } else {
        QMessageBox::critical(
            this, tr("Error"),
            tr("Error: Could not write the file.", "information that file was not written succsessfully") + "\n\n"
                + cmdout);
    }
}

void MainWindow::setConnections()
{
    connect(ui->pushApplyPatch, &QPushButton::pressed, this, &MainWindow::applyPatch);
    connect(ui->pushCreatePatch, &QPushButton::pressed, this, &MainWindow::createPatch);
    connect(ui->pushPatchLocation, &QPushButton::pressed, this, &MainWindow::onSelectDir);
    connect(ui->pushSelectInput, &QPushButton::pressed, this, [this] { onSelectFile(ui->textInput); });
    connect(ui->pushSelectOutput, &QPushButton::pressed, this, &MainWindow::onSelectDir);
    connect(ui->pushSelectPatch, &QPushButton::pressed, this, [this] { onSelectFile(ui->textApplyPatch); });
    connect(ui->pushSelectSource, &QPushButton::pressed, this, [this] { onSelectFile(ui->textSource); });
    connect(ui->pushSelectTarget, &QPushButton::pressed, this, [this] { onSelectFile(ui->textTarget); });
    connect(ui->textPatch, &QLineEdit::editingFinished, this, [this] { checkAllinfo(); });
    connect(ui->textSource, &QLineEdit::editingFinished, this, [this] {
        if (checkFile(ui->textSource->text()) && !ui->textTarget->text().isEmpty()) {
            setPatchName();
        }
        checkAllinfo();
    });
    connect(ui->textTarget, &QLineEdit::editingFinished, this, [this] {
        if (checkFile(ui->textTarget->text()) && !ui->textSource->text().isEmpty()) {
            setPatchName();
        }
        checkAllinfo();
    });
    connect(ui->textInput, &QLineEdit::editingFinished, this, [this] {
        checkFile(ui->textInput->text());
        checkAllinfo();
    });
    connect(ui->textApplyPatch, &QLineEdit::editingFinished, this, [this] {
        checkFile(ui->textApplyPatch->text());
        checkAllinfo();
    });

    // Drag and drop connections
    connect(ui->textSource, &DropLineEdit::fileDropped, this, [this](const QString &filePath) {
        if (checkFile(filePath)) {
            QDir::setCurrent(QFileInfo(filePath).absolutePath());
            if (!ui->textTarget->text().isEmpty()) {
                setPatchName();
            }
        }
        checkAllinfo();
    });
    connect(ui->textTarget, &DropLineEdit::fileDropped, this, [this](const QString &filePath) {
        if (checkFile(filePath)) {
            QDir::setCurrent(QFileInfo(filePath).absolutePath());
            if (!ui->textSource->text().isEmpty()) {
                setPatchName();
            }
        }
        checkAllinfo();
    });
    connect(ui->textInput, &DropLineEdit::fileDropped, this, [this](const QString &filePath) {
        if (checkFile(filePath)) {
            QDir::setCurrent(QFileInfo(filePath).absolutePath());
        }
        checkAllinfo();
    });
    connect(ui->textApplyPatch, &DropLineEdit::fileDropped, this, [this](const QString &filePath) {
        if (checkFile(filePath)) {
            QDir::setCurrent(QFileInfo(filePath).absolutePath());
        }
        checkAllinfo();
    });

    setProgressDialog();
    connect(&timer, &QTimer::timeout, this, &MainWindow::updateBar);
    connect(&cmd, &Cmd::started, this, &MainWindow::cmdStart);
    connect(&cmd, &Cmd::done, this, &MainWindow::cmdDone);
}

void MainWindow::setPatchName()
{
    QString sourceBase = QFileInfo(ui->textSource->text()).completeBaseName();
    QString targetBase = QFileInfo(ui->textTarget->text()).completeBaseName();
    QString targetDir = QFileInfo(ui->textTarget->text()).absolutePath();
    QString patchName;
    if (sourceBase == targetBase) {
        patchName = sourceBase + "-patch.xdelta3";
    } else {
        QString prefix = findCommonPrefix(sourceBase, targetBase);
        if (prefix.length() < 3) {
            patchName = sourceBase + "_to_" + targetBase + ".xdelta3";
        } else {
            patchName = prefix + sourceBase.mid(prefix.length()) + "_to_" + targetBase.mid(prefix.length()) + ".xdelta3";
        }
    }
    ui->textPatch->setText(targetDir + "/" + patchName);
}

void MainWindow::setProgressDialog()
{
    progress = new QProgressDialog(this);
    bar = new QProgressBar(progress);
    auto *pushCancel = new QPushButton(tr("Cancel", "stop an action in progress"));
    connect(pushCancel, &QPushButton::clicked, this, [this] { cmd.terminate(); });
    bar->setMaximum(100);
    progress->setWindowModality(Qt::WindowModal);
    progress->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
                             | Qt::WindowStaysOnTopHint);
    progress->setCancelButton(pushCancel);
    progress->setAutoClose(false);
    progress->setBar(bar);
    bar->setTextVisible(false);
    progress->reset();
}

QString MainWindow::findCommonPrefix(const QString &str1, const QString &str2)
{
    int i = 0;
    while (i < str1.length() && i < str2.length() && str1.at(i) == str2.at(i)) {
        i++;
    }
    return str1.left(i);
}

QString MainWindow::formatElapsedTime(qint64 ms)
{
    QTime time {0, 0};
    time = time.addMSecs(static_cast<int>(ms));
    return QLocale().toString(time, "mm:ss");
}

QString MainWindow::formatFileSize(qint64 bytes)
{
    QLocale locale;
    if (bytes < 1024) {
        return locale.toString(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        return locale.toString(bytes / 1024.0, 'f', 1) + " KiB";
    } else if (bytes < 1024 * 1024 * 1024) {
        return locale.toString(bytes / (1024.0 * 1024.0), 'f', 1) + " MiB";
    } else {
        return locale.toString(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GiB";
    }
}

void MainWindow::updateBar()
{
    Cmd cmd2;
    QString output;
    if (cmd2.run("progress -c xdelta3 | tail -n2", &output, true)) {
        bool ok {false};
        QString percentage = output.trimmed().section("%", 0, 0);
        int prog = static_cast<int>(percentage.toDouble(&ok));
        if (ok) {
            bar->setValue(prog);
        }
    }
    QString elapsedStr = formatElapsedTime(elapsedTimer.elapsed());
    progress->setLabelText(tr("%1 elapsed", "elapsed time, leave %1 untranslated").arg(elapsedStr) + '\n' + output);
}
