/*
 * mod-player-bot-guildhouse.cpp
 *
 * This module teleports all online guild bots to the guild house (GM Island)
 * when a real (human) player logs in, enters zone 876, or when a timer periodically
 * checks for real players in zone 876 .
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Guild.h"
#include "Chat.h"
#include "Log.h"
#include "Database/DatabaseEnv.h"
#include "Configuration/Config.h"
#include "ObjectAccessor.h"
#include "PlayerbotMgr.h"
#include "PlayerbotAI.h"
#include "Configuration/Config.h"
#include "Group.h"

static uint32 g_MoveBotsToGuildhouseCheckFrequency = 60; // in seconds

// Helper: Determine if a player is a bot using the PlayerbotsMgr.
static bool IsPlayerBot(Player* player)
{
    if (!player)
        return false;
    PlayerbotAI* botAI = sPlayerbotsMgr->GetPlayerbotAI(player);
    return botAI && botAI->IsBotAI();
}

// Safety check: Returns true if the bot is in a safe state for teleporting to the guild house.
static bool IsBotSafeForGuildHouse(Player* bot)
{
    if (!bot)
    {
        return false;
    }
    if (!bot->IsInWorld())
    {
        return false;
    }
    if (!bot->IsAlive())
    {
        return false;
    }
    if (bot->IsInCombat())
    {
        return false;
    }
    if (bot->InBattleground() || bot->InArena() || bot->inRandomLfgDungeon() || bot->InBattlegroundQueue())
    {
        return false;
    }
    if (bot->IsInFlight())
    {
        return false;
    }
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && !IsPlayerBot(member))
            {
                return false;
            }
        }
    }
    if(bot->GetZoneId() == 876)
    {
        return false;
    }
    return true;
}

// Helper: Retrieve guild house data and update (teleport) all online guild bots
// in the same guild as the given real (human) player.
static void TryToSummonGuildBotsToGuildhouseForRealPlayer(Player* realPlayer)
{
    Guild* guild = realPlayer->GetGuild();
    if (!guild)
    {
        return;
    }

    QueryResult result = CharacterDatabase.Query(
        "SELECT `id`, `guild`, `phase`, `map`, `positionX`, `positionY`, `positionZ`, `orientation` "
        "FROM `guild_house` WHERE `guild` = {}", realPlayer->GetGuildId());
    if (!result)
    {
        return;
    }

    uint32 guildHousePhase = 0;
    uint32 mapId = 0;
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f, ori = 0.0f;
    {
        Field* fields = result->Fetch();
        guildHousePhase = fields[2].Get<uint32>();
        mapId = fields[3].Get<uint32>();
        posX = fields[4].Get<float>();
        posY = fields[5].Get<float>();
        posZ = fields[6].Get<float>();
        ori = fields[7].Get<float>();
    }

    // Iterate over all online players.
    for (auto const& itr : ObjectAccessor::GetPlayers())
    {
        Player* target = itr.second;
        // If the target is in the same guild and is identified as a bot...
        if (target && target->GetGuildId() == realPlayer->GetGuildId() && IsPlayerBot(target))
        {
            // ...and if the bot is safe for teleport, then update it.
            if (!IsBotSafeForGuildHouse(target))
            {
                continue;
            }
            target->TeleportTo(mapId, posX, posY, posZ, ori);
            target->SetPhaseMask(guildHousePhase, true);
            ChatHandler(target->GetSession()).PSendSysMessage("PlayerBot: Teleported to Guild House.");
        }
    }
}

// PlayerScript that triggers the update when a real player logs in or enters zone 876.
class PlayerBotGuildHouseScript : public PlayerScript
{
public:
    PlayerBotGuildHouseScript() : PlayerScript("PlayerBotGuildHouseScript") {}

    void OnPlayerLogin(Player* player) override
    {
        // Only for real (human) players in zone 876 (GM Island).
        if (player && !IsPlayerBot(player) && player->GetZoneId() == 876)
        {
            TryToSummonGuildBotsToGuildhouseForRealPlayer(player);
        }
    }

    void OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 /*newArea*/) override
    {
        // When a real player enters zone 876 (GM Island).
        if (newZone == 876 && player && !IsPlayerBot(player))
        {
            TryToSummonGuildBotsToGuildhouseForRealPlayer(player);
        }
    }
};

// WorldScript that periodically checks if any real player is in zone 876
// and then triggers TryToSummonGuildBotsToGuildhouseForRealPlayer.
class BotGuildHouseTimerWorldScript : public WorldScript
{
public:
    BotGuildHouseTimerWorldScript() : WorldScript("BotGuildHouseTimerWorldScript"), m_timer(0) {}

    void OnStartup() override
    {
        g_MoveBotsToGuildhouseCheckFrequency = sConfigMgr->GetOption<uint32>("PlayerbotGuildhouse.MoveBotsToGuildhouseCheckFrequency", 60);
    }

    void OnUpdate(uint32 diff) override
    {
        m_timer += diff;
        // Run the check every 30 seconds.
        if (m_timer < (g_MoveBotsToGuildhouseCheckFrequency * 1000))
        {
            return;
        }
        m_timer = 0;

        // Iterate over all online players.
        for (auto const& itr : ObjectAccessor::GetPlayers())
        {
            Player* player = itr.second;
            // For every real player in zone 876, update its guild bots.
            if (player && !IsPlayerBot(player) && player->GetZoneId() == 876)
            {
                TryToSummonGuildBotsToGuildhouseForRealPlayer(player);
            }
        }
    }
private:
    uint32 m_timer;
};

void Addmod_player_bot_guildhouseScripts()
{
    new PlayerBotGuildHouseScript();
    new BotGuildHouseTimerWorldScript();
}
