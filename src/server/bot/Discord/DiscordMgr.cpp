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

#include "DiscordMgr.h"
#include "Config.h"
#include "Log.h"
#include "StopWatch.h"
#include "Util.h"
#include "StringFormat.h"
#include "Containers.h"
#include <dpp/cluster.h>
#include <dpp/message.h>

namespace
{
    // Owner
    constexpr auto OWNER_ID = 365169287926906883; // Winfidonarleyan | <@365169287926906883>
    constexpr auto OWNER_MENTION = "<@365169287926906883>";
}

DiscordMgr* DiscordMgr::instance()
{
    static DiscordMgr instance;
    return &instance;
}

DiscordMgr::~DiscordMgr()
{
    if (_bot)
        _bot->shutdown();
}

void DiscordMgr::LoadConfig(bool reload)
{
    auto oldEnable{ _isEnable };
    _isEnable = sConfigMgr->GetOption<bool>("Discord.Bot.Enable", true);

    if (!reload && !_isEnable)
        return;

    // If after reload config disable - stop bot
    if (reload && oldEnable && !_isEnable)
    {
        LOG_WARN("discord", "Stop discord bot after config reload");
        Stop();
        return;
    }

    // Load config options
    {
        _botToken = sConfigMgr->GetOption<std::string>("Discord.Bot.Token", "");
        if (_botToken.empty())
        {
            LOG_FATAL("discord", "> Empty bot token for discord. Disable system");
            _isEnable = false;
            return;
        }

        _guildID = sConfigMgr->GetOption<int64>("Discord.Guild.ID", 0);
        if (!_guildID)
        {
            LOG_FATAL("discord", "> Empty guild id for discord. Disable system");
            _isEnable = false;
            return;
        }
    }

    // Start bot if after reload config option set to enable
    if (reload && !oldEnable && _isEnable)
        Start();
}

void DiscordMgr::Start()
{
    if (!_isEnable)
        return;

    LOG_INFO("server.loading", "Loading discord bot...");

    StopWatch sw;

    _bot = std::make_unique<dpp::cluster>(_botToken, dpp::i_unverified_default_intents, 0, 0, 1, true,
        dpp::cache_policy_t{ dpp::cp_aggressive, dpp::cp_aggressive, dpp::cp_aggressive }, 1);

    // Prepare logs
    ConfigureLogs();

    _bot->start(dpp::st_return);

    // Check bot in guild, category and text channels
    CheckGuild();

    // Prepare commands
    ConfigureCommands();

    LOG_INFO("server.loading", ">> Discord bot is initialized in {}", sw);
    LOG_INFO("server.loading", "");
}

void DiscordMgr::Stop()
{
    if (_bot)
        _bot->shutdown();

    _bot.reset();
}

void DiscordMgr::SendDefaultMessage(std::string_view message, uint64 channelID)
{
    if (!_isEnable || !_bot)
        return;

    dpp::message discordMessage;
    discordMessage.channel_id = channelID;
    discordMessage.content = message;

    _bot->message_create(discordMessage);
}

void DiscordMgr::SendEmbedMessage(DiscordEmbedMsg const& embed, uint64 channelID)
{
    if (!_isEnable || !_bot)
        return;

    _bot->message_create(dpp::message(channelID, *embed.GetMessage()));
}

void DiscordMgr::ConfigureLogs()
{
    if (!_isEnable)
        return;

    _bot->on_ready([this](const auto&)
    {
        LOG_INFO("discord.bot", "DiscordBot: Logged in as {}", _bot->me.username);
    });

    _bot->on_log([](const dpp::log_t& event)
    {
        switch (event.severity)
        {
            case dpp::ll_trace:
                LOG_TRACE("discord.bot", "DiscordBot: {}", event.message);
                break;
            case dpp::ll_debug:
                LOG_DEBUG("discord.bot", "DiscordBot: {}", event.message);
                break;
            case dpp::ll_info:
                LOG_INFO("discord.bot", "DiscordBot: {}", event.message);
                break;
            case dpp::ll_warning:
                LOG_WARN("discord.bot", "DiscordBot: {}", event.message);
                break;
            case dpp::ll_error:
                LOG_ERROR("discord.bot", "DiscordBot: {}", event.message);
                break;
            case dpp::ll_critical:
                LOG_CRIT("discord.bot", "DiscordBot: {}", event.message);
                break;
            default:
                break;
        }
    });
}

void DiscordMgr::ConfigureCommands()
{
    // Message clean commands
    dpp::slashcommand randomCommand{ "random", "Зарандомить команды для валоранта", _bot->me.id };

    dpp::command_option channelOption{ dpp::co_channel, "channel", "Войс канал с игроками", true };
    channelOption.channel_types.emplace_back(dpp::channel_type::CHANNEL_VOICE);
    randomCommand.add_option(channelOption);

    _bot->guild_command_create(randomCommand, _guildID);

    _bot->on_slashcommand([this](dpp::slashcommand_t const& event)
    {
        auto commandName{ event.command.get_command_name() };

        if (commandName != "random")
            return;

        // Start make message
        auto embedMsg = std::make_shared<DiscordEmbedMsg>();
        embedMsg->SetTitle("Рандомайзер игроков");
        auto channelID{ event.command.channel_id };

        // Get count parameters
        auto targetChannelID = std::get<dpp::snowflake>(event.get_parameter("channel"));

        auto targetChannel = find_channel(targetChannelID);
        if (!targetChannel)
        {
            embedMsg->SetColor(DiscordMessageColor::Red);
            embedMsg->SetDescription(Warhead::StringFormat("Указанного канала не существует: <#{}>", uint64(targetChannelID)));

            dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
            event.reply(replyMessage);
            return;
        }

        if (targetChannel->get_type() != dpp::channel_type::CHANNEL_VOICE)
        {
            embedMsg->SetColor(DiscordMessageColor::Red);
            embedMsg->SetDescription(Warhead::StringFormat("Указанный канал не является войсом: <#{}>", uint64(targetChannelID)));

            dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
            event.reply(replyMessage);
            return;
        }

        auto voiceMembers = targetChannel->get_voice_members();
        if (voiceMembers.empty())
        {
            embedMsg->SetColor(DiscordMessageColor::Red);
            embedMsg->SetDescription(Warhead::StringFormat("Указанный канал пустой: <#{}>", uint64(targetChannelID)));

            dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
            event.reply(replyMessage);
        }

        if (voiceMembers.size() == 1)
        {
            embedMsg->SetColor(DiscordMessageColor::White);
            embedMsg->SetDescription(Warhead::StringFormat("В указанном канале <#{}> только один боец и это <@{}>", uint64(targetChannelID), uint64(voiceMembers.begin()->first)));

            dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
            event.reply(replyMessage);
            return;
        }

        std::vector<uint64> sortedMembers;

        for (auto const& [id, state] : voiceMembers)
            sortedMembers.emplace_back(id);

        // Time to random this list
        Warhead::Containers::RandomShuffle(sortedMembers);

        std::pair<std::string, std::string> memberList;
        std::size_t maxPlayerInCommand{ 5 };

        if (sortedMembers.size() <= 8)
            maxPlayerInCommand = sortedMembers.size() / 2;

        std::size_t playersInCommand{};
        bool needFillSecondCommand{};

        std::string spectators;

        for (auto const& id : sortedMembers)
        {
            if (needFillSecondCommand && playersInCommand >= maxPlayerInCommand)
            {
                spectators += Warhead::StringFormat("<@{}>\n", id);
                continue;
            }

            if (++playersInCommand > maxPlayerInCommand)
            {
                needFillSecondCommand = true;
                playersInCommand = 1;
            }

            if (!needFillSecondCommand)
                memberList.first += Warhead::StringFormat("{}. <@{}>\n", playersInCommand, id);
            else
                memberList.second += Warhead::StringFormat("{}. <@{}>\n", playersInCommand, id);
        }

        embedMsg->SetColor(DiscordMessageColor::Teal);
        embedMsg->AddEmbedField("Атакеры", memberList.first, true);
        embedMsg->AddEmbedField("Деферы", memberList.second, true);

        if (!spectators.empty())
            embedMsg->AddEmbedField("Зрители", spectators);

        dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
        event.reply(replyMessage);
    });
}

void DiscordMgr::CheckGuild()
{
    StopWatch sw;

    auto const& guilds = _bot->current_user_get_guilds_sync();
    if (guilds.empty())
    {
        LOG_ERROR("discord", "DiscordBot: Not found guilds. Disable bot");
        return;
    }

    bool isExistGuild{};

    for (auto const& [guildID, guild] : guilds)
    {
        if (guildID == _guildID)
        {
            isExistGuild = true;
            break;
        }
    }

    if (!isExistGuild)
    {
        LOG_ERROR("discord", "DiscordBot: Not found config guild: {}. Disable bot", _guildID);
        _isEnable = false;
        Stop();
        return;
    }

    LOG_DEBUG("discord", "DiscordBot: Found config guild: {}", _guildID);
}
