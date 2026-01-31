/*
 * mod-player-bot-guildhouse.cpp
 *
 * Periodically teleports guild bots in and out of their guild houses
 *
 * On each cycle the script reads all configured guild house records
 * For each guild, if require‑real‑player flag is set it only runs when
 *   at least one real player is online in that guild
 * It gathers all online safe bots
 * It teleports a random subset of those bots into the guild house location
 * It saves their original positions for later restoration
 * It then teleports a random subset of bots out of the guild house if they
 *   fail a urand check
 * Bots return to their saved original positions (or hearthstone if lost)
 * Each bot receives a system message on teleport in or out
 * If debug is enabled, additional INFO logs are emitted
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
#include "Group.h"
#include "World.h"

#include <unordered_map>
#include <vector>
#include <tuple>
#include <thread>
#include <chrono>

// Interval between teleport cycles in seconds
static uint32 g_TeleportCycleFrequency    = 120;
// Maximum bots to teleport per wave
static uint32 g_TeleportBatchSize         = 5;
// If true require at least one real player in guild before teleport
static bool   g_RequireRealPlayer         = true;
// Percent chance to teleport a picked bot into guildhouse (default 60%)
static uint32 g_EntryChancePercent        = 60;
// Percent chance to teleport a picked bot out of guildhouse (default 40%)
static uint32 g_ExitChancePercent         = 40;
// Enable detailed debug logging (default false)
static bool   g_DebugEnabled              = false;

// Map of bot GUID to their original location tuple
static std::unordered_map<ObjectGuid, std::tuple<uint32,float,float,float,float>> g_PreviousLocations;
// Map of guild ID to list of bot GUIDs currently in the guild house
static std::unordered_map<uint32, std::vector<ObjectGuid>>                        g_GuildhouseBots;

static bool IsPlayerBot(Player* player)
{
    if (!player) return false;
    PlayerbotAI* ai = PlayerbotsMgr::instance().GetPlayerbotAI(player);
    return ai && ai->IsBotAI();
}

static bool IsBotSafeForGuildHouse(Player* bot)
{
    if (!bot || !bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat())
        return false;
    if (bot->InBattleground() || bot->InArena() || bot->inRandomLfgDungeon() || bot->InBattlegroundQueue())
        return false;
    if (bot->IsInFlight())
        return false;
    if (bot->GetZoneId() == 876)
        return false;
    if (Group* grp = bot->GetGroup())
    {
        for (GroupReference* ref = grp->GetFirstMember(); ref; ref = ref->next())
        {
            Player* m = ref->GetSource();
            if (m && !IsPlayerBot(m))
                return false;
        }
    }
    return true;
}

class BotGuildHouseTimerWorldScript : public WorldScript
{
public:
    BotGuildHouseTimerWorldScript()
      : WorldScript("BotGuildHouseTimerWorldScript"), m_timer(0)
    {}

    void OnStartup() override
    {
        g_TeleportCycleFrequency    = sConfigMgr->GetOption<uint32>("PlayerbotGuildhouse.TeleportCycleFrequency", g_TeleportCycleFrequency);
        g_TeleportBatchSize         = sConfigMgr->GetOption<uint32>("PlayerbotGuildhouse.StaggeredTeleport.BatchSize", g_TeleportBatchSize);
        g_RequireRealPlayer         = sConfigMgr->GetOption<bool>  ("PlayerbotGuildhouse.RequireRealPlayer", g_RequireRealPlayer);
        g_EntryChancePercent        = sConfigMgr->GetOption<uint32>("PlayerbotGuildhouse.EntryChancePercent", g_EntryChancePercent);
        g_ExitChancePercent         = sConfigMgr->GetOption<uint32>("PlayerbotGuildhouse.ExitChancePercent", g_ExitChancePercent);
        g_DebugEnabled              = sConfigMgr->GetOption<bool>  ("PlayerbotGuildhouse.DebugEnabled", g_DebugEnabled);
    }

    void OnUpdate(uint32 diff) override
    {
        m_timer += diff;
        if (m_timer < g_TeleportCycleFrequency * 1000)
            return;
        m_timer = 0;

        std::unordered_map<uint32,bool> processed;

        // Sync bots already inside guild house (zone 876) on restart
        for (auto const& entry : ObjectAccessor::GetPlayers())
        {
            Player* bot = entry.second;
            if (bot && IsPlayerBot(bot) && bot->GetZoneId() == 876)
            {
                uint32 guildId = bot->GetGuildId();
                auto& vec = g_GuildhouseBots[guildId];
                ObjectGuid guid = bot->GetGUID();
                if (std::find(vec.begin(), vec.end(), guid) == vec.end())
                {
                    vec.push_back(guid);
                    if (g_DebugEnabled)
                        LOG_INFO("server.loading","[BotGuildHouse] Discovered existing bot {} in guildhouse for guild {}",
                                 bot->GetName().c_str(), guildId);
                }
            }
        }

        if (g_DebugEnabled)
            LOG_INFO("server.loading","[BotGuildHouse] Cycle start");

        // ENTRY phase
        auto guildRes = CharacterDatabase.Query(
            "SELECT guild,phase,map,positionX,positionY,positionZ,orientation FROM guild_house");
        if (guildRes)
        {
            do
            {
                Field* f = guildRes->Fetch();
                uint32 guildId = f[0].Get<uint32>();
                if (processed[guildId])
                    continue;
                processed[guildId] = true;
                if (g_DebugEnabled)
                    LOG_INFO("server.loading","[BotGuildHouse] Processing guild {}", guildId);

                if (g_RequireRealPlayer)
                {
                    bool hasReal = false;
                    for (auto const& e : ObjectAccessor::GetPlayers())
                    {
                        Player* p = e.second;
                        if (p && p->GetGuildId() == guildId && !IsPlayerBot(p))
                        {
                            hasReal = true;
                            break;
                        }
                    }
                    if (!hasReal)
                        continue;
                }

                uint32 phase = f[1].Get<uint32>();
                uint32 mapId = f[2].Get<uint32>();
                float  x     = f[3].Get<float>();
                float  y     = f[4].Get<float>();
                float  z     = f[5].Get<float>();
                float  ori   = f[6].Get<float>();

                std::vector<Player*> candidates;
                for (auto const& q : ObjectAccessor::GetPlayers())
                {
                    Player* bot = q.second;
                    if (bot && bot->GetGuildId() == guildId && IsPlayerBot(bot) && IsBotSafeForGuildHouse(bot))
                    {
                        candidates.push_back(bot);
                        if (g_DebugEnabled)
                            LOG_INFO("server.loading","[BotGuildHouse] Candidate bot {} ({})",
                                     bot->GetName().c_str(), bot->GetGUID().GetRawValue());
                    }
                }
                if (candidates.empty())
                    continue;

                uint32 maxCount   = std::min<uint32>(candidates.size(), g_TeleportBatchSize);
                uint32 entryCount = urand(1, maxCount);
                for (uint32 i = 0; i < entryCount; ++i)
                {
                    uint32 idx = urand(i, candidates.size() - 1);
                    std::swap(candidates[i], candidates[idx]);
                    Player* bot = candidates[i];

                    uint32 rollIn = urand(1, 100);
                    if (g_DebugEnabled)
                        LOG_INFO("server.loading","[BotGuildHouse] Entry roll {} vs {} for {}",
                                 rollIn, g_EntryChancePercent, bot->GetName().c_str());
                    if (rollIn > g_EntryChancePercent)
                        continue;

                    ObjectGuid guid = bot->GetGUID();
                    g_PreviousLocations[guid] = std::make_tuple(
                        bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetOrientation());
                    bot->TeleportTo(mapId, x, y, z, ori);
                    bot->SetPhaseMask(phase, true);
                    ChatHandler(bot->GetSession()).PSendSysMessage("PlayerBot: Teleported to Guild House.");
                    if (g_DebugEnabled)
                        LOG_INFO("server.loading","[BotGuildHouse] Teleported in {}", bot->GetName().c_str());
                    g_GuildhouseBots[guildId].push_back(guid);
                }
            }
            while (guildRes->NextRow());
        }

        // EXIT phase
        for (auto const& kv : processed)
        {
            uint32 guildId = kv.first;
            auto& vec = g_GuildhouseBots[guildId];
            if (vec.empty())
                continue;

            uint32 maxOut    = std::min<uint32>(vec.size(), g_TeleportBatchSize);
            uint32 exitCount = urand(1, maxOut);
            for (uint32 i = 0; i < exitCount; ++i)
            {
                uint32 idx = urand(0, vec.size() - 1);
                ObjectGuid guid = vec[idx];
                if (Player* bot = ObjectAccessor::FindPlayer(guid); bot && bot->IsInWorld())
                {
                    uint32 rollOut = urand(1, 100);
                    if (g_DebugEnabled)
                        LOG_INFO("server.loading","[BotGuildHouse] Exit roll {} vs {} for {}",
                                 rollOut, g_ExitChancePercent, bot->GetName().c_str());
                    if (rollOut <= g_ExitChancePercent)
                    {
                        auto it = g_PreviousLocations.find(guid);
                        if (it != g_PreviousLocations.end())
                        {
                            auto const& t = it->second;
                            bot->TeleportTo(
                                std::get<0>(t), std::get<1>(t), std::get<2>(t),
                                std::get<3>(t), std::get<4>(t));
                            ChatHandler(bot->GetSession()).PSendSysMessage("PlayerBot: Left Guild House.");
                            if (g_DebugEnabled)
                                LOG_INFO("server.loading","[BotGuildHouse] Teleported out {}", bot->GetName().c_str());
                            g_PreviousLocations.erase(it);
                        }
                        else
                        {
                            // fallback to hearthstone
                            bot->CastSpell(bot, 8690, true);
                            ChatHandler(bot->GetSession()).PSendSysMessage("PlayerBot: Left Guild House (hearthstone).");
                            if (g_DebugEnabled)
                                LOG_INFO("server.loading","[BotGuildHouse] Fallback exit {} using hearthstone",
                                         bot->GetName().c_str());
                        }
                    }
                }
                vec.erase(vec.begin() + idx);
                if (vec.empty())
                    break;
            }
        }
    }

private:
    uint32 m_timer;  // Accumulates elapsed milliseconds
};

void Addmod_player_bot_guildhouseScripts()
{
    new BotGuildHouseTimerWorldScript();
}
