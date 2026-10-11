#include "ExternalBrowser.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>

namespace lr {
bool openBrowserUrl(const QUrl &url) {
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme()!="https" && url.scheme()!="http")) return false;
#ifdef Q_OS_LINUX
    if (QSysInfo::kernelVersion().contains("microsoft",Qt::CaseInsensitive)) {
        auto chrome=QStandardPaths::findExecutable("chrome.exe");
        if (chrome.isEmpty()) {
            for (const auto &path : QStringList{
                     "/mnt/c/Program Files/Google/Chrome/Application/chrome.exe",
                     "/mnt/c/Program Files (x86)/Google/Chrome/Application/chrome.exe"}) {
                if (QFileInfo(path).isFile()) { chrome=path; break; }
            }
        }
        // Invoke Chrome directly, without a shell or a separate browser profile.
        // WSL interop forwards the URL to Windows Chrome's normal instance.
        if (!chrome.isEmpty())
            return QProcess::startDetached(chrome,{url.toString(QUrl::FullyEncoded)});
    }
#endif
    return QDesktopServices::openUrl(url);
}
}
