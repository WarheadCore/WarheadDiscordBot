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

#ifndef _DISCORD_MGR_H_
#define _DISCORD_MGR_H_

#include "DiscordEmbedMsg.h"
#include "Duration.h"
#include <utility>
#include <list>
#include <unordered_map>

namespace dpp
{
    class commandhandler;
    class cluster;

    struct slashcommand_t;
}

struct ConfirmButton
{
    uint64 KeepRoleId{};
    uint64 DeleteRoleId{};
    uint64 GuildId{};
    std::list<uint64> Members;
};

class WH_BOT_API DiscordMgr
{
public:
    DiscordMgr() = default;
    ~DiscordMgr();

    static DiscordMgr* instance();

    void LoadConfig(bool reload);
    void Start();
    void Stop();
    static bool NormalizePlayerName(std::string& name);

    void SendDefaultMessage(std::string_view message, uint64 channelID);
    void SendEmbedMessage(DiscordEmbedMsg const& embed, uint64 channelID);

    void AddGuildNickName(uint64 guildId, uint64 userId, uint64 channelId, std::string_view nickName, std::string_view gameSpec, int32 ilvl);

private:
    void ConfigureLogs();
    void ConfigureCommands();
    static void GuildAddHandler(dpp::slashcommand_t const& event);
    void GuildGetPlayersHandler(dpp::slashcommand_t const& event);
    void CheckRolesHandler(dpp::slashcommand_t const& event);
    void CheckGuild();

    ConfirmButton* GetConfirmButton(uint64 authorId);
    void AddConfirmButton(uint64 authorId, ConfirmButton confirmButton);
    void DeleteConfirmButton(uint64 authorId);

    std::unique_ptr<dpp::cluster> _bot;

    // Config
    std::string _botToken;

    std::unordered_map<uint64, ConfirmButton> _confirmButtons;

    DiscordMgr(DiscordMgr const&) = delete;
    DiscordMgr(DiscordMgr&&) = delete;
    DiscordMgr& operator=(DiscordMgr const&) = delete;
    DiscordMgr& operator=(DiscordMgr&&) = delete;
};

#define sDiscordMgr DiscordMgr::instance()

#endif
