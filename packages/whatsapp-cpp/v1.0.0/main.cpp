#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QElapsedTimer>
#include <QIcon>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEnginePermission>
#include <QWebEngineDownloadRequest>
#include <QWebEngineSettings>

#ifdef Q_OS_LINUX
#include <unistd.h>
#include <sys/types.h>
#endif


// ============================================================
// Permission name
// ============================================================

static QString permissionName(
    QWebEnginePermission::PermissionType type)
{
    using Type =
        QWebEnginePermission::PermissionType;

    switch (type) {

    case Type::MediaAudioCapture:
        return "microphone";

    case Type::MediaVideoCapture:
        return "camera";

    case Type::MediaAudioVideoCapture:
        return "camera and microphone";

    case Type::Notifications:
        return "notifications";

    default:
        return "this feature";
    }
}


// ============================================================
// Linux memory helpers
// ============================================================

#ifdef Q_OS_LINUX

static long readProcessRssKb(pid_t pid)
{
    QFile file(
        QString("/proc/%1/status").arg(pid)
    );

    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
        return 0;

    while (!file.atEnd()) {

        const QByteArray line =
            file.readLine();

        if (line.startsWith("VmRSS:")) {

            const QList<QByteArray> parts =
                line.simplified().split(' ');

            if (parts.size() >= 2)
                return parts[1].toLongLong();
        }
    }

    return 0;
}


static pid_t readParentPid(pid_t pid)
{
    QFile file(
        QString("/proc/%1/status").arg(pid)
    );

    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text))
        return -1;

    while (!file.atEnd()) {

        const QByteArray line =
            file.readLine();

        if (line.startsWith("PPid:")) {

            const QList<QByteArray> parts =
                line.simplified().split(' ');

            if (parts.size() >= 2)
                return parts[1].toLongLong();
        }
    }

    return -1;
}


static long totalProcessTreeRssKb()
{
    const pid_t root =
        getpid();

    QDir proc("/proc");

    const QStringList entries =
        proc.entryList(
            QDir::Dirs |
            QDir::NoDotAndDotDot
        );

    long total = 0;

    for (const QString &entry :
         entries) {

        bool ok = false;

        const pid_t pid =
            entry.toLongLong(&ok);

        if (!ok)
            continue;

        pid_t current = pid;

        for (int depth = 0;
             depth < 32;
             ++depth) {

            if (current <= 1)
                break;

            if (current == root) {

                total +=
                    readProcessRssKb(pid);

                break;
            }

            const pid_t parent =
                readParentPid(current);

            if (parent <= 0 ||
                parent == current)
                break;

            current = parent;
        }
    }

    return total;
}


static QString formatMemory(long kb)
{
    if (kb < 1024)
        return QString("%1 KB")
            .arg(kb);

    const double mb =
        kb / 1024.0;

    if (mb < 1024.0) {

        return QString("%1 MB")
            .arg(
                QString::number(
                    mb,
                    'f',
                    1
                )
            );
    }

    const double gb =
        mb / 1024.0;

    return QString("%1 GB")
        .arg(
            QString::number(
                gb,
                'f',
                2
            )
        );
}

#endif


// ============================================================
// WhatsApp Window
// ============================================================

class WhatsAppWindow : public QMainWindow
{
public:

    explicit WhatsAppWindow()
        : QMainWindow(),
          browser(new QWebEngineView(this)),
          profile(nullptr),
          page(nullptr)
    {
        setWindowTitle(
            "WhatsApp"
        );

        setMinimumSize(
            900,
            600
        );

        resize(
            1280,
            820
        );

        startupTimer.start();

        setupWebEngine();

        setupConnections();

        setCentralWidget(
            browser
        );

        loadWhatsApp();


        // ----------------------------------------------------
        // Startup performance measurement
        // ----------------------------------------------------

        QTimer::singleShot(
            5000,
            this,
            [this]()
            {
                printStartupPerformance();
                printMemoryUsage();
            }
        );


        // ----------------------------------------------------
        // Periodic memory measurement
        // ----------------------------------------------------

        auto *memoryTimer =
            new QTimer(this);

        connect(
            memoryTimer,
            &QTimer::timeout,
            this,
            [this]()
            {
                printMemoryUsage();
            }
        );

        memoryTimer->start(
            30000
        );
    }


private:

    QWebEngineView *browser;

    QWebEngineProfile *profile;

    QWebEnginePage *page;

    QElapsedTimer startupTimer;


    // ========================================================
    // WebEngine / persistent profile
    // ========================================================

    void setupWebEngine()
    {
        /*
         * IMPORTANT
         *
         * This directory is intentionally fixed.
         *
         * It contains:
         *
         * - Cookies
         * - IndexedDB
         * - LocalStorage
         * - Session Storage
         * - Web permissions
         * - other persistent Chromium data
         *
         * DO NOT change this path if you want the
         * WhatsApp session to survive application restarts.
         */

        const QString profilePath =
            QDir::homePath() +
            "/.local/share/whatsapp/QtWebEngine/WhatsApp";


        const QString cachePath =
            QDir::homePath() +
            "/.cache/whatsapp/QtWebEngine/WhatsApp";


        // Make sure directories exist.

        if (!QDir().mkpath(profilePath)) {

            qWarning()
                << "[Storage] Could not create profile:"
                << profilePath;
        }


        if (!QDir().mkpath(cachePath)) {

            qWarning()
                << "[Storage] Could not create cache:"
                << cachePath;
        }


        qDebug()
            << "[Storage] Profile:"
            << profilePath;

        qDebug()
            << "[Storage] Cache:"
            << cachePath;

        qDebug()
            << "[Storage] Profile writable:"
            << QFileInfo(
                profilePath
            ).isWritable();

        qDebug()
            << "[Storage] Cache writable:"
            << QFileInfo(
                cachePath
            ).isWritable();


        // ----------------------------------------------------
        // Persistent profile
        // ----------------------------------------------------

        profile =
            new QWebEngineProfile(
                "WhatsApp",
                this
            );


        profile->setPersistentStoragePath(
            profilePath
        );


        profile->setCachePath(
            cachePath
        );


        /*
         * Force cookies onto disk.
         *
         * This is one of the most important settings for
         * keeping WhatsApp logged in.
         */

        profile->setPersistentCookiesPolicy(
            QWebEngineProfile::
                ForcePersistentCookies
        );


        /*
         * Store browser permissions on disk.
         */

        profile->setPersistentPermissionsPolicy(
            QWebEngineProfile::
                PersistentPermissionsPolicy::
                StoreOnDisk
        );


        /*
         * Use disk HTTP cache.
         */

        profile->setHttpCacheType(
            QWebEngineProfile::
                DiskHttpCache
        );


        // ----------------------------------------------------
        // User agent
        // ----------------------------------------------------

        profile->setHttpUserAgent(
            "Mozilla/5.0 (X11; Linux x86_64) "
            "AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/131.0.0.0 Safari/537.36"
        );


        // ----------------------------------------------------
        // Web page
        // ----------------------------------------------------

        page =
            new QWebEnginePage(
                profile,
                browser
            );


        browser->setPage(
            page
        );


        // ----------------------------------------------------
        // WebEngine settings
        // ----------------------------------------------------

        QWebEngineSettings *settings =
            page->settings();


        settings->setAttribute(
            QWebEngineSettings::
                LocalStorageEnabled,
            true
        );


        settings->setAttribute(
            QWebEngineSettings::
                JavascriptEnabled,
            true
        );


        settings->setAttribute(
            QWebEngineSettings::
                JavascriptCanOpenWindows,
            true
        );


        // ----------------------------------------------------
        // Diagnostics
        // ----------------------------------------------------

        qDebug()
            << "[Storage] offTheRecord:"
            << profile->isOffTheRecord();

        qDebug()
            << "[Storage] storageName:"
            << profile->storageName();

        qDebug()
            << "[Storage] persistentStoragePath:"
            << profile->persistentStoragePath();

        qDebug()
            << "[Storage] cachePath:"
            << profile->cachePath();

        qDebug()
            << "[Storage] cookies policy:"
            << profile->persistentCookiesPolicy();

        qDebug()
            << "[Storage] permissions policy:"
            << profile->persistentPermissionsPolicy();
    }


    // ========================================================
    // Connections
    // ========================================================

    void setupConnections()
    {
        // ----------------------------------------------------
        // Load finished
        // ----------------------------------------------------

        connect(
            browser,
            &QWebEngineView::loadFinished,
            this,
            [this](bool success)
            {
                if (success) {

                    qDebug()
                        << "[WhatsApp]"
                        << "Page loaded successfully.";

                } else {

                    qWarning()
                        << "[WhatsApp]"
                        << "Page failed to load.";
                }
            }
        );


        // ----------------------------------------------------
        // Window title
        // ----------------------------------------------------

        connect(
            browser,
            &QWebEngineView::titleChanged,
            this,
            [this](const QString &title)
            {
                if (!title.isEmpty()) {

                    setWindowTitle(
                        title
                    );

                } else {

                    setWindowTitle(
                        "WhatsApp"
                    );
                }
            }
        );


        // ----------------------------------------------------
        // Permissions
        // ----------------------------------------------------

        connect(
            page,
            &QWebEnginePage::permissionRequested,
            this,
            [this](
                QWebEnginePermission permission)
            {
                handlePermission(
                    permission
                );
            }
        );


        // ----------------------------------------------------
        // Downloads
        // ----------------------------------------------------

        connect(
            profile,
            &QWebEngineProfile::
                downloadRequested,
            this,
            [this](
                QWebEngineDownloadRequest *download)
            {
                handleDownload(
                    download
                );
            }
        );
    }


    // ========================================================
    // WhatsApp URL
    // ========================================================

    void loadWhatsApp()
    {
        qDebug()
            << "[WhatsApp]"
            << "Loading persistent profile...";

        browser->setUrl(
            QUrl(
                "https://web.whatsapp.com/"
            )
        );
    }


    // ========================================================
    // Permissions
    // ========================================================

    void handlePermission(
        QWebEnginePermission permission)
    {
        using Type =
            QWebEnginePermission::
                PermissionType;


        const Type type =
            permission.permissionType();


        switch (type) {

        case Type::MediaAudioCapture:

        case Type::MediaVideoCapture:

        case Type::MediaAudioVideoCapture:

        case Type::Notifications:

            break;


        default:

            permission.deny();

            return;
        }


        const QString name =
            permissionName(
                type
            );


        QMessageBox box(
            this
        );


        box.setWindowTitle(
            "WhatsApp Permission"
        );


        box.setText(
            QString(
                "WhatsApp wants to access your %1."
            ).arg(
                name
            )
        );


        box.setInformativeText(
            "Do you want to allow this permission?"
        );


        box.setIcon(
            QMessageBox::Question
        );


        QPushButton *allow =
            box.addButton(
                "Allow",
                QMessageBox::AcceptRole
            );


        box.addButton(
            "Deny",
            QMessageBox::RejectRole
        );


        box.exec();


        if (box.clickedButton() ==
            allow) {

            permission.grant();

            qDebug()
                << "[Permission] Allowed:"
                << name;

        } else {

            permission.deny();

            qDebug()
                << "[Permission] Denied:"
                << name;
        }
    }


    // ========================================================
    // Safe filename
    // ========================================================

    QString safeFilename(
        QString filename)
    {
        filename =
            filename.trimmed();


        if (filename.isEmpty())
            filename = "download";


        // Prevent directory traversal.

        filename.replace(
            "/",
            "_"
        );

        filename.replace(
            "\\",
            "_"
        );


        QString cleaned;


        for (const QChar character :
             filename) {

            const ushort code =
                character.unicode();


            /*
             * Remove ASCII control characters.
             *
             * We intentionally don't use QChar::isControl()
             * because that API isn't available in the Qt
             * version we're compiling against.
             */

            if (code < 0x20 ||
                code == 0x7F) {

                continue;
            }


            cleaned += character;
        }


        if (cleaned.isEmpty())
            cleaned = "download";


        if (cleaned == "." ||
            cleaned == "..") {

            cleaned =
                "download";
        }


        return cleaned;
    }


    // ========================================================
    // Unique download filename
    // ========================================================

    QString uniqueDownloadPath(
        const QString &directory,
        const QString &filename)
    {
        QString path =
            directory +
            "/" +
            filename;


        if (!QFileInfo::exists(path))
            return path;


        QFileInfo info(
            path
        );


        const QString base =
            info.completeBaseName();


        const QString extension =
            info.suffix();


        for (int number = 1;
             number < 10000;
             ++number) {

            QString candidate;


            if (extension.isEmpty()) {

                candidate =
                    QString(
                        "%1 (%2)"
                    ).arg(
                        base
                    ).arg(
                        number
                    );

            } else {

                candidate =
                    QString(
                        "%1 (%2).%3"
                    ).arg(
                        base
                    ).arg(
                        number
                    ).arg(
                        extension
                    );
            }


            path =
                directory +
                "/" +
                candidate;


            if (!QFileInfo::exists(path))
                return path;
        }


        return directory +
               "/download";
    }


    // ========================================================
    // Downloads
    // ========================================================

    void handleDownload(
        QWebEngineDownloadRequest *download)
    {
        if (!download)
            return;


        QString downloadsPath =
            QStandardPaths::
                writableLocation(
                    QStandardPaths::
                        DownloadLocation
                );


        if (downloadsPath.isEmpty()) {

            downloadsPath =
                QDir::homePath() +
                "/Downloads";
        }


        QDir().mkpath(
            downloadsPath
        );


        QString filename =
            safeFilename(
                download->
                    downloadFileName()
            );


        const QString fullPath =
            uniqueDownloadPath(
                downloadsPath,
                filename
            );


        QFileInfo info(
            fullPath
        );


        download->setDownloadDirectory(
            info.absolutePath()
        );


        download->setDownloadFileName(
            info.fileName()
        );


        download->accept();


        qDebug()
            << "[Download] Started:"
            << fullPath;


        // ----------------------------------------------------
        // Download progress
        // ----------------------------------------------------

        connect(
            download,
            &QWebEngineDownloadRequest::
                receivedBytesChanged,
            this,
            [download]()
            {
                qDebug()
                    << "[Download]"
                    << download->receivedBytes()
                    << "/"
                    << download->totalBytes();
            }
        );


        // ----------------------------------------------------
        // Download completion
        // ----------------------------------------------------

        connect(
            download,
            &QWebEngineDownloadRequest::
                isFinishedChanged,
            this,
            [download]()
            {
                if (!download->isFinished())
                    return;


                const auto state =
                    download->state();


                if (
                    state ==
                    QWebEngineDownloadRequest::
                        DownloadCompleted
                ) {

                    qDebug()
                        << "[Download] Completed:"
                        << download->
                            downloadFileName();

                } else if (
                    state ==
                    QWebEngineDownloadRequest::
                        DownloadCancelled
                ) {

                    qWarning()
                        << "[Download] Cancelled:"
                        << download->
                            downloadFileName();

                } else {

                    qWarning()
                        << "[Download] Failed:"
                        << download->
                            downloadFileName();
                }
            }
        );
    }


    // ========================================================
    // Startup profiling
    // ========================================================

    void printStartupPerformance()
    {
        qDebug()
            << "[Performance]"
            << "Startup timer:"
            << startupTimer.elapsed()
            << "ms";
    }


    // ========================================================
    // Memory profiling
    // ========================================================

    void printMemoryUsage()
    {
#ifdef Q_OS_LINUX

        const long memoryKb =
            totalProcessTreeRssKb();


        qDebug()
            << "[Performance]"
            << "WhatsApp process tree RSS:"
            << formatMemory(
                memoryKb
            );

#endif
    }
};


// ============================================================
// Main
// ============================================================


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("WhatsApp");
    QApplication::setApplicationVersion("1.0");
    QApplication::setOrganizationName("Sahil");
    QApplication::setOrganizationDomain("local.whatsapp");

    // Icon embedded in the executable
    const QIcon appIcon(":/icons/whatsapp-cpp.png");
    app.setWindowIcon(appIcon);

    WhatsAppWindow window;
    window.setWindowIcon(appIcon);
    window.show();

    return app.exec();
}
