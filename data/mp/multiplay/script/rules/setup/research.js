function setupResearch(player)	// inside hackNetOff()
{
	enableResearch("R-Sys-Sensor-Turret01", player);
	enableResearch("R-Wpn-MG1Mk1", player);
	enableResearch("R-Sys-Engineering01", player);

	// Research speed cheat similar to the original version of Nexus.
	if (!isMultiplayer && playerData[player].isAI && (playerData[player].difficulty >= HARD) && (playerData[player].name === "Nexus"))
	{
		for (var i = 0; i < maxPlayers; ++i)
		{
			if (allianceExistsBetween(i, player))
			{
				completeResearch("R-Struc-Research-Upgrade-Nexus", i);
			}
		}
	}
}
