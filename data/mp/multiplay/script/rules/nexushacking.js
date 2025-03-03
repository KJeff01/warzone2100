// Nexus AI absorb functionality. Multiplayer compatible!

function nex_defined(what)
{
    return (typeof what !== "undefined");
}

function nex_playerHasBuiltHQ(player)
{
    const hqs = enumStruct(player, HQ);

    for (let i = 0, len = hqs.length; i < len; ++i)
    {
        const obj = hqs[i];
        if (obj.status === BUILT)
        {
            return true;
        }
    }

    return false;
}

function nex_playerIsInsaneNexus(player)
{
    return (playerData[player].isAI &&
        (playerData[player].difficulty >= INSANE) &&
        (playerData[player].name.toLowerCase().includes("nexus")));
}

function nex_isPlayerAlive(player)
{
    const FAC_STAT = "A0LightFactory";
    const CYB_STAT = "A0CyborgFactory"

    if ((countStruct(FAC_STAT, player) +
        countStruct(CYB_STAT, player) +
        countDroid(DROID_ANY, player)) > 0)
    {
        return true;
    }

    return false;
}

function nex_getAliveEnemyPlayers(player)
{
    const numEnemies = [];
    const DERRICK_STAT = "A0ResourceExtractor";
    const FACTORY_STAT = "A0BaBaFactory";

    for (let i = 0; i < maxPlayers; ++i)
    {
        if (i !== player && !allianceExistsBetween(player, i) && nex_isPlayerAlive(i))
        {
            numEnemies.push(i);
        }
    }

    if (nex_defined(scavengerPlayer) &&
        (countStruct(FACTORY_STAT, scavengerPlayer) +
        countStruct(DERRICK_STAT, scavengerPlayer) +
        countDroid(DROID_ANY, scavengerPlayer)) > 0)
    {
        numEnemies.push(scavengerPlayer);
    }

    return numEnemies;
}

function nex_nexusAbsorb()
{
    let minStartTime = (12 * 60 * 1000);
    // Change starting time based off base level.
    if (baseType === CAMP_BASE)
    {
        minStartTime = (10 * 60 * 1000);
    }
    else if (baseType === CAMP_WALLS)
    {
        minStartTime = (8 * 60 * 1000);
    }
    // Give other players time before hacking starts.
	if (gameTime <= minStartTime)
	{
		return;
	}

    for (let i = 0; i < maxPlayers; ++i)
    {
        // Randomly fail and can't hack without HQ, Also only applies to Insane Nexus AIs.
    	if (!nex_playerIsInsaneNexus(i) || (syncRandom(100) > 20) || !nex_playerHasBuiltHQ(i))
    	{
    		continue;
    	}

    	const objects = [];
    	const enemies = nex_getAliveEnemyPlayers(i);
    	if (enemies.length === 0)
    	{
    		return;
    	}

    	const RAND_ENEMY = enemies[syncRandom(enemies.length)];
        // Enemies of this Nexus AI with Resistance Circuits researched have higher chances to resist hack attempts.
        if (getResearch("R-Sys-Resistance-Circuits", RAND_ENEMY).done && (syncRandom(100) < 15))
        {
            continue;
        }
    	const ABSORB_TYPE = syncRandom(3);
        const DONATE_CHANCE = 40;

    	if (ABSORB_TYPE === 0 || ABSORB_TYPE === 2)
    	{
    		const droids = enumDroid(RAND_ENEMY);
    		if (droids.length > 0)
    		{
    			objects.push(droids[syncRandom(droids.length)]);
    		}
    	}
    	if (ABSORB_TYPE === 1 || ABSORB_TYPE === 2)
    	{
    		const structs = enumStruct(RAND_ENEMY).filter(function(obj) {
    			return obj.status === BUILT;
    		});
    		if (structs.length > 0)
    		{
    			objects.push(structs[syncRandom(structs.length)]);
    		}
    	}

    	for (let x = 0, len = objects.length; x < len; ++x)
    	{
    		const obj = objects[x];
    		if (syncRandom(100) < DONATE_CHANCE)
    		{
    			if (!donateObject(obj, i))
    			{
    				removeObject(obj, true);
    			}
    		}
    		else
    		{
    			removeObject(obj, true);
    		}
    	}
    }
}
