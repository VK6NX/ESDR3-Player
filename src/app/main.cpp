// SPDX-License-Identifier: GPL-3.0-or-later
// (c)VK6NX 2026
// vk6nx.net
#include "app/PlayerController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ESDR3_Player"));
    app.setOrganizationName(QStringLiteral("VK6NX"));
    app.setOrganizationDomain(QStringLiteral("vk6nx.net"));
    app.setApplicationVersion(QStringLiteral(ESDR3_VERSION));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    esdr3::PlayerController player(nullptr);
    esdr3::PlayerController::setInstance(&player);

    QQmlApplicationEngine engine;
    QObject::connect(&player, &esdr3::PlayerController::languageChanged, &engine,
                     [&engine] { engine.retranslate(); });

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("ESDR3Player", "Main");

    const QStringList args = app.arguments();
    QString file;
    bool autoPlay = false;
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == QLatin1String("--play")) autoPlay = true;
        else if (args.at(i) == QLatin1String("--mute")) player.setMute(true);
        else if (args.at(i) == QLatin1String("--lang") && i + 1 < args.size()) player.setLanguage(args.at(++i));
        else if (!args.at(i).startsWith(QLatin1Char('-'))) file = args.at(i);
    }
    if (!file.isEmpty()) {
        player.openPath(file);
        if (autoPlay) QTimer::singleShot(300, &player, [&player] { player.play(); });
    }

    const QString shot = qEnvironmentVariable("ESDR3_SCREENSHOT");
    if (!shot.isEmpty()) {
        QTimer::singleShot(5000, &app, [&engine, shot] {
            for (QObject* obj : engine.rootObjects())
                if (auto* win = qobject_cast<QQuickWindow*>(obj)) {
                    win->grabWindow().save(shot);
                    break;
                }
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
