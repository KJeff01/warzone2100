
////////////////////////////////////////////////////////////////////////////////
// AI droid movement automation.
////////////////////////////////////////////////////////////////////////////////
//;; ## camManageGroup(group, order, data)
//;;
//;; Tell `libcampaign.js` to manage a certain group. The group would
//;; be permanently managed depending on the high-level orders given.
//;; For each order, data parameter is a JavaScript object that controls
//;; different aspects of behavior. The order parameter is one of:
//;; * `CAM_ORDER_ATTACK` Pursue human player, preferably around the given position.
//;;   The following optional data object fields are available, none of which is required:
//;;   * `pos` Position or list of positions to attack. If pos is a list, first positions in the list will be attacked first.
//;;   * `radius` Circle radius around `pos` to scan for targets.
//;;   * `fallback` Position to retreat.
//;;   * `morale` An integer from `1` to `100`. If that high percentage of the original group dies,
//;;     fall back to the fallback position. If new droids are added to the group, it can recover and attack again.
//;;   * `count` Override size of the original group. If unspecified, number of droids in the group at call time.
//;;     Retreat on low morale and regroup is calculated against this value.
//;;   * `repair` Health percentage to fall back to repair facility, if any.
//;;   * `regroup` If set to `true`, the group will not move forward unless it has at least `count` droids in its biggest cluster.
//;;     If `count` is set to `-1`, at least ⅔ of group's droids should be in the biggest cluster.
//;; * `CAM_ORDER_DEFEND` Protect the given position. If too far, retreat back there ignoring fire.
//;;   The following data object fields are available:
//;;   * `pos` Position to defend.
//;;   * `radius` Circle radius around `pos` to scan for targets.
//;;   * `count` Override size of the original group. If unspecified, number of droids in the group at call time.
//;;     Regroup is calculated against this value.
//;;   * `repair` Health percentage to fall back to repair facility, if any.
//;;   * `regroup` If set to `true`, the group will not move forward unless it has at least `count` droids in its biggest cluster.
//;;     If `count` is set to `-1`, at least ⅔ of group's droids should be in the biggest cluster.
//;; * `CAM_ORDER_PATROL` Move droids randomly between a given list of positions. The following data object fields are available:
//;;   * `pos` An array of positions to patrol between.
//;;   * `interval` Change positions every this many milliseconds.
//;;   * `count` Override size of the original group. If unspecified, number of droids in the group at call time.
//;;     Regroup is calculated against this value.
//;;   * `repair` Health percentage to fall back to repair facility, if any.
//;;   * `regroup` If set to `true`, the group will not move forward unless it has at least `count` droids in its biggest cluster.
//;;     If `count` is set to `-1`, at least ⅔ of group's droids should be in the biggest cluster.
//;; * `CAM_ORDER_COMPROMISE` Same as `CAM_ORDER_ATTACK`, just stay near the last (or only)
//;;   attack position instead of looking for the player around the whole map. Useful for offworld missions,
//;;   with player's LZ as the final position. The following data object fields are available:
//;;   * `pos` Position or list of positions to compromise.
//;;     If pos is a list, first positions in the list will be compromised first.
//;;   * `radius` Circle radius around `pos` to scan for targets.
//;;   * `count` Override size of the original group. If unspecified, number of droids in the group at call time.
//;;     Regroup is calculated against this value.
//;;   * `repair` Health percentage to fall back to repair facility, if any.
//;;   * `regroup` If set to `true`, the group will not move forward unless it has at least `count` droids in its biggest cluster.
//;;     If `count` is set to `-1`, at least ⅔ of group's droids should be in the biggest cluster.
//;; * `CAM_ORDER_FOLLOW` Assign the group to commander. The sub-order is defined to be given to the commander.
//;;   When commander dies, the group continues to execute the sub-order. The following data object fields are available:
//;;   * `droid` Commander droid label.
//;;   * `order` The order to give to the commander.
//;;   * `data` Data of the commander's order.
//;;   * `repair` Health percentage to fall back to repair facility, if any.
//;;
//;; @param {string} group
//;; @param {number} order
//;; @param {Object} data
//;; @returns {void}
//;;
function camManageGroup(group, order, data)
{
	let saneData = data;
	if (!camDef(saneData))
	{
		saneData = {};
	}
	if (camDef(saneData.pos)) // sanitize pos now to make ticks faster
	{
		if (camIsString(saneData.pos)) // single label?
		{
			saneData.pos = [ saneData.pos ];
		}
		else if (!camDef(saneData.pos.length)) // single position object?
		{
			saneData.pos = [ saneData.pos ];
		}
		for (let i = 0, l = saneData.pos.length; i < l; ++i) // array of labels?
		{
			saneData.pos[i] = camMakePos(saneData.pos[i]);
		}
	}
	if (camDef(__camGroupInfo[group]) && order !== __camGroupInfo[group].order)
	{
		camTrace("Group", group, "receives a new order:", camOrderToString(order));
	}
	__camGroupInfo[group] = {
		target: undefined,
		order: order,
		data: saneData,
		count: camDef(saneData.count) ? saneData.count : groupSize(group)
	};
	if (order === CAM_ORDER_FOLLOW)
	{
		const __COMMANDER_GROUP = camMakeGroup([getObject(data.droid)]);
		camManageGroup(__COMMANDER_GROUP, data.order, data.data);
	}
	// apply orders instantly
	queue("__camTacticsTickForGroup", __CAM_TICKS_PER_FRAME, group);
}

//;; ## camStopManagingGroup(group)
//;;
//;; Tell `libcampaign.js` to stop managing a certain group.
//;;
//;; @param {string} group
//;; @returns {void}
//;;
function camStopManagingGroup(group)
{
	if (!camDef(__camGroupInfo[group]))
	{
		camTrace("Not managing", group, "anyway");
		return;
	}
	camTrace("Cease managing", group);
	delete __camGroupInfo[group];
}

//;; ## camOrderToString(order)
//;;
//;; Print campaign order as string, useful for debugging.
//;;
//;; @param {number} order
//;; @returns {string}
//;;
function camOrderToString(order)
{
	let orderString;
	switch (order)
	{
		case CAM_ORDER_ATTACK:
			orderString = "ATTACK";
			break;
		case CAM_ORDER_DEFEND:
			orderString = "DEFEND";
			break;
		case CAM_ORDER_PATROL:
			orderString = "PATROL";
			break;
		case CAM_ORDER_COMPROMISE:
			orderString = "COMPROMISE";
			break;
		case CAM_ORDER_FOLLOW:
			orderString = "FOLLOW";
			break;
		default:
			orderString = "UNKNOWN";
	}
	return orderString;
}

//////////// privates
function __camFindGroupAvgCoordinate(groupID)
{
	const droids = enumGroup(groupID);
	const __LEN = droids.length;
	const avgCoord = {x: 0, y: 0};

	if (droids.length === 0)
	{
		return null;
	}

	for (let i = 0; i < __LEN; ++i)
	{
		const droid = droids[i];
		avgCoord.x += droid.x;
		avgCoord.y += droid.y;
	}

	// This global is constantly changing for the tactics code per group
	__camGroupAvgCoord.x = Math.floor(avgCoord.x / __LEN);
	__camGroupAvgCoord.y = Math.floor(avgCoord.y / __LEN);
}

function __camDistToGroupAverage(obj1, obj2)
{
	const __DIST1 = distBetweenTwoPoints(__camGroupAvgCoord.x, __camGroupAvgCoord.y, obj1.x, obj1.y);
	const __DIST2 = distBetweenTwoPoints(__camGroupAvgCoord.x, __camGroupAvgCoord.y, obj2.x, obj2.y);
	return (__DIST1 - __DIST2);
}

function __camUpdateSectors()
{
	const __SECTOR_CHUNK_SIZE = 8;
	const __SUBSECTOR_CHUNK_SIZE = (__SECTOR_CHUNK_SIZE / 2);
	if ((mapWidth * mapHeight) <= (__SECTOR_CHUNK_SIZE * __SECTOR_CHUNK_SIZE))
	{
		camDebug("Map is too small for sector analysis");
		return;
	}
	//Initialize the map sectors if not already done.
	if (__camRetreatSectorMap.length === 0)
	{
		let lastIdx = 0;
		let widthIdx = 0;
		let heightIdx = 0;
		const __WIDTH_LIMIT = Math.ceil(mapWidth / __SECTOR_CHUNK_SIZE);
		const __HEIGHT_LIMIT = Math.ceil(mapHeight / __SECTOR_CHUNK_SIZE);
		while (heightIdx < __HEIGHT_LIMIT)
		{
			if (widthIdx > __WIDTH_LIMIT)
			{
				widthIdx = 0;
				++heightIdx;
				continue;
			}
			let spX = (widthIdx * __SECTOR_CHUNK_SIZE);
			let spY = (heightIdx * __SECTOR_CHUNK_SIZE);
			if (spX > mapWidth) { spX = __camRetreatSectorMap[lastIdx].area.x1 + 1; }
			if (spY > mapHeight) { spY = __camRetreatSectorMap[lastIdx].area.y1 + 1; }
			let epX = ((spX + __SECTOR_CHUNK_SIZE) - 1);
			let epY = ((spY + __SECTOR_CHUNK_SIZE) - 1);
			if (epX > mapWidth) { epX = __camRetreatSectorMap[lastIdx].area.x2 + 1; }
			if (epY > mapHeight) { epY = __camRetreatSectorMap[lastIdx].area.y2 + 1; }
			let area = {x1: spX, y1: spY: idx, x2: epX, y2: epY};
			__camRetreatSectorMap.push({
				area: area,
				index: lastIdx,
				playerObjects: 0,
				poisoned: {state: false, lastCheck: 0},
				dangerous: false,
				artilleryWatched: false,
				antiAir: false
			});
			++lastIdx;
			++widthIdx;
		}
	}
	// Scan each sector and update their threat analysis data.
	for (let i = 0, len = __camRetreatSectorMap.length; i < len; ++i)
	{
		const sector = __camRetreatSectorMap[i];
		const objects = enumArea(sector.x1, sector.y1, sector.x2, sector.y2, CAM_HUMAN_PLAYER, false);
		sector.playerObjects = objects.length;

		for (let j = 0, len2 = objects.length; j < len2; ++j)
		{
			const obj = objects[j];
			if (obj.type === STRUCTURE)
			{
				if (obj.stattype === WALL || obj.stattype === GATE)
				{
					continue;
				}
				else if (obj.isSensor)
				{
					sector.artilleryWatched = true;
					sector.dangerous = true;
				}
				else if (obj.stattype === DEFENSE)
				{
					sector.dangerous = true;
					if (obj.canHitAir)
					{
						sector.antiAir = true;
					}
				}
			}
			else if (obj.type === DROID)
			{
				if (obj.isSensor)
				{
					sector.artilleryWatched = true;
					sector.dangerous = true;
				}
				else if (obj.droidType === DROID_WEAPON || obj.droidType === DROID_CYBORG)
				{
					sector.dangerous = true;
					if (obj.canHitAir)
					{
						sector.antiAir = true;
					}
				}
			}
		}
		__camRetreatSectorMap[i] = sector;
	}

	__camUnpoisonSectors();
}

// Something bad happened to a unit so poison sectors nearby it.
function __camPoisonSectors(pos)
{
	if (!camDef(pos))
	{
		camDebug("Bad or undefined position during sector poison attempt");
		return;
	}
	const __MAX_SECTORS_POISONED_AROUND_UNIT = 9;
	const __POISONED_DISTANCE = 6;
	let poisonedCount = 0;
	for (let i = 0, len = __camRetreatSectorMap.length; i < len; ++i)
	{
		let poison = false;
		const sector = __camRetreatSectorMap[i];
		if (camDist(sector.area.x1, sector.area.y1, pos.x, pos.y) <= __POISONED_DISTANCE) { poison = true; }
		else if (camDist(sector.area.x1, sector.area.y2, pos.x, pos.y) <= __POISONED_DISTANCE) { poison = true; }
		else if (camDist(sector.area.x2, sector.area.y1, pos.x, pos.y) <= __POISONED_DISTANCE) { poison = true; }
		else if (camDist(sector.area.x2, sector.area.y2, pos.x, pos.y) <= __POISONED_DISTANCE) { poison = true; }
		if (poison)
		{
			__camRetreatSectorMap[i].poisoned.state = true;
			__camRetreatSectorMap[i].poisoned.lastCheck = gameTime;
			++poisonedCount;
			if (poisonedCount >= __MAX_SECTORS_POISONED_AROUND_UNIT)
			{
				break; // Can only poison 9 sectors around self.
			}
		}
	}
}

// Allow units to feel these zones again. Potentially.
function __camUnpoisonSectors()
{
	const __UNPOISON_TIME = camSecondsToMilliseconds(30);
	for (let i = 0, len = __camRetreatSectorMap.length; i < len; ++i)
	{
		const sector = __camRetreatSectorMap[i];
		if (sector.poisoned.state && (sector.poisoned.lastCheck + __UNPOISON_TIME < gameTime))
		{
			__camRetreatSectorMap[i].poisoned.state = false;
		}
	}
}

// The retreating unit will run to a VERY safe position first, then feel out these zones and mark them as safe.
function __camFindSafeRetreatArea(obj, scanRadius)
{
	if (!camDef(obj) || (!camDef(obj.x) || !camDef(obj.y)))
	{
		camDebug("Invalid parameter");
		return;
	}
	const __SCAN_RADIUS = ((camDef(scanRadius)) ? scanRadius : 16);
	const center = {x: obj.x, y: obj.y};
	const potentialSectors = [];

	for (let i = 0, len = __camRetreatSectorMap.length; i < len; ++i)
	{
		const sector = __camRetreatSectorMap[i];
		if (sector.dangerous || sector.poisoned.state)
		{
			continue;
		}
		potentialSectors.push(sector);
	}

	potentialSectors = potentialSectors.sort(function(sec) { camDist(Math.floor((sec.area.x1 + sec.area.x2) / 2), Math.floor((sec.area.y1 + sec.area.y2) / 2), center.x, center.y) });

	for (let i = 0, len = potentialSectors.length; i < len; ++i)
	{
		const sector = potentialSectors[i];
		const midPos = {x: Math.floor((sec.area.x1 + sec.area.x2) / 2), y: Math.floor((sec.area.y1 + sec.area.y2) / 2)};
		const __MAX_ATTEMPTS = 20;
		let attempt = 0;
		while (attempt < __MAX_ATTEMPTS)
		{

		}
	}
}

function __camPickTarget(group)
{
	let targets = [];
	const gi = __camGroupInfo[group];
	const droids = enumGroup(group);
	__camFindGroupAvgCoordinate(group);
	switch (gi.order)
	{
		case CAM_ORDER_ATTACK:
		{
			if (camDef(gi.target))
			{
				targets = enumRange(gi.target.x, gi.target.y,__CAM_TARGET_TRACKING_RADIUS, CAM_HUMAN_PLAYER, false).filter((obj) => (
					obj.type === STRUCTURE || (obj.type === DROID && !isVTOL(obj))
				));
			}
		}
			// fall-through! we just don't track targets on COMPROMISE
		case CAM_ORDER_COMPROMISE:
		{
			if (camDef(gi.data.pos))
			{
				for (let i = 0; i < gi.data.pos.length; ++i)
				{
					const compromisePos = gi.data.pos[i];
					if (targets.length > 0)
					{
						break;
					}
					let radius = gi.data.radius;
					if (!camDef(radius))
					{
						radius = __CAM_PLAYER_BASE_RADIUS;
					}
					targets = enumRange(compromisePos.x, compromisePos.y, radius, CAM_HUMAN_PLAYER, false);
				}
			}
			if (gi.order === CAM_ORDER_COMPROMISE && targets.length === 0)
			{
				if (!camDef(gi.data.pos))
				{
					camDebug("'pos' is required for COMPROMISE order");
					return undefined;
				}
				else
				{
					targets = [ gi.data.pos[gi.data.pos.length - 1] ];
				}
			}
			const dr = droids[0];
			targets = targets.filter((obj) => (
				propulsionCanReach(dr.propulsion, dr.x, dr.y, obj.x, obj.y)
			));
			if (targets.length === 0)
			{
				targets = enumStruct(CAM_HUMAN_PLAYER).filter((obj) => (
					propulsionCanReach(dr.propulsion, dr.x, dr.y, obj.x, obj.y)
				));
				if (targets.length === 0)
				{
					targets = enumDroid(CAM_HUMAN_PLAYER).filter((obj) => (
						propulsionCanReach(dr.propulsion, dr.x, dr.y, obj.x, obj.y) &&
							(obj.type === STRUCTURE || (obj.type === DROID && !isVTOL(obj)))
					));
					if (targets.length === 0)
					{
						targets = enumDroid(CAM_HUMAN_PLAYER).filter((obj) => (
							propulsionCanReach(dr.propulsion, dr.x, dr.y, obj.x, obj.y)
						));
					}
				}
			}
			break;
		}
		case CAM_ORDER_DEFEND:
		{
			if (!camDef(gi.data.pos))
			{
				camDebug("'pos' is required for DEFEND order");
				return undefined;
			}
			const defendPos = gi.data.pos[0];
			let radius = gi.data.radius;
			if (!camDef(radius))
			{
				radius = __CAM_DEFENSE_RADIUS;
			}
			if (camDef(gi.target) && camDist(gi.target, defendPos) < radius)
			{
				targets = enumRange(gi.target.x, gi.target.y,
				                    __CAM_TARGET_TRACKING_RADIUS,
				                    CAM_HUMAN_PLAYER, false);
			}
			if (targets.length === 0)
			{
				targets = enumRange(defendPos.x, defendPos.y, radius, CAM_HUMAN_PLAYER, false);
			}
			if (targets.length === 0)
			{
				targets = [ defendPos ];
			}
			break;
		}
		default:
		{
			camDebug("Unsupported group order", gi.order, camOrderToString(gi.order));
			break;
		}
	}
	if (targets.length === 0)
	{
		return undefined;
	}
	targets.sort(__camDistToGroupAverage);
	const target = targets[0];
	if (camDef(target) && camDef(target.type) && target.type === DROID && camIsTransporter(target))
	{
		return undefined;
	}
	__camGroupInfo[group].target = { x: target.x, y: target.y };
	return __camGroupInfo[group].target;
}

function __camTacticsTick()
{
	let dt = __CAM_TICKS_PER_FRAME;
	for (const group in __camGroupInfo)
	{
		//Remove groups with no droids.
		if (groupSize(group) === 0)
		{
			let remove = true;
			const __REMOVABLE = __camGroupInfo[group].data.removable;
			//Useful if the group has manual management (seen in cam1-3 script).
			if (camDef(__REMOVABLE) && !__REMOVABLE)
			{
				remove = false;
			}
			if (remove)
			{
				camStopManagingGroup(group);
				break;
			}
		}
		queue("__camTacticsTickForGroup", dt, group);
		dt += __CAM_TICKS_PER_FRAME;
	}
	//Emulate a queue...
	removeTimer("__camTacticsTick");
	setTimer("__camTacticsTick", dt);
}

//Return the range (in tiles) a droid will scout for stuff to attack around it.
function __camScanRange(order, drType)
{
	let rng = __CAM_TARGET_TRACKING_RADIUS; //default
	switch (order)
	{
		case CAM_ORDER_ATTACK:
		case CAM_ORDER_DEFEND:
		case CAM_ORDER_FOLLOW:
			rng = __CAM_TARGET_TRACKING_RADIUS;
			break;
		case CAM_ORDER_PATROL:
			rng = 5;
			break;
		case CAM_ORDER_COMPROMISE:
			rng = 2; //very small so they don't track stuff too far
			break;
		default:
			camDebug("Unsupported group order", order, camOrderToString(order));
	}

	if (drType === DROID_SENSOR)
	{
		rng = Math.floor(rng * 1.5);
	}

	return rng;
}

function __camTacticsTickForGroup(group)
{
	const gi = __camGroupInfo[group];
	if (!camDef(gi))
	{
		return;
	}
	const rawDroids = enumGroup(group);
	if (rawDroids.length === 0)
	{
		return;
	}

	const __CLOSE_Z = 1;
	let healthyDroids = [];
	const repair = {
		hasFacility: enumStruct(rawDroids[0].player, REPAIR_FACILITY).length > 0,
		pos: (rawDroids[0].player === CAM_NEXUS) ? true : false,
		percent: camDef(gi.data.repair) ? gi.data.repair : 66,
	};

	//repair
	if (repair.hasFacility || camDef(repair.pos))
	{
		profile("__camUpdateSectors");
		for (let i = 0, len = rawDroids.length; i < len; ++i)
		{
			const droid = rawDroids[i];
			let repairLikeAction = false;

			if (droid.order === DORDER_RTR)
			{
				continue;
			}

			if (isVTOL(droid))
			{
				// Don't repair VTOLs.
			}
			//has a repair facility so use it
			else if (repair.hasFacility && camDef(gi.data.repair))
			{
				if (droid.health < repair.percent)
				{
					orderDroid(droid, DORDER_RTR);
					repairLikeAction = true;
				}
			}
			//Or they have auto-repair and run to some position for a while
			else if (!repair.hasFacility && repair.pos)
			{
				let lastHitSpecific = 0;
				if (camDef(gi.memberData))
				{
					for (let i = 0, len = gi.memberData.length; i < len; ++i)
					{
						const data = gi.memberData[i];
						if (droid.id === data.id)
						{
							lastHitSpecific = data.lastHit;
							break;
						}
					}
				}
				if (droid.health < repair.percent)
				{
					repairLikeAction = true;
					const __SHOULD_RUN = ((gameTime - lastHitSpecific) < __CAM_RUN_AWAY_WHEN_HIT_LAST);
					if ((droid.order !== DORDER_MOVE) && __SHOULD_RUN)
					{
						let loc = profile("__camFindSafeRetreatArea", {x: droid.x, y: droid.y});
						orderDroidLoc(droid, DORDER_MOVE, loc.x, loc.y);
					}
				}
			}

			if (!repairLikeAction)
			{
				healthyDroids.push(droid);
			}
		}
	}
	else
	{
		healthyDroids = rawDroids;
	}

	if (camDef(gi.data.regroup) && gi.data.regroup && healthyDroids.length > 0)
	{
		const ret = __camFindClusters(healthyDroids, __CAM_CLUSTER_SIZE);
		const groupX = ret.xav[ret.maxIdx];
		const groupY = ret.yav[ret.maxIdx];
		const droids = ret.clusters[ret.maxIdx];

		for (let i = 0, len = ret.clusters.length; i < len; ++i)
		{
			if (i !== ret.maxIdx) // move other droids towards main cluster
			{
				for (let j = 0, len2 = ret.clusters[i].length; j < len2; ++j)
				{
					const droid = ret.clusters[i][j];
					if (droid.order !== DORDER_RTR)
					{
						orderDroidLoc(droid, DORDER_MOVE, groupX, groupY);
					}
				}
			}
		}

		// not enough droids grouped?
		if ((gi.count < 0) ? (ret.maxCount < (groupSize(group) * 0.66)) : (ret.maxCount < gi.count))
		{
			const __HIT_RECENTLY = ((gameTime - gi.lastHit) < __CAM_FALLBACK_TIME_ON_REGROUP);

			for (let i = 0, len = droids.length; i < len; ++i)
			{
				const droid = droids[i];
				if (droid.order === DORDER_RTR)
				{
					continue;
				}

				if (__HIT_RECENTLY && (enumStruct(droid.player, HQ).length > 0))
				{
					if (droid.order !== DORDER_RTB)
					{
						orderDroid(droid, DORDER_RTB);
					}
				}
				else if (droid.order !== DORDER_HOLD)
				{
					orderDroid(droid, DORDER_HOLD);
				}
			}
			return;
		}
	}

	//Target choosing
	let target;
	let patrolPos;

	switch (gi.order)
	{
		case CAM_ORDER_ATTACK:
		case CAM_ORDER_DEFEND:
		case CAM_ORDER_COMPROMISE:
			target = __camPickTarget(group);
			if (!camDef(target))
			{
				return;
			}
			break;
		case CAM_ORDER_PATROL:
		case CAM_ORDER_FOLLOW:
			//do nothing here
			break;
		default:
			camDebug("Unknown group order given: " + gi.order);
			return;
	}

	const __DEFENDING = (gi.order === CAM_ORDER_DEFEND);
	const __TRACK = (gi.order === CAM_ORDER_COMPROMISE);

	for (let i = 0, len = healthyDroids.length; i < len; ++i)
	{
		const droid = healthyDroids[i];
		const __VTOL_UNIT = (droid.type === DROID && isVTOL(droid));

		if (droid.player === CAM_HUMAN_PLAYER)
		{
			camDebug("Controlling a human player's droid");
		}

		//Rearm vtols.
		if (__VTOL_UNIT)
		{
			const __ARM = droid.weapons[0].armed;
			const __IS_REARMING = droid.order === DORDER_REARM;

			if ((__ARM < 1) || (__IS_REARMING && (__ARM < 100 || droid.health < 100)))
			{
				const __HAVE_PADS = enumStruct(droid.player, REARM_PAD).length > 0;
				if (__HAVE_PADS && !__IS_REARMING)
				{
					orderDroid(droid, DORDER_REARM);
				}
				continue; //Rearming. Try not to attack stuff.
			}
		}

		if (gi.order === CAM_ORDER_FOLLOW)
		{
			const commander = getObject(gi.data.droid);
			if (commander === null)
			{
				// the commander is dead? let the group execute his last will.
				camManageGroup(group, gi.data.order, gi.data.data);
				return;
			}
			if (droid.droidType !== DROID_COMMAND && droid.order !== DORDER_COMMANDERSUPPORT)
			{
				orderDroidObj(droid, DORDER_COMMANDERSUPPORT, commander);
				continue;
			}
		}

		if (gi.order === CAM_ORDER_DEFEND)
		{
			// fall back to defense position
			const dPos = gi.data.pos[0];
			const __DIST = camDist(droid.x, droid.y, dPos.x, dPos.y);
			let radius = gi.data.radius;
			if (!camDef(radius))
			{
				radius = __CAM_DEFENSE_RADIUS;
			}
			if (__DIST > radius)
			{
				orderDroidLoc(droid, DORDER_MOVE, target.x, target.y);
				continue;
			}
		}

		if (gi.order === CAM_ORDER_PATROL)
		{
			if (!camDef(gi.data.interval))
			{
				gi.data.interval = camSecondsToMilliseconds(60);
			}
			if (!camDef(gi.lastmove) || !camDef(gi.lastspot))
			{
				gi.lastspot = 0;
				gi.lastmove = 0;
			}
			else
			{
				if (gameTime - gi.lastmove > gi.data.interval)
				{
					// find random new position to visit
					const list = [];
					for (let j = 0, len2 = gi.data.pos.length; j < len2; ++j)
					{
						if (j !== gi.lastspot)
						{
							list.push(j);
						}
					}
					gi.lastspot = list[camRand(list.length)];
					gi.lastmove = gameTime;
				}
			}
			patrolPos = gi.data.pos[gi.lastspot];
			//I will leave this here for clarity so that it don't look
			//like patrol picks a target.
			if (camDef(patrolPos))
			{
				target = camMakePos(patrolPos);
			}
		}

		if (camDef(target) && camDist(droid.x, droid.y, target.x, target.y) >= __CAM_CLOSE_RADIUS)
		{
			let closeByObj;
			const __ARTILLERY_LIKE = (droid.isCB || droid.hasIndirect || droid.isSensor);
			let closeBy = enumRange(droid.x, droid.y, __camScanRange(gi.order, droid.droidType), CAM_HUMAN_PLAYER, __TRACK);

			if (closeBy.length > 0)
			{
				__camFindGroupAvgCoordinate(group);
				closeBy.sort(__camDistToGroupAverage);
				closeByObj = closeBy[0];
			}

			//We only care about explicit observe/attack if the object is close
			//on the z coordinate. We should not chase things up or down hills
			//that may be far away, at least path-wise.
			if (closeByObj && !__VTOL_UNIT && !__ARTILLERY_LIKE)
			{
				if (Math.abs(droid.z - closeByObj.z) > __CLOSE_Z)
				{
					closeByObj = undefined;
				}
			}

			if (closeByObj && ((closeByObj.type === DROID) && isVTOL(closeByObj) && (isVTOL(droid) || !droid.canHitAir)))
			{
				closeByObj = undefined;
			}

			if (!__DEFENDING && closeByObj)
			{
				if (droid.droidType === DROID_SENSOR)
				{
					orderDroidObj(droid, DORDER_OBSERVE, closeByObj);
				}
				else
				{
					orderDroidObj(droid, DORDER_ATTACK, closeByObj);
				}
			}
			else
			{
				if (__DEFENDING || !(__ARTILLERY_LIKE || __VTOL_UNIT))
				{
					orderDroidLoc(droid, DORDER_MOVE, target.x, target.y);
				}
				else
				{
					orderDroidLoc(droid, DORDER_SCOUT, target.x, target.y);
				}
			}
		}
	}
}

function __camCheckGroupMorale(group)
{
	const gi = __camGroupInfo[group];
	if (!camDef(gi.data.morale))
	{
		return;
	}
	// morale is %.
	const __MSIZE = Math.floor((100 - gi.data.morale) * gi.count / 100);
	const __GSIZE = groupSize(group);
	switch (gi.order)
	{
		case CAM_ORDER_ATTACK:
		{
			if (__GSIZE > __MSIZE)
			{
				break;
			}
			camTrace("Group", group, "falls back");
			gi.order = CAM_ORDER_DEFEND;
			// swap pos and fallback
			const temp = gi.data.pos;
			gi.data.pos = [ camMakePos(gi.data.fallback) ];
			gi.data.fallback = temp;
			// apply orders instantly
			queue("__camTacticsTickForGroup", __CAM_TICKS_PER_FRAME, group);
			break;
		}
		case CAM_ORDER_DEFEND:
		{
			if (__GSIZE <= __MSIZE)
			{
				break;
			}
			camTrace("Group", group, "restores");
			gi.order = CAM_ORDER_ATTACK;
			// swap pos and fallback
			const temp = gi.data.pos;
			gi.data.pos = gi.data.fallback;
			gi.data.fallback = temp[0];
			// apply orders instantly
			queue("__camTacticsTickForGroup", __CAM_TICKS_PER_FRAME, group);
			break;
		}
		default:
		{
			camDebug("Group order doesn't support morale", camOrderToString(gi.order));
			break;
		}
	}
}
