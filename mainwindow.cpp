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
#include <QPushButton>
#include <QFile>
#include <QFileDialog>
#include <QLocale>
#include <QMessageBox>
#include <QProcess>
#include <QScreen>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTabBar>
#include <QTemporaryFile>
#include <QtDBus/QtDBus>
#include <QTime>

#include <algorithm>
#include <chrono>
#include <cstdio>

#include <sys/stat.h>

using namespace std::chrono_literals;

namespace
{
bool isNumericVersion(const QString &value)
{
    if (value.isEmpty())
        return false;
    for (const QString &part : value.split('.', Qt::KeepEmptyParts)) {
        if (part.isEmpty() || !std::all_of(part.cbegin(), part.cend(), [](QChar c) { return c.isDigit(); }))
            return false;
    }
    return true;
}
} // namespace

MainWindow::MainWindow(const QString &patchFile, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    progressProcess = new QProcess(this);
    connect(progressProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::handleProgressOutput);
    connect(progressProcess, &QProcess::readyReadStandardError, this, &MainWindow::handleProgressOutput);

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
    ui->tabWidget->setTabVisible(2, false);
}

MainWindow::~MainWindow()
{
    if (cmd.state() != QProcess::NotRunning) {
        cmd.disconnect();
        cmd.terminate();
        if (!cmd.waitForFinished(1000)) {
            cmd.kill();
        }
        // cmdFinished() won't run (disconnected), so drop the partial temp file
        // ourselves; the user's original file was never touched.
        if (!outputTempPath.isEmpty() && outputTempPath != outputFinalPath) {
            QFile::remove(outputTempPath);
        }
    }
    settings.setValue("geometry", saveGeometry());
    settings.setValue("compression_level", ui->spinCompressionLevel->value());
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
    ui->progressBar->setValue(0);
    lastProg = -1;
    lastStatsLine.clear();
    etaMs = -1;
    etaTick = 0;
    etaSizeStr.clear();
    // Note: targetFileSize is intentionally NOT reset here. cmdStart() runs
    // asynchronously from the process's started() signal and would otherwise
    // race with (and clobber) the printhdrs probe in applyPatch(). Each
    // operation resets it up front instead.
    progressMissing = false;
    ui->labelProgressFile->setText("");
    ui->labelProgressStats->setText("");
    
    // Switch to progress tab and hide others
    ui->tabWidget->setTabVisible(0, false);
    ui->tabWidget->setTabVisible(1, false);
    ui->tabWidget->setTabVisible(2, true);
    ui->tabWidget->setCurrentIndex(2);
    // Hide tab bar for a "full page" feel
    ui->tabWidget->findChild<QTabBar*>()->hide();

    if (!timer.isActive()) {
        timer.start(1s);
    }
    setCursor(QCursor(Qt::BusyCursor));
}

void MainWindow::handleProgressOutput()
{
    const QString rawOutput = (progressProcess->readAllStandardOutput() + progressProcess->readAllStandardError()).trimmed();
    if (rawOutput.isEmpty()) {
        return;
    }

    static const QRegularExpression re(R"((\d+(?:\.\d+)?)%)");
    const QStringList lines = rawOutput.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        auto match = re.match(line);
        if (match.hasMatch()) {
            bool ok = false;
            double val = match.captured(1).toDouble(&ok);
            if (ok) {
                lastProg = static_cast<int>(val);
                lastStatsLine = line.trimmed();
                break;
            }
        }
    }
}

void MainWindow::cmdFinished(bool success, const QString &output)
{
    updateTaskbar(0, false);
    timer.stop();
    setCursor(QCursor(Qt::ArrowCursor));
    // Ensure a determinate range first: the bar may have been left indeterminate
    // (setRange(0, 0)) by updateBar(), where maximum() is 0 and the fill would
    // never reach 100%.
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(100);
    
    // Restore selection interface
    ui->tabWidget->setTabVisible(0, true);
    ui->tabWidget->setTabVisible(1, true);
    ui->tabWidget->setTabVisible(2, false);
    ui->tabWidget->setCurrentIndex(currentOp == Operation::ApplyPatch ? 1 : 0);
    ui->tabWidget->findChild<QTabBar*>()->show();

    QString elapsedStr = formatElapsedTime(elapsedTimer.elapsed());

    // Promote the temporary file to its final location only on success, so the
    // user's original file is never touched if the run failed or was cancelled.
    QString failureDetail = output;
    const bool wroteToTemp = !outputTempPath.isEmpty() && outputTempPath != outputFinalPath;
    if (success && wroteToTemp) {
        const QByteArray tempName = QFile::encodeName(outputTempPath);
        const QByteArray finalName = QFile::encodeName(outputFinalPath);

        // Give the result the mode a direct create would have. QTemporaryFile
        // makes the temp 0600 and xdelta3 -f keeps it, which would otherwise
        // leave output as 0600 instead of e.g. 0644.
        if (QFile::exists(outputFinalPath)) {
            // Overwriting: keep the mode the user's existing file had.
            QFile::setPermissions(outputTempPath, QFile::permissions(outputFinalPath));
        } else {
            // New file: honor the umask as a normal create would. Qt exposes no
            // umask accessor, so this case stays POSIX.
            const mode_t mask = ::umask(0);
            ::umask(mask);
            ::chmod(tempName.constData(), 0666 & ~mask);
        }

        // Atomically replace the destination in a single rename(2) so the
        // original is never removed before the replacement is in place: on
        // failure the original survives untouched and only the temp is dropped.
        if (std::rename(tempName.constData(), finalName.constData()) != 0) {
            success = false;
            failureDetail = tr("Could not move the temporary file to '%1'.").arg(outputFinalPath);
        }
    }
    // Clean up the temporary file on any non-success outcome (failure, cancel,
    // or a failed promotion). The original file is left untouched.
    if (!success && wroteToTemp) {
        QFile::remove(outputTempPath);
    }

    if (currentOp == Operation::ApplyPatch) {
        QString location = QFileInfo(outputFinalPath).absolutePath();
        if (success) {
            QMessageBox::information(
                this, tr("Success", "information that file was successfully written"),
                tr("File was successfully written to '%1' directory.", "information that file was successfully written")
                        .arg(location)
                    + '\n'
                    + tr("Took %1 to patch the file.", "elapsed time, leave %1 untranslated").arg(elapsedStr));
        } else {
            if (cancelled) {
                QMessageBox::information(this, tr("Cancelled"), tr("Operation was cancelled."));
            } else {
                QMessageBox box(QMessageBox::Critical, tr("Error"),
                                tr("Error: Could not write the file.",
                                   "information that there was an error creating the file"),
                                QMessageBox::Ok, this);
                if (!failureDetail.isEmpty()) {
                    box.setDetailedText(failureDetail);
                }
                box.exec();
            }
        }
    } else if (currentOp == Operation::CreatePatch) {
        if (success) {
            QString patchPath = QFileInfo(outputFinalPath).absoluteFilePath();
            QString patchSize = formatFileSize(QFileInfo(patchPath).size());
            QMessageBox::information(
                this, tr("Success", "information on file written successfully"),
                tr("File '%1' was successfully written.", "information on file written successfully").arg(patchPath)
                    + '\n'
                    + tr("Patch size: %1", "size of the created patch file").arg(patchSize) + '\n'
                    + tr("Took %1 to create the patch.", "elapsed time, leave %1 untranslated").arg(elapsedStr));
        } else {
            if (cancelled) {
                QMessageBox::information(this, tr("Cancelled"), tr("Operation was cancelled."));
            } else {
                QMessageBox box(QMessageBox::Critical, tr("Error"),
                                tr("Error: Could not write the file.",
                                   "information that file was not written successfully"),
                                QMessageBox::Ok, this);
                if (!failureDetail.isEmpty()) {
                    box.setDetailedText(failureDetail);
                }
                box.exec();
            }
        }
    }
    outputTempPath.clear();
    outputFinalPath.clear();
    currentOp = Operation::None;
}

QString MainWindow::dirSettingsKey(QLineEdit *lineEdit) const
{
    if (lineEdit == ui->textSource)     return "last_dir_source";
    if (lineEdit == ui->textTarget)     return "last_dir_target";
    if (lineEdit == ui->textPatch)      return "last_dir_patch";
    if (lineEdit == ui->textInput)      return "last_dir_input";
    if (lineEdit == ui->textApplyPatch) return "last_dir_apply_patch";
    if (lineEdit == ui->textOutput)     return "last_dir_output";
    return "last_dir";
}

void MainWindow::updateTaskbar(int percent, bool visible)
{
    QDBusMessage signal = QDBusMessage::createSignal("/", "com.canonical.Unity.LauncherEntry", "Update");
    signal << "application://xdelta3-gui.desktop";
    QVariantMap properties;
    properties.insert("progress", static_cast<double>(percent) / 100.0);
    properties.insert("progress-visible", visible);
    signal << properties;
    QDBusConnection::sessionBus().send(signal);
}

void MainWindow::onSelectFile(QLineEdit *lineEdit, const QString &filter)
{
    QString key = dirSettingsKey(lineEdit);
    QString lastDir = settings.value(key, QDir::homePath()).toString();
    QString selected = QFileDialog::getOpenFileName(this, tr("Select file", "choose a file"), lastDir, filter);
    if (checkFile(selected)) {
        lineEdit->setText(selected);
        QString path = QFileInfo(selected).absolutePath();

        settings.setValue(key, path);
    } else {
        checkAllinfo();
        return;
    }
    if (lineEdit == ui->textTarget) {
        setPatchName();
    } else if (lineEdit == ui->textInput || lineEdit == ui->textApplyPatch) {
        setOutputName();
    }
    checkAllinfo();
}

void MainWindow::onSelectDir()
{
    bool isCreatePatch = ui->tabWidget->currentWidget() == ui->tabCreatePatch;
    QLineEdit *le = isCreatePatch ? ui->textPatch : ui->textOutput;
    QString currentPath = le->text();
    QString key = dirSettingsKey(le);
    
    QString initialPath;
    if (QFileInfo(currentPath).isAbsolute()) {
        initialPath = currentPath;
    } else {
        QString lastDir = settings.value(key, QDir::homePath()).toString();
        initialPath = lastDir + "/" + QFileInfo(currentPath).fileName();
    }

    QString filter = isCreatePatch ? tr("Delta Files (*.xdelta3);;All Files (*)", "kinds of files to choose") 
                                   : tr("All Files (*)", "kinds of files to choose");
    QString title = isCreatePatch ? tr("Select where to save the delta file", "dialog title") 
                                   : tr("Select where to save the patched file", "dialog title");

    QString selected = QFileDialog::getSaveFileName(this, title, initialPath, filter);
    if (selected.isEmpty()) {
        return;
    }

    le->setText(selected);
    settings.setValue(key, QFileInfo(selected).absolutePath());
    checkAllinfo();
}

void MainWindow::applyPatch()
{
    if (ui->textOutput->text().isEmpty()) {
        ui->pushApplyPatch->setDisabled(true);
        QMessageBox::warning(this, tr("Error"), tr("Please enter a name for the output file."));
        return;
    }

    if (!checkFile(ui->textInput->text()) || !checkFile(ui->textApplyPatch->text())) {
        return;
    }

    QFileInfo outInfo(ui->textOutput->text());
    if (outInfo.exists()) {
        if (!outInfo.isWritable()) {
            QMessageBox::critical(this, tr("Error"), tr("Output file is not writable."));
            return;
        }
        if (QMessageBox::No
            == QMessageBox::question(
                this, tr("File exists"),
                tr("Output file exists, do you want to overwrite?", "warning about overwriting an existing file"))) {
            return;
        }
    } else if (!QFileInfo(outInfo.absolutePath()).isWritable()) {
        QMessageBox::critical(this, tr("Error"), tr("Output directory is not writable."));
        return;
    }

    const QString finalPath = ui->textOutput->text();
    const QString tempPath = makeTempPath(finalPath);
    if (tempPath.isEmpty()) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not create a temporary file in the destination directory."));
        return;
    }

    elapsedTimer.restart();
    cancelled = false;
    currentOp = Operation::ApplyPatch;
    const quint64 thisRun = ++operationId;
    targetFileSize = -1;
    outputFinalPath = finalPath;
    outputTempPath = tempPath;
    QStringList args;
    args << "-f" << "decode" << "-s" << ui->textInput->text() << ui->textApplyPatch->text() << outputTempPath;
    cmd.runAsync("xdelta3", args);

    // Probe the patched-file size asynchronously (printhdrs can be slow for
    // large patches, so don't block the UI). The result feeds the native
    // progress estimate in updateBar(); apply it only if this exact run is still
    // current, so a late result from a prior run can't poison a later operation.
    auto *proc = new QProcess(this);
    connect(proc, &QProcess::errorOccurred, proc, &QObject::deleteLater);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc, thisRun](int, QProcess::ExitStatus) {
                qint64 total = 0;
                const QString out = proc->readAllStandardOutput() + proc->readAllStandardError();
                for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
                    if (line.contains("VCDIFF target window length:")) {
                        bool ok;
                        qint64 n = line.section(':', -1).trimmed().toLongLong(&ok);
                        if (ok)
                            total += n;
                    }
                }
                if (total > 0 && operationId == thisRun)
                    targetFileSize = total;
                proc->deleteLater();
            });
    proc->start("xdelta3", {"printhdrs", ui->textApplyPatch->text()});
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
    QFileInfo info(fileName);
    if (!info.isFile()) {
        QMessageBox::warning(
            this, tr("File not found", "warning about file not found"),
            tr("File '%1' not found or not a file, please double-check the input.", "warning about file not found")
                .arg(fileName));
        return false;
    }
    if (!info.isReadable()) {
        QMessageBox::warning(
            this, tr("Permission denied"),
            tr("File '%1' is not readable. Please check your permissions.").arg(fileName));
        return false;
    }
    return true;
}

void MainWindow::validateFile(QLineEdit *le)
{
    QString error;
    QString text = le->text();
    if (text.isEmpty()) {
        le->setStyleSheet("");
        le->setToolTip("");
        return;
    }
    QFileInfo info(text);
    if (le == ui->textPatch || le == ui->textOutput) {
        if (info.exists()) {
            if (info.isDir()) {
                error = tr("Path exists but is a directory.");
            } else if (!info.isWritable()) {
                error = tr("File is not writable.");
            }
        } else {
            if (!info.absoluteDir().exists()) {
                error = tr("Parent directory does not exist.");
            } else if (!QFileInfo(info.absolutePath()).isWritable()) {
                error = tr("Parent directory is not writable.");
            } else if (info.fileName().isEmpty()) {
                error = tr("Filename cannot be empty.");
            }
        }
    } else {
        if (!info.exists()) {
            error = tr("File does not exist.");
        } else if (!info.isFile()) {
            error = tr("Path is a directory, not a file.");
        } else if (!info.isReadable()) {
            error = tr("File is not readable (check permissions).");
        }
    }
    bool ok = error.isEmpty();
    le->setStyleSheet(ok ? "" : "QLineEdit { color: red; }");
    le->setToolTip(error);
}

void MainWindow::createPatch()
{
    if (!checkFile(ui->textSource->text()) || !checkFile(ui->textTarget->text())) {
        return;
    }

    // Encoding a file against itself produces an empty/pointless patch; the
    // canonical paths catch symlinks and relative/absolute spellings too.
    const QString sourceCanonical = QFileInfo(ui->textSource->text()).canonicalFilePath();
    const QString targetCanonical = QFileInfo(ui->textTarget->text()).canonicalFilePath();
    if (!sourceCanonical.isEmpty() && sourceCanonical == targetCanonical) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Source and target are the same file; the patch would be empty."));
        return;
    }

    QFileInfo patchInfo(ui->textPatch->text());
    if (patchInfo.exists()) {
        if (!patchInfo.isWritable()) {
            QMessageBox::critical(this, tr("Error"), tr("Delta file is not writable."));
            return;
        }
        if (QMessageBox::No
            == QMessageBox::question(
                this, tr("File exists", "warning about overwriting file"),
                tr("Delta file exists, do you want to overwrite?", "warning about overwriting file"))) {
            return;
        }
    } else if (!QFileInfo(patchInfo.absolutePath()).isWritable()) {
        QMessageBox::critical(this, tr("Error"), tr("Output directory is not writable."));
        return;
    }

    const QString finalPath = ui->textPatch->text();
    const QString tempPath = makeTempPath(finalPath);
    if (tempPath.isEmpty()) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Could not create a temporary file in the destination directory."));
        return;
    }

    elapsedTimer.restart();
    cancelled = false;
    currentOp = Operation::CreatePatch;
    ++operationId;
    targetFileSize = -1;
    outputFinalPath = finalPath;
    outputTempPath = tempPath;
    QStringList args;
    args << "-f" << "encode" << ("-" + QString::number(ui->spinCompressionLevel->value()));
    if (ui->comboCompression->currentIndex() != 0) {
        args << "-S" << ui->comboCompression->currentText().toLower();
    }
    args << "-s" << ui->textSource->text()
         << ui->textTarget->text() << outputTempPath;
    cmd.runAsync("xdelta3", args);
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

    connect(ui->textSource, &QLineEdit::textChanged, this, [this] { validateFile(ui->textSource); });
    connect(ui->textTarget, &QLineEdit::textChanged, this, [this] { validateFile(ui->textTarget); });
    connect(ui->textPatch, &QLineEdit::textChanged, this, [this] { validateFile(ui->textPatch); });
    connect(ui->textInput, &QLineEdit::textChanged, this, [this] { validateFile(ui->textInput); });
    connect(ui->textApplyPatch, &QLineEdit::textChanged, this, [this] { validateFile(ui->textApplyPatch); });
    connect(ui->textOutput, &QLineEdit::textChanged, this, [this] { validateFile(ui->textOutput); });

    // Drag and drop connections
    connect(ui->textSource, &DropLineEdit::fileDropped, this, [this](const QString &filePath) {
        if (checkFile(filePath)) {
            QString path = QFileInfo(filePath).absolutePath();
    
            settings.setValue(dirSettingsKey(ui->textSource), path);
            if (!ui->textTarget->text().isEmpty()) {
                setPatchName();
            }
        }
        checkAllinfo();
    });
    connect(ui->textTarget, &DropLineEdit::fileDropped, this, [this](const QString &filePath) {
        if (checkFile(filePath)) {
            QString path = QFileInfo(filePath).absolutePath();
    
            settings.setValue(dirSettingsKey(ui->textTarget), path);
            if (!ui->textSource->text().isEmpty()) {
                setPatchName();
            }
        }
        checkAllinfo();
    });
    connect(ui->textInput, &DropLineEdit::fileDropped, this, [this](const QString &filePath) {
        if (checkFile(filePath)) {
            QString path = QFileInfo(filePath).absolutePath();
    
            settings.setValue(dirSettingsKey(ui->textInput), path);
            setOutputName();
        }
        checkAllinfo();
    });
    connect(ui->textApplyPatch, &DropLineEdit::fileDropped, this, [this](const QString &filePath) {
        if (checkFile(filePath)) {
            QString path = QFileInfo(filePath).absolutePath();

            settings.setValue(dirSettingsKey(ui->textApplyPatch), path);
            setOutputName();
        }
        checkAllinfo();
    });
    // Output fields: a dropped path is a save destination (it may not exist yet),
    // so don't validate it with checkFile — just remember its directory.
    connect(ui->textPatch, &DropLineEdit::fileDropped, this, [this](const QString &filePath) {
        settings.setValue(dirSettingsKey(ui->textPatch), QFileInfo(filePath).absolutePath());
        checkAllinfo();
    });
    connect(ui->textOutput, &DropLineEdit::fileDropped, this, [this](const QString &filePath) {
        settings.setValue(dirSettingsKey(ui->textOutput), QFileInfo(filePath).absolutePath());
        checkAllinfo();
    });

    connect(ui->pushCancelProgress, &QPushButton::clicked, this, [this] {
        cancelled = true;
        cmd.terminate();
    });

    connect(&timer, &QTimer::timeout, this, &MainWindow::updateBar);
    connect(&cmd, &Cmd::started, this, &MainWindow::cmdStart);
    connect(&cmd, &Cmd::commandFinished, this, &MainWindow::cmdFinished);
}

void MainWindow::setPatchName()
{
    QString sourceBase = QFileInfo(ui->textSource->text()).completeBaseName();
    QString targetBase = QFileInfo(ui->textTarget->text()).completeBaseName();
    QString targetDir = QFileInfo(ui->textTarget->text()).absolutePath();
    QString patchName;
    if (sourceBase == targetBase)
        patchName = sourceBase + "-patch.xdelta3";
    else
        patchName = sourceBase + "_to_" + targetBase + ".xdelta3";
    ui->textPatch->setText(targetDir + "/" + patchName);
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

// Reserve a unique temporary path in the same directory as finalPath so the
// result can be promoted with an atomic rename and the user's original file is
// never touched until the operation succeeds. Returns an empty string if a temp
// file cannot be created; callers must abort rather than write in place, or the
// preserve-the-original guarantee is lost.
QString MainWindow::makeTempPath(const QString &finalPath)
{
    QFileInfo fi(finalPath);
    QTemporaryFile tmp(fi.absolutePath() + "/." + fi.fileName() + ".part-XXXXXX");
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        return QString();
    }
    const QString name = tmp.fileName();
    tmp.close();
    return name;
}

void MainWindow::setOutputName()
{
    QString inputPath = ui->textInput->text();
    QString patchPath = ui->textApplyPatch->text();

    if (inputPath.isEmpty()) {
        return;
    }

    QString inputBase = QFileInfo(inputPath).completeBaseName();
    QString inputSuffix = QFileInfo(inputPath).suffix();
    QString inputDir = QFileInfo(inputPath).absolutePath();
    QString outputName;

    if (!patchPath.isEmpty()) {
        QString patchBase = QFileInfo(patchPath).completeBaseName();
        // If patch name contains "_to_VERSION", try to extract that version
        if (patchBase.contains("_to_")) {
            QString targetVersion = patchBase.section("_to_", -1);
            // Remove .xdelta3 if it's there (shouldn't be in completeBaseName but just in case)
            if (targetVersion.endsWith(".xdelta3")) {
                targetVersion.chop(QStringLiteral(".xdelta3").size());
            }

            // Try to find if inputBase ends with a version we should replace
            // e.g. app-1.0 -> app-1.1. Only '-' and '_' are valid separators;
            // dots are part of version numbers, not separators.
            int lastSep = qMax(inputBase.lastIndexOf('-'), inputBase.lastIndexOf('_'));

            if (lastSep != -1 && isNumericVersion(inputBase.mid(lastSep + 1))) {
                outputName = inputBase.left(lastSep + 1) + targetVersion;
            } else if (isNumericVersion(inputBase)) {
                outputName = targetVersion;
            } else {
                outputName = inputBase + "." + targetVersion;
            }

            if (!inputSuffix.isEmpty()) {
                outputName += "." + inputSuffix;
            }
        }
    }

    if (outputName.isEmpty()) {
        outputName = inputBase + ".patched";
        if (!inputSuffix.isEmpty()) {
            outputName += "." + inputSuffix;
        }
    }

    ui->textOutput->setText(inputDir + "/" + outputName);
}

void MainWindow::updateBar()
{
    QString statsLine;
    int prog = -1;

    // 1. Try native progress estimation for Apply Patch
    if (currentOp == Operation::ApplyPatch && targetFileSize > 0) {
        qint64 currentSize = QFileInfo(outputTempPath).size();
        prog = static_cast<int>(currentSize * 100 / targetFileSize);
        if (prog > 100) prog = 100;

        qint64 elapsed = elapsedTimer.elapsed();
        if (elapsed > 0) {
            double speed = static_cast<double>(currentSize) / (elapsed / 1000.0);
            statsLine = tr("%1% (%2 / %3) %4/s")
                            .arg(prog)
                            .arg(formatFileSize(currentSize))
                            .arg(formatFileSize(targetFileSize))
                            .arg(formatFileSize(static_cast<qint64>(speed)));
        }
    }

    // 2. Fallback to external 'progress' tool if native estimation didn't work (e.g. Create Patch)
    if (prog == -1) {
        if (!progressMissing) {
            if (progressProcess->state() == QProcess::NotRunning) {
                progressProcess->start("progress", {"-c", "xdelta3"});
            }
            if (lastProg != -1) {
                prog = lastProg;
                statsLine = lastStatsLine;
            }
        }
        if (!progressMissing && QStandardPaths::findExecutable("progress").isEmpty()) {
            progressMissing = true;
        }
    }

    if (prog != -1) {
        ui->progressBar->setValue(prog);
        ui->progressBar->setRange(0, 100);
        updateTaskbar(prog, true);
    } else {
        ui->progressBar->setRange(0, 0); // Indeterminate mode
        updateTaskbar(0, false);
    }

    // Recalculate ETA every 5 seconds; estimate output size only for Create Patch
    if (etaTick % 5 == 0 && etaTick > 0 && prog > 0) {
        qint64 elapsed = elapsedTimer.elapsed();
        etaMs = qMax(0LL, elapsed * (100 - prog) / prog);

        if (currentOp == Operation::CreatePatch) {
            qint64 currentSize = QFileInfo(outputTempPath).size();
            if (currentSize > 0)
                etaSizeStr = formatFileSize(currentSize * 100LL / prog);
        }
    }
    etaTick++;

    QString outputPath = (currentOp == Operation::CreatePatch) ? ui->textPatch->text() : ui->textOutput->text();
    QString fileName = QFileInfo(outputPath).fileName();

    QString label;
    if (etaMs >= 0 && prog > 0) {
        label = tr("~%1 remaining", "estimated time remaining, leave %1 untranslated")
                    .arg(formatElapsedTime(etaMs));
    }
    if (currentOp == Operation::ApplyPatch && targetFileSize >= 0) {
        const auto sizeStr = tr("%1 output size", "exact output file size from delta, leave %1 untranslated")
                                 .arg(formatFileSize(targetFileSize));
        if (!label.isEmpty())
            label += QStringLiteral(" · ");
        label += sizeStr;
    } else if (!etaSizeStr.isEmpty()) {
        const auto sizeStr = tr("~%1 estimated size", "estimated output file size, leave %1 untranslated")
                                 .arg(etaSizeStr);
        if (!label.isEmpty())
            label += QStringLiteral(" · ");
        label += sizeStr;
    }

    if (!fileName.isEmpty()) {
        ui->labelProgressFile->setText(currentOp == Operation::ApplyPatch
                                           ? tr("Writing file: %1").arg(fileName)
                                           : tr("Creating file: %1").arg(fileName));
    }
    if (!statsLine.isEmpty()) {
        label += '\n' + statsLine;
    }
    ui->labelProgressStats->setText(label);
}
