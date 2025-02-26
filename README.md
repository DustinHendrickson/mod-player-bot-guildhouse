# AzerothCore Module: PlayerBot Guild House Integration

> **Disclaimer:** This module requires both the [Playerbots module](https://github.com/liyunfan1223/mod-playerbots) and the [Guild House module](https://github.com/azerothcore/mod-guildhouse). Please ensure that both modules are installed and running before using this module.

Overview
--------
The PlayerBot Guild House module for AzerothCore automatically teleports guild bots to the guild house (GM Island) when a real (human) player logs in, enters zone 876, or is detected during a periodic safety check. This gives the guildhouse a more lively atmosphere to emulate an active real guild.

Features
--------
- **Automatic Teleportation:**  
  When a real player logs in or enters zone 876 (GM Island), the module teleports all online, safe guild bots of the same guild to the designated guild house location.

- **Periodic Safety Checks:**  
  A configurable timer periodically scans for real players in zone 876 and teleports any safe guild bots that were missed initially due to being unsafe to teleport.

- **Safety Verification:**  
  Bots are only teleported if they meet safety criteria such as being alive, not in combat, and not engaged in battlegrounds, arenas, or other conflicting activities. Bots in groups with non-bot players are excluded to prevent disrupting ongoing group activities.

- **Guild House Data Retrieval:**  
  The module queries the guild house database (provided by the Guild House module) to fetch the appropriate teleportation coordinates, ensuring bots are moved to the correct location and phase.

Installation
------------
1. **Clone the Module**  
   Ensure that the AzerothCore Playerbots fork and the Guild House module are installed and running. Clone the module into your AzerothCore modules directory:
   
       cd /path/to/azerothcore/modules
       git clone https://github.com/DustinHendrickson/mod-player-bot-guildhouse.git

2. **Recompile AzerothCore**  
   Rebuild the project with the new module:
   
       cd /path/to/azerothcore
       mkdir build && cd build
       cmake ..
       make -j$(nproc)

3. **Configure the Module**  
   Rename the configuration file:
   
       mv /path/to/azerothcore/modules/mod_player_bot_guildhouse.conf.dist /path/to/azerothcore/modules/mod_player_bot_guildhouse.conf

4. **Restart the Server**  
   Launch the world server:
   
       ./worldserver

Configuration Options
---------------------
Edit your `mod_player_bot_guildhouse.conf` file to customize the module behavior. Below is the configuration for the PlayerBot Guild House module:

    [worldserver]
    
    ##############################################
    # mod-player-bot-guildhouse configuration
    ##############################################
    #
    #    PlayerbotGuildhouse.MoveBotsToGuildhouseCheckFrequency
    #        Description: The frequency (in seconds) at which the check to move safe guildbots is performed.
    #        Default:     60
    PlayerbotGuildhouse.MoveBotsToGuildhouseCheckFrequency = 60

Debugging and Troubleshooting
-----------------------------
- **Module Not Triggering Teleports:**  
  Ensure that real players are present in zone 876 (GM Island) and that both the Playerbots and Guild House modules are correctly installed and configured.

- **Safety Checks Preventing Teleportation:**  
  Bots engaged in combat, flight, battlegrounds, or grouped with non-bot players will not be teleported. Verify bot statuses if unexpected behavior occurs.

License
-------
This module is released under the GNU GPL v2 license, in accordance with AzerothCore's licensing model.

Contribution
------------
Created by Dustin Hendrickson.

Pull requests and issues are welcome. Please ensure that contributions adhere to AzerothCore's coding standards.
