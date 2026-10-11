#pragma once
#include <QTemporaryDir>
#include <memory>

namespace lr {
// Keep the returned directory alive until after QGuiApplication is destroyed.
std::unique_ptr<QTemporaryDir> prepareWslgRuntime(
    const QString &wslgRuntime = QStringLiteral("/mnt/wslg/runtime-dir"));
}
