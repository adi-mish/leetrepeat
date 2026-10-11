#include "RuntimeEnvironment.h"
#include <QDebug>
#include <QDir>
#include <QFile>

#ifdef Q_OS_LINUX
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lr {
std::unique_ptr<QTemporaryDir> prepareWslgRuntime(const QString &wslgRuntime) {
#ifdef Q_OS_LINUX
    if (!QDir(wslgRuntime).exists()) return {};
    const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    struct stat info {};
    const auto runtimeBytes = QFile::encodeName(runtime);
    if (QDir::isAbsolutePath(runtime)
        && ::stat(runtimeBytes.constData(), &info) == 0 && S_ISDIR(info.st_mode)
        && info.st_uid == ::geteuid() && (info.st_mode & 0777) == 0700
        && ::access(runtimeBytes.constData(), W_OK | X_OK) == 0)
        return {};

    // Resolve the existing socket before changing its relative lookup directory.
    // WSLg can expose its socket even when the session points at a missing /run/user.
    const auto display = qEnvironmentVariable("WAYLAND_DISPLAY", "wayland-0");
    auto socket = display;
    const auto isSocket = [](const QString &path) {
        struct stat socketInfo {};
        return ::stat(QFile::encodeName(path).constData(), &socketInfo) == 0
            && S_ISSOCK(socketInfo.st_mode);
    };
    if (QDir::isRelativePath(display)) {
        socket = QDir(runtime).absoluteFilePath(display);
        if (runtime.isEmpty() || !isSocket(socket))
            socket = QDir(wslgRuntime).absoluteFilePath(display);
    }
    if (!isSocket(socket)) return {};

    auto fallback = std::make_unique<QTemporaryDir>(
        QDir::tempPath() + "/leetrepeat-runtime-XXXXXX");
    if (!fallback->isValid()) {
        qWarning() << "Cannot create LeetRepeat runtime directory:" << fallback->errorString();
        return {};
    }
    // QTemporaryDir creates a private 0700 directory. Do not chmod WSLg's shared
    // directory or require root access to repair /run/user.
    qputenv("XDG_RUNTIME_DIR", QFile::encodeName(fallback->path()));
    qputenv("WAYLAND_DISPLAY", QFile::encodeName(socket));
    return fallback;
#else
    Q_UNUSED(wslgRuntime);
    return {};
#endif
}
}
