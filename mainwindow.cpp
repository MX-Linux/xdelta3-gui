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
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QTime>

#include <algorithm>
#include <chrono>

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
    connect(progressProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::handleProgressOutput);

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
    if (cmd.state() != QProcess::NotRunning) {
        cmd.disconnect();
        cmd.terminate();
        if (!cmd.waitForFinished(1000)) {
            cmd.kill();
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
    bar->setValue(0);
    lastProg = -1;
    lastStatsLine.clear();
    etaMs = -1;
    etaTick = 0;
    etaSizeStr.clear();
    targetFileSize = -1;
    progress->setLabelText("");
    if (!timer.isActive()) {
        timer.start(1s);
    }
    setCursor(QCursor(Qt::BusyCursor));
    ui->tabWidget->setEnabled(false);
}

void MainWindow::cmdFinished(bool success, const QString &output)
{
    timer.stop();
    setCursor(QCursor(Qt::ArrowCursor));
    bar->setValue(bar->maximum());
    progress->close();
    ui->tabWidget->setEnabled(true);

    QString elapsedStr = formatElapsedTime(elapsedTimer.elapsed());

    if (currentOp == Operation::ApplyPatch) {
        QString location = QFileInfo(ui->textOutput->text()).absolutePath();
        if (success) {
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
            if (cancelled) {
                QMessageBox::information(this, tr("Cancelled"), tr("Operation was cancelled."));
            } else {
                QMessageBox::critical(
                    this, tr("Error"),
                    tr("Error: Could not write the file.", "information that there was an error creating the file")
                        + "\n\n" + output);
            }
        }
    } else if (currentOp == Operation::CreatePatch) {
        if (success) {
            QString patchPath = QFileInfo(ui->textPatch->text()).absoluteFilePath();
            QString patchSize = formatFileSize(QFileInfo(patchPath).size());
            QMessageBox::information(
                this, tr("Success", "information on file written successfully"),
                tr("File '%1' was successfully written.", "information on file written successfully").arg(patchPath)
                    + '\n'
                    + tr("Patch size: %1", "size of the created patch file").arg(patchSize) + '\n'
                    + tr("Took %1 to create the patch.", "elapsed time, leave %1 untranslated").arg(elapsedStr));
        } else {
            if (QFile::exists(ui->textPatch->text())) {
                QFile::remove(ui->textPatch->text());
            }
            if (cancelled) {
                QMessageBox::information(this, tr("Cancelled"), tr("Operation was cancelled."));
            } else {
                QMessageBox::critical(
                    this, tr("Error"),
                    tr("Error: Could not write the file.", "information that file was not written successfully")
                        + "\n\n" + output);
            }
        }
    }
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
                tr("Output file exists, do you want to overwrite?", "warning about overwritting an existing file"))) {
            return;
        }
    } else if (!QFileInfo(outInfo.absolutePath()).isWritable()) {
        QMessageBox::critical(this, tr("Error"), tr("Output directory is not writable."));
        return;
    }

    progress->show();
    elapsedTimer.restart();
    cancelled = false;
    currentOp = Operation::ApplyPatch;
    QStringList args;
    args << "-f" << "decode" << "-s" << ui->textInput->text() << ui->textApplyPatch->text() << ui->textOutput->text();
    cmd.runAsync("xdelta3", args);

    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc](int, QProcess::ExitStatus) {
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
                if (total > 0)
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

void MainWindow::createPatch()
{
    if (!checkFile(ui->textSource->text()) || !checkFile(ui->textTarget->text())) {
        return;
    }

    bool force = false;
    QFileInfo patchInfo(ui->textPatch->text());
    if (patchInfo.exists()) {
        if (!patchInfo.isWritable()) {
            QMessageBox::critical(this, tr("Error"), tr("Delta file is not writable."));
            return;
        }
        if (QMessageBox::No
            == QMessageBox::question(
                this, tr("File exists", "warning about overwritting file"),
                tr("Delta file exists, do you want to overwrite?", "warning about overwritting file"))) {
            return;
        } else {
            force = true;
        }
    } else if (!QFileInfo(patchInfo.absolutePath()).isWritable()) {
        QMessageBox::critical(this, tr("Error"), tr("Output directory is not writable."));
        return;
    }

    progress->show();
    elapsedTimer.restart();
    cancelled = false;
    currentOp = Operation::CreatePatch;
    QStringList args;
    if (force) {
        args << "-f";
    }
    args << "encode" << ("-" + QString::number(ui->spinCompressionLevel->value()));
    if (ui->comboCompression->currentIndex() != 0) {
        args << "-S" << ui->comboCompression->currentText().toLower();
    }
    args << "-s" << ui->textSource->text()
         << ui->textTarget->text() << ui->textPatch->text();
    cmd.runAsync("xdelta3", args);
}

void MainWindow::handleProgressOutput()
{
    const QString rawOutput = (progressProcess->readAllStandardOutput() + progressProcess->readAllStandardError()).trimmed();
    if (rawOutput.isEmpty()) {
        return;
    }
    const QStringList lines = rawOutput.split('\n', Qt::SkipEmptyParts);
    bool ok {false};
    for (const QString &line : lines) {
        int pctIdx = line.indexOf('%');
        if (pctIdx != -1) {
            int prog = static_cast<int>(line.left(pctIdx).trimmed().toDouble(&ok));
            if (ok) {
                lastProg = prog;
                lastStatsLine = line.trimmed();
            }
            break;
        }
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
        QFileInfo info(text);
        if (le == ui->textPatch || le == ui->textOutput) {
            if (info.exists()) {
                ok = info.isFile() && info.isWritable();
            } else {
                ok = info.absoluteDir().exists() && QFileInfo(info.absolutePath()).isWritable() && !info.fileName().isEmpty();
            }
        } else {
            ok = info.isFile() && info.isReadable();
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

    setProgressDialog();
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

void MainWindow::setProgressDialog()
{
    progress = new QProgressDialog(this);
    bar = new QProgressBar(progress);
    auto *pushCancel = new QPushButton(tr("Cancel", "stop an action in progress"));
    connect(pushCancel, &QPushButton::clicked, this, [this] {
        cancelled = true;
        cmd.terminate();
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
        qint64 currentSize = QFileInfo(ui->textOutput->text()).size();
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
        static bool progressMissing = false;
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
        bar->setValue(prog);
        bar->setRange(0, 100);
    } else {
        bar->setRange(0, 0); // Indeterminate mode
    }

    // Recalculate ETA every 5 seconds; estimate output size only for Create Patch
    if (etaTick % 5 == 0 && etaTick > 0 && prog > 0) {
        qint64 elapsed = elapsedTimer.elapsed();
        etaMs = qMax(0LL, elapsed * (100 - prog) / prog);

        if (currentOp == Operation::CreatePatch) {
            qint64 currentSize = QFileInfo(ui->textPatch->text()).size();
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
        label += tr(" · %1 output size", "exact output file size from delta, leave %1 untranslated")
                     .arg(formatFileSize(targetFileSize));
    } else if (!etaSizeStr.isEmpty()) {
        label += tr(" · ~%1 estimated size", "estimated output file size, leave %1 untranslated")
                     .arg(etaSizeStr);
    }
    if (!fileName.isEmpty()) {
        label += '\n' + tr("Creating file: %1").arg(fileName);
    }
    if (!statsLine.isEmpty()) {
        label += '\n' + statsLine;
    }
    progress->setLabelText(label);
}
