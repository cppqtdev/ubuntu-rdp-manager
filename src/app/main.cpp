// Ubuntu RDP Manager — unprivileged Qt Quick front end.
#include "ApplicationController.h"
#include "DBusTypes.h"
#include "ProcessRunner.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>

#include <cstdio>
#include <unistd.h>

namespace {
int g_qmlWarnings = 0;
QtMessageHandler g_previousHandler = nullptr;

void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const bool fromQml = (ctx.file && QByteArray(ctx.file).startsWith("qrc:/")) || msg.contains(QLatin1String("qrc:/"))
                         || msg.contains(QLatin1String(".qml:"));
    if (type != QtDebugMsg && type != QtInfoMsg && fromQml)
        ++g_qmlWarnings; // runtime QML warnings/errors are counted for --smoke-test
    if (g_previousHandler)
        g_previousHandler(type, ctx, msg);
}
} // namespace

// Qt 6.8 cannot switch light/dark at runtime on Linux (QStyleHints::setColorScheme is honoured
// only on Windows/macOS), and FluentWinUI3 picks its light assets only when the platform reports
// Qt::ColorScheme::Light. The gtk3 platform theme reports what GTK_THEME says, so the user's
// choice is applied through the environment before QGuiApplication starts (verified 2026-10-24).
static QString configureColorScheme(int argc, char *argv[])
{
    QString mode = QSettings(QStringLiteral("ubuntu-rdp-manager"), QStringLiteral("ubuntu-rdp-manager"))
                       .value(QStringLiteral("ui/theme"), QStringLiteral("system")).toString();
    for (int i = 1; i + 1 < argc; ++i)
        if (qstrcmp(argv[i], "--theme") == 0)
            mode = QString::fromLocal8Bit(argv[i + 1]);
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME"))
        qputenv("QT_QPA_PLATFORMTHEME", "gtk3");
    // Only the X11 (xcb) platform plugin is bundled; on Wayland desktops it runs through XWayland.
    // Choosing it explicitly avoids Qt first trying, and warning about, the missing wayland plugin.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");
    if (mode == QLatin1String("dark"))
        qputenv("GTK_THEME", "Adwaita:dark");
    else if (mode == QLatin1String("light"))
        qputenv("GTK_THEME", "Adwaita");
    return mode;
}

int main(int argc, char *argv[])
{
    if (::geteuid() == 0 && !qEnvironmentVariableIsSet("URDM_ALLOW_ROOT_GUI")) {
        std::fprintf(stderr, "Ubuntu RDP Manager must not run as root. Start it as a normal user; "
                             "administrative actions are authorized individually through polkit.\n");
        return 1;
    }
    QGuiApplication::setOrganizationName(QStringLiteral("ubuntu-rdp-manager"));
    QGuiApplication::setApplicationName(QStringLiteral("ubuntu-rdp-manager"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Ubuntu RDP Manager"));
    QGuiApplication::setApplicationVersion(QStringLiteral(URDM_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("ubuntu-rdp-manager"));
    QQuickStyle::setStyle(QStringLiteral("FluentWinUI3"));
    const QString startupTheme = configureColorScheme(argc, argv);

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/UbuntuRdpManager/resources/icons/ubuntu-rdp-manager.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Manage the xrdp remote desktop server"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption screenshot(QStringLiteral("screenshot"), QStringLiteral("Save a screenshot of the window and exit."), QStringLiteral("file"));
    QCommandLineOption page(QStringLiteral("page"), QStringLiteral("Open the given page index."), QStringLiteral("index"));
    QCommandLineOption theme(QStringLiteral("theme"), QStringLiteral("system, light or dark."), QStringLiteral("mode"));
    QCommandLineOption size(QStringLiteral("size"), QStringLiteral("Window size WxH."), QStringLiteral("size"));
    QCommandLineOption delay(QStringLiteral("delay"), QStringLiteral("Milliseconds before the screenshot."), QStringLiteral("ms"), QStringLiteral("3500"));
    QCommandLineOption smoke(QStringLiteral("smoke-test"), QStringLiteral("Load every page, report QML warnings and exit."));
    parser.addOptions({screenshot, page, theme, size, delay, smoke});
    parser.process(app);

    urdm::registerDBusTypes();
    g_previousHandler = qInstallMessageHandler(messageHandler);

    urdm::app::ApplicationController controller(startupTheme);
    urdm::app::ApplicationController::setInstance(&controller);
    if (parser.isSet(page))
        controller.setCurrentPage(parser.value(page).toInt());

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(2); },
                     Qt::QueuedConnection);
    // Predictable shutdown: no child process may outlive (or be destroyed under) the application.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() { urdm::app::ProcessRunner::shutdown(); });
    engine.loadFromModule("UbuntuRdpManager", "Main");
    if (engine.rootObjects().isEmpty()) {
        urdm::app::ProcessRunner::shutdown();
        return 2;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());

    if (parser.isSet(size) && window) {
        const QStringList wh = parser.value(size).split(QLatin1Char('x'));
        if (wh.size() == 2)
            window->resize(wh.at(0).toInt(), wh.at(1).toInt());
    }

    if (parser.isSet(screenshot) && window) {
        const QString file = parser.value(screenshot);
        QTimer::singleShot(parser.value(delay).toInt(), &app, [window, file]() {
            const bool ok = window->grabWindow().save(file);
            std::fprintf(stderr, "screenshot %s: %s\n", qPrintable(file), ok ? "saved" : "FAILED");
            QCoreApplication::exit(ok ? 0 : 3);
        });
    }

    if (parser.isSet(smoke)) {
        // Visit every page so lazily loaded screens are instantiated, then report.
        auto *timer = new QTimer(&app);
        auto pageIndex = std::make_shared<int>(0);
        QObject::connect(timer, &QTimer::timeout, &app, [&controller, timer, pageIndex]() {
            if (*pageIndex < 11) {
                controller.setCurrentPage((*pageIndex)++);
                return;
            }
            timer->stop();
            std::fprintf(stderr, "smoke-test: %d QML warnings/errors\n", g_qmlWarnings);
            QCoreApplication::exit(g_qmlWarnings == 0 ? 0 : 4);
        });
        timer->start(700);
    }
    return QGuiApplication::exec();
}
