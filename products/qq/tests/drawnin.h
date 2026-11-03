// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// Whether a suite takes its path for frames drawn in software. Two things must agree: the renderer says it is Mesa's software
// renderer, and tools/dev/products.sh, which always sets QOR_EXPECTED_RENDERER, expects software on this machine. If they
// disagree the suite stops, whichever way round: a GPU machine that falls back on software (GALLIUM_DRIVER=llvmpipe,
// LIBGL_ALWAYS_SOFTWARE=1, a render node Mesa cannot use) is not eased, and a machine with no GPU device that draws with some
// other renderer is not called software. With no expectation, as when a suite is run by hand or on Windows, or with no renderer
// yet, the answer is no, and the case takes its strict path. So only a person who sets both by hand, outside the script, is
// eased on a machine with a GPU, and the suite says what drew it. What software alone no longer checks is in docs/CI.md.

#pragma once

#include <QByteArray>
#include <QQuickWindow>
#include <QtGlobal>
#include <rhi/qrhi.h>

namespace qq {

inline bool drawnInSoftware(const QQuickWindow *window)
{
    const QRhi *rhi = window->rhi();
    if (!rhi)
        return false;
    const QByteArray renderer = rhi->driverInfo().deviceName;
    const QByteArray expected = qgetenv("QOR_EXPECTED_RENDERER");
    const bool software = renderer.contains("llvmpipe");
    static bool said = false;
    if (!said) {
        said = true;
        qInfo("Drawn by %s; expected: %s", renderer.constData(), expected.isEmpty() ? "nothing said" : expected.constData());
    }
    if ((expected == "software" && !software) || (expected == "gpu" && software))
        qFatal("The renderer is %s, and products.sh expected %s here: this run's checks would not be what it says they are.",
               renderer.constData(), expected.constData());
    return software && expected == "software";
}

}  // namespace qq
