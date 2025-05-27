# AzerothCore Module: PlayerBot Guild House Integration

> **Disclaimer:** This module requires both the [Playerbots module](https://github.com/liyunfan1223/mod-playerbots) and the [Guild House module](https://github.com/azerothcore/mod-guildhouse). Ensure both are installed and loaded before enabling this integration.

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Installation](#installation)
4. [Configuration](#configuration)
5. [Debugging and Troubleshooting](#debugging-and-troubleshooting)
6. [License](#license)
7. [Contribution](#contribution)

## Overview

This module runs a repeating world-script timer that periodically teleports guild bots into and out of their guild house (zone 876, GM Island). Each cycle consists of an **entry phase**—moving a random subset of safe bots into the house—and an **exit phase**—returning a random subset back to their saved locations (or hearthstone fallback). On server startup it also discovers any bots already in zone 876 and tracks them.

## Features

- **Periodic Teleport Cycle**  
  A configurable timer triggers every **TeleportCycleFrequency** seconds to process all guild houses.

- **Entry Phase**  
  For each guild record in `guild_house`, if the real-player requirement is met, the script:
    1. Gathers all online, safe bots (alive, out of combat, not in battlegrounds/arenas/LFG/flight, not already in zone 876, not grouped with real players).  
    2. Picks up to **StaggeredTeleport.BatchSize** bots at random.  
    3. Rolls for each bot against **EntryChancePercent**.  
    4. Saves its current position, teleports it to the house location and phase, sends a system message, then staggers next teleport by **DelaySecondsMin–DelaySecondsMax** seconds.

- **Exit Phase**  
  For each guild’s bots currently tracked in the house:
    1. Picks up to **StaggeredTeleport.BatchSize** bots at random.  
    2. Rolls each against **ExitChancePercent**.  
    3. If passed, restores the bot to its saved location (or casts hearthstone spell 8690 if lost) and sends a system message.

- **Real Player Requirement**  
  If **RequireRealPlayer** is `true`, entry phase only runs for guilds with at least one real human online.

- **Position Persistence**  
  Original bot positions are stored in memory each cycle and used for exit teleportation.

- **Server Restart Sync**  
  On startup the script detects any bots already in zone 876 and includes them in the exit tracking list.

- **Debug Logging**  
  When **DebugEnabled** is `true`, detailed INFO logs are emitted at each step.

## Installation

1. **Clone the Module**  
    cd /path/to/azerothcore/modules  
    git clone https://github.com/DustinHendrickson/mod-player-bot-guildhouse.git

2. **Recompile AzerothCore**  
    cd /path/to/azerothcore  
    mkdir build && cd build  
    cmake ..  
    make -j$(nproc)

3. **Enable and Configure**  
    mv /path/to/azerothcore/modules/mod_player_bot_guildhouse.conf.dist /path/to/azerothcore/modules/mod_player_bot_guildhouse.conf

4. **Restart the Server**  
    ./worldserver

## Configuration

Edit `mod_player_bot_guildhouse.conf`:

    # Interval between teleport cycles in seconds.
    PlayerbotGuildhouse.TeleportCycleFrequency      = 120

    # Maximum bots to teleport per phase.
    PlayerbotGuildhouse.StaggeredTeleport.BatchSize = 5

    # Require at least one real player in guild before entry phase.
    PlayerbotGuildhouse.RequireRealPlayer         = true

    # Percent chance for a candidate bot to enter the guild house.
    PlayerbotGuildhouse.EntryChancePercent        = 60

    # Percent chance for a tracked bot to exit the guild house.
    PlayerbotGuildhouse.ExitChancePercent         = 40

    # Enable detailed debug logging.
    PlayerbotGuildhouse.DebugEnabled              = false

## Debugging and Troubleshooting

- **No Bots Moving**  
    - Confirm the cycle frequency is reached (`TeleportCycleFrequency`).  
    - If `RequireRealPlayer` is `true`, verify at least one human is online in the guild.

- **Bots Skipped by Safety Checks**  
    - Ensure bots are alive, out of combat, not in battlegrounds/arenas/LFG queues, not flying, and not in zone 876.  
    - Check that bots are not grouped with any real players.

- **Unexpected Exit Behavior**  
    - Verify that saved positions exist for each bot.  
    - If missing, bots use the hearthstone fallback spell 8690.

- **Enable Debug Logs**  
    - Set `DebugEnabled = true` to view detailed INFO messages in your server console.

## License

This module is released under the GNU GPL v2 license, matching AzerothCore’s licensing.

## Contribution

Created and maintained by Dustin Hendrickson. Pull requests and issues are welcome. Please adhere to AzerothCore coding standards.