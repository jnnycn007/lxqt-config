/*
    Copyright (C) 2013-2014  Hong Jen Yee (PCMan) <pcman.tw@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <LXQt/SingleApplication>
#include <LXQt/ConfigDialog>
#include <LXQt/ConfigDialogCmdLineOptions>
#include <LXQt/Settings>
#include <QCommandLineParser>
#include <QMessageBox>
#include "mouseconfig.h"
#include "keyboardconfig.h"
#include "../liblxqt-config-cursor/selectwnd.h"
#include "../lxqt-config-appearance/configothertoolkits.h"
#include "keyboardlayoutconfig.h"

#ifdef WITH_TOUCHPAD
#include "touchpadconfig.h"
#include "touchpaddevice.h"
#endif

int main(int argc, char** argv) {
    LXQt::SingleApplication app(argc, argv);

    QString waylandMsg = QObject::tr("LXQt input settings are currently unsupported under Wayland.\n\nMouse, touchpad and keyboard can be configured in the settings of the compositor.");

    QCommandLineParser parser;
    LXQt::ConfigDialogCmdLineOptions dlgOptions;
    parser.setApplicationDescription(QStringLiteral("LXQt Config Input"));
    const QString VERINFO = QStringLiteral(LXQT_CONFIG_VERSION
                                           "\n\nliblxqt:   " LXQT_VERSION
                                           "\nQt:        " QT_VERSION_STR);
    app.setApplicationVersion(VERINFO);

    dlgOptions.setCommandLine(&parser);
#ifdef WITH_TOUCHPAD
    QCommandLineOption loadOption(QStringLiteral("load-touchpad"),
            app.tr("Load last touchpad settings."));
    parser.addOption(loadOption);
#endif
    parser.addVersionOption();
    parser.addHelpOption();
    parser.process(app);
    dlgOptions.process(parser);

    QByteArray configName = qgetenv("LXQT_SESSION_CONFIG");
    if(configName.isEmpty())
      configName = "session";
    LXQt::Settings settings(QString::fromUtf8(configName));

#ifdef WITH_TOUCHPAD
    bool loadLastTouchpadSettings = parser.isSet(loadOption);
    if (loadLastTouchpadSettings) {
        if (QGuiApplication::platformName() == QLatin1String("wayland"))
            qDebug().noquote() << waylandMsg; // Under wayland, show a debug message and exit.
        else
            TouchpadDevice::loadSettings(&settings);
        return 0;
    }
#endif

    LXQt::ConfigDialog dlg(QObject::tr("Keyboard and Mouse Settings"), &settings);
    dlg.setButtons(QDialogButtonBox::Apply|QDialogButtonBox::Close|QDialogButtonBox::Reset);
    dlg.enableButton(QDialogButtonBox::Apply, false); // disable Apply button in the beginning
    app.setActivationWindow(&dlg);

    LXQt::Settings qtSettings(QStringLiteral("lxqt"));

    LXQt::Settings configAppearanceSettings(QStringLiteral("lxqt-config-appearance"));
    ConfigOtherToolKits *configOtherToolKits = nullptr;
    bool controlGTK(configAppearanceSettings.value(QStringLiteral("ControlGTKThemeEnabled")).toBool());
    if (controlGTK)
    {
        configOtherToolKits = new ConfigOtherToolKits(&qtSettings, &configAppearanceSettings, &dlg);
        configOtherToolKits->startXsettingsd();
    }

    /*** Mouse Config ***/
    MouseConfig* mouseConfig = new MouseConfig(&settings, &qtSettings, &dlg);
    dlg.addPage(mouseConfig, QObject::tr("Mouse"), QStringLiteral("input-mouse"));
    QObject::connect(&dlg, &LXQt::ConfigDialog::reset, mouseConfig, &MouseConfig::reset);
    QObject::connect(mouseConfig, &MouseConfig::settingsChanged, &dlg, [&dlg] {
        dlg.enableButton(QDialogButtonBox::Apply, true); // enable Apply button when something is changed
    });

    /*** Cursor Theme ***/
    SelectWnd* cursorConfig = new SelectWnd(&settings, &dlg);
    cursorConfig->setCurrent();
    dlg.addPage(cursorConfig, QObject::tr("Cursor"), QStringLiteral("preferences-desktop-theme"));
    QObject::connect(cursorConfig, &SelectWnd::settingsChanged, &dlg, [&dlg] {
        dlg.enableButton(QDialogButtonBox::Apply, true);
    });
    if (!controlGTK)
        cursorConfig->showExtraInfo(QStringLiteral("\n") + QObject::tr("To apply the changes also to GTK, check LXQt Appearance Configuration → GTK Style."));

    /*** Keyboard Config ***/
    KeyboardConfig* keyboardConfig = new KeyboardConfig(&settings, &qtSettings, &dlg);
    dlg.addPage(keyboardConfig, QObject::tr("Keyboard"), QStringLiteral("input-keyboard"));
    QObject::connect(&dlg, &LXQt::ConfigDialog::reset, keyboardConfig, &KeyboardConfig::reset);
    QObject::connect(keyboardConfig, &KeyboardConfig::settingsChanged, &dlg, [&dlg] {
        dlg.enableButton(QDialogButtonBox::Apply, true);
    });

    /*** Keyboard Layout ***/
    KeyboardLayoutConfig* keyboardLayoutConfig = nullptr;
    if (QGuiApplication::platformName() == QLatin1String("xcb"))
    {
        keyboardLayoutConfig = new KeyboardLayoutConfig(&settings, &dlg);
        dlg.addPage(keyboardLayoutConfig, QObject::tr("Keyboard Layout"), QStringLiteral("input-keyboard"));
        QObject::connect(&dlg, &LXQt::ConfigDialog::reset, keyboardLayoutConfig, &KeyboardLayoutConfig::reset);
        QObject::connect(keyboardLayoutConfig, &KeyboardLayoutConfig::settingsChanged, &dlg, [&dlg] {
            dlg.enableButton(QDialogButtonBox::Apply, true);
        });
    }

#ifdef WITH_TOUCHPAD
    TouchpadConfig* touchpadConfig = nullptr;
    if (QGuiApplication::platformName() == QLatin1String("xcb"))
    {
        /*** Touchpad Config ***/
        touchpadConfig = new TouchpadConfig(&settings, &dlg);
        dlg.addPage(touchpadConfig, QObject::tr("Mouse and Touchpad"), QStringLiteral("input-tablet"));
        QObject::connect(&dlg, &LXQt::ConfigDialog::reset, touchpadConfig, &TouchpadConfig::reset);
        QObject::connect(touchpadConfig, &TouchpadConfig::settingsChanged, &dlg, [&dlg] {
            dlg.enableButton(QDialogButtonBox::Apply, true);
        });
    }
#endif

    // apply all changes on clicking Apply
    QObject::connect(&dlg, &LXQt::ConfigDialog::clicked, [=, &dlg] (QDialogButtonBox::StandardButton btn) {
        if (btn == QDialogButtonBox::Apply)
        {
            mouseConfig->applyConfig();
            cursorConfig->applyCusorTheme();
            if (configOtherToolKits)
            {
                // GTK3
                configOtherToolKits->setGTKConfig(QStringLiteral("3.0"));
                if(QGuiApplication::platformName() == QStringLiteral("wayland"))
                    configOtherToolKits->setGsettingsConfig(QStringLiteral("3.0"));
                // GTK2
                configOtherToolKits->setGTKConfig(QStringLiteral("2.0"));
                // Update xsettingsd
                configOtherToolKits->setXSettingsConfig();
            }
            keyboardConfig->applyConfig();
            if (keyboardLayoutConfig)
                keyboardLayoutConfig->applyConfig();
#ifdef WITH_TOUCHPAD
            if (touchpadConfig)
                touchpadConfig->applyConfig();
#endif
            // disable Apply button after changes are applied
            dlg.enableButton(btn, false);
        }
        else if (btn == QDialogButtonBox::Reset)
            dlg.enableButton(QDialogButtonBox::Apply, false); // disable Apply button on resetting too
    });

    dlg.setWindowIcon(QIcon::fromTheme(QStringLiteral("input-keyboard")));

    LXQt::Settings mConfigInputSettings(QStringLiteral("lxqt-config-input"));
    dlg.resize(mConfigInputSettings.value(QStringLiteral("size")).toSize().expandedTo(QSize(600, 400)));

    const QString initialPage = dlgOptions.page();
    if (!initialPage.isEmpty())
        dlg.showPage(initialPage);

    dlg.exec();
    mConfigInputSettings.setValue(QStringLiteral("size"), dlg.size());
    return 0;
}
