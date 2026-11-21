// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once
#include <QtCore/QString>

class QGuiApplication;
class QQmlApplicationEngine;

// Linked only when QOR_QONSOLE_VERIFICATION is explicitly enabled.
bool startQonsoleVerification(QGuiApplication &app, QQmlApplicationEngine &engine, const QString &outputDirectory);
