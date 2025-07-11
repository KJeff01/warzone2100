/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2020  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/**
 * @file stats.c
 *
 * Store common stats for weapons, components, brains, etc.
 *
 */
#include <string.h>
#include <algorithm>

#include "lib/framework/frame.h"
#include "lib/framework/strres.h"
#include "lib/framework/frameresource.h"
#include "lib/framework/fixedpoint.h"
#include "lib/ivis_opengl/imd.h"
#include "lib/gamelib/gtime.h"
#include "objects.h"
#include "stats.h"
#include "map.h"
#include "main.h"
#include "lib/sound/audio_id.h"
#include "projectile.h"
#include "text.h"
#include "notifications.h"
#include <unordered_map>

#define WEAPON_TIME		100

#define DEFAULT_DROID_RESISTANCE	150

/* The stores for the different stats */
std::vector<BODY_STATS> asBodyStats;
std::vector<BRAIN_STATS> asBrainStats;
std::vector<PROPULSION_STATS> asPropulsionStats;
std::vector<SENSOR_STATS> asSensorStats;
std::vector<ECM_STATS> asECMStats;
std::vector<REPAIR_STATS> asRepairStats;
std::vector<WEAPON_STATS> asWeaponStats;
std::vector<CONSTRUCT_STATS> asConstructStats;
std::vector<PROPULSION_TYPES> asPropulsionTypes;
static int		*asTerrainTable;

//used to hold the modifiers cross refd by weapon effect and propulsion type
WEAPON_MODIFIER		asWeaponModifier[WE_NUMEFFECTS][PROPULSION_TYPE_NUM];
WEAPON_MODIFIER		asWeaponModifierBody[WE_NUMEFFECTS][SIZE_NUM];

//stores for each players component states - can be either UNAVAILABLE, REDUNDANT, FOUND or AVAILABLE
UBYTE		*apCompLists[MAX_PLAYERS][COMP_NUMCOMPONENTS];

//store for each players Structure states
UBYTE		*apStructTypeLists[MAX_PLAYERS];

static std::unordered_map<WzString, BASE_STATS *> lookupStatPtr;
static std::unordered_map<WzString, COMPONENT_STATS *> lookupCompStatPtr;
static size_t statModelLoadingFailures = 0;

static bool getMovementModel(const WzString &movementModel, MOVEMENT_MODEL *model);
static bool statsGetAudioIDFromString(const WzString &szStatName, const WzString &szWavName, int *piWavID);

/***********************************************************************************
*	Dealloc the extra storage tables
***********************************************************************************/
//Deallocate the storage assigned for the Propulsion Types table
static void deallocPropulsionTypes()
{
	asPropulsionTypes.clear();
}

//dealloc the storage assigned for the terrain table
static void deallocTerrainTable()
{
	free(asTerrainTable);
	asTerrainTable = nullptr;
}

/*******************************************************************************
*		Generic stats macros/functions
*******************************************************************************/

/* Macro to allocate memory for a set of stats */
#define ALLOC_STATS(numEntries, list, type) \
	ASSERT((numEntries) < ~STAT_MASK + 1, "Number of stats entries too large for " #type);\
	if (!list.empty()) list.clear(); \
	list.resize(numEntries); \
	return true

/*Deallocate all the stats assigned from input data*/
bool statsShutDown()
{
	lookupStatPtr.clear();
	lookupCompStatPtr.clear();

	asWeaponStats.clear();
	asBrainStats.clear();
	asPropulsionStats.clear();
	asRepairStats.clear();
	asConstructStats.clear();
	asECMStats.clear();
	asSensorStats.clear();
	asBodyStats.clear();
	deallocPropulsionTypes();
	deallocTerrainTable();

	statModelLoadingFailures = 0;

	return true;
}

size_t getStatModelLoadingFailures()
{
	return statModelLoadingFailures;
}

/*******************************************************************************
*		Allocate stats functions
*******************************************************************************/
/* Allocate Weapon stats */
bool statsAllocWeapons(UDWORD	numStats)
{
	ALLOC_STATS(numStats, asWeaponStats, WEAPON_STATS);
}
/* Allocate Body Stats */
bool statsAllocBody(UDWORD	numStats)
{
	ALLOC_STATS(numStats, asBodyStats, BODY_STATS);
}
/* Allocate Brain Stats */
bool statsAllocBrain(UDWORD	numStats)
{
	ALLOC_STATS(numStats, asBrainStats, BRAIN_STATS);
}
/* Allocate Propulsion Stats */
bool statsAllocPropulsion(UDWORD	numStats)
{
	ALLOC_STATS(numStats, asPropulsionStats, PROPULSION_STATS);
}
/* Allocate Sensor Stats */
bool statsAllocSensor(UDWORD	numStats)
{
	ALLOC_STATS(numStats, asSensorStats, SENSOR_STATS);
}
/* Allocate Ecm Stats */
bool statsAllocECM(UDWORD	numStats)
{
	ALLOC_STATS(numStats, asECMStats, ECM_STATS);
}

/* Allocate Repair Stats */
bool statsAllocRepair(UDWORD	numStats)
{
	ALLOC_STATS(numStats, asRepairStats, REPAIR_STATS);
}

/* Allocate Construct Stats */
bool statsAllocConstruct(UDWORD	numStats)
{
	ALLOC_STATS(numStats, asConstructStats, CONSTRUCT_STATS);
}

/*******************************************************************************
*		Load stats functions
*******************************************************************************/

static optional<WzString> deprecatedModelUpgrade(WzString& filename)
{
	static const std::unordered_map<WzString, WzString> deprecatedModelNames = {
		// base
		{"mibnkdrl.pie", "mibnkdr.pie"},
		{"mibnkdrr.pie", ""},
		{"prhlhtr3.pie", "prhhtr3.pie"},
		{"prhrhtr3.pie", ""},
		{"prhltrk3.pie", "prhtrk3.pie"},
		{"prhrtrk3.pie", ""},
		{"prhlvtl1.pie", "prhvtl1.pie"},
		{"prhrvtl1.pie", ""},
		{"prhlvtl1.pie", "prhvtl1.pie"},
		{"prhrvtl1.pie", ""},
		{"prhlvtl2.pie", "prhvtl2.pie"},
		{"prhrvtl2.pie", ""},
		{"prhlvtl3.pie", "prhvtl3.pie"},
		{"prhrvtl3.pie", ""},
		{"prhlvtl4.pie", "prhvtl4.pie"},
		{"prhrvtl4.pie", ""},
		{"prhlwhl1.pie", "prhwhl1.pie"},
		{"prhrwhl1.pie", ""},
		{"prllhtr1.pie", "prlhtr1.pie"},
		{"prlrhtr1.pie", ""},
		{"prlltrk1.pie", "prltrk1.pie"},
		{"prlrtrk1.pie", ""},
		{"prllvtl1.pie", "prlvtl1.pie"},
		{"prlrvtl1.pie", ""},
		{"prllvtl2.pie", "prlvtl2.pie"},
		{"prlrvtl2.pie", ""},
		{"prllvtl3.pie", "prlvtl3.pie"},
		{"prlrvtl3.pie", ""},
		{"prllwhl1.pie", "prlwhl1.pie"},
		{"prlrwhl1.pie", ""},
		{"prmlhtr2.pie", "prmhtr2.pie"},
		{"prmrhtr2.pie", ""},
		{"prmltrk2.pie", "prmtrk2.pie"},
		{"prmrtrk2.pie", ""},
		{"prmlvtl1.pie", "prmvtl1.pie"},
		{"prmrvtl1.pie", ""},
		{"prmlwhl1.pie", "prmwhl1.pie"},
		{"prmrwhl1.pie", ""},
		// mp
		{"prslwhl1.pie", "prswhl1.pie"},
		{"prsrwhl1.pie", ""},
		{"prslvtl1.pie", "prsvtl1.pie"},
		{"prsrvtl1.pie", ""},
		{"prsltrk4.pie", "prstrk4.pie"},
		{"prsrtrk4.pie", ""},
		{"prslhtr4.pie", "prshtr4.pie"},
		{"prsrhtr4.pie", ""},
	};

	WzString name(filename.toLower());
	auto it = deprecatedModelNames.find(name);
	if (it != deprecatedModelNames.end())
	{
		debug(LOG_INFO, "Deprecation notice: The model \"%s\" is referenced in a mod's stats JSON file, and is deprecated. Please check the latest base stats models for how to update this mod for future compatibility.", name.toUtf8().c_str());
		return it->second;
	}
	return nullopt;
}

static iIMDBaseShape *statsGetIMD(WzConfig &json, BASE_STATS *psStats, const WzString& key, const WzString& key2 = WzString())
{
	iIMDBaseShape *retval = nullptr;
	if (json.contains(key))
	{
		auto value = json.json(key);
		if (value.is_object())
		{
			ASSERT_OR_RETURN(nullptr, !key2.isEmpty(), "Cannot look up a JSON object with an empty key!");
			auto obj = value;
			if (obj.find(key2.toUtf8()) == obj.end())
			{
				return nullptr;
			}
			value = obj[key2.toUtf8()];
		}
		WzString filename = json_variant(value).toWzString();
		deprecatedModelUpgrade(filename); // FUTURE TODO: Use output of this to auto-upgrade old model references? (Check for empty string)
		retval = modelGet(filename);
		if (retval == nullptr)
		{
			++statModelLoadingFailures;
		}
		ASSERT(retval != nullptr, "Cannot find the PIE model %s for stat %s in %s",
		       filename.toUtf8().c_str(), getStatsName(psStats), json.fileName().toUtf8().c_str());
	}
	return retval;
}

static void loadStats(WzConfig &json, BASE_STATS *psStats, size_t index)
{
	psStats->id = json.group();
	psStats->name = json.string("name");
	psStats->index = index;
	ASSERT(lookupStatPtr.find(psStats->id) == lookupStatPtr.end(), "Duplicate ID found! (%s)", psStats->id.toUtf8().c_str());
	lookupStatPtr.insert(std::make_pair(psStats->id, psStats));
}

void loadStructureStats_BaseStats(WzConfig &json, STRUCTURE_STATS *psStats, size_t index)
{
	loadStats(json, psStats, index);
}

void unloadStructureStats_BaseStats(const STRUCTURE_STATS &psStats)
{
	lookupStatPtr.erase(psStats.id);
}

static void loadCompStats(WzConfig &json, COMPONENT_STATS *psStats, size_t index)
{
	loadStats(json, psStats, index);
	lookupCompStatPtr.insert(std::make_pair(psStats->id, psStats));
	psStats->buildPower = json.value("buildPower", 0).toUInt();
	psStats->buildPoints = json.value("buildPoints", 0).toUInt();
	psStats->designable = json.value("designable", false).toBool();
	psStats->weight = json.value("weight", 0).toUInt();
	WzString useType = json.value("usageClass", "").toWzString();
	if (useType.toStdString().find("SuperCyborg") != std::string::npos)
	{
		psStats->usageClass = UsageClass::SuperCyborg;
	}
	else if (useType.toStdString().find("Cyborg") != std::string::npos)
	{
		psStats->usageClass = UsageClass::Cyborg;
	}
	psStats->getBase().hitpoints = json.value("hitpoints", 0).toUInt();
	psStats->getBase().hitpointPct = json.value("hitpointPct", 100).toUInt();

	WzString dtype = json.value("droidType", "DROID").toWzString();
	psStats->droidTypeOverride = DROID_ANY;
	if (dtype.compare("PERSON") == 0)
	{
		psStats->droidTypeOverride = DROID_PERSON;
	}
	else if (dtype.compare("TRANSPORTER") == 0)
	{
		psStats->droidTypeOverride = DROID_TRANSPORTER;
	}
	else if (dtype.compare("SUPERTRANSPORTER") == 0)
	{
		psStats->droidTypeOverride = DROID_SUPERTRANSPORTER;
	}
	else if (dtype.compare("CYBORG") == 0)
	{
		psStats->droidTypeOverride = DROID_CYBORG;
	}
	else if (dtype.compare("CYBORG_SUPER") == 0)
	{
		psStats->droidTypeOverride = DROID_CYBORG_SUPER;
	}
	else if (dtype.compare("CYBORG_CONSTRUCT") == 0)
	{
		psStats->droidTypeOverride = DROID_CYBORG_CONSTRUCT;
	}
	else if (dtype.compare("CYBORG_REPAIR") == 0)
	{
		psStats->droidTypeOverride = DROID_CYBORG_REPAIR;
	}
	else if (dtype.compare("DROID_CONSTRUCT") == 0)
	{
		psStats->droidTypeOverride = DROID_CONSTRUCT;
	}
	else if (dtype.compare("DROID_ECM") == 0)
	{
		psStats->droidTypeOverride = DROID_ECM;
	}
	else if (dtype.compare("DROID_COMMAND") == 0)
	{
		psStats->droidTypeOverride = DROID_COMMAND;
	}
	else if (dtype.compare("DROID_SENSOR") == 0)
	{
		psStats->droidTypeOverride = DROID_SENSOR;
	}
	else if (dtype.compare("DROID_REPAIR") == 0)
	{
		psStats->droidTypeOverride = DROID_REPAIR;
	}
	else if (dtype.compare("DROID") != 0)
	{
		debug(LOG_ERROR, "Unrecognized droidType %s", dtype.toUtf8().c_str());
	}
}

/*Load the weapon stats from the file exported from Access*/
bool loadWeaponStats(WzConfig &ini)
{
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	statsAllocWeapons(list.size());
	// Hack to make sure ZNULLWEAPON is always first in list
	auto nullweapon = std::find(list.begin(), list.end(), WzString::fromUtf8("ZNULLWEAPON"));
	ASSERT_OR_RETURN(false, nullweapon != list.end(), "ZNULLWEAPON is mandatory");
	std::iter_swap(nullweapon, list.begin());
	for (size_t i = 0; i < list.size(); ++i)
	{
		WEAPON_STATS *psStats = &asWeaponStats[i];
		std::vector<WzString> flags;

		ini.beginGroup(list[i]);
		loadCompStats(ini, psStats, i);
		psStats->compType = COMP_WEAPON;

		psStats->radiusLife = ini.value("radiusLife", 0).toUInt();

		psStats->base.shortRange = ini.value("shortRange").toUInt();
		psStats->base.maxRange = ini.value("longRange").toUInt();
		psStats->base.minRange = ini.value("minRange", 0).toUInt();
		psStats->base.hitChance = ini.value("longHit", 100).toUInt();
		psStats->base.shortHitChance = ini.value("shortHit", 100).toUInt();
		psStats->base.firePause = ini.value("firePause").toUInt();
		psStats->base.numRounds = ini.value("numRounds").toUInt();
		psStats->base.reloadTime = ini.value("reloadTime").toUInt();
		psStats->base.damage = ini.value("damage").toUInt();
		psStats->base.minimumDamage = ini.value("minimumDamage", 0).toInt();
		psStats->base.empRadius = ini.value("empRadius", 0).toUInt();
		psStats->base.radius = ini.value("radius", 0).toUInt();
		psStats->base.radiusDamage = ini.value("radiusDamage", 0).toUInt();
		psStats->base.periodicalDamageTime = ini.value("periodicalDamageTime", 0).toUInt();
		psStats->base.periodicalDamage = ini.value("periodicalDamage", 0).toUInt();
		psStats->base.periodicalDamageRadius = ini.value("periodicalDamageRadius", 0).toUInt();
		// multiply time stats
		psStats->base.firePause *= WEAPON_TIME;
		psStats->base.periodicalDamageTime *= WEAPON_TIME;
		psStats->radiusLife *= WEAPON_TIME;
		psStats->base.reloadTime *= WEAPON_TIME;
		// copy for upgrades
		for (int j = 0; j < MAX_PLAYERS; j++)
		{
			psStats->upgrade[j] = psStats->base;
		}

		psStats->numExplosions = ini.value("numExplosions").toUInt();
		psStats->flightSpeed = ini.value("flightSpeed", 1).toUInt();
		psStats->rotate = ini.value("rotate").toUInt();
		psStats->minElevation = ini.value("minElevation").toInt();
		psStats->maxElevation = ini.value("maxElevation").toInt();
		psStats->recoilValue = ini.value("recoilValue").toUInt();
		psStats->effectSize = ini.value("effectSize").toUInt();
		std::vector<WzString> flags_raw = ini.value("flags", 0).toWzStringList();
		// convert flags entries to lowercase
		std::transform(
					   flags_raw.begin(),
					   flags_raw.end(),
					   std::back_inserter(flags),
					   [](const WzString& in) { return in.toLower(); }
		);
		psStats->vtolAttackRuns = ini.value("numAttackRuns", 0).toUInt();
		psStats->penetrate = ini.value("penetrate", false).toBool();
		// weapon size limitation
		int weaponSize = ini.value("weaponSize", WEAPON_SIZE_ANY).toInt();
		ASSERT(weaponSize <= WEAPON_SIZE_ANY, "Bad weapon size for %s", list[i].toUtf8().c_str());
		psStats->weaponSize = (WEAPON_SIZE)weaponSize;

		ASSERT(psStats->flightSpeed > 0, "Invalid flight speed for %s", list[i].toUtf8().c_str());

		psStats->ref = STAT_WEAPON + i;

		//get the IMD for the component
		psStats->pIMD = statsGetIMD(ini, psStats, "model");
		psStats->pMountGraphic = statsGetIMD(ini, psStats, "mountModel");
		if (GetGameMode() == GS_NORMAL)
		{
			psStats->pMuzzleGraphic = statsGetIMD(ini, psStats, "muzzleGfx");
			psStats->pInFlightGraphic = statsGetIMD(ini, psStats, "flightGfx");
			psStats->pTargetHitGraphic = statsGetIMD(ini, psStats, "hitGfx");
			psStats->pTargetMissGraphic = statsGetIMD(ini, psStats, "missGfx");
			psStats->pWaterHitGraphic = statsGetIMD(ini, psStats, "waterGfx");
			psStats->pTrailGraphic = statsGetIMD(ini, psStats, "trailGfx");
		}
		psStats->fireOnMove = ini.value("fireOnMove", true).toBool();

		//set the weapon class
		if (!getWeaponClass(ini.value("weaponClass").toWzString(), &psStats->weaponClass))
		{
			debug(LOG_ERROR, "Invalid weapon class for weapon %s - assuming KINETIC", getStatsName(psStats));
			psStats->weaponClass = WC_KINETIC;
		}

		//set the subClass
		if (!getWeaponSubClass(ini.value("weaponSubClass").toWzString().toUtf8().c_str(), &psStats->weaponSubClass))
		{
			return false;
		}

		// set max extra weapon range on misses, make this modifiable one day by mod makers
		if (psStats->weaponSubClass == WSC_MGUN || psStats->weaponSubClass == WSC_COMMAND)
		{
			psStats->distanceExtensionFactor = 120;
		}
		else if (psStats->weaponSubClass == WSC_AAGUN)
		{
			psStats->distanceExtensionFactor = 100;
		}
		else // default
		{
			psStats->distanceExtensionFactor = 150;
		}

		//set the weapon effect
		if (!getWeaponEffect(ini.value("weaponEffect").toWzString(), &psStats->weaponEffect))
		{
			debug(LOG_FATAL, "loadWepaonStats: Invalid weapon effect for weapon %s", getStatsName(psStats));
			return false;
		}

		//set periodical damage weapon class
		WzString periodicalDamageWeaponClass = ini.value("periodicalDamageWeaponClass", "").toWzString();
		if (periodicalDamageWeaponClass.compare("") == 0)
		{
			//was not setted in ini - use default value
			psStats->periodicalDamageWeaponClass = psStats->weaponClass;
		}
		else if (!getWeaponClass(periodicalDamageWeaponClass, &psStats->periodicalDamageWeaponClass))
		{
			debug(LOG_ERROR, "Invalid periodicalDamageWeaponClass for weapon %s - assuming same class as weapon", getStatsName(psStats));
			psStats->periodicalDamageWeaponClass = psStats->weaponClass;
		}

		//set periodical damage weapon subclass
		WzString periodicalDamageWeaponSubClass = ini.value("periodicalDamageWeaponSubClass", "").toWzString();
		if (periodicalDamageWeaponSubClass.compare("") == 0)
		{
			//was not setted in ini - use default value
			psStats->periodicalDamageWeaponSubClass = psStats->weaponSubClass;
		}
		else if (!getWeaponSubClass(periodicalDamageWeaponSubClass.toUtf8().c_str(), &psStats->periodicalDamageWeaponSubClass))
		{
			debug(LOG_ERROR, "Invalid periodicalDamageWeaponSubClass for weapon %s - assuming same subclass as weapon", getStatsName(psStats));
			psStats->periodicalDamageWeaponSubClass = psStats->weaponSubClass;
		}

		//set periodical damage weapon effect
		WzString periodicalDamageWeaponEffect = ini.value("periodicalDamageWeaponEffect", "").toWzString();
		if (periodicalDamageWeaponEffect.compare("") == 0)
		{
			//was not setted in ini - use default value
			psStats->periodicalDamageWeaponEffect = psStats->weaponEffect;
		}
		else if (!getWeaponEffect(periodicalDamageWeaponEffect.toUtf8().c_str(), &psStats->periodicalDamageWeaponEffect))
		{
			debug(LOG_ERROR, "Invalid periodicalDamageWeaponEffect for weapon %s - assuming same effect as weapon", getStatsName(psStats));
			psStats->periodicalDamageWeaponEffect = psStats->weaponEffect;
		}

		//set the movement model
		if (!getMovementModel(ini.value("movement").toWzString(), &psStats->movementModel))
		{
			return false;
		}

		unsigned int shortRange = psStats->upgrade[0].shortRange;
		unsigned int longRange = psStats->upgrade[0].maxRange;
		unsigned int shortHit = psStats->upgrade[0].shortHitChance;
		unsigned int longHit = psStats->upgrade[0].hitChance;
		if (shortRange > longRange)
		{
			debug(LOG_ERROR, "%s, Short range (%d) is greater than long range (%d)", getStatsName(psStats), shortRange, longRange);
		}
		if (shortRange == longRange && shortHit != longHit)
		{
			debug(LOG_ERROR, "%s, shortHit and longHit should be equal if the ranges are the same", getStatsName(psStats));
		}

		// set the face Player value
		psStats->facePlayer = ini.value("facePlayer", false).toBool();

		// set the In flight face Player value
		psStats->faceInFlight = ini.value("faceInFlight", false).toBool();

		//set the light world value
		psStats->lightWorld = ini.value("lightWorld", false).toBool();

		// interpret flags
		psStats->surfaceToAir = SHOOT_ON_GROUND; // default
		if (std::find(flags.begin(), flags.end(), "aironly") != flags.end()) // "AirOnly"
		{
			psStats->surfaceToAir = SHOOT_IN_AIR;
		}
		else if (std::find(flags.begin(), flags.end(), "shootair") != flags.end()) // "ShootAir"
		{
			psStats->surfaceToAir |= SHOOT_IN_AIR;
		}
		if (std::find(flags.begin(), flags.end(), "nofriendlyfire") != flags.end()) // "NoFriendlyFire"
		{
			psStats->flags.set(WEAPON_FLAG_NO_FRIENDLY_FIRE, true);
		}

		//set the weapon sounds to default value
		psStats->iAudioFireID = NO_SOUND;
		psStats->iAudioImpactID = NO_SOUND;

		// load sounds
		int weaponSoundID, explosionSoundID;
		WzString szWeaponWav = ini.value("weaponWav", "-1").toWzString();
		WzString szExplosionWav = ini.value("explosionWav", "-1").toWzString();
		bool result = statsGetAudioIDFromString(list[i], szWeaponWav, &weaponSoundID);
		ASSERT_OR_RETURN(false, result, "Weapon sound %s not found for %s", szWeaponWav.toUtf8().c_str(), getStatsName(psStats));
		result = statsGetAudioIDFromString(list[i], szExplosionWav, &explosionSoundID);
		ASSERT_OR_RETURN(false, result, "Explosion sound %s not found for %s", szExplosionWav.toUtf8().c_str(), getStatsName(psStats));
		psStats->iAudioFireID = weaponSoundID;
		psStats->iAudioImpactID = explosionSoundID;

		ini.endGroup();
	}

	return true;
}

bool loadBodyStats(WzConfig &ini)
{
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	statsAllocBody(list.size());
	// Hack to make sure ZNULLBODY is always first in list
	auto nullbody = std::find(list.begin(), list.end(), WzString::fromUtf8("ZNULLBODY"));
	ASSERT_OR_RETURN(false, nullbody != list.end(), "ZNULLBODY is mandatory");
	std::iter_swap(nullbody, list.begin());
	for (size_t i = 0; i < list.size(); ++i)
	{
		BODY_STATS *psStats = &asBodyStats[i];

		ini.beginGroup(list[i]);
		loadCompStats(ini, psStats, i);
		psStats->compType = COMP_BODY;

		psStats->weaponSlots = ini.value("weaponSlots").toInt();
		psStats->bodyClass = ini.value("class").toWzString();
		psStats->base.thermal = ini.value("armourHeat").toInt();
		psStats->base.armour = ini.value("armourKinetic").toInt();
		psStats->base.power = ini.value("powerOutput").toInt();
		psStats->base.resistance = ini.value("resistance", DEFAULT_DROID_RESISTANCE).toInt();
		for (int j = 0; j < MAX_PLAYERS; j++)
		{
			psStats->upgrade[j] = psStats->base;
		}
		psStats->ref = STAT_BODY + i;
		if (!getBodySize(ini.value("size").toWzString(), &psStats->size))
		{
			ASSERT(false, "Unknown body size for %s", getStatsName(psStats));
			return false;
		}
		psStats->pIMD = statsGetIMD(ini, psStats, "model");

		ini.endGroup();
	}

	// now get the extra stuff ... hack it together with above later, moved here from
	// separate function

	// allocate space
	for (int numStats = 0; numStats < asBodyStats.size(); ++numStats)
	{
		BODY_STATS *psBodyStat = &asBodyStats[numStats];
		psBodyStat->ppIMDList.resize(asPropulsionStats.size() * NUM_PROP_SIDES, nullptr);
		psBodyStat->ppMoveIMDList.resize(asPropulsionStats.size() * NUM_PROP_SIDES, nullptr);
		psBodyStat->ppStillIMDList.resize(asPropulsionStats.size() * NUM_PROP_SIDES, nullptr);
	}
	for (size_t i = 0; i < list.size(); ++i)
	{
		WzString propulsionName, leftIMD, rightIMD;
		BODY_STATS *psBodyStat = nullptr;
		int numStats;

		ini.beginGroup(list[i]);
		if (!ini.contains("propulsionExtraModels"))
		{
			ini.endGroup();
			continue;
		}
		ini.beginGroup("propulsionExtraModels");
		//get the body stats
		for (numStats = 0; numStats < asBodyStats.size(); ++numStats)
		{
			psBodyStat = &asBodyStats[numStats];
			if (list[i].compare(psBodyStat->id) == 0)
			{
				break;
			}
		}
		if (numStats == asBodyStats.size()) // not found
		{
			debug(LOG_FATAL, "Invalid body name %s", list[i].toUtf8().c_str());
			return false;
		}
		std::vector<WzString> keys = ini.childKeys();
		for (size_t j = 0; j < keys.size(); j++)
		{
			for (numStats = 0; numStats < asPropulsionStats.size(); numStats++)
			{
				PROPULSION_STATS *psPropulsionStat = &asPropulsionStats[numStats];
				if (keys[j].compare(psPropulsionStat->id) == 0)
				{
					break;
				}
			}
			if (numStats == asPropulsionStats.size())
			{
				debug(LOG_FATAL, "Invalid propulsion name %s", keys[j].toUtf8().c_str());
				return false;
			}
			//allocate the left and right propulsion IMDs + movement and standing still animations
			// NOTE: New base stats files + IMDs use a single combined model (in the "left" key) for both sides...
			//       however, older mods / stats files may still rely on having both left + right
			psBodyStat->ppIMDList[numStats * NUM_PROP_SIDES + LEFT_PROP] = statsGetIMD(ini, psBodyStat, keys[j], WzString::fromUtf8("left"));
			psBodyStat->ppIMDList[numStats * NUM_PROP_SIDES + RIGHT_PROP] = statsGetIMD(ini, psBodyStat, keys[j], WzString::fromUtf8("right"));
			psBodyStat->ppMoveIMDList[numStats] = statsGetIMD(ini, psBodyStat, keys[j], WzString::fromUtf8("moving"));
			psBodyStat->ppStillIMDList[numStats] = statsGetIMD(ini, psBodyStat, keys[j], WzString::fromUtf8("still"));
		}
		ini.endGroup();
		ini.endGroup();
	}

	return true;
}

/*Load the Brain stats from the file exported from Access*/
bool loadBrainStats(WzConfig &ini)
{
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	statsAllocBrain(list.size());
	// Hack to make sure ZNULLBRAIN is always first in list
	auto nullbrain = std::find(list.begin(), list.end(), WzString::fromUtf8("ZNULLBRAIN"));
	ASSERT_OR_RETURN(false, nullbrain != list.end(), "ZNULLBRAIN is mandatory");
	std::iter_swap(nullbrain, list.begin());
	bool retVal = true;
	for (size_t i = 0; i < list.size(); ++i)
	{
		BRAIN_STATS *psStats = &asBrainStats[i];

		ini.beginGroup(list[i]);
		loadCompStats(ini, psStats, i);
		psStats->compType = COMP_BRAIN;

		psStats->weight = ini.value("weight", 0).toInt();
		psStats->base.maxDroids = ini.value("maxDroids").toInt();
		psStats->base.maxDroidsMult = ini.value("maxDroidsMult").toInt();
		psStats->shield.initialShieldPointsPercent = ini.value("initialShieldPointsPercent").toInt();
		psStats->shield.additiveShieldPointsPercent = ini.value("additiveShieldPointsPercent").toInt();
		psStats->shield.initialShieldRegenTime = ini.value("initialShieldRegenTime").toInt();
		psStats->shield.shieldRegenTimeDec = ini.value("shieldRegenTimeDec").toInt();
		psStats->shield.initialShieldInterruptRegenTime = ini.value("initialShieldInterruptRegenTime").toInt();
		psStats->shield.shieldInterruptRegenTimeDec = ini.value("shieldInterruptRegenTimeDec").toInt();
		psStats->shield.shieldPointsPerStep = ini.value("shieldPointsPerStep").toInt();
		auto rankNames = ini.json("ranks");
		ASSERT(rankNames.is_array(), "ranks is not an array");
		for (const auto& v : rankNames)
		{
			psStats->rankNames.push_back(v.get<std::string>());
		}
		auto rankThresholds = ini.json("thresholds");
		for (const auto& v : rankThresholds)
		{
			psStats->base.rankThresholds.push_back(v.get<int>());
		}
		psStats->ref = STAT_BRAIN + i;

		for (int j = 0; j < MAX_PLAYERS; j++)
		{
			psStats->upgrade[j] = psStats->base;
		}

		// check weapon attached
		psStats->psWeaponStat = nullptr;
		if (ini.contains("turret"))
		{
			int weapon = getCompFromName(COMP_WEAPON, ini.value("turret").toWzString());
			if (weapon >= 0)
			{
				psStats->psWeaponStat = &asWeaponStats[weapon];
			}
			else
			{
				ASSERT(weapon >= 0, "Unable to find weapon for brain %s", getStatsName(psStats));
				retVal = false;
			}
		}
		psStats->designable = ini.value("designable", false).toBool();
		ini.endGroup();
	}
	return retVal;
}


/*returns the propulsion type based on the string name passed in */
bool getPropulsionType(const char *typeName, PROPULSION_TYPE *type)
{
	if (strcmp(typeName, "Wheeled") == 0)
	{
		*type = PROPULSION_TYPE_WHEELED;
	}
	else if (strcmp(typeName, "Tracked") == 0)
	{
		*type = PROPULSION_TYPE_TRACKED;
	}
	else if (strcmp(typeName, "Legged") == 0)
	{
		*type = PROPULSION_TYPE_LEGGED;
	}
	else if (strcmp(typeName, "Hover") == 0)
	{
		*type = PROPULSION_TYPE_HOVER;
	}
	else if (strcmp(typeName, "Lift") == 0)
	{
		*type = PROPULSION_TYPE_LIFT;
	}
	else if (strcmp(typeName, "Propellor") == 0)
	{
		*type = PROPULSION_TYPE_PROPELLOR;
	}
	else if (strcmp(typeName, "Half-Tracked") == 0)
	{
		*type = PROPULSION_TYPE_HALF_TRACKED;
	}
	else
	{
		debug(LOG_ERROR, "getPropulsionType: Invalid Propulsion type %s - assuming Hover", typeName);
		*type = PROPULSION_TYPE_HOVER;

		return false;
	}

	return true;
}

/*Load the Propulsion stats from the file exported from Access*/
bool loadPropulsionStats(WzConfig &ini)
{
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	statsAllocPropulsion(list.size());
	// Hack to make sure ZNULLPROP is always first in list
	auto nullprop = std::find(list.begin(), list.end(), WzString::fromUtf8("ZNULLPROP"));
	ASSERT_OR_RETURN(false, nullprop != list.end(), "ZNULLPROP is mandatory");
	std::iter_swap(nullprop, list.begin());
	bool retVal = true;
	for (size_t i = 0; i < list.size(); ++i)
	{
		PROPULSION_STATS *psStats = &asPropulsionStats[i];

		ini.beginGroup(list[i]);
		loadCompStats(ini, psStats, i);
		psStats->compType = COMP_PROPULSION;

		psStats->base.hitpointPctOfBody = ini.value("hitpointPctOfBody", 0).toInt();
		psStats->maxSpeed = ini.value("speed").toInt();
		psStats->ref = STAT_PROPULSION + i;
		psStats->turnSpeed = ini.value("turnSpeed", DEG(1) / 3).toInt();
		ASSERT(psStats->turnSpeed != 0, "\"%s\".\"turnSpeed\" is 0", psStats->id.toUtf8().c_str());
		psStats->spinSpeed = ini.value("spinSpeed", DEG(3) / 4).toInt();
		ASSERT(psStats->spinSpeed != 0, "\"%s\".\"spinSpeed\" is 0", psStats->id.toUtf8().c_str());
		psStats->spinAngle = ini.value("spinAngle", 180).toInt();
		ASSERT(psStats->spinAngle != 0, "\"%s\".\"spinAngle\" is 0", psStats->id.toUtf8().c_str());
		psStats->acceleration = ini.value("acceleration", 250).toInt();
		ASSERT(psStats->acceleration != 0, "\"%s\".\"acceleration\" is 0", psStats->id.toUtf8().c_str());
		psStats->deceleration = ini.value("deceleration", 800).toInt();
		psStats->skidDeceleration = ini.value("skidDeceleration", 600).toInt();
		psStats->pIMD = nullptr;
		psStats->pIMD = statsGetIMD(ini, psStats, "model");
		for (int j = 0; j < MAX_PLAYERS; j++)
		{
			psStats->upgrade[j] = psStats->base;
		}
		if (!getPropulsionType(ini.value("type").toWzString().toUtf8().c_str(), &psStats->propulsionType))
		{
			debug(LOG_FATAL, "loadPropulsionStats: Invalid Propulsion type for %s", getStatsName(psStats));
			retVal = false;
		}
		ini.endGroup();
	}

	return retVal;
}

/*Load the Sensor stats from the file exported from Access*/
bool loadSensorStats(WzConfig &ini)
{
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	statsAllocSensor(list.size());
	// Hack to make sure ZNULLSENSOR is always first in list
	auto nullsensor = std::find(list.begin(), list.end(), WzString::fromUtf8("ZNULLSENSOR"));
	ASSERT_OR_RETURN(false, nullsensor != list.end(), "ZNULLSENSOR is mandatory");
	std::iter_swap(nullsensor, list.begin());
	for (size_t i = 0; i < list.size(); ++i)
	{
		SENSOR_STATS *psStats = &asSensorStats[i];

		ini.beginGroup(list[i]);
		loadCompStats(ini, psStats, i);
		psStats->compType = COMP_SENSOR;

		psStats->base.range = ini.value("range").toInt();
		for (int j = 0; j < MAX_PLAYERS; j++)
		{
			psStats->upgrade[j] = psStats->base;
		}

		psStats->ref = STAT_SENSOR + i;

		WzString location = ini.value("location").toWzString();
		if (location.compare("DEFAULT") == 0)
		{
			psStats->location = LOC_DEFAULT;
		}
		else if (location.compare("TURRET") == 0)
		{
			psStats->location = LOC_TURRET;
		}
		else
		{
			ASSERT(false, "Invalid Sensor location: %s", location.toUtf8().c_str());
		}
		WzString type = ini.value("type").toWzString();
		if (type.compare("STANDARD") == 0)
		{
			psStats->type = STANDARD_SENSOR;
		}
		else if (type.compare("INDIRECT CB") == 0)
		{
			psStats->type = INDIRECT_CB_SENSOR;
		}
		else if (type.compare("VTOL CB") == 0)
		{
			psStats->type = VTOL_CB_SENSOR;
		}
		else if (type.compare("VTOL INTERCEPT") == 0)
		{
			psStats->type = VTOL_INTERCEPT_SENSOR;
		}
		else if (type.compare("SUPER") == 0)
		{
			psStats->type = SUPER_SENSOR;
		}
		else if (type.compare("RADAR DETECTOR") == 0)
		{
			psStats->type = RADAR_DETECTOR_SENSOR;
		}
		else
		{
			ASSERT(false, "Invalid Sensor type: %s", type.toUtf8().c_str());
		}

		//get the IMD for the component
		psStats->pIMD = statsGetIMD(ini, psStats, "sensorModel");
		psStats->pMountGraphic = statsGetIMD(ini, psStats, "mountModel");

		ini.endGroup();
	}
	return true;
}

/*Load the ECM stats from the file exported from Access*/
bool loadECMStats(WzConfig &ini)
{
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	statsAllocECM(list.size());
	// Hack to make sure ZNULLECM is always first in list
	auto nullecm = std::find(list.begin(), list.end(), WzString::fromUtf8("ZNULLECM"));
	ASSERT_OR_RETURN(false, nullecm != list.end(), "ZNULLECM is mandatory");
	std::iter_swap(nullecm, list.begin());
	for (size_t i = 0; i < list.size(); ++i)
	{
		ECM_STATS *psStats = &asECMStats[i];

		ini.beginGroup(list[i]);
		loadCompStats(ini, psStats, i);
		psStats->compType = COMP_ECM;

		psStats->base.range = ini.value("range").toInt();
		for (int j = 0; j < MAX_PLAYERS; j++)
		{
			psStats->upgrade[j] = psStats->base;
		}

		psStats->ref = STAT_ECM + i;

		WzString location = ini.value("location").toWzString();
		if (location.compare("DEFAULT") == 0)
		{
			psStats->location = LOC_DEFAULT;
		}
		else if (location.compare("TURRET") == 0)
		{
			psStats->location = LOC_TURRET;
		}
		else
		{
			ASSERT(false, "Invalid ECM location: %s", location.toUtf8().c_str());
		}

		//get the IMD for the component
		psStats->pIMD = statsGetIMD(ini, psStats, "sensorModel");
		psStats->pMountGraphic = statsGetIMD(ini, psStats, "mountModel");

		ini.endGroup();
	}
	return true;
}

/*Load the Repair stats from the file exported from Access*/
bool loadRepairStats(WzConfig &ini)
{
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	statsAllocRepair(list.size());
	// Hack to make sure ZNULLREPAIR is always first in list
	auto nullrepair = std::find(list.begin(), list.end(), WzString::fromUtf8("ZNULLREPAIR"));
	ASSERT_OR_RETURN(false, nullrepair != list.end(), "ZNULLREPAIR is mandatory");
	std::iter_swap(nullrepair, list.begin());
	bool retVal = true;
	for (size_t i = 0; i < list.size(); ++i)
	{
		REPAIR_STATS *psStats = &asRepairStats[i];

		ini.beginGroup(list[i]);
		loadCompStats(ini, psStats, i);
		psStats->compType = COMP_REPAIRUNIT;

		psStats->base.repairPoints = ini.value("repairPoints").toInt();
		for (int j = 0; j < MAX_PLAYERS; j++)
		{
			psStats->upgrade[j] = psStats->base;
		}
		psStats->time = ini.value("time", 0).toInt() * WEAPON_TIME;

		psStats->ref = STAT_REPAIR + i;

		WzString location = ini.value("location").toWzString();
		if (location.compare("DEFAULT") == 0)
		{
			psStats->location = LOC_DEFAULT;
		}
		else if (location.compare("TURRET") == 0)
		{
			psStats->location = LOC_TURRET;
		}
		else
		{
			ASSERT(false, "Invalid Repair location: %s", location.toUtf8().c_str());
		}

		//check its not 0 since we will be dividing by it at a later stage
		if (!(psStats->time > 0))
		{
			ASSERT(psStats->time > 0, "Repair delay cannot be zero for %s", getStatsName(psStats));
			psStats->time = 1;
			retVal = false;
		}

		//get the IMD for the component
		psStats->pIMD = statsGetIMD(ini, psStats, "model");
		psStats->pMountGraphic = statsGetIMD(ini, psStats, "mountModel");

		ini.endGroup();
	}
	return retVal;
}

/*Load the Construct stats from the file exported from Access*/
bool loadConstructStats(WzConfig &ini)
{
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	statsAllocConstruct(list.size());
	// Hack to make sure ZNULLCONSTRUCT is always first in list
	auto nullconstruct = std::find(list.begin(), list.end(), WzString::fromUtf8("ZNULLCONSTRUCT"));
	ASSERT_OR_RETURN(false, nullconstruct != list.end(), "ZNULLCONSTRUCT is mandatory");
	std::iter_swap(nullconstruct, list.begin());
	for (size_t i = 0; i < list.size(); ++i)
	{
		CONSTRUCT_STATS *psStats = &asConstructStats[i];

		ini.beginGroup(list[i]);
		loadCompStats(ini, psStats, i);
		psStats->compType = COMP_CONSTRUCT;

		psStats->base.constructPoints = ini.value("constructPoints").toInt();
		for (int j = 0; j < MAX_PLAYERS; j++)
		{
			psStats->upgrade[j] = psStats->base;
		}
		psStats->ref = STAT_CONSTRUCT + i;

		//get the IMD for the component
		psStats->pIMD = statsGetIMD(ini, psStats, "sensorModel");
		psStats->pMountGraphic = statsGetIMD(ini, psStats, "mountModel");

		ini.endGroup();
	}
	return true;
}


/*Load the Propulsion Types from the file exported from Access*/
bool loadPropulsionTypes(WzConfig &ini)
{
	const unsigned int NumTypes = PROPULSION_TYPE_NUM;

	//allocate storage for the stats
	asPropulsionTypes.resize(NumTypes);
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();

	for (int i = 0; i < NumTypes; ++i)
	{
		PROPULSION_TYPE type;

		ini.beginGroup(list[i]);
		unsigned multiplier = ini.value("multiplier").toUInt();

		//set the pointer for this record based on the name
		if (!getPropulsionType(list[i].toUtf8().c_str(), &type))
		{
			debug(LOG_FATAL, "Invalid Propulsion type - %s", list[i].toUtf8().c_str());
			return false;
		}

		PROPULSION_TYPES *pPropType = &asPropulsionTypes[type];

		WzString flightName = ini.value("flightName").toWzString();
		if (flightName.compare("GROUND") == 0)
		{
			pPropType->travel = GROUND;
		}
		else if (flightName.compare("AIR") == 0)
		{
			pPropType->travel = AIR;
		}
		else
		{
			ASSERT(false, "Invalid travel type for Propulsion: %s", flightName.toUtf8().c_str());
		}

		//don't care about this anymore! AB FRIDAY 13/11/98
		//want it back again! AB 27/11/98
		if (multiplier > UWORD_MAX)
		{
			ASSERT(false, "loadPropulsionTypes: power Ratio multiplier too high");
			//set to a default value since not life threatening!
			multiplier = 100;
		}
		pPropType->powerRatioMult = (UWORD)multiplier;

		//initialise all the sound variables
		pPropType->startID = NO_SOUND;
		pPropType->idleID = NO_SOUND;
		pPropType->moveOffID = NO_SOUND;
		pPropType->moveID = NO_SOUND;
		pPropType->hissID = NO_SOUND;
		pPropType->shutDownID = NO_SOUND;

		ini.endGroup();
	}

	return true;
}

bool loadTerrainTable(WzConfig &ini)
{
	asTerrainTable = (int *)malloc(sizeof(*asTerrainTable) * PROPULSION_TYPE_NUM * TER_MAX);
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	for (int i = 0; i < list.size(); ++i)
	{
		ini.beginGroup(list[i]);
		int terrainType = ini.value("id").toInt();
		ini.beginGroup("speedFactor");
		asTerrainTable[terrainType * PROPULSION_TYPE_NUM + PROPULSION_TYPE_WHEELED] = ini.value("wheeled", 100).toUInt();
		asTerrainTable[terrainType * PROPULSION_TYPE_NUM + PROPULSION_TYPE_TRACKED] = ini.value("tracked", 100).toUInt();
		asTerrainTable[terrainType * PROPULSION_TYPE_NUM + PROPULSION_TYPE_LEGGED] = ini.value("legged", 100).toUInt();
		asTerrainTable[terrainType * PROPULSION_TYPE_NUM + PROPULSION_TYPE_HOVER] = ini.value("hover", 100).toUInt();
		asTerrainTable[terrainType * PROPULSION_TYPE_NUM + PROPULSION_TYPE_LIFT] = ini.value("lift", 100).toUInt();
		asTerrainTable[terrainType * PROPULSION_TYPE_NUM + PROPULSION_TYPE_PROPELLOR] = ini.value("propellor", 100).toUInt();
		asTerrainTable[terrainType * PROPULSION_TYPE_NUM + PROPULSION_TYPE_HALF_TRACKED] = ini.value("half-tracked", 100).toUInt();
		ini.endGroup();
		ini.endGroup();
	}
	return true;
}

static bool statsGetAudioIDFromString(const WzString &szStatName, const WzString &szWavName, int *piWavID)
{
	if (szWavName.compare("-1") == 0)
	{
		*piWavID = NO_SOUND;
	}
	else if ((*piWavID = audio_GetIDFromStr(szWavName.toUtf8().c_str())) == NO_SOUND)
	{
		debug(LOG_FATAL, "Could not get ID %d for sound %s", *piWavID, szWavName.toUtf8().c_str());
		return false;
	}
	if ((*piWavID < 0 || *piWavID > ID_MAX_SOUND) && *piWavID != NO_SOUND)
	{
		debug(LOG_FATAL, "Invalid ID - %d for sound %s", *piWavID, szStatName.toUtf8().c_str());
		return false;
	}
	return true;
}

bool loadWeaponModifiers(WzConfig &ini)
{
	//initialise to 100%
	for (int i = 0; i < WE_NUMEFFECTS; i++)
	{
		for (int j = 0; j < PROPULSION_TYPE_NUM; j++)
		{
			asWeaponModifier[i][j] = 100;
		}
		for (int j = 0; j < SIZE_NUM; j++)
		{
			asWeaponModifierBody[i][j] = 100;
		}
	}
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	for (int i = 0; i < list.size(); i++)
	{
		WEAPON_EFFECT effectInc;
		PROPULSION_TYPE propInc;

		ini.beginGroup(list[i]);
		//get the weapon effect inc
		if (!getWeaponEffect(list[i], &effectInc))
		{
			debug(LOG_FATAL, "Invalid Weapon Effect - %s", list[i].toUtf8().c_str());
			continue;
		}
		std::vector<WzString> keys = ini.childKeys();
		for (int j = 0; j < keys.size(); j++)
		{
			int modifier = ini.value(keys.at(j)).toInt();
			if (!getPropulsionType(keys.at(j).toUtf8().data(), &propInc))
			{
				// If not propulsion, must be body
				BODY_SIZE body = SIZE_NUM;
				if (!getBodySize(keys.at(j), &body))
				{
					debug(LOG_FATAL, "Invalid Propulsion or Body type - %s", keys.at(j).toUtf8().c_str());
					continue;
				}
				asWeaponModifierBody[effectInc][body] = modifier;
			}
			else // is propulsion
			{
				asWeaponModifier[effectInc][propInc] = modifier;
			}
		}
		ini.endGroup();
	}
	return true;
}

/*Load the propulsion type sounds from the file exported from Access*/
bool loadPropulsionSounds(const char *pFileName)
{
	SDWORD	i, startID, idleID, moveOffID, moveID, hissID, shutDownID;
	PROPULSION_TYPE type;

	ASSERT(asPropulsionTypes.size() != 0, "loadPropulsionSounds: Propulsion type stats not loaded");

	WzConfig ini(pFileName, WzConfig::ReadOnlyAndRequired);
	std::vector<WzString> list = ini.childGroups();
	for (i = 0; i < list.size(); ++i)
	{
		ini.beginGroup(list[i]);
		if (!statsGetAudioIDFromString(list[i], ini.value("szStart").toWzString(), &startID))
		{
			return false;
		}
		if (!statsGetAudioIDFromString(list[i], ini.value("szIdle").toWzString(), &idleID))
		{
			return false;
		}
		if (!statsGetAudioIDFromString(list[i], ini.value("szMoveOff").toWzString(), &moveOffID))
		{
			return false;
		}
		if (!statsGetAudioIDFromString(list[i], ini.value("szMove").toWzString(), &moveID))
		{
			return false;
		}
		if (!statsGetAudioIDFromString(list[i], ini.value("szHiss").toWzString(), &hissID))
		{
			return false;
		}
		if (!statsGetAudioIDFromString(list[i], ini.value("szShutDown").toWzString(), &shutDownID))
		{
			return false;
		}
		if (!getPropulsionType(list[i].toUtf8().c_str(), &type))
		{
			debug(LOG_FATAL, "Invalid Propulsion type - %s", list[i].toUtf8().c_str());
			return false;
		}
		PROPULSION_TYPES *pPropType = &asPropulsionTypes[type];
		pPropType->startID = (SWORD)startID;
		pPropType->idleID = (SWORD)idleID;
		pPropType->moveOffID = (SWORD)moveOffID;
		pPropType->moveID = (SWORD)moveID;
		pPropType->hissID = (SWORD)hissID;
		pPropType->shutDownID = (SWORD)shutDownID;

		ini.endGroup();
	}

	return (true);
}

//get the speed factor for a given terrain type and propulsion type
UDWORD getSpeedFactor(UDWORD type, UDWORD propulsionType)
{
	ASSERT(propulsionType < PROPULSION_TYPE_NUM, "The propulsion type is too large");
	return asTerrainTable[type * PROPULSION_TYPE_NUM + propulsionType];
}

int getCompFromName(COMPONENT_TYPE compType, const WzString &name)
{
	return getCompFromID(compType, name);
}

int getCompFromID(COMPONENT_TYPE compType, const WzString &name)
{
	COMPONENT_STATS *psComp = nullptr;
	auto it = lookupCompStatPtr.find(WzString::fromUtf8(name.toUtf8().c_str()));
	if (it != lookupCompStatPtr.end())
	{
		psComp = it->second;
	}
	ASSERT_OR_RETURN(-1, psComp, "No such component ID [%s] found", name.toUtf8().c_str());
	ASSERT_OR_RETURN(-1, compType == psComp->compType, "Wrong component type for ID %s", name.toUtf8().c_str());
	ASSERT_OR_RETURN(-1, psComp->index <= INT_MAX, "Component index is too large for ID %s", name.toUtf8().c_str());
	return static_cast<int>(psComp->index);
}

/// Get the component for a stat based on the name alone.
/// Returns NULL if record not found
COMPONENT_STATS *getCompStatsFromName(const WzString &name)
{
	COMPONENT_STATS *psComp = nullptr;
	auto it = lookupCompStatPtr.find(name);
	if (it != lookupCompStatPtr.end())
	{
		psComp = it->second;
	}
	/*if (!psComp)
	{
		debug(LOG_ERROR, "Not found: %s", name.toUtf8().c_str());
		for (auto& it2 : lookupCompStatPtr)
		{
			debug(LOG_ERROR, "    %s", it2.second->id.toUtf8().c_str());
		}
	}*/
	return psComp;
}

BASE_STATS *getBaseStatsFromName(const WzString &name)
{
	BASE_STATS *psStat = nullptr;
	auto it = lookupStatPtr.find(name);
	if (it != lookupStatPtr.end())
	{
		psStat = it->second;
	}
	return psStat;
}

/*sets the store to the body size based on the name passed in - returns false
if doesn't compare with any*/
bool getBodySize(const WzString &size, BODY_SIZE *pStore)
{
	if (!strcmp(size.toUtf8().c_str(), "LIGHT"))
	{
		*pStore = SIZE_LIGHT;
		return true;
	}
	else if (!strcmp(size.toUtf8().c_str(), "MEDIUM"))
	{
		*pStore = SIZE_MEDIUM;
		return true;
	}
	else if (!strcmp(size.toUtf8().c_str(), "HEAVY"))
	{
		*pStore = SIZE_HEAVY;
		return true;
	}
	else if (!strcmp(size.toUtf8().c_str(), "SUPER HEAVY"))
	{
		*pStore = SIZE_SUPER_HEAVY;
		return true;
	}

	ASSERT(false, "Invalid size - %s", size.toUtf8().c_str());
	return false;
}

/*returns the weapon sub class based on the string name passed in */
bool getWeaponSubClass(const char *subClass, WEAPON_SUBCLASS *wclass)
{
	if (strcmp(subClass, "CANNON") == 0)
	{
		*wclass = WSC_CANNON;
	}
	else if (strcmp(subClass, "MORTARS") == 0)
	{
		*wclass = WSC_MORTARS;
	}
	else if (strcmp(subClass, "MISSILE") == 0)
	{
		*wclass = WSC_MISSILE;
	}
	else if (strcmp(subClass, "ROCKET") == 0)
	{
		*wclass = WSC_ROCKET;
	}
	else if (strcmp(subClass, "ENERGY") == 0)
	{
		*wclass = WSC_ENERGY;
	}
	else if (strcmp(subClass, "GAUSS") == 0)
	{
		*wclass = WSC_GAUSS;
	}
	else if (strcmp(subClass, "FLAME") == 0)
	{
		*wclass = WSC_FLAME;
	}
	else if (strcmp(subClass, "HOWITZERS") == 0)
	{
		*wclass = WSC_HOWITZERS;
	}
	else if (strcmp(subClass, "MACHINE GUN") == 0)
	{
		*wclass = WSC_MGUN;
	}
	else if (strcmp(subClass, "ELECTRONIC") == 0)
	{
		*wclass = WSC_ELECTRONIC;
	}
	else if (strcmp(subClass, "A-A GUN") == 0)
	{
		*wclass = WSC_AAGUN;
	}
	else if (strcmp(subClass, "SLOW MISSILE") == 0)
	{
		*wclass = WSC_SLOWMISSILE;
	}
	else if (strcmp(subClass, "SLOW ROCKET") == 0)
	{
		*wclass = WSC_SLOWROCKET;
	}
	else if (strcmp(subClass, "LAS_SAT") == 0)
	{
		*wclass = WSC_LAS_SAT;
	}
	else if (strcmp(subClass, "BOMB") == 0)
	{
		*wclass = WSC_BOMB;
	}
	else if (strcmp(subClass, "COMMAND") == 0)
	{
		*wclass = WSC_COMMAND;
	}
	else if (strcmp(subClass, "EMP") == 0)
	{
		*wclass = WSC_EMP;
	}
	else
	{
		ASSERT(!"Invalid weapon sub class", "Invalid weapon sub class: %s", subClass);
		return false;
	}

	return true;
}

/*returns the weapon sub class based on the string name passed in */
const char *getWeaponSubClass(WEAPON_SUBCLASS wclass)
{
	switch (wclass)
	{
	case WSC_CANNON: return "CANNON";
	case WSC_MORTARS: return "MORTARS";
	case WSC_MISSILE: return "MISSILE";
	case WSC_ROCKET: return "ROCKET";
	case WSC_ENERGY: return "ENERGY";
	case WSC_GAUSS: return "GAUSS";
	case WSC_FLAME: return "FLAME";
	case WSC_HOWITZERS: return "HOWITZERS";
	case WSC_MGUN: return "MACHINE GUN";
	case WSC_ELECTRONIC: return "ELECTRONIC";
	case WSC_AAGUN: return "A-A GUN";
	case WSC_SLOWMISSILE: return "SLOW MISSILE";
	case WSC_SLOWROCKET: return "SLOW ROCKET";
	case WSC_LAS_SAT: return "LAS_SAT";
	case WSC_BOMB: return "BOMB";
	case WSC_COMMAND: return "COMMAND";
	case WSC_EMP: return "EMP";
	case WSC_NUM_WEAPON_SUBCLASSES: break;
	}
	ASSERT(false, "No such weapon subclass");
	return "Bad weapon subclass";
}

/* returns the translated weapon sub class name - for local display purposes only */
const char *getWeaponSubClassDisplayName(WEAPON_SUBCLASS wclass, bool shortForm)
{
	switch (wclass)
	{
	case WSC_CANNON:
		return _("Cannons");
	case WSC_MORTARS:
		return _("Mortars");
	case WSC_MISSILE:
		return _("Missiles");
	case WSC_ROCKET:
		return _("Rockets");
	case WSC_ENERGY:
		return _("Energy");
	case WSC_GAUSS:
		return _("Gauss");
	case WSC_FLAME:
		// TRANSLATORS: "Flame" weapons class
		return _("Flame");
	case WSC_HOWITZERS:
		return _("Howitzers");
	case WSC_MGUN:
		if (shortForm)
		{
			// TRANSLATORS: A short-form for "Machine Guns" (should be shorter than the translated full form - if not, just use the full translation for "Machine Guns")
			return _("MG");
		}
		else
		{
			return _("Machine Guns");
		}
	case WSC_ELECTRONIC:
		// TRANSLATORS: "Electronic" weapon class
		return _("Electronic");
	case WSC_AAGUN:
		if (shortForm)
		{
			// TRANSLATORS: A short-form for "Anti-Air" weapons class (should be shorter than the translated full form - if not, just use the full translation for "Anti-Air")
			return _("A-A");
		}
		else
		{
			return _("Anti-Air");
		}
	case WSC_SLOWMISSILE:
		return _("Slow Missile");
	case WSC_SLOWROCKET:
		return _("Slow Rocket");
	case WSC_LAS_SAT:
		if (shortForm)
		{
			// TRANSLATORS: A short-form for "Laser Satellite" weapons class - some just use "Las-Sat" or "LasSat"
			return _("Las-Sat");
		}
		else
		{
			return _("Laser Satellite");
		}
	case WSC_BOMB:
		return _("Bombs");
	case WSC_COMMAND:
		return _("Command");
	case WSC_EMP:
		// TRANSLATORS: "EMP" (electromagnetic pulse) weapons class - probably should keep it as "EMP" (or the respective acronym in your language)
		return _("EMP");
	case WSC_NUM_WEAPON_SUBCLASSES: break;
	}
	ASSERT(false, "No such weapon subclass");
	return "";
}

/*returns the movement model based on the string name passed in */
bool getMovementModel(const WzString &movementModel, MOVEMENT_MODEL *model)
{
	if (strcmp(movementModel.toUtf8().c_str(), "DIRECT") == 0)
	{
		*model = MM_DIRECT;
	}
	else if (strcmp(movementModel.toUtf8().c_str(), "INDIRECT") == 0)
	{
		*model = MM_INDIRECT;
	}
	else if (strcmp(movementModel.toUtf8().c_str(), "HOMING-DIRECT") == 0)
	{
		*model = MM_HOMINGDIRECT;
	}
	else if (strcmp(movementModel.toUtf8().c_str(), "HOMING-INDIRECT") == 0)
	{
		*model = MM_HOMINGINDIRECT;
	}
	else
	{
		// We've got problem if we got here
		ASSERT(!"Invalid movement model", "Invalid movement model: %s", movementModel.toUtf8().c_str());
		return false;
	}

	return true;
}

const StringToEnum<WEAPON_EFFECT> mapUnsorted_WEAPON_EFFECT[] =
{
	{"ANTI PERSONNEL",      WE_ANTI_PERSONNEL       },
	{"ANTI TANK",           WE_ANTI_TANK            },
	{"BUNKER BUSTER",       WE_BUNKER_BUSTER        },
	{"ARTILLERY ROUND",     WE_ARTILLERY_ROUND      },
	{"FLAMER",              WE_FLAMER               },
	{"ANTI AIRCRAFT",       WE_ANTI_AIRCRAFT        },
	{"ALL ROUNDER",         WE_ANTI_AIRCRAFT        },  // Alternative name for WE_ANTI_AIRCRAFT.
};
const StringToEnumMap<WEAPON_EFFECT> map_WEAPON_EFFECT = mapUnsorted_WEAPON_EFFECT;

bool getWeaponEffect(const WzString& weaponEffect, WEAPON_EFFECT *effect)
{
	if (strcmp(weaponEffect.toUtf8().c_str(), "ANTI PERSONNEL") == 0)
	{
		*effect = WE_ANTI_PERSONNEL;
	}
	else if (strcmp(weaponEffect.toUtf8().c_str(), "ANTI TANK") == 0)
	{
		*effect = WE_ANTI_TANK;
	}
	else if (strcmp(weaponEffect.toUtf8().c_str(), "BUNKER BUSTER") == 0)
	{
		*effect = WE_BUNKER_BUSTER;
	}
	else if (strcmp(weaponEffect.toUtf8().c_str(), "ARTILLERY ROUND") == 0)
	{
		*effect = WE_ARTILLERY_ROUND;
	}
	else if (strcmp(weaponEffect.toUtf8().c_str(), "FLAMER") == 0)
	{
		*effect = WE_FLAMER;
	}
	else if (strcmp(weaponEffect.toUtf8().c_str(), "ANTI AIRCRAFT") == 0 || strcmp(weaponEffect.toUtf8().c_str(), "ALL ROUNDER") == 0)
	{
		*effect = WE_ANTI_AIRCRAFT;
	}
	else
	{
		ASSERT(!"Invalid weapon effect", "Invalid weapon effect: %s", weaponEffect.toUtf8().c_str());
		return false;
	}

	return true;
}

/*returns the weapon effect string based on the enum passed in */
const char *getWeaponEffect(WEAPON_EFFECT effect)
{
	switch (effect)
	{
	case WE_ANTI_PERSONNEL: return "ANTI PERSONNEL";
	case WE_ANTI_TANK: return "ANTI TANK";
	case WE_BUNKER_BUSTER: return "BUNKER BUSTER";
	case WE_ARTILLERY_ROUND: return "ARTILLERY ROUND";
	case WE_FLAMER: return "FLAMER";
	case WE_ANTI_AIRCRAFT: return "ANTI AIRCRAFT";
	case WE_NUMEFFECTS: break;
	}
	ASSERT(false, "No such weapon effect");
	return "Bad weapon effect";
}

bool getWeaponClass(const WzString& weaponClassStr, WEAPON_CLASS *weaponClass)
{
	if (weaponClassStr.compare("KINETIC") == 0)
	{
		*weaponClass = WC_KINETIC;
	}
	else if (weaponClassStr.compare("HEAT") == 0)
	{
		*weaponClass = WC_HEAT;
	}
	else
	{
		ASSERT(false, "Bad weapon class %s", weaponClassStr.toUtf8().c_str());
		return false;
	};
	return true;
}

#define ASSERT_PLAYER_OR_RETURN(retVal, player) \
	ASSERT_OR_RETURN(retVal, player >= 0 && player < MAX_PLAYERS, "Invalid player: %" PRIu32 "", player);

/*Access functions for the upgradeable stats of a weapon*/
int weaponFirePause(const WEAPON_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].firePause;
}

/* Reload time is reduced for weapons with salvo fire */
int weaponReloadTime(const WEAPON_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].reloadTime;
}

int weaponLongHit(const WEAPON_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].hitChance;
}

int weaponShortHit(const WEAPON_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].shortHitChance;
}

int weaponDamage(const WEAPON_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].damage;
}

int weaponRadDamage(const WEAPON_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].radiusDamage;
}

int weaponPeriodicalDamage(const WEAPON_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].periodicalDamage;
}

int sensorRange(const SENSOR_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].range;
}

int ecmRange(const ECM_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].range;
}

int repairPoints(const REPAIR_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].repairPoints;
}

int constructorPoints(const CONSTRUCT_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].constructPoints;
}

int bodyPower(const BODY_STATS& psStats, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	return psStats.upgrade[player].power;
}

int bodyArmour(const BODY_STATS& psStats, int player, WEAPON_CLASS weaponClass)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	switch (weaponClass)
	{
	case WC_KINETIC:
		return psStats.upgrade[player].armour;
	case WC_HEAT:
		return psStats.upgrade[player].thermal;
	case WC_NUM_WEAPON_CLASSES:
		break;
	}
	ASSERT(false, "Unknown weapon class");
	return 0;	// Should never get here.
}

//calculates the weapons ROF based on the fire pause and the salvos
int weaponROF(const WEAPON_STATS& psStat, int player)
{
	ASSERT_PLAYER_OR_RETURN(0, player);
	int rof = 0;
	// if there are salvos
	if (player >= 0
	    && psStat.upgrade[player].numRounds
	    && psStat.upgrade[player].reloadTime != 0)
	{
		// Rounds per salvo multiplied with the number of salvos per minute
		rof = psStat.upgrade[player].numRounds * 60 * GAME_TICKS_PER_SEC / weaponReloadTime(psStat, player);
	}

	if (rof == 0)
	{
		rof = weaponFirePause(psStat, player);
		if (rof != 0)
		{
			rof = (UWORD)(60 * GAME_TICKS_PER_SEC / rof);
		}
		//else leave it at 0
	}
	return rof;
}

/* Check if an object has a weapon */
bool objHasWeapon(const BASE_OBJECT *psObj)
{
	//check if valid type
	if (psObj->type == OBJ_DROID)
	{
		if (((const DROID *)psObj)->numWeaps > 0)
		{
			return true;
		}
	}
	else if (psObj->type == OBJ_STRUCTURE)
	{
		if (((const STRUCTURE *)psObj)->numWeaps > 0)
		{
			return true;
		}
	}

	return false;
}

SENSOR_STATS *objActiveRadar(const BASE_OBJECT *psObj)
{
	SENSOR_STATS	*psStats = nullptr;
	int				compIndex;

	switch (psObj->type)
	{
	case OBJ_DROID:
		if (((const DROID *)psObj)->droidType != DROID_SENSOR && ((const DROID *)psObj)->droidType != DROID_COMMAND)
		{
			return nullptr;
		}
		compIndex = ((const DROID *)psObj)->asBits[COMP_SENSOR];
		ASSERT_OR_RETURN(nullptr, compIndex < asSensorStats.size(), "Invalid range referenced for numSensorStats, %d > %zu", compIndex, asSensorStats.size());
		psStats = &asSensorStats[compIndex];
		break;
	case OBJ_STRUCTURE:
		psStats = ((const STRUCTURE *)psObj)->pStructureType->pSensor;
		if (psStats == nullptr || psStats->location != LOC_TURRET || ((const STRUCTURE *)psObj)->status != SS_BUILT)
		{
			return nullptr;
		}
		break;
	default:
		break;
	}
	return psStats;
}

bool objRadarDetector(const BASE_OBJECT *psObj)
{
	if (psObj->type == OBJ_STRUCTURE)
	{
		const STRUCTURE *psStruct = (const STRUCTURE *)psObj;

		return (psStruct->status == SS_BUILT && psStruct->pStructureType->pSensor && psStruct->pStructureType->pSensor->type == RADAR_DETECTOR_SENSOR);
	}
	else if (psObj->type == OBJ_DROID)
	{
		const DROID *psDroid = (const DROID *)psObj;
		SENSOR_STATS *psSensor = psDroid->getSensorStats();

		return (psSensor && psSensor->type == RADAR_DETECTOR_SENSOR);
	}
	return false;
}

gfx_api::texture* loadImageForWeapSubclass(WEAPON_SUBCLASS subClass)
{
	const char* imagePath = nullptr;
	switch (subClass)
	{
		case WSC_MGUN:
			imagePath = "images/intfac/wsc_mgun.png";
			break;
		case WSC_CANNON:
			imagePath = "images/intfac/wsc_cannon.png";
			break;
		case WSC_MORTARS:
			imagePath = "images/intfac/wsc_mortars.png";
			break;
		case WSC_MISSILE:
			imagePath = "images/intfac/wsc_missile.png";
			break;
		case WSC_ROCKET:
			imagePath = "images/intfac/wsc_rocket.png";
			break;
		case WSC_ENERGY:
			imagePath = "images/intfac/wsc_energy.png";
			break;
		case WSC_GAUSS:
			imagePath = "images/intfac/wsc_gauss.png";
			break;
		case WSC_FLAME:
			imagePath = "images/intfac/wsc_flame.png";
			break;
		//case WSC_CLOSECOMBAT:
		case WSC_HOWITZERS:
			imagePath = "images/intfac/wsc_howitzers.png";
			break;
		case WSC_ELECTRONIC:
			imagePath = "images/intfac/wsc_electronic.png";
			break;
		case WSC_AAGUN:
			imagePath = "images/intfac/wsc_aagun.png";
			break;
		case WSC_SLOWMISSILE:
			imagePath = "images/intfac/wsc_slowmissile.png";
			break;
		case WSC_SLOWROCKET:
			imagePath = "images/intfac/wsc_slowrocket.png";
			break;
		case WSC_LAS_SAT:
			imagePath = "images/intfac/wsc_las_sat.png";
			break;
		case WSC_BOMB:
			imagePath = "images/intfac/wsc_bomb.png";
			break;
		case WSC_COMMAND:
			imagePath = "images/intfac/wsc_command.png";
			break;
		case WSC_EMP:
			imagePath = "images/intfac/wsc_emp.png";
			break;
		case WSC_NUM_WEAPON_SUBCLASSES:	/** The number of enumerators in this enum.	 */
			break;
	}
	ASSERT_OR_RETURN(nullptr, imagePath != nullptr, "No image path");
	WZ_Notification_Image img(imagePath);
	return img.loadImageToTexture();
}
