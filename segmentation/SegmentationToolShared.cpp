#include "SegmentationToolShared.h"

#include "KoResourcePaths.h"
#include <KSharedConfig.h>
#include <klocalizedstring.h>

#include <QCoreApplication>
#include <QMessageBox>
#include <QString>

#include <string>

namespace
{

bool openLibrary(QLibrary &library)
{
    library.setFileName("dlimgedit");
    if (library.load()) {
        using dlimgInitType = decltype(dlimg_init) *;
        dlimgInitType dlimgInit = reinterpret_cast<dlimgInitType>(library.resolve("dlimg_init"));
        dlimg::initialize(dlimgInit());
        return true;
    }
    return false;
}

}

QSharedPointer<SegmentationToolShared> SegmentationToolShared::create()
{
    QSharedPointer<SegmentationToolShared> result(new SegmentationToolShared());
    if (!result->m_cpu && !result->m_gpu) {
        return nullptr;
    }
    return result;
}

SegmentationToolShared::SegmentationToolShared()
{
    if (!openLibrary(m_lib)) {
        QString path = QCoreApplication::applicationDirPath(); // TODO: linux
        QString library = "dlimgedit.dll"; // TODO: linux
        QMessageBox::critical(
            nullptr,
            i18nc("@title:window", "Krita"),
            i18n("Failed to load library '", library, "' from path", path, "\n", m_lib.errorString()));
        return;
    }

    m_config = KSharedConfig::openConfig()->group("SegmentationToolPlugin");
    QString backendString = m_config.readEntry("backend", "cpu");
    dlimg::Backend backend = backendString == "gpu" ? dlimg::Backend::gpu : dlimg::Backend::cpu;
    if (backend == dlimg::Backend::gpu && !dlimg::Environment::is_supported(backend)) {
        backend = dlimg::Backend::cpu;
    }

    QString err = initialize(backend);
    if (!err.isEmpty()) {
        QMessageBox::critical(nullptr,
                              i18nc("@title:window", "Krita"),
                              i18n("Failed to initialize segmentation tool plugin.\n", err));
        return;
    }
}

QString SegmentationToolShared::initialize(dlimg::Backend backend)
{
    std::string modelDir = QString(KoResourcePaths::getApplicationRoot() + "/share/krita/ai_models").toStdString();
    dlimg::Environment &env = backend == dlimg::Backend::gpu ? m_gpu : m_cpu;
    dlimg::Options opts;
    opts.model_directory = modelDir.c_str();
    opts.backend = backend;
    try {
        env = dlimg::Environment(opts);
    } catch (const std::exception &e) {
        return QString(e.what());
    }
    m_backend = backend;
    m_config.writeEntry("backend", backend == dlimg::Backend::gpu ? "gpu" : "cpu");
    return QString();
}

dlimg::Environment const &SegmentationToolShared::environment() const
{
    return m_backend == dlimg::Backend::gpu ? m_gpu : m_cpu;
}

bool SegmentationToolShared::setBackend(dlimg::Backend backend)
{
    if (backend == m_backend) {
        return true;
    }
    QString err = initialize(backend);
    if (!err.isEmpty()) {
        QMessageBox::warning(nullptr,
                             i18nc("@title:window", "Krita"),
                             i18n("Error while trying to switch segmentation backend.\n", err));
        return false;
    }
    Q_EMIT backendChanged(m_backend);
    return true;
}
