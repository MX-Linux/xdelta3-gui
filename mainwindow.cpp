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

namespace
{
bool isNumericVersion(const QString &value, QStringList *parts = nullptr)
{
    if (value.isEmpty()) {
        return false;
    }

    QStringList splitParts = value.split('.', Qt::KeepEmptyParts);
    if (splitParts.isEmpty()) {
        return false;
    }

    for (const QString &part : splitParts) {
        if (part.isEmpty()) {
            return false;
        }
        for (QChar c : part) {
            if (!c.isDigit()) {
                return false;
            }
        }
    }

    if (parts != nullptr) {
        *parts = splitParts;
    }
    return true;
}

bool useCompactTargetVersion(const QString &sourceBase, const QString &targetBase)
{
    QStringList sourceParts;
    QStringList targetParts;
    if (!isNumericVersion(sourceBase, &sourceParts) || !isNumericVersion(targetBase, &targetParts)) {
        return false;
    }

    if (sourceParts.size() != targetParts.size() || sourceParts.size() < 2) {
        return false;
    }

    for (qsizetype i = 0; i < sourceParts.size() - 1; ++i) {
        if (sourceParts.at(i) != targetParts.at(i)) {
            return false;
        }
    }

    return sourceParts.last() != targetParts.last();
}
} // namespace

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
    int compIdx = ui->comboCompression->findText(settings.value("secondary_compression", "None").toString());
    ui->comboCompression->setCurrentIndex(compIdx != -1 ? compIdx : 0);
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

void MainWindow::onSelectFile(QLineEdit *lineEdit, const QString &filter)
{
    QString selected
        = QFileDialog::getOpenFileName(this, tr("Select file", "choose a file"), QDir::currentPath(), filter);
    if (checkFile(selected)) {
        lineEdit->setText(selected);
        QDir::setCurrent(QFileInfo(selected).absolutePath());
    } else {
        checkAllinfo();
        return;
    }
    if (lineEdit == ui->textTarget) {
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

    if (ui->tabWidget->currentWidget() == ui->tabCreatePatch) {
        ui->textPatch->setText(path + "/" + name);
    } else {
        ui->textOutput->setText(path + "/" + name);
    }
    checkAllinfo();
}

void MainWindow::applyPatch()
{
    if (ui->textOutput->text().isEmpty()) {
        ui->pushApplyPatch->setDisabled(true);
        QMessageBox::warning(this, tr("Error"), tr("Please enter a name for the output file."));
        return;
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
    QStringList args;
    args << "-f" << "decode" << "-s" << ui->textInput->text() << ui->textApplyPatch->text() << ui->textOutput->text();
    bool res = cmd.run("xdelta3", args, &cmdout);
    QString elapsedStr = formatElapsedTime(elapsedTimer.elapsed());
    QString location = QFileInfo(ui->textOutput->text()).absolutePath();

    if (res) {
        QMessageBox::information(
            this, tr("Success", "information that file was successfully written"),
            tr("File was successfully written to '%1' directory.", "information that file was successfully written")
                    .arg(location)
                + '\n'
                + tr("Took %1 to patch the file.", "elapsed time, leave %1 untranslated").arg(elapsedStr));
    } else {
        if (QFile::exists(ui->textOutput->text())) {
            QFile::remove(ui->textOutput->text());
        }
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
            tr("File '%1' not found or not a file, please double-check the input.", "warning about file not found")
                .arg(fileName));
        return false;
    } else {
        return true;
    }
}

void MainWindow::createPatch()
{
    bool force = false;
    if (QFileInfo(ui->textPatch->text()).isFile()) {
        if (QMessageBox::No
            == QMessageBox::question(
                this, tr("File exists", "warning about overwritting file"),
                tr("Delta file exists, do you want to overwrite?", "warning about overwritting file"))) {
            return;
        } else {
            force = true;
        }
    }
    progress->show();
    elapsedTimer.restart();
    QString cmdout;
    QStringList args;
    if (force) {
        args << "-f";
    }
    args << "encode" << ("-" + QString::number(ui->spinCompressionLevel->value()));
    if (ui->comboCompression->currentText() != "None") {
        args << "-S" << ui->comboCompression->currentText().toLower();
    }
    args << "-s" << ui->textSource->text()
         << ui->textTarget->text() << ui->textPatch->text();
    bool res = cmd.run("xdelta3", args, &cmdout);
    QString elapsedStr = formatElapsedTime(elapsedTimer.elapsed());
    if (res) {
        QString patchPath = QFileInfo(ui->textPatch->text()).absoluteFilePath();
        QString patchSize = formatFileSize(QFileInfo(patchPath).size());
        QMessageBox::information(this, tr("Success", "information on file written successfully"),
                                  tr("File '%1' was successfully written.", "information on file written successfully")
                                          .arg(patchPath)
                                      + '\n'
                                      + tr("Patch size: %1", "size of the created patch file").arg(patchSize)
                                      + '\n'
                                      + tr("Took %1 to create the patch.", "elapsed time, leave %1 untranslated")
                                            .arg(elapsedStr));
    } else {
        QMessageBox::critical(
            this, tr("Error"),
            tr("Error: Could not write the file.", "information that file was not written successfully") + "\n\n"
                + cmdout);
    }
}

void MainWindow::setConnections()
{
    connect(ui->pushApplyPatch, &QPushButton::pressed, this, &MainWindow::applyPatch);
    connect(ui->pushCreatePatch, &QPushButton::pressed, this, &MainWindow::createPatch);
    connect(ui->pushPatchLocation, &QPushButton::pressed, this, &MainWindow::onSelectDir);
    const QString isoFilter = tr("ISO Files (*.iso);;All Files (*)", "kinds of files to choose");
    const QString deltaFilter = tr("Delta Files (*.xdelta3);;All Files (*)", "kinds of files to choose");
    connect(ui->pushSelectInput, &QPushButton::pressed, this,
            [this, isoFilter] { onSelectFile(ui->textInput, isoFilter); });
    connect(ui->pushSelectOutput, &QPushButton::pressed, this, &MainWindow::onSelectDir);
    connect(ui->pushSelectPatch, &QPushButton::pressed, this,
            [this, deltaFilter] { onSelectFile(ui->textApplyPatch, deltaFilter); });
    connect(ui->pushSelectSource, &QPushButton::pressed, this,
            [this, isoFilter] { onSelectFile(ui->textSource, isoFilter); });
    connect(ui->pushSelectTarget, &QPushButton::pressed, this,
            [this, isoFilter] { onSelectFile(ui->textTarget, isoFilter); });
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
        if (checkFile(ui->textApplyPatch->text())) {
            setOutputName();
        }
        checkAllinfo();
    });
    connect(ui->textOutput, &QLineEdit::editingFinished, this, [this] { checkAllinfo(); });

    auto validate = [this](QLineEdit *le) {
        bool ok = false;
        QString text = le->text();
        if (text.isEmpty()) {
            le->setStyleSheet("");
            return;
        }
        if (le == ui->textPatch || le == ui->textOutput) {
            QFileInfo info(text);
            ok = info.absoluteDir().exists() && !info.fileName().isEmpty();
        } else {
            ok = QFileInfo(text).isFile();
        }
        le->setStyleSheet(ok ? "" : "QLineEdit { color: red; }");
    };

    connect(ui->textSource, &QLineEdit::textChanged, this, [this, validate] { validate(ui->textSource); });
    connect(ui->textTarget, &QLineEdit::textChanged, this, [this, validate] { validate(ui->textTarget); });
    connect(ui->textPatch, &QLineEdit::textChanged, this, [this, validate] { validate(ui->textPatch); });
    connect(ui->textInput, &QLineEdit::textChanged, this, [this, validate] { validate(ui->textInput); });
    connect(ui->textApplyPatch, &QLineEdit::textChanged, this, [this, validate] { validate(ui->textApplyPatch); });
    connect(ui->textOutput, &QLineEdit::textChanged, this, [this, validate] { validate(ui->textOutput); });

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
            setOutputName();
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
    } else if (sourceBase.startsWith(targetBase + "_")) {
        // Source has a suffix (e.g., Blah_blah_beta1) and target is final (e.g., Blah_blah)
        QString suffix = sourceBase.mid(targetBase.length() + 1); // Remove targetBase and underscore
        patchName = targetBase + "-" + suffix + "_to_final.xdelta3";
    } else if (isNumericVersion(sourceBase) && isNumericVersion(targetBase)) {
        if (useCompactTargetVersion(sourceBase, targetBase)) {
            patchName = sourceBase + "_to_" + targetBase.section('.', -1) + ".xdelta3";
        } else {
            patchName = sourceBase + "_to_" + targetBase + ".xdelta3";
        }
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
    connect(pushCancel, &QPushButton::clicked, this, [this] {
        cmd.terminate();
        // Give it a moment to terminate before attempting to delete
        QTimer::singleShot(500, this, [this] {
            if (ui->tabWidget->currentWidget() == ui->tabCreatePatch) {
                if (QFile::exists(ui->textPatch->text())) {
                    QFile::remove(ui->textPatch->text());
                }
            } else {
                if (QFile::exists(ui->textOutput->text())) {
                    QFile::remove(ui->textOutput->text());
                }
            }
        });
    });
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
    int seconds = static_cast<int>(ms / 1000) % 60;
    int minutes = static_cast<int>(ms / (1000 * 60)) % 60;
    int hours = static_cast<int>(ms / (1000 * 60 * 60));

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    return QLocale().toString(QTime(0, minutes, seconds), "mm:ss");
}

QString MainWindow::formatFileSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes);
}

void MainWindow::setOutputName()
{
    if (!ui->textOutput->text().isEmpty())
        return;

    QString patchPath = ui->textApplyPatch->text();
    QString patchBase = QFileInfo(patchPath).completeBaseName();

    int toIdx = patchBase.indexOf("_to_");
    if (toIdx == -1)
        return;

    QString targetPart = patchBase.mid(toIdx + 4);
    if (targetPart.isEmpty())
        return;

    QString inputPath = ui->textInput->text();
    QString dir = inputPath.isEmpty() ? QFileInfo(patchPath).absolutePath()
                                      : QFileInfo(inputPath).absolutePath();
    QString ext = inputPath.isEmpty() ? QString() : "." + QFileInfo(inputPath).suffix();

    ui->textOutput->setText(dir + "/" + targetPart + ext);
}

void MainWindow::updateBar()
{
    Cmd cmd2;
    QString output;
    QString rawOutput;
    if (cmd2.run("progress -c xdelta3", &rawOutput, Cmd::Quiet)) {
        const QStringList lines = rawOutput.split('\n', Qt::SkipEmptyParts);
        output = lines.mid(qMax(0, lines.size() - 2)).join('\n');
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
