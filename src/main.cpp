#define _CRT_SECURE_NO_WARNINGS
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>
#include <iostream>
#include <fstream>
#include <csignal>
#include <thread>
#include <filesystem>
#include <json/json.h>

// Ensure controllers are linked
#include "api/controllers/HealthController.h"
#include "api/controllers/UserController.h"
#include "api/controllers/MetricsController.h"
#include "api/otel/OtelManager.h"

void handleSignal(int signum) {
    LOG_INFO << "Received signal " << signum << ". Initiating graceful shutdown...";
    api::otel::OtelManager::instance().shutdown();
    drogon::app().quit();
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    std::cout << "=========================================================\n"
              << "            Backend Server (Drogon + RapidJSON)          \n"
              << "=========================================================\n";

    // Config path: defaults to config.json or path provided as first argument
    std::string configPath = "config.json";
    if (argc > 1 && argv[1][0] != '-') {
        configPath = argv[1];
    }

    // Resolve relative path if run from build directory
    if (!std::filesystem::exists(configPath)) {
        if (std::filesystem::exists("../config.json")) {
            configPath = "../config.json";
        } else if (std::filesystem::exists("../../config.json")) {
            configPath = "../../config.json";
        }
    }

    // Enforce single source of truth: config.json must exist. No fallback permitted.
    if (!std::filesystem::exists(configPath)) {
        std::cerr << "\n=========================================================\n"
                  << " [FATAL ERROR] Configuration file not found!\n"
                  << " Searched: " << std::filesystem::absolute(configPath).string() << "\n\n"
                  << " Single source of truth requires 'config.json' to exist.\n"
                  << " No hardcoded fallbacks are permitted.\n"
                  << " Please ensure 'config.json' is present before starting.\n"
                  << "=========================================================\n\n";
        return 1;
    }

    // Load entire server and database configuration from config.json
    try {
        LOG_INFO << "Loading configuration from: " << std::filesystem::absolute(configPath).string();
        drogon::app().loadConfigFile(configPath);
    } catch (const std::exception& e) {
        std::cerr << "\n[FATAL ERROR] Failed to parse '" << configPath << "': " << e.what() << "\n\n";
        return 1;
    }

    // Parse config root for OpenTelemetry initialization
    try {
        std::ifstream configFile(configPath);
        Json::Value configRoot;
        Json::CharReaderBuilder readerBuilder;
        std::string errs;
        if (Json::parseFromStream(readerBuilder, configFile, &configRoot, &errs)) {
            api::otel::OtelManager::instance().initialize(configRoot);
            api::otel::OtelManager::instance().setupDrogonHooks();
        }
    } catch (const std::exception& e) {
        LOG_WARN << "Failed to parse OpenTelemetry section from config: " << e.what();
    }

    unsigned int hardwareCores = std::thread::hardware_concurrency();
    size_t configThreads = drogon::app().getThreadNum();
    size_t activeThreads = (configThreads == 0) ? hardwareCores : configThreads;

    std::cout << "[INFO] Configuration source:              " << std::filesystem::absolute(configPath).string() << "\n";
    std::cout << "[INFO] CPU hardware concurrency detected: " << hardwareCores << " cores\n";
    std::cout << "[INFO] Worker threads (from config.json): " << activeThreads << " threads\n";
    std::cout << "[INFO] Drogon HTTP Server starting at http://0.0.0.0:8080\n";
    std::cout << "[INFO] Observability endpoints:\n";
    std::cout << "  - GET    /api/v1/metrics (Prometheus scrape)\n";
    std::cout << "  - OTLP   http://127.0.0.1:4318 (Traces, Metrics, Logs)\n";
    std::cout << "[INFO] Available endpoints:\n";
    std::cout << "  - GET    /api/v1/health\n";
    std::cout << "  - GET    /api/v1/ping\n";
    std::cout << "  - GET    /api/v1/users?limit=50&offset=0\n";
    std::cout << "  - GET    /api/v1/users/{id}\n";
    std::cout << "  - GET    /api/v1/users/availability/{username}\n";
    std::cout << "  - GET    /api/v1/users/check-username?username={name}\n";
    std::cout << "  - POST   /api/v1/users\n";
    std::cout << "  - PUT    /api/v1/users/{id}\n";
    std::cout << "  - DELETE /api/v1/users/{id}\n";
    std::cout << "=========================================================\n";

    drogon::app().run();

    api::otel::OtelManager::instance().shutdown();

    std::cout << "[INFO] Server stopped gracefully.\n";
    return 0;
}