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
#include "DiscordConfigMgr.h"
#include "BotMgr.h"
#include "Config.h"
#include "Log.h"
#include "StopWatch.h"
#include "Util.h"
#include "StringFormat.h"
#include "Containers.h"
#include "DatabaseEnv.h"
#include "StringConvert.h"
#include "dpp/commandhandler.h"
#include <dpp/cluster.h>
#include <dpp/message.h>
#include <list>

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
    Stop();
}

void DiscordMgr::LoadConfig(bool /*reload*/)
{
    _botToken = sConfigMgr->GetOption<std::string>("Discord.Bot.Token", "");
    if (_botToken.empty())
    {
        LOG_FATAL("discord", "> Empty bot token for discord. Disable system");
        ABORT();
        return;
    }

    sDiscordConfigMgr->LoadConfig();
}

void DiscordMgr::Start()
{
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
    if (!_bot)
        return;

    dpp::message discordMessage;
    discordMessage.channel_id = channelID;
    discordMessage.content = message;

    _bot->message_create(discordMessage);
}

void DiscordMgr::SendEmbedMessage(DiscordEmbedMsg const& embed, uint64 channelID)
{
    if (!_bot)
        return;

    _bot->message_create(dpp::message(channelID, *embed.GetMessage()));
}

void DiscordMgr::ConfigureLogs()
{
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
    dpp::slashcommand guildAddCommand{ "guild-add", "Получить роль участника", _bot->me.id };
    dpp::slashcommand guildCheck{ "guild-players-list", "Получить список участников", _bot->me.id };
    dpp::slashcommand checkRolesCommand{ "check-roles", "Проверить роли участников (25 человек максимум)", _bot->me.id };

    dpp::command_option roleOptionKeep{ dpp::co_role, "role_keep", "Роль, которую нужно оставить", true };
    dpp::command_option roleOptionDelete{ dpp::co_role, "role_del", "Роль, которую нужно удалить, если есть та, которую нужно оставить", true };
    dpp::command_option maxUsers{ dpp::co_integer, "max_users", "Максимальное количество пользователей для удаления роли", true };

    maxUsers.set_min_value(1);
    maxUsers.set_max_value(20);

    checkRolesCommand.add_option(roleOptionKeep);
    checkRolesCommand.add_option(roleOptionDelete);
    checkRolesCommand.add_option(maxUsers);

    // Admin only
    guildCheck.set_default_permissions(0);
    checkRolesCommand.set_default_permissions(0);

    _bot->current_user_get_guilds([this, guildAddCommand, guildCheck, checkRolesCommand](dpp::confirmation_callback_t const& callback)
    {
        if (callback.is_error())
            return;

        auto guilds = std::get<dpp::guild_map>(callback.value);
        if (guilds.empty())
            return;

        for (auto const& [id, guild] : guilds)
        {
            _bot->guild_command_create(guildAddCommand, id);
            _bot->guild_command_create(guildCheck, id);
            _bot->guild_command_create(checkRolesCommand, id);
        }
    });

    _bot->on_slashcommand([this](dpp::slashcommand_t const& event)
    {
        auto commandName{ event.command.get_command_name() };
        if (commandName == "guild-add")
            GuildAddHandler(event);
        else if (commandName == "check-roles")
            CheckRolesHandler(event);
        else if (commandName == "guild-players-list")
            GuildGetPlayersHandler(event);
    });

    _bot->on_button_click([this](dpp::button_click_t const& event)
    {
        auto channelID{ event.command.channel_id };
        auto authorID{ event.command.member.user_id};
        auto embedMsg = std::make_shared<DiscordEmbedMsg>();
        embedMsg->SetTitle("Проверка ролей - Выполнение");

        bool isAuthor = event.custom_id.starts_with(Warhead::StringFormat("{}", authorID));
        bool isConfirm = event.custom_id == Warhead::StringFormat("{}_Confirm", authorID);
        bool isDelete = event.custom_id == Warhead::StringFormat("{}_Delete", authorID);

        if (!isAuthor)
        {
            embedMsg->SetColor(DiscordMessageColor::Red);
            embedMsg->SetDescription("Вы не являетесь автором этого запроса");

            dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
            replyMessage.set_flags(dpp::m_ephemeral);
            event.reply(replyMessage);
            return;
        }

        if (!isConfirm && !isDelete)
        {
            embedMsg->SetColor(DiscordMessageColor::Red);
            embedMsg->SetDescription("Странная ситуация, не могу понять, куда вы нажали о.о");

            dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
            replyMessage.set_flags(dpp::m_ephemeral);
            event.reply(replyMessage);
            return;
        }

        if (isDelete)
        {
            _bot->message_delete(event.command.message_id, channelID);

            embedMsg->SetColor(DiscordMessageColor::Indigo);
            embedMsg->SetDescription("Запрос удалён");

            dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
            replyMessage.set_flags(dpp::m_ephemeral);
            event.reply(replyMessage);
            return;
        }

        if (!isConfirm)
        {
            embedMsg->SetColor(DiscordMessageColor::Red);
            embedMsg->SetDescription("Странная ситуация (2), не могу понять, куда вы нажали о.о");

            dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
            replyMessage.set_flags(dpp::m_ephemeral);
            event.reply(replyMessage);
            return;
        }

        auto confirm = GetConfirmButton(authorID);
        if (!confirm)
        {
            embedMsg->SetColor(DiscordMessageColor::Red);
            embedMsg->SetDescription("Не найден запрос на проверку");

            dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
            replyMessage.set_flags(dpp::m_ephemeral);
            event.reply(replyMessage);
            return;
        }

        embedMsg->SetColor(DiscordMessageColor::Indigo);
        embedMsg->SetDescription("Запрос на сервер дискорда был отправлен");

        dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
        replyMessage.set_flags(dpp::m_ephemeral);
        event.reply(replyMessage);

        for (auto const memberId : confirm->Members)
            _bot->guild_member_delete_role(confirm->GuildId, memberId, confirm->DeleteRoleId);

        DeleteConfirmButton(authorID);
    });

    _bot->on_form_submit([this](dpp::form_submit_t const& event)
    {
        std::string nickName = std::get<std::string>(event.components[0].components[0].value);
        std::string gameSpec = std::get<std::string>(event.components[1].components[0].value);
        std::string ilvlStr = std::get<std::string>(event.components[2].components[0].value);
        auto channelId = event.command.channel_id;

        auto embedMsg = std::make_shared<DiscordEmbedMsg>();
        embedMsg->SetTitle("Вступление в гильдию");

        if (!NormalizePlayerName(nickName))
        {
            embedMsg->SetColor(DiscordMessageColor::Red);
            embedMsg->SetDescription(Warhead::StringFormat("Введённый ник `{}` некорректный", nickName));

            dpp::message replyMessage{ channelId, *embedMsg->GetMessage() };
            replyMessage.set_flags(dpp::m_ephemeral);
            event.reply(replyMessage);
            return;
        }

        auto avgIlvl = Warhead::StringTo<int32>(ilvlStr);
        if (!avgIlvl)
        {
            embedMsg->SetColor(DiscordMessageColor::Red);
            embedMsg->SetDescription(Warhead::StringFormat("Введённый уровень предметов `{}` некорректный", ilvlStr));

            dpp::message replyMessage{ channelId, *embedMsg->GetMessage() };
            replyMessage.set_flags(dpp::m_ephemeral);
            event.reply(replyMessage);
            return;
        }

        embedMsg->SetColor(DiscordMessageColor::Cyan);
        embedMsg->AddEmbedField("Ник в гильдии", nickName);
        embedMsg->AddEmbedField("Специализация", gameSpec);
        embedMsg->AddEmbedField("Уровень предметов", ilvlStr);
        embedMsg->SetDescription("Запрос на выдачу роли отправлен");

        dpp::message replyMessage{ channelId, *embedMsg->GetMessage() };
        replyMessage.set_flags(dpp::m_ephemeral);
        event.reply(replyMessage);

        AddGuildNickName(event.command.guild_id, event.command.member.user_id, channelId, nickName, gameSpec, *avgIlvl);
    });
}

void DiscordMgr::CheckGuild()
{
    StopWatch sw;

    auto const& guilds = _bot->current_user_get_guilds_sync();
    if (guilds.empty())
    {
        LOG_ERROR("discord", "DiscordBot: Not found guilds. Disable bot");
        ABORT();
        return;
    }

    for (auto const& [id, guild] : guilds)
    {

    }
}

ConfirmButton* DiscordMgr::GetConfirmButton(uint64 authorId)
{
    return Warhead::Containers::MapGetValuePtr(_confirmButtons, authorId);
}

void DiscordMgr::AddConfirmButton(uint64 authorId, ConfirmButton confirmButton)
{
    auto oldConfirm = GetConfirmButton(authorId);
    if (oldConfirm)
    {
        *oldConfirm = std::move(confirmButton);
        return;
    }

    _confirmButtons.emplace(authorId, std::move(confirmButton));
}

void DiscordMgr::DeleteConfirmButton(uint64 authorId)
{
    _confirmButtons.erase(authorId);
}

bool DiscordMgr::NormalizePlayerName(std::string& name)
{
    if (name.empty())
        return false;

    if (name.find(' ') != std::string::npos)
        return false;

    std::wstring tmp;
    if (!Utf8toWStr(name, tmp))
        return false;

    wstrToLower(tmp);
    if (!tmp.empty())
        tmp[0] = wcharToUpper(tmp[0]);

    return WStrToUtf8(tmp, name);
}

void DiscordMgr::AddGuildNickName(uint64 guildId, uint64 userId, uint64 channelId, std::string_view nickName, std::string_view gameSpec, int32 ilvl)
{
    std::string saveNickName{ nickName };
    NormalizePlayerName(saveNickName);

    auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_SEL_NICKNAME);
    stmt->SetArguments(guildId, saveNickName);

    sBotMgr->GetQueryProcessor().AddCallback(DiscordDatabase.AsyncQuery(stmt)).WithPreparedCallback([this, guildId, userId, channelId, saveNickName, gameSpec = std::string(gameSpec), ilvl](PreparedQueryResult result)
    {
        auto embedMsg = std::make_shared<DiscordEmbedMsg>();
        embedMsg->SetTitle("Вступление в гильдию");

        if (result)
        {
            embedMsg->SetColor(DiscordMessageColor::Red);
            embedMsg->SetDescription(Warhead::StringFormat("Персонаж `{}` уже есть в базе", saveNickName));

            SendEmbedMessage(*embedMsg, channelId);
            return;
        }

        // INSERT INTO `guild_players` (`discord_guild_id`, `user_id`, `nickname`, `ilvl`, `game_spec`) VALUES (?, ?, ?, ?, ?)
        auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_INS_NICKNAME);
        stmt->SetArguments(guildId, userId, saveNickName, ilvl, gameSpec);
        DiscordDatabase.Execute(stmt);

        embedMsg->SetColor(DiscordMessageColor::Indigo);
        embedMsg->SetDescription(Warhead::StringFormat("Персонаж `{}` был добавлен в базу.", saveNickName));

        auto guildConfig = sDiscordConfigMgr->GetConfig(guildId);
        if (guildConfig && guildConfig->EnableRoleAdd)
        {
            auto roleIdUser = guildConfig->RoleIdUser;

            _bot->guild_get_member(guildId, userId, [this, guildId, userId, channelId, roleIdUser](dpp::confirmation_callback_t const& callback)
            {
                if (callback.is_error())
                    return;

                auto userInfo = callback.get<dpp::guild_member>();
                auto& userRoles = userInfo.roles;

                auto roleFind = std::find(userRoles.begin(), userRoles.end(), roleIdUser);
                if (roleFind != userRoles.end())
                    return;

                // Add user role
                _bot->guild_member_add_role(guildId, userId, roleIdUser, [this, userId, roleIdUser, channelId](dpp::confirmation_callback_t const& callback)
                {
                    auto embedMsg = std::make_shared<DiscordEmbedMsg>();
                    embedMsg->SetTitle("Выдача роли участника гильдии");
                    embedMsg->SetColor(DiscordMessageColor::Teal);
                    embedMsg->SetDescription(Warhead::StringFormat("<@{}> получил роль участника", userId));

                    SendEmbedMessage(*embedMsg, channelId);
                });
            });
        }

        SendEmbedMessage(*embedMsg, channelId);
    });
}

void DiscordMgr::GuildAddHandler(const dpp::slashcommand_t &event)
{
    dpp::interaction_modal_response modal("addModal", "Получить роль участника гильдии");

    dpp::component nickName;
    nickName.set_label("Ник");
    nickName.set_id("nickname");
    nickName.set_type(dpp::cot_text);
    nickName.set_text_style(dpp::text_short);
    modal.add_component(nickName);

    /* Add another text component in the next row, as required by Discord */
    modal.add_row();

    dpp::component className;
    className.set_label("Специализация");
    className.set_id("gamespec");
    className.set_type(dpp::cot_text);
    className.set_text_style(dpp::text_short);
    modal.add_component(className);

    /* Add another text component in the next row, as required by Discord */
    modal.add_row();

    dpp::component ilvl;
    ilvl.set_label("Средний уровень предметов");
    ilvl.set_id("avgilvl");
    ilvl.set_type(dpp::cot_text);
    ilvl.set_text_style(dpp::text_short);
    modal.add_component(ilvl);

    event.dialog(modal);
}

void DiscordMgr::CheckRolesHandler(const dpp::slashcommand_t &event)
{
    // Start make message
    auto embedMsg = std::make_shared<DiscordEmbedMsg>();
    embedMsg->SetTitle("Проверка ролей");
    auto channelID{ event.command.channel_id };

    // Get count parameters
    auto targetRoleKeepId = std::get<dpp::snowflake>(event.get_parameter("role_keep"));
    auto targetRoleKeep = find_role(targetRoleKeepId);
    if (!targetRoleKeep)
    {
        embedMsg->SetColor(DiscordMessageColor::Red);
        embedMsg->SetDescription(Warhead::StringFormat("Указанной роли не существует: <@&{}> ({})", uint64(targetRoleKeep), uint64(targetRoleKeep)));

        dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
        replyMessage.set_flags(dpp::m_ephemeral);
        event.reply(replyMessage);
        return;
    }

    auto targetRoleDeleteId = std::get<dpp::snowflake>(event.get_parameter("role_del"));
    auto targetRoleDelete = find_role(targetRoleDeleteId);
    if (!targetRoleDelete)
    {
        embedMsg->SetColor(DiscordMessageColor::Red);
        embedMsg->SetDescription(Warhead::StringFormat("Указанной роли не существует: <@&{}> ({})", uint64(targetRoleDelete), uint64(targetRoleDelete)));

        dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
        replyMessage.set_flags(dpp::m_ephemeral);
        event.reply(replyMessage);
        return;
    }

    auto maxUsersCheck = std::get<int64>(event.get_parameter("max_users"));
    auto authorId = event.command.member.user_id;

    ConfirmButton confirmButton;
    confirmButton.GuildId = event.command.guild_id;
    confirmButton.KeepRoleId = targetRoleKeepId;
    confirmButton.DeleteRoleId = targetRoleDeleteId;

    auto members = _bot->guild_get_members_sync(confirmButton.GuildId, maxUsersCheck, 0);
    if (members.empty())
    {
        embedMsg->SetColor(DiscordMessageColor::Red);
        embedMsg->SetDescription("Пользователи не найдены. Пустой сервер?");

        dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
        replyMessage.set_flags(dpp::m_ephemeral);
        event.reply(replyMessage);
        return;
    }

    for (auto& [memberId, member] : members)
    {
        bool foundKeepRole{};
        bool foundDelRole{};

        for (auto const memberRoleId : member.roles)
        {
            if (memberRoleId == targetRoleKeepId)
            {
                foundKeepRole = true;
                continue;
            }

            if (memberRoleId == targetRoleDeleteId)
                foundDelRole = true;
        }

        if (foundKeepRole && foundDelRole)
            confirmButton.Members.emplace_back(memberId);
    }

    if (confirmButton.Members.empty())
    {
        embedMsg->SetColor(DiscordMessageColor::Red);
        embedMsg->SetDescription("Пользователи по условию не найдены");

        dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };
        replyMessage.set_flags(dpp::m_ephemeral);
        event.reply(replyMessage);
        return;
    }

    embedMsg->SetColor(DiscordMessageColor::Indigo);
    embedMsg->AddEmbedField("Оставить роль", Warhead::StringFormat("<@&{}> ({})", uint64(targetRoleKeepId), uint64(targetRoleKeepId)));
    embedMsg->AddEmbedField("Удалить роль", Warhead::StringFormat("<@&{}> ({})", uint64(targetRoleDeleteId), uint64(targetRoleDeleteId)));

    uint8 index{};
    for (auto const memberId : confirmButton.Members)
        embedMsg->AddDescription(Warhead::StringFormat("{}. <@{}>\n", ++index, uint64(memberId)));

    AddConfirmButton(authorId, std::move(confirmButton));

    dpp::message replyMessage{ channelID, *embedMsg->GetMessage() };

    replyMessage.add_component(dpp::component().add_component(
            dpp::component().set_label("Подтвердить!").
                    set_type(dpp::cot_button).
                    set_style(dpp::cos_danger).
                    set_id(Warhead::StringFormat("{}_Confirm", authorId))));
    replyMessage.add_component(dpp::component().add_component(
            dpp::component().set_label("Удалить запрос").
                    set_type(dpp::cot_button).
                    set_style(dpp::cos_danger).
                    set_id(Warhead::StringFormat("{}_Delete", authorId))));

    replyMessage.set_flags(dpp::m_ephemeral);
    event.reply(replyMessage);
}

void DiscordMgr::GuildGetPlayersHandler(const dpp::slashcommand_t &event)
{
    auto replyMsg = std::make_shared<DiscordEmbedMsg>();
    replyMsg->SetTitle("Участники гильдии");

    auto waiter = std::make_shared<std::promise<void>>();
    auto channelId = event.command.channel_id;
    auto stmt = DiscordDatabase.GetPreparedStatement(DISCORD_SEL_NICKNAMES);
    stmt->SetArguments(uint64(event.command.guild_id));

    sBotMgr->GetQueryProcessor().AddCallback(DiscordDatabase.AsyncQuery(stmt)).WithPreparedCallback([this, waiter, replyMsg, channelId](PreparedQueryResult result)
    {
        if (!result)
        {
            replyMsg->SetColor(DiscordMessageColor::Red);
            replyMsg->SetDescription("Учаcтники не найдены");

            waiter->set_value();
            return;
        }
        else
        {
            replyMsg->SetColor(DiscordMessageColor::Cyan);
            replyMsg->AddEmbedField("Количество участников", Warhead::StringFormat("{}", result->GetRowCount()));

            waiter->set_value();
        }

        auto msg = std::make_shared<DiscordEmbedMsg>();
        msg->SetTitle("Участники гильдии");
        msg->SetColor(DiscordMessageColor::Indigo);
        uint8 count{};

        for (auto& row : *result)
        {
            if (++count >= 23)
            {
                SendEmbedMessage(*msg, channelId);

                auto msg = std::make_shared<DiscordEmbedMsg>();
                msg->SetTitle("Участники гильдии");
                msg->SetColor(DiscordMessageColor::Indigo);
            }

            // SELECT `nickname`, `ilvl`, `game_spec`, `twinks` FROM `guild_players` WHERE `discord_guild_id` = ?
            auto nickName = row[0].Get<std::string_view>();
            auto ilvl = row[1].Get<int32>();
            auto gameSpec = row[2].Get<std::string_view>();

            msg->AddEmbedField(nickName, Warhead::StringFormat("Спек: `{}`. Илвл: `{}`", gameSpec, ilvl));
        }

        SendEmbedMessage(*msg, channelId);
    });

    waiter->get_future().wait();

    dpp::message replyMessage{ channelId, *replyMsg->GetMessage() };
    replyMessage.set_flags(dpp::m_ephemeral);
    event.reply(replyMessage);
}
