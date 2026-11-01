// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// Whether Mesa's software renderer draws a window, by what the renderer says of itself, so that nothing set in the
// environment by mistake can ease a check on a GPU. With any other renderer, or none yet, the answer is no, and a case
// takes its strict path. What software alone no longer checks is listed in docs/CI.md.

#pragma once

#include <QQuickWindow>
#include <rhi/qrhi.h>

namespace qq {

inline bool drawnInSoftware(const QQuickWindow *window)
{
    const QRhi *rhi = window->rhi();
    return rhi && rhi->driverInfo().deviceName.contains("llvmpipe");
}

}  // namespace qq
