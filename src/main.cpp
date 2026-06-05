#include <csignal>
#include <iostream>
#include <string>
#include <vector>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"

#ifdef ENABLE_DDE_PLUGIN
#include <QCoreApplication>
#include "WallpaperEngine/DDE/DBusService.h"
#include "WallpaperEngine/DDE/WorkspaceManager.h"
#include "WallpaperEngine/DDE/MonitorTracker.h"
#endif

WallpaperEngine::Application::WallpaperApplication* app;

void signalhandler (const int sig) {
    if (app == nullptr) {
	return;
    }

    app->signal (sig);
}

void initLogging () {
    sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
    sLog.addError (new std::ostream (std::cerr.rdbuf ()));
}

#ifdef ENABLE_DDE_PLUGIN
int runDDEPlugin(int argc, char* argv[]) {
    // Filter out --dde-plugin from argv before passing to ApplicationContext
    std::vector<char*> filteredArgv;
    filteredArgv.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) != "--dde-plugin") {
            filteredArgv.push_back(argv[i]);
        }
    }
    int filteredArgc = static_cast<int>(filteredArgv.size());

    QCoreApplication qtApp(filteredArgc, filteredArgv.data());

    initLogging();
    sLog.out("Starting in DDE plugin mode");

    // Initialize config path
    std::filesystem::path configPath = std::filesystem::path(getenv("HOME")) / ".config" / "wallpaperengine" / "config.json";

    // Create core components
    WallpaperEngine::DDE::WorkspaceManager workspaceMgr(configPath);
    workspaceMgr.loadFromConfig();

    WallpaperEngine::DDE::MonitorTracker monitorTracker;
    monitorTracker.startTracking();

    // Register DBus service (no WallpaperApplication needed in DDE mode)
    WallpaperEngine::DDE::DBusService dbusService;
    dbusService.setWorkspaceManager(&workspaceMgr);
    dbusService.setMonitorTracker(&monitorTracker);

    if (!dbusService.registerService()) {
        sLog.error("Failed to register DBus service, exiting");
        delete app;
        return 1;
    }

    sLog.out("DDE plugin mode running, waiting for DBus calls...");

    int ret = qtApp.exec();

    return ret;
}
#endif

int main (int argc, char* argv[]) {
    try {
#ifdef ENABLE_DDE_PLUGIN
        // Check if DDE plugin mode is requested
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--dde-plugin") {
                return runDDEPlugin(argc, argv);
            }
        }
#endif

	// if type parameter is specified, this is a subprocess, so no logging should be enabled from our side
	bool enableLogging = true;
	const std::string typeZygote = "--type=zygote";
	const std::string typeUtility = "--type=utility";

	for (int i = 1; i < argc; i++) {
	    if (strncmp (typeZygote.c_str (), argv[i], typeZygote.size ()) == 0) {
		enableLogging = false;
		break;
	    }

	    if (strncmp (typeUtility.c_str (), argv[i], typeUtility.size ()) == 0) {
		enableLogging = false;
		break;
	    }
	}

	if (enableLogging) {
	    initLogging ();
	}

	WallpaperEngine::Application::ApplicationContext appContext (argc, argv);

	appContext.loadSettingsFromArgv ();

	app = new WallpaperEngine::Application::WallpaperApplication (appContext);

	// halt if the list-properties option was specified
	if (appContext.settings.general.onlyListProperties) {
	    delete app;
	    return 0;
	}

	// attach signals to gracefully stop
	std::signal (SIGINT, signalhandler);
	std::signal (SIGTERM, signalhandler);
	std::signal (SIGKILL, signalhandler);

	// show the wallpaper application
	app->show ();

	// remove signal handlers before destroying app
	std::signal (SIGINT, SIG_DFL);
	std::signal (SIGTERM, SIG_DFL);
	std::signal (SIGKILL, SIG_DFL);

	delete app;

	return 0;
    } catch (const std::exception& e) {
	std::cerr << e.what () << std::endl;
	return 1;
    }
}
