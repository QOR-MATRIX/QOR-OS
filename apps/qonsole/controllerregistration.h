// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include <QtCore/QList>

namespace qor::qonsole {

struct ControllerRegistration
{
    quint16 page;
    quint16 usage;
    quint32 flags;
    quintptr target;
};

// A readback is evidence only if exactly one registration matches the requested
// usage, flags and non-null target. Other usages do not establish this request.
inline bool registrationMatchesRequest(const QList<ControllerRegistration> &devices,
                                       const ControllerRegistration &request, bool readbackSucceeded)
{
    if (!readbackSucceeded || !request.target)
        return false;
    int count = 0;
    bool matches = false;
    for (const auto &device : devices) {
        if (device.page == request.page && device.usage == request.usage) {
            ++count;
            matches = device.flags == request.flags && device.target == request.target;
        }
    }
    return count == 1 && matches;
}

} // namespace qor::qonsole
