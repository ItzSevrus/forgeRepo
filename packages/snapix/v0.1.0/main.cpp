
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QDebug>

static void fail(const QString &message, int code = 1)
{
    qCritical().noquote() << "snapix:" << message;
    ::exit(code);
}



static QByteArray runCommand(const QString &program,
                             const QStringList &arguments)
{
    const QString executable =
        QStandardPaths::findExecutable(program);

    if (executable.isEmpty())
        fail("Cannot find executable: " + program);

    QProcess process;
    process.setProgram(executable);
    process.setArguments(arguments);

    // Preserve interactive input and capture command output.
    process.setInputChannelMode(QProcess::ForwardedInputChannel);
    process.setProcessChannelMode(QProcess::SeparateChannels);

    qInfo().noquote() << "Starting:" << program;

    process.start();

    if (!process.waitForStarted(5000))
        fail("Could not start " + program + ": " +
             process.errorString());

    if (!process.waitForFinished(-1)) {
        const QString error = process.errorString();
        process.kill();
        process.waitForFinished();
        fail(program + " failed: " + error);
    }

    const QByteArray stdoutData = process.readAllStandardOutput();
    const QByteArray stderrData = process.readAllStandardError();

    if (process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
        fail(program + " exited with code " +
             QString::number(process.exitCode()) +
             (stderrData.isEmpty()
                  ? QString()
                  : QStringLiteral(": ") +
                    QString::fromLocal8Bit(stderrData).trimmed()),
             2);
    }

    return stdoutData;
}


static QByteArray capture(bool selectRegion, const QString &tempPath)
{
    QStringList arguments;

    if (selectRegion) {
        const QByteArray geometry = runCommand("slurp", {});
        const QString selection =
            QString::fromLocal8Bit(geometry).trimmed();

        if (selection.isEmpty())
            fail("No region selected.", 2);

        arguments << "-g" << selection;
    }

    arguments << tempPath;
    runCommand("grim", arguments);

    QFile file(tempPath);
    if (!file.open(QIODevice::ReadOnly))
        fail("Cannot read captured image: " + file.errorString());

    const QByteArray data = file.readAll();
    if (data.isEmpty())
        fail("Capture produced an empty file.");

    // Verify that the PNG signature is present.
    const QByteArray pngSignature =
        QByteArray::fromHex("89504e470d0a1a0a");

    if (!data.startsWith(pngSignature))
        fail("Capture is not a valid PNG file.");

    // Decode the image to catch malformed or truncated PNG data.
    QImageReader reader(tempPath);
    reader.setDecideFormatFromContent(true);

    if (reader.format().toLower() != "png")
        fail("Captured image format is not PNG.");

    if (reader.read().isNull())
        fail("PNG validation failed: " + reader.errorString());

    return data;
}

static void copyToClipboard(const QByteArray &png)
{
    QProcess process;
    process.setProgram("wl-copy");
    process.setArguments({"--type", "image/png"});
    process.start();

    if (!process.waitForStarted())
        fail("Could not start wl-copy: " + process.errorString());

    qint64 offset = 0;
    while (offset < png.size()) {
        const qint64 written =
            process.write(png.constData() + offset, png.size() - offset);

        if (written < 0)
            fail("Failed to send PNG to clipboard.");

        offset += written;

        if (process.bytesToWrite() > 0 &&
            !process.waitForBytesWritten(-1)) {
            fail("Clipboard write failed: " + process.errorString());
        }
    }

    process.closeWriteChannel();

    if (!process.waitForFinished(-1) ||
        process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
        fail("wl-copy failed: " +
             QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("snapix");
    QCoreApplication::setApplicationVersion("0.1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "A lightweight screenshot utility for Hyprland and Wayland.");

    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption fullOption(
        "full", "Capture the full screen.");
    QCommandLineOption selectOption(
        "select", "Select a rectangular screen region.");
    QCommandLineOption copyOption(
        "copy", "Copy the captured PNG to the clipboard.");
    QCommandLineOption outputOption(
        {"o", "output"}, "Output PNG file path.",
        "path");

    parser.addOption(fullOption);
    parser.addOption(selectOption);
    parser.addOption(copyOption);
    parser.addOption(outputOption);
    parser.process(app);

    if (parser.isSet(fullOption) && parser.isSet(selectOption))
        fail("--full and --select cannot be used together.", 2);

    const bool selectRegion = parser.isSet(selectOption);
    const bool copy = parser.isSet(copyOption);

    QString outputPath = parser.value(outputOption);

    if (outputPath.isEmpty()) {
        const QString pictures =
            QStandardPaths::writableLocation(
                QStandardPaths::PicturesLocation);

        if (pictures.isEmpty())
            fail("Cannot determine the Pictures directory.");

        const QString dirPath = pictures + "/Screenshots";

        if (!QDir().mkpath(dirPath))
            fail("Cannot create screenshot directory: " + dirPath);

        outputPath = dirPath + "/" +
            QDateTime::currentDateTime().toString(
                "yyyy-MM-dd_HH-mm-ss") + ".png";
    }

    QFileInfo outputInfo(outputPath);

    if (outputInfo.suffix().compare("png", Qt::CaseInsensitive) != 0)
        fail("This version supports PNG output only.", 2);

    if (!QDir().mkpath(outputInfo.absolutePath()))
        fail("Cannot create output directory.");

    // Never overwrite an existing screenshot.
    if (QFileInfo::exists(outputPath))
        fail("Output file already exists: " + outputPath, 2);

    const QString tempPath =
        outputInfo.absolutePath() + "/.snapix-" +
        QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";

    QByteArray png;

    if (QStandardPaths::findExecutable("grim").isEmpty())
        fail("grim is not installed.");

    if (selectRegion &&
        QStandardPaths::findExecutable("slurp").isEmpty())
        fail("slurp is not installed.");

    if (copy &&
        QStandardPaths::findExecutable("wl-copy").isEmpty())
        fail("wl-clipboard is not installed.");

    // Capture, validate, then publish the final file atomically.
    png = capture(selectRegion, tempPath);
    QFile::remove(tempPath);

    QSaveFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly))
        fail("Cannot create output file: " + output.errorString());

    if (output.write(png) != png.size()) {
        output.cancelWriting();
        fail("Failed to write the complete PNG.");
    }

    if (!output.commit())
        fail("Failed to finalize screenshot: " + output.errorString());

    // Copy the exact validated PNG bytes; do not re-encode.
    if (copy)
        copyToClipboard(png);

    qInfo().noquote() << "Saved:" << outputPath;
    if (copy)
        qInfo() << "Copied PNG to clipboard.";

    return 0;
}
