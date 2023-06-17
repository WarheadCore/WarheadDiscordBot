/*
 * This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Config.h"
#include "Timer.h"
#include "Log.h"
#include "SignalHandlerMgr.h"
#include "BotMgr.h"
#include "IoContextMgr.h"
#include "OpenSSLCrypto.h"
#include "DiscordMgr.h"
#include "DatabaseMgr.h"
#include "DatabaseEnv.h"
#include "ThreadPool.h"
#include <boost/version.hpp>
#include <filesystem>
#include <thread>
#include <openssl/crypto.h>
#include <openssl/opensslv.h>

namespace fs = std::filesystem;
constexpr auto WARHEAD_SERVER_CONFIG = "DiscordBot.conf";

bool StartDB();
void ServerUpdateLoop();

/// Launch the server
int main()
{
    // Set signal handlers
    sSignalMgr->Initialize([]()
    {
        BotMgr::StopNow();
    });

    // Command line parsing
    auto configFile = fs::path(sConfigMgr->GetConfigPath() + WARHEAD_SERVER_CONFIG);

    // Add file and args in config
    sConfigMgr->Configure(configFile.generic_string());
    if (!sConfigMgr->LoadAppConfigs())
        return 1;

    // Init logging
    sLog->Initialize();

    // Initialize the random number generator
    srand((unsigned int)GetEpochTime().count());

    OpenSSLCrypto::threadsSetup();

    LOG_INFO("core", "> Using configuration file:       {}", sConfigMgr->GetFilename());
    LOG_INFO("core", "> Using logs directory:           {}", sLog->GetLogsDir());
    LOG_INFO("core", "> Using SSL version:              {} (library: {})", OPENSSL_VERSION_TEXT, OpenSSL_version(OPENSSL_VERSION));
    LOG_INFO("core", "> Using Boost version:            {}.{}.{}", BOOST_VERSION / 100000, BOOST_VERSION / 100 % 1000, BOOST_VERSION % 100);
    LOG_INFO("core", "> Using DB client version:        {}", sDatabaseMgr->GetClientInfo());
    LOG_INFO("core", "> Using DB server version:        {}", sDatabaseMgr->GetServerVersion());

    // Initialize the database connection
    if (!StartDB())
        return 1;

    std::shared_ptr<void> dbHandle(nullptr, [](void*) { sDatabaseMgr->CloseAllConnections(); });

    auto threadPool = std::make_unique<Warhead::ThreadPool>(1);
    threadPool->PostWork([]() { sIoContextMgr->Run(); });

    // Load discord config
    sDiscordMgr->LoadConfig(false);

    // Start discord
    sDiscordMgr->Start();

    ServerUpdateLoop();

    LOG_INFO("server", "Halting process...");
    return 0;
}

/// Initialize connection to the database
bool StartDB()
{
    sDatabaseMgr->AddDatabase(DiscordDatabase, "Discord");
    if (!sDatabaseMgr->Load())
        return false;

    LOG_INFO("server", "Started discord database connection pool.");
    LOG_INFO("server", "");
    return true;
}

void ServerUpdateLoop()
{
    auto realCurrTime{ 0ms };
    auto realPrevTime = GetTimeMS();

    // While we have not ServerMgr::_stopEvent, update the server
    while (!BotMgr::IsStopped())
    {
        realCurrTime = GetTimeMS();

        auto diff = GetMSTimeDiff(realPrevTime, realCurrTime);
        if (diff == 0ms)
        {
            // sleep until enough time passes that we can update all timers
            std::this_thread::sleep_for(1ms);
            continue;
        }

        sBotMgr->Update(diff);
        realPrevTime = realCurrTime;
    }
}
