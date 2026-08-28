/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include <QApplication>
#include <QStyleFactory>
#include <QLocalSocket>
#include <QLocalServer>
#include "include/Browser.h"

int main(int argc, char *argv[]) {

    // REQUIRED ON WINDOWS: Ensures shared OpenGL contexts between the UI and QtWebEngine (Chromium render processes).
    // Without this, the application immediately throws an Access Violation (0xc0000005) on Windows display drivers.
    #ifdef Q_OS_WIN
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    #endif
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // Hardening Chromium for privacy and performance
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
        // "--disable-frame-rate-limit " // Removes chromium FPS limit (Chromium default: 60)
        "--disable-dns-prefetch " // Prevents DNS lookups before clicking links
        "--disable-features=WebRtcHideLocalIpsWithMdns," // Disables mDNS to prevent local IP leakage
            "UserAgentClientHint," // Disables Client Hints to reduce fingerprinting
            "UserAgentClientHintFullVersionList," // Hides detailed browser version from sites
            "UserAgentClientHintReduced," // Standardizes User-Agent strings
            "ClientHints," // Disables modern browser-info requests
            "NetworkPrediction," // Disables pre-fetching of web content
            "PrivacySandboxSettings," // Disables Google's "Privacy Sandbox" tracking
            "TopicsAPI," // Blocks ad-tracking interest categorization
            "Fledge," // Blocks remarketing ad-tracking (TURTLEDOVE)
            "AttributionReporting," // Disables ad-conversion measurement API
            "OmniboxSuggestions," // Disables search suggestions from remote servers
            "TranslateUI," // Disables integrated Google Translate
            "AutofillServerCommunication," // Stops sending form data to Google
            "BackgroundFetch," // Blocks background downloads
            "BackgroundTimerThrottling," // Saves CPU by slowing background tabs
            "Reporting," // Disables enterprise/diagnostic reporting
            "NetworkErrorLogging " // Blocks network status reporting to servers
        "--force-webrtc-ip-handling-policy=disable_non_proxied_udp " // Forces WebRTC to use proxy (avoids IP leak)
        "--disable-device-discovery-notifications " // Prevents Chromium from showing notifications about discovered devices
        "--disable-background-networking " // Stops Google services from talking in background
        "--disable-domain-reliability " // Disables Google's error reporting service
        "--disable-client-side-phishing-detection " // Stops sending URLs to Google for scanning
        "--metrics-recording-only "// Prevents sending telemetry data to Google
        "--remove-cross-origin-referrers " // Enhances privacy by stripping referrer headers
        "--disable-breakpad " // Disables crash reporting
        "--disable-sync " // Disables Google Account synchronization
        "--disable-smooth-scrolling " // Reduces resource usage/latency
        "--disable-background-sync " // Stops background data syncing
        "--disable-service-worker-background-sync " // Disables Service Worker sync in background
        "--enable-features=StrictOriginIsolation " // Better security by isolating sites in processes
        "--block-insecure-private-network-requests " // Prevents public sites from attacking local network
        "--no-pings " // Blocks <a ping> tracking notifications
        "--force-color-profile=srgb " // Standardizes color to prevent GPU fingerprinting
        "--enable-zero-copy " // Optimizes memory by reducing texture copying
        "--disable-extensions " // Disables all browser extensions
        "--disable-device-info " // Hides hardware specifics from websites
        "--site-per-process "); // Enforces strict sandbox isolation per site

    QApplication app(argc, argv);

    // Set application identity
    app.setOrganizationName("EPLS");
    app.setApplicationName("NullA Browser");
    app.setOrganizationDomain("epls.itch.io");
    app.setStyle(QStyleFactory::create("Fusion"));

    QString serverName = "NullA_Browser_Server";
    QString startUrl = (argc > 1) ? QString::fromLocal8Bit(argv[1]) : "";

    // Single Instance Logic: Check if the browser is already running
    QLocalSocket socket;
    socket.connectToServer(serverName);

    if (socket.waitForConnected(500)) {
        if (!startUrl.isEmpty()) {
            socket.write(startUrl.toUtf8());
            socket.waitForBytesWritten();
        }
        return 0; // Exit current instance and pass URL to existing one
    }

    // Launch main browser window
    Browser b(startUrl);

    // Setup Local Server to handle URLs from secondary instances
    QLocalServer server;
    if (server.listen(serverName)) {
        QObject::connect(&server, &QLocalServer::newConnection, [&]() {
            QLocalSocket *clientSocket = server.nextPendingConnection();
            QObject::connect(clientSocket, &QLocalSocket::readyRead, [&, clientSocket]() {
                QString urlFromOtherInstance = QString::fromUtf8(clientSocket->readAll());
                b.openUrlInNewTab(urlFromOtherInstance);
                b.raise(); // Bring window to front
                b.activateWindow(); // Focus window
                clientSocket->deleteLater();
            });
        });
    }

    b.show();
    return app.exec();
}
