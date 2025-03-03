function setupResearch(player)	// inside hackNetOff()
{
	enableResearch("R-Sys-Sensor-Turret01", player);
	enableResearch("R-Wpn-MG1Mk1", player);
	enableResearch("R-Sys-Engineering01", player);

	// Give Nexus AIs a research speed boost on Hard and above.
	if (!challenge &&
		playerData[player].isAI &&
		(playerData[player].difficulty >= HARD) &&
		(playerData[player].name.toLowerCase().includes("nexus")))
	{
		enableResearch("R-Struc-Research-Upgrade-Nexus", player);
		completeResearch("R-Struc-Research-Upgrade-Nexus", player);
	}
}
