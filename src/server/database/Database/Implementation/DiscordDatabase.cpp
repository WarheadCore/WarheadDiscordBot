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

#include "DiscordDatabase.h"

DiscordDatabasePool DiscordDatabase;

void DiscordDatabasePool::DoPrepareStatements()
{
    SetStatementSize(MAX_LOGIN_DATABASE_STATEMENTS);

    // Nickname
    PrepareStatement(DISCORD_SEL_NICKNAMES, "SELECT `nickname`, `ilvl`, `game_spec`, `twinks` FROM `guild_players` WHERE `discord_guild_id` = ?", ConnectionFlags::Async);
    PrepareStatement(DISCORD_SEL_NICKNAME, "SELECT `nickname` FROM `guild_players` WHERE `discord_guild_id` = ? AND `nickname` = ?", ConnectionFlags::Async);
    PrepareStatement(DISCORD_INS_NICKNAME, "INSERT INTO `guild_players` (`discord_guild_id`, `user_id`, `nickname`, `ilvl`, `game_spec`) VALUES (?, ?, ?, ?, ?)", ConnectionFlags::Async);
    PrepareStatement(DISCORD_UPD_NICKNAME, "UPDATE `guild_players` SET `twinks` = ? WHERE `discord_guild_id` = ? AND nickname LIKE ?", ConnectionFlags::Async);
    PrepareStatement(DISCORD_UPD_ILVL, "UPDATE `guild_players` SET `ilvl` = ? WHERE `discord_guild_id` = ? AND nickname LIKE ?", ConnectionFlags::Async);
}
