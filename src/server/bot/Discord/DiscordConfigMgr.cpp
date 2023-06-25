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

#include "DiscordConfigMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Containers.h"
#include "StopWatch.h"

DiscordConfigMgr* DiscordConfigMgr::instance()
{
    static DiscordConfigMgr instance;
    return &instance;
}

void DiscordConfigMgr::LoadConfig()
{
    StopWatch sw;

    auto result = DiscordDatabase.Query("SELECT `guild_id`, `enable_role_check`, `enable_role_add`, `role_id_user`, `role_id_friend` FROM `guild_config`");
    if (!result)
    {
        LOG_INFO("loading", "Loaded 0 guild config");
        return;
    }

    for (auto const& row : *result)
    {
        auto guildId = row[0].Get<uint64>();
        if (GetConfig(guildId))
        {
            LOG_ERROR("sql", "Guid config: Exit guild id: {}", guildId);
            continue;
        }

        DiscordGuildConfig config{};
        config.EnableRoleCheck  = row[1].Get<bool>();
        config.EnableRoleAdd    = row[2].Get<bool>();
        config.RoleIdUser       = row[3].Get<uint64>();
        config.RoleIdFriend     = row[4].Get<uint64>();

        _guildConfigs.emplace(guildId, config);
    }

    LOG_INFO("loading", "Loaded {} guild config in {}", _guildConfigs.size(), sw);
    LOG_INFO("loading", "");
}

DiscordGuildConfig* DiscordConfigMgr::GetConfig(uint64 guidId)
{
    return Warhead::Containers::MapGetValuePtr(_guildConfigs, guidId);
}
