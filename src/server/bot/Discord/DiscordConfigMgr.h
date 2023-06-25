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

#ifndef _DISCORD_CONFIG_MGR_H_
#define _DISCORD_CONFIG_MGR_H_

#include "Define.h"
#include <unordered_map>

struct DiscordGuildConfig
{
    bool EnableRoleCheck{};
    bool EnableRoleAdd{};
    uint64 RoleIdUser{};
    uint64 RoleIdFriend{};
};

class WH_BOT_API DiscordConfigMgr
{
public:
    DiscordConfigMgr() = default;
    ~DiscordConfigMgr() = default;

    static DiscordConfigMgr* instance();

    void LoadConfig();
    DiscordGuildConfig* GetConfig(uint64 guidId);

private:
    std::unordered_map<uint64, DiscordGuildConfig> _guildConfigs;

    DiscordConfigMgr(DiscordConfigMgr const&) = delete;
    DiscordConfigMgr(DiscordConfigMgr&&) = delete;
    DiscordConfigMgr& operator=(DiscordConfigMgr const&) = delete;
    DiscordConfigMgr& operator=(DiscordConfigMgr&&) = delete;
};

#define sDiscordConfigMgr DiscordConfigMgr::instance()

#endif
