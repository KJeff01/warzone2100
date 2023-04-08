
//Expects an object with a structure's stattype and status.
function structureTypeValue(what)
{
	if (!defined(what))
	{
		debugMessage("structureTypeValue. Undefined parameter.");
		return 0;
	}

	if (!defined(what.stattype) || !defined(what.status))
	{
		debugMessage("structureTypeValue. No stattype or status property.");
		return 0;
	}

	for (let i = 0, len = STANDARD_TARGET_WEIGHTS.length; i < len; ++i)
	{
		var target = STANDARD_TARGET_WEIGHTS[i];

		if (what.stattype === target.stat)
		{
			return what.status === BUILT ? target.value : Math.floor(target.value * 0.67);
		}
	}

	return 1;
}

function hasDefinableTarget()
{
	if (!defined(targetInfo.what))
	{
		return false;
	}

	var type = targetInfo.what.type;
	var player = targetInfo.what.player;
	var id = targetInfo.what.id;

	if (allianceExistsBetween(me, player))
	{
		resetTargetData();
		return false;
	}

	if (getObject(type, player, id) === null)
	{
		targetInfo.what = undefined;
		return false;
	}

	return true;
}

function orderGroupLoc(group, x, y, order)
{
	if (!defined(group))
	{
		debugMessage("orderGroupLoc. Undefined group.");
		return false;
	}

	if (!defined(x) || !defined(y))
	{
		debugMessage("orderGroupLoc. Undefined x or y coordinate.");
		return false;
	}

	if (!defined(order))
	{
		order = DORDER_SCOUT;
	}

	var droids = enumGroup(group);

	for (let i = 0, len = droids.length; i < len; ++i)
	{
		var droid = droids[i];

		if (droid.order !== DORDER_RTR)
		{
			orderDroidLoc(droid, order, x, y);
		}
	}
}

function findMostValuableTarget()
{
	var enemy = getCurrentEnemy();

	if (!defined(enemy))
	{
		return; //Stop self attacking behavior since getCurrentEnemy() might return undefined
	}

	//Now focuses on what is visible first!
	var objects = enumArea(0, 0, mapWidth, mapHeight, ENEMIES, true);

	if (objects.length === 0)
	{
		objects = enumStruct(enemy);

		if (objects.length === 0)
		{
			objects = enumDroid(enemy);
		}
	}

	var bestTarget;
	var bestValue = 0;

	for (let i = 0, len = objects.length; i < len; ++i)
	{
		var obj = objects[i];

		if (allianceExistsBetween(me, obj.player))
		{
			return 0;
		}

		var currentValue;

		if (obj.type !== STRUCTURE)
		{
			currentValue = 1; //Very low priority target
		}
		else
		{
			currentValue = structureTypeValue({stattype: obj.stattype, status: obj.status});
		}

		if (currentValue > bestValue)
		{
			bestValue = currentValue;
			bestTarget = {type: obj.type, player: obj.player, id: obj.id};
		}
	}

	if (bestValue > 0)
	{
		setObjectAsTarget(bestTarget);
	}
}

function doAllOutAttack()
{
	if (hasDefinableTarget() && !helpingAlly())
	{
		const MIN_ATTACKERS = 10; //was 40 originally.
		var droids = enumGroup(groups.attackers);
		var len = droids.length;
		var target = getObject(targetInfo.what.type, targetInfo.what.player, targetInfo.what.id);

		if (len < MIN_ATTACKERS)
		{
			return;
		}

		for (let i = 0; i < len; ++i)
		{
			var droid = droids[i];

			//eventAttacked can snatch one of these droids and make them focus
			//on something that attacked them while moving to the target. Allow
			//a small chance to refocus on something else if already attacking.
			if (droid.order !== DORDER_RTR)
			{
				if (random(100) < 92 && droid.order === DORDER_ATTACK)
				{
					continue;
				}

				orderDroidObj(droid, DORDER_ATTACK, target);
			}
		}
	}
}

// Check the status of our base threat (eventAttacked does the actual defending).
function watchBaseThreat()
{
	// See if we can stop defending
	if (defendingOwnBase() === true && numEnemiesInBase() === 0)
	{
		stopHelpingAlly(); //Stop defending our own base.
		//Let allies know we don't need their help anymore
		chat(ALLIES, REQUESTS.safety);

		return false;
	}

	// See if our base is in trouble
	return baseInTrouble();
}

// Nexus absorb attack like in Gamma campaign! Does not work in online multiplayer.
function nexusAbsorb()
{
	if (gameTime <= minutesToMilliseconds(15))
	{
		return;
	}
	if ((random(100) > 20) || (enumStruct(me, HQ).length === 0))
	{
		return;
	}

	var stuff = [];
	var enemies = getAliveEnemyPlayers();

	if (enemies.length === 0)
	{
		return;
	}

	var randomEnemy = enemies[random(enemies.length)];
	var typeOfAbsorb = random(3);

	if (typeOfAbsorb === 0 || typeOfAbsorb === 2)
	{
		var droids = enumDroid(randomEnemy);
		if (droids.length > 0)
		{
			stuff.push(droids[random(droids.length)]);
		}
	}
	if (typeOfAbsorb === 1 || typeOfAbsorb === 2)
	{
		var structs = enumStruct(randomEnemy).filter(function(obj) {
			return obj.status === BUILT;
		});
		if (structs.length > 0)
		{
			stuff.push(structs[random(structs.length)]);
		}
	}

	for (var i = 0, len = stuff.length; i < len; ++i)
	{
		var obj = stuff[i];
		if (!(obj.type === DROID && obj.droidType === DROID_CONSTRUCT) && (random(100) < 40))
		{
			if (!donateObject(obj, me))
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

//Spread out over 2 ticks.
function tacticsMain()
{
	queue("findMostValuableTarget", TICK);

	if (!watchBaseThreat())
	{
		queue("doAllOutAttack", TICK * 2);
	}
}
