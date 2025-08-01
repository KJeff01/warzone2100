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
/*!
 * \file structure.c
 *
 * Store Structure stats.
 * WARNING!!!!!!
 * By the picking of these code-bombs, something wicked this way comes. This
 * file is almost as evil as hci.c
 */
#include <string.h>
#include <algorithm>

#include "lib/framework/frame.h"
#include "lib/framework/geometry.h"
#include "lib/framework/physfs_ext.h"
#include "lib/ivis_opengl/imd.h"
#include "objects.h"
#include "ai.h"
#include "map.h"
#include "lib/gamelib/gtime.h"
#include "visibility.h"
#include "structure.h"
#include "research.h"
#include "hci.h"
#include "power.h"
#include "miscimd.h"
#include "effects.h"
#include "combat.h"
#include "lib/sound/audio.h"
#include "lib/sound/audio_id.h"
#include "stats.h"
#include "lib/framework/math_ext.h"
#include "display3d.h"
#include "geometry.h"
// FIXME Direct iVis implementation include!
#include "lib/ivis_opengl/piematrix.h"
#include "lib/framework/fixedpoint.h"
#include "order.h"
#include "droid.h"
#include "action.h"
#include "group.h"
#include "transporter.h"
#include "fpath.h"
#include "mission.h"
#include "levels.h"
#include "console.h"
#include "cmddroid.h"
#include "feature.h"
#include "mapgrid.h"
#include "projectile.h"
#include "intdisplay.h"
#include "display.h"
#include "difficulty.h"
#include "game.h"
#include "qtscript.h"
#include "multiplay.h"
#include "lib/netplay/sync_debug.h"
#include "multigifts.h"
#include "loop.h"
#include "template.h"
#include "scores.h"
#include "gateway.h"
#include "multistat.h"
#include "keybind.h"

#include "random.h"
#include <functional>
#include <unordered_map>

//Maximium slope of the terrain for building a structure
#define MAX_INCLINE		50//80//40

/* droid construction smoke cloud constants */
#define	DROID_CONSTRUCTION_SMOKE_OFFSET	30
#define	DROID_CONSTRUCTION_SMOKE_HEIGHT	20

//used to calculate how often to increase the resistance level of a structure
#define RESISTANCE_INTERVAL			2000

//Value is stored for easy access to this structure stat
UDWORD			factoryModuleStat;
UDWORD			powerModuleStat;
UDWORD			researchModuleStat;

//holder for all StructureStats
STRUCTURE_STATS		*asStructureStats = nullptr;
UDWORD				numStructureStats = 0;
static std::unordered_map<WzString, STRUCTURE_STATS *> lookupStructStatPtr;
optional<int> structureDamageBaseExperienceLevel;

//used to hold the modifiers cross refd by weapon effect and structureStrength
STRUCTSTRENGTH_MODIFIER		asStructStrengthModifier[WE_NUMEFFECTS][NUM_STRUCT_STRENGTH];

//specifies which numbers have been allocated for the assembly points for the factories
static std::vector<bool>       factoryNumFlag[MAX_PLAYERS][NUM_FLAG_TYPES];

// the number of different (types of) droids that can be put into a production run
#define MAX_IN_RUN		9

//the list of what to build - only for selectedPlayer
std::vector<ProductionRun> asProductionRun[NUM_FACTORY_TYPES];

//stores which player the production list has been set up for
SBYTE               productionPlayer;

/* destroy building construction droid stat pointer */
static	STRUCTURE_STATS	*g_psStatDestroyStruct = nullptr;

// the structure that was last hit
STRUCTURE	*psLastStructHit;

//flag for drawing all sat uplink sees
static		UBYTE	satUplinkExists[MAX_PLAYERS];
//flag for when the player has one built - either completely or partially
static		UBYTE	lasSatExists[MAX_PLAYERS];

static bool setFunctionality(STRUCTURE *psBuilding, STRUCTURE_TYPE functionType);
static void setFlagPositionInc(FUNCTIONALITY *pFunctionality, UDWORD player, UBYTE factoryType);
static void informPowerGen(STRUCTURE *psStruct);
static bool electronicReward(STRUCTURE *psStructure, UBYTE attackPlayer);
static void factoryReward(UBYTE losingPlayer, UBYTE rewardPlayer);
static void repairFacilityReward(UBYTE losingPlayer, UBYTE rewardPlayer);
static void findAssemblyPointPosition(UDWORD *pX, UDWORD *pY, UDWORD player);
static void removeStructFromMap(STRUCTURE *psStruct);
static void resetResistanceLag(STRUCTURE *psBuilding);
static int structureTotalReturn(const STRUCTURE *psStruct);
static void parseFavoriteStructs();
static void packFavoriteStructs();
static bool structureHasModules(const STRUCTURE *psStruct);

// last time the maximum units message was displayed
static UDWORD	lastMaxUnitMessage;

// max number of units
static int droidLimit[MAX_PLAYERS];
// max number of commanders
static int commanderLimit[MAX_PLAYERS];
// max number of constructors
static int constructorLimit[MAX_PLAYERS];

static std::vector<WzString> favoriteStructs;

#define MAX_UNIT_MESSAGE_PAUSE 40000

static void auxStructureNonblocking(STRUCTURE *psStructure)
{
	StructureBounds b = getStructureBounds(psStructure);

	for (int i = 0; i < b.size.x; i++)
	{
		for (int j = 0; j < b.size.y; j++)
		{
			int x = b.map.x + i;
			int y = b.map.y + j;
			MAPTILE *psTile = mapTile(x, y);
			if (psTile->psObject == psStructure)
			{
				auxClearAll(x, y, AUXBITS_BLOCKING | AUXBITS_OUR_BUILDING | AUXBITS_NONPASSABLE);
			}
			else
			{
				// Likely a script-queued object removal for a position where the script immediately replaced the old struct - just log
				debug(LOG_WZ, "Skipping blocking bit clear - structure %" PRIu32 " is not the recorded tile object at (%d, %d)", psStructure->id, x, y);
			}
		}
	}
}

static void auxStructureBlocking(STRUCTURE *psStructure)
{
	StructureBounds b = getStructureBounds(psStructure);

	for (int i = 0; i < b.size.x; i++)
	{
		for (int j = 0; j < b.size.y; j++)
		{
			auxSetAllied(b.map.x + i, b.map.y + j, psStructure->player, AUXBITS_OUR_BUILDING);
			auxSetAll(b.map.x + i, b.map.y + j, AUXBITS_BLOCKING | AUXBITS_NONPASSABLE);
		}
	}
}

static void auxStructureOpenGate(STRUCTURE *psStructure)
{
	StructureBounds b = getStructureBounds(psStructure);

	for (int i = 0; i < b.size.x; i++)
	{
		for (int j = 0; j < b.size.y; j++)
		{
			auxClearAll(b.map.x + i, b.map.y + j, AUXBITS_BLOCKING);
		}
	}
}

static void auxStructureClosedGate(STRUCTURE *psStructure)
{
	StructureBounds b = getStructureBounds(psStructure);

	for (int i = 0; i < b.size.x; i++)
	{
		for (int j = 0; j < b.size.y; j++)
		{
			auxSetEnemy(b.map.x + i, b.map.y + j, psStructure->player, AUXBITS_NONPASSABLE);
			auxSetAll(b.map.x + i, b.map.y + j, AUXBITS_BLOCKING);
		}
	}
}

bool IsStatExpansionModule(const STRUCTURE_STATS *psStats)
{
	// If the stat is any of the 3 expansion types ... then return true
	return psStats->type == REF_POWER_MODULE ||
	       psStats->type == REF_FACTORY_MODULE  ||
	       psStats->type == REF_RESEARCH_MODULE;
}

static int numStructureModules(STRUCTURE const *psStruct)
{
	return psStruct->capacity;
}

bool STRUCTURE::isBlueprint() const
{
	return (status == SS_BLUEPRINT_VALID ||
	        status == SS_BLUEPRINT_INVALID ||
	        status == SS_BLUEPRINT_PLANNED ||
	        status == SS_BLUEPRINT_PLANNED_BY_ALLY);
}

bool isBlueprint(const BASE_OBJECT *psObject)
{
	return psObject != nullptr && psObject->type == OBJ_STRUCTURE && ((const STRUCTURE*)psObject)->isBlueprint();
}

// Add smoke effect to cover the droid's emergence from the factory or when building structures.
// DISPLAY ONLY - does not affect game state.
static void displayConstructionCloud(const Vector3i &pos)
{
	const Vector2i coordinates = {pos.x, pos.y};
	const MAPTILE *psTile = mapTile(map_coord(coordinates));
	if (!tileIsClearlyVisible(psTile))
	{
		return;
	}

	Vector3i iVecEffect;

	iVecEffect.x = pos.x;
	iVecEffect.y = map_Height(pos.x, pos.y) + DROID_CONSTRUCTION_SMOKE_HEIGHT;
	iVecEffect.z = pos.y;
	addEffect(&iVecEffect, EFFECT_CONSTRUCTION, CONSTRUCTION_TYPE_DRIFTING, false, nullptr, 0, gameTime - deltaGameTime + 1);
	iVecEffect.x = pos.x - DROID_CONSTRUCTION_SMOKE_OFFSET;
	iVecEffect.z = pos.y - DROID_CONSTRUCTION_SMOKE_OFFSET;
	addEffect(&iVecEffect, EFFECT_CONSTRUCTION, CONSTRUCTION_TYPE_DRIFTING, false, nullptr, 0, gameTime - deltaGameTime + 1);
	iVecEffect.z = pos.y + DROID_CONSTRUCTION_SMOKE_OFFSET;
	addEffect(&iVecEffect, EFFECT_CONSTRUCTION, CONSTRUCTION_TYPE_DRIFTING, false, nullptr, 0, gameTime - deltaGameTime + 1);
	iVecEffect.x = pos.x + DROID_CONSTRUCTION_SMOKE_OFFSET;
	addEffect(&iVecEffect, EFFECT_CONSTRUCTION, CONSTRUCTION_TYPE_DRIFTING, false, nullptr, 0, gameTime - deltaGameTime + 1);
	iVecEffect.z = pos.y - DROID_CONSTRUCTION_SMOKE_OFFSET;
	addEffect(&iVecEffect, EFFECT_CONSTRUCTION, CONSTRUCTION_TYPE_DRIFTING, false, nullptr, 0, gameTime - deltaGameTime + 1);
}

void initStructLimits()
{
	for (unsigned i = 0; i < numStructureStats; ++i)
	{
		memset(asStructureStats[i].curCount, 0, sizeof(asStructureStats[i].curCount));
	}
}

void structureInitVars()
{
	ASSERT(asStructureStats == nullptr, "Failed to cleanup prior asStructureStats?");

	asStructureStats = nullptr;
	lookupStructStatPtr.clear();
	numStructureStats = 0;
	factoryModuleStat = 0;
	powerModuleStat = 0;
	researchModuleStat = 0;
	lastMaxUnitMessage = 0;
	structureDamageBaseExperienceLevel = nullopt;

	initStructLimits();
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		droidLimit[i] = INT16_MAX;
		commanderLimit[i] = INT16_MAX;
		constructorLimit[i] = INT16_MAX;
		for (int j = 0; j < NUM_FLAG_TYPES; j++)
		{
			factoryNumFlag[i][j].clear();
		}
	}
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		satUplinkExists[i] = false;
		lasSatExists[i] = false;
	}
	//initialise the selectedPlayer's production run
	for (unsigned type = 0; type < NUM_FACTORY_TYPES; ++type)
	{
		asProductionRun[type].clear();
	}
	//set up at beginning of game which player will have a production list
	productionPlayer = (SBYTE)selectedPlayer;
}

/*Initialise the production list and set up the production player*/
void changeProductionPlayer(UBYTE player)
{
	//clear the production run
	for (unsigned type = 0; type < NUM_FACTORY_TYPES; ++type)
	{
		asProductionRun[type].clear();
	}
	//set this player to have the production list
	productionPlayer = player;
}


/*initialises the flag before a new data set is loaded up*/
void initFactoryNumFlag()
{
	for (unsigned i = 0; i < MAX_PLAYERS; i++)
	{
		//initialise the flag
		for (unsigned j = 0; j < NUM_FLAG_TYPES; j++)
		{
			factoryNumFlag[i][j].clear();
		}
	}
}

//called at start of missions
void resetFactoryNumFlag()
{
	for (unsigned int i = 0; i < MAX_PLAYERS; i++)
	{
		for (int type = 0; type < NUM_FLAG_TYPES; type++)
		{
			// reset them all to false
			factoryNumFlag[i][type].clear();
		}
		//look through the list of structures to see which have been used
		for (const STRUCTURE *psStruct : apsStructLists[i])
		{
			FLAG_TYPE type;
			switch (psStruct->pStructureType->type)
			{
			case REF_FACTORY:        type = FACTORY_FLAG; break;
			case REF_CYBORG_FACTORY: type = CYBORG_FLAG;  break;
			case REF_VTOL_FACTORY:   type = VTOL_FLAG;    break;
			case REF_REPAIR_FACILITY: type = REPAIR_FLAG; break;
			default: continue;
			}

			int inc = -1;
			if (type == REPAIR_FLAG)
			{
				REPAIR_FACILITY *psRepair = &psStruct->pFunctionality->repairFacility;
				if (psRepair->psDeliveryPoint != nullptr)
				{
					inc = psRepair->psDeliveryPoint->factoryInc;
				}
			}
			else
			{
				FACTORY *psFactory = &psStruct->pFunctionality->factory;
				if (psFactory->psAssemblyPoint != nullptr)
				{
					inc = psFactory->psAssemblyPoint->factoryInc;
				}
			}
			if (inc >= 0)
			{
				factoryNumFlag[i][type].resize(std::max<size_t>(factoryNumFlag[i][type].size(), inc + 1), false);
				factoryNumFlag[i][type][inc] = true;
			}
		}
	}
}

int getStructureDamageBaseExperienceLevel()
{
	// COMPAT NOTES:
	//
	// Default / compat structure damage handling (the only option for many years - from at least 2.0.10-4.4.2):
	//
	// This causes the game to treat structures at a base experience level of 1 instead of 0 when calculating damage to them,
	// yielding actualDamage at 94% of the base damage value. Or, in other words, structures are a bit tougher
	// than the raw numbers in the stats files would suggest, and get a hidden experience level boost.
	//
	// However, structure.json created and tested during this long period may be expecting this outcome / behavior,
	// So unless it's explicitly specified in the special `_config_` dict, it always defaults to `1`.

	return structureDamageBaseExperienceLevel.value_or(1);
}

static const StringToEnum<STRUCTURE_TYPE> map_STRUCTURE_TYPE[] =
{
	{ "HQ",                 REF_HQ                  },
	{ "FACTORY",            REF_FACTORY             },
	{ "FACTORY MODULE",     REF_FACTORY_MODULE      },
	{ "RESEARCH",           REF_RESEARCH            },
	{ "RESEARCH MODULE",    REF_RESEARCH_MODULE     },
	{ "POWER GENERATOR",    REF_POWER_GEN           },
	{ "POWER MODULE",       REF_POWER_MODULE        },
	{ "RESOURCE EXTRACTOR", REF_RESOURCE_EXTRACTOR  },
	{ "DEFENSE",            REF_DEFENSE             },
	{ "WALL",               REF_WALL                },
	{ "CORNER WALL",        REF_WALLCORNER          },
	{ "REPAIR FACILITY",    REF_REPAIR_FACILITY     },
	{ "COMMAND RELAY",      REF_COMMAND_CONTROL     },
	{ "DEMOLISH",           REF_DEMOLISH            },
	{ "CYBORG FACTORY",     REF_CYBORG_FACTORY      },
	{ "VTOL FACTORY",       REF_VTOL_FACTORY        },
	{ "LAB",                REF_LAB                 },
	{ "GENERIC",            REF_GENERIC             },
	{ "REARM PAD",          REF_REARM_PAD           },
	{ "MISSILE SILO",       REF_MISSILE_SILO        },
	{ "SAT UPLINK",         REF_SAT_UPLINK          },
	{ "GATE",               REF_GATE                },
	{ "LASSAT",             REF_LASSAT              },
	{ "FORTRESS",           REF_FORTRESS            },
};

static const StringToEnum<STRUCT_STRENGTH> map_STRUCT_STRENGTH[] =
{
	{"SOFT",        STRENGTH_SOFT           },
	{"MEDIUM",      STRENGTH_MEDIUM         },
	{"HARD",        STRENGTH_HARD           },
	{"BUNKER",      STRENGTH_BUNKER         },
};

static void initModuleStats(unsigned i, STRUCTURE_TYPE type)
{
	//need to work out the stats for the modules - HACK! - But less hacky than what was here before.
	switch (type)
	{
	case REF_FACTORY_MODULE:
		//store the stat for easy access later on
		factoryModuleStat = i;
		break;

	case REF_RESEARCH_MODULE:
		//store the stat for easy access later on
		researchModuleStat = i;
		break;

	case REF_POWER_MODULE:
		//store the stat for easy access later on
		powerModuleStat = i;
		break;
	default:
		break;
	}
}

template <typename T, size_t N>
inline
size_t sizeOfArray(const T(&)[ N ])
{
	return N;
}

#define STRUCTURE_JSON_CONFIG_DICT_KEY "_config_"

/* load the structure stats from the ini file */
bool loadStructureStats(WzConfig &ini)
{
	const WzString CONFIG_DICT_KEY_STR = STRUCTURE_JSON_CONFIG_DICT_KEY;

	std::map<WzString, STRUCTURE_TYPE> structType;
	for (unsigned i = 0; i < sizeOfArray(map_STRUCTURE_TYPE); ++i)
	{
		structType.emplace(WzString::fromUtf8(map_STRUCTURE_TYPE[i].string), map_STRUCTURE_TYPE[i].value);
	}

	std::map<WzString, STRUCT_STRENGTH> structStrength;
	for (unsigned i = 0; i < sizeOfArray(map_STRUCT_STRENGTH); ++i)
	{
		structStrength.emplace(WzString::fromUtf8(map_STRUCT_STRENGTH[i].string), map_STRUCT_STRENGTH[i].value);
	}

	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	asStructureStats = new STRUCTURE_STATS[list.size()];
	numStructureStats = 0;
	size_t statWriteIdx = 0;
	for (size_t readIdx = 0; readIdx < list.size(); ++readIdx)
	{
		if (list[readIdx] == CONFIG_DICT_KEY_STR)
		{
			// handle the special config dict
			ini.beginGroup(list[readIdx]);

			// baseStructDamageExpLevel
			bool convValueSuccess = false;
			auto baseStructDamageExpLevel = ini.value("baseStructDamageExpLevel", 1).toInt(&convValueSuccess);
			if (!convValueSuccess)
			{
				baseStructDamageExpLevel = 1; // reset to old default
			}
			if (baseStructDamageExpLevel >= 0 && baseStructDamageExpLevel < 10)
			{
				if (!structureDamageBaseExperienceLevel.has_value())
				{
					structureDamageBaseExperienceLevel = baseStructDamageExpLevel;
				}
				else
				{
					if (structureDamageBaseExperienceLevel.value() != baseStructDamageExpLevel)
					{
						debug(LOG_ERROR, "Non-matching structure JSON baseStructDamageExpLevel");
						debug(LOG_INFO, "Structure JSON file \"%s\" has specified a baseStructDamageExpLevel (\"%d\") that does not match the first loaded structure JSON's baseStructDamageExpLevel (\"%d\")", ini.fileName().toUtf8().c_str(), baseStructDamageExpLevel, structureDamageBaseExperienceLevel.value());
					}
				}
			}
			else
			{
				ASSERT_OR_RETURN(false, false, "Invalid _config_ \"baseStructDamageExpLevel\" value: \"%d\"", baseStructDamageExpLevel);
			}

			ini.endGroup();
			continue;
		}

		ini.beginGroup(list[readIdx]);
		STRUCTURE_STATS *psStats = &asStructureStats[statWriteIdx];
		loadStructureStats_BaseStats(ini, psStats, statWriteIdx);
		psStats->ref = STAT_STRUCTURE + statWriteIdx;

		// set structure type
		WzString type = ini.value("type", "").toWzString();
		if (structType.find(type) == structType.end())
		{
			ASSERT(false, "Invalid type '%s' of structure '%s'", type.toUtf8().c_str(), getID(psStats));
			// nothing we can really do here except skip this stat
			unloadStructureStats_BaseStats(*psStats); // must be called to properly clear from lookup tables (that loadStructureStats_BaseStats adds it to)
			ini.endGroup();
			continue;
		}
		psStats->type = structType[type];

		// save indexes of special structures for futher use
		initModuleStats(statWriteIdx, psStats->type);  // This function looks like a hack. But slightly less hacky than before.

		if (ini.contains("userLimits"))
		{
			Vector3i limits = ini.vector3i("userLimits");
			psStats->minLimit = limits[0];
			psStats->maxLimit = limits[2];
			psStats->base.limit = limits[1];
		}
		else
		{
			psStats->minLimit = 0;
			psStats->maxLimit = LOTS_OF;
			psStats->base.limit = LOTS_OF;
		}
		psStats->base.research = ini.value("researchPoints", 0).toInt();
		psStats->base.moduleResearch = ini.value("moduleResearchPoints", 0).toInt();
		psStats->base.production = ini.value("productionPoints", 0).toInt();
		psStats->base.moduleProduction = ini.value("moduleProductionPoints", 0).toInt();
		psStats->base.repair = ini.value("repairPoints", 0).toInt();
		psStats->base.power = ini.value("powerPoints", 0).toInt();
		psStats->base.modulePower = ini.value("modulePowerPoints", 0).toInt();
		psStats->base.rearm = ini.value("rearmPoints", 0).toInt();
		psStats->base.resistance = ini.value("resistance", 0).toUInt();
		psStats->base.hitpoints = ini.value("hitpoints", 1).toUInt();
		psStats->base.armour = ini.value("armour", 0).toUInt();
		psStats->base.thermal = ini.value("thermal", 0).toUInt();
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			psStats->upgrade[i].limit = psStats->base.limit;
			psStats->upgrade[i].research = psStats->base.research;
			psStats->upgrade[i].moduleResearch = psStats->base.moduleResearch;
			psStats->upgrade[i].power = psStats->base.power;
			psStats->upgrade[i].modulePower = psStats->base.modulePower;
			psStats->upgrade[i].repair = psStats->base.repair;
			psStats->upgrade[i].production = psStats->base.production;
			psStats->upgrade[i].moduleProduction = psStats->base.moduleProduction;
			psStats->upgrade[i].rearm = psStats->base.rearm;
			psStats->upgrade[i].resistance = ini.value("resistance", 0).toUInt();
			psStats->upgrade[i].hitpoints = ini.value("hitpoints", 1).toUInt();
			psStats->upgrade[i].armour = ini.value("armour", 0).toUInt();
			psStats->upgrade[i].thermal = ini.value("thermal", 0).toUInt();
		}

		psStats->flags = 0;
		std::vector<WzString> flags = ini.value("flags").toWzStringList();
		for (size_t i = 0; i < flags.size(); i++)
		{
			if (flags[i] == "Connected")
			{
				psStats->flags |= STRUCTURE_CONNECTED;
			}
		}

		// set structure strength
		WzString strength = ini.value("strength", "").toWzString();
		if (structStrength.find(strength) == structStrength.end())
		{
			ASSERT(false, "Invalid strength '%s' of structure '%s'", strength.toUtf8().c_str(), getID(psStats));
			strength = map_STRUCT_STRENGTH[0].string; // just default to the first strength - stat file creator needs to fix the json!
		}
		psStats->strength = structStrength[strength];

		// set baseWidth
		psStats->baseWidth = ini.value("width", 0).toUInt();
		if (psStats->baseWidth > 100)
		{
			ASSERT(false, "Invalid width '%d' for structure '%s'", psStats->baseWidth, getID(psStats));
			psStats->baseWidth = 100;
		}

		// set baseBreadth
		psStats->baseBreadth = ini.value("breadth", 0).toUInt();
		if (psStats->baseBreadth >= 100)
		{
			ASSERT(false, "Invalid breadth '%d' for structure '%s'", psStats->baseBreadth, getID(psStats));
			psStats->baseBreadth = 99;
		}

		psStats->height = ini.value("height").toUInt();
		psStats->powerToBuild = ini.value("buildPower").toUInt();
		psStats->buildPoints = ini.value("buildPoints").toUInt();

		// set structure models
		std::vector<WzString> models = ini.value("structureModel").toWzStringList();
		for (size_t j = 0; j < models.size(); j++)
		{
			iIMDBaseShape *imd = modelGet(models[j].trimmed());
			ASSERT(imd != nullptr, "Cannot find the PIE structureModel '%s' for structure '%s'", models[j].toUtf8().c_str(), getID(psStats));
			psStats->pIMD.push_back(imd);
		}

		// set base model
		WzString baseModel = ini.value("baseModel", "").toWzString();
		if (baseModel.compare("") != 0)
		{
			iIMDBaseShape *imd = modelGet(baseModel);
			ASSERT(imd != nullptr, "Cannot find the PIE baseModel '%s' for structure '%s'", baseModel.toUtf8().c_str(), getID(psStats));
			psStats->pBaseIMD = imd;
		}

		int ecm = getCompFromName(COMP_ECM, ini.value("ecmID", "ZNULLECM").toWzString());
		if (ecm >= 0)
		{
			psStats->pECM = &asECMStats[ecm];
		}
		else
		{
			ASSERT(ecm >= 0, "Invalid ECM found for '%s'", getID(psStats));
		}

		int sensor = getCompFromName(COMP_SENSOR, ini.value("sensorID", "ZNULLSENSOR").toWzString());
		if (sensor >= 0)
		{
			psStats->pSensor = &asSensorStats[sensor];
		}
		else
		{
			ASSERT(sensor >= 0, "Invalid sensor found for structure '%s'", getID(psStats));
		}

		// set list of weapons
		std::fill_n(psStats->psWeapStat, MAX_WEAPONS, (WEAPON_STATS *)nullptr);
		std::vector<WzString> weapons = ini.value("weapons").toWzStringList();
		if (weapons.size() > MAX_WEAPONS)
		{
			ASSERT(false, "Too many weapons are attached to structure '%s'. Maximum is %d", getID(psStats), MAX_WEAPONS);
			weapons.resize(MAX_WEAPONS);
		}
		psStats->numWeaps = weapons.size();
		for (unsigned j = 0; j < psStats->numWeaps; ++j)
		{
			WzString weaponsID = weapons[j].trimmed();
			int weapon = getCompFromName(COMP_WEAPON, weaponsID);
			if (weapon >= 0)
			{
				WEAPON_STATS *pWeap = &asWeaponStats[weapon];
				psStats->psWeapStat[j] = pWeap;
			}
			else
			{
				// couldn't find the weapon - truncate weapons at this item
				ASSERT(false, "Invalid item '%s' in list of weapons of structure '%s' ", weaponsID.toUtf8().c_str(), getID(psStats));
				psStats->numWeaps = j;
				break;
			}
		}

		// check used structure turrets
		int types = 0;
		types += psStats->numWeaps != 0;
		types += psStats->pECM != nullptr && psStats->pECM->location == LOC_TURRET;
		types += psStats->pSensor != nullptr && psStats->pSensor->location == LOC_TURRET;
		ASSERT(types <= 1, "Too many turret types for structure '%s'", getID(psStats));

		psStats->combinesWithWall = ini.value("combinesWithWall", false).toBool();

		ini.endGroup();

		lookupStructStatPtr.insert(std::make_pair(psStats->id, psStats));
		++statWriteIdx;
	}
	numStructureStats = statWriteIdx;
	parseFavoriteStructs();

	/* get global dummy stat pointer - GJ */
	g_psStatDestroyStruct = nullptr;
	for (unsigned iID = 0; iID < numStructureStats; ++iID)
	{
		if (asStructureStats[iID].type == REF_DEMOLISH)
		{
			g_psStatDestroyStruct = asStructureStats + iID;
			break;
		}
	}
	ASSERT_OR_RETURN(false, g_psStatDestroyStruct, "Destroy structure stat not found");

	return true;
}

/* set the current number of structures of each type built */
void setCurrentStructQuantity(bool displayError)
{
	for (unsigned player = 0; player < MAX_PLAYERS; player++)
	{
		for (unsigned inc = 0; inc < numStructureStats; inc++)
		{
			asStructureStats[inc].curCount[player] = 0;
		}
		for (const STRUCTURE *psCurr : apsStructLists[player])
		{
			unsigned inc = psCurr->pStructureType - asStructureStats;
			asStructureStats[inc].curCount[player]++;
			if (displayError)
			{
				//check quantity never exceeds the limit
				ASSERT(asStructureStats[inc].curCount[player] <= asStructureStats[inc].upgrade[player].limit,
				       "There appears to be too many %s on this map!", getStatsName(&asStructureStats[inc]));
			}
		}
	}
}

/*Load the Structure Strength Modifiers from the file exported from Access*/
bool loadStructureStrengthModifiers(WzConfig &ini)
{
	//initialise to 100%
	for (unsigned i = 0; i < WE_NUMEFFECTS; ++i)
	{
		for (unsigned j = 0; j < NUM_STRUCT_STRENGTH; ++j)
		{
			asStructStrengthModifier[i][j] = 100;
		}
	}
	ASSERT(ini.isAtDocumentRoot(), "WzConfig instance is in the middle of traversal");
	std::vector<WzString> list = ini.childGroups();
	for (size_t i = 0; i < list.size(); i++)
	{
		WEAPON_EFFECT effectInc;
		ini.beginGroup(list[i]);
		if (!getWeaponEffect(list[i], &effectInc))
		{
			debug(LOG_FATAL, "Invalid Weapon Effect - %s", list[i].toUtf8().c_str());
			ini.endGroup();
			continue;
		}
		std::vector<WzString> keys = ini.childKeys();
		for (size_t j = 0; j < keys.size(); j++)
		{
			const WzString& strength = keys.at(j);
			int modifier = ini.value(strength).toInt();
			// FIXME - add support for dynamic categories
			if (strength.compare("SOFT") == 0)
			{
				asStructStrengthModifier[effectInc][0] = modifier;
			}
			else if (strength.compare("MEDIUM") == 0)
			{
				asStructStrengthModifier[effectInc][1] = modifier;
			}
			else if (strength.compare("HARD") == 0)
			{
				asStructStrengthModifier[effectInc][2] = modifier;
			}
			else if (strength.compare("BUNKER") == 0)
			{
				asStructStrengthModifier[effectInc][3] = modifier;
			}
			else
			{
				debug(LOG_ERROR, "Unsupported structure strength %s", strength.toUtf8().c_str());
			}
		}
		ini.endGroup();
	}
	return true;
}

bool structureStatsShutDown()
{
	packFavoriteStructs();
	if (asStructureStats)
	{
		for (unsigned i = 0; i < numStructureStats; ++i)
		{
			unloadStructureStats_BaseStats(asStructureStats[i]);
		}
	}
	lookupStructStatPtr.clear();
	delete[] asStructureStats;
	asStructureStats = nullptr;
	numStructureStats = 0;
	return true;
}

// TODO: The abandoned code needs to be factored out, see: saveMissionData
void handleAbandonedStructures()
{
	// TODO: do something here
}

/* Deals damage to a Structure.
 * \param psStructure structure to deal damage to
 * \param damage amount of damage to deal
 * \param weaponClass the class of the weapon that deals the damage
 * \param weaponSubClass the subclass of the weapon that deals the damage
 * \return < 0 when the dealt damage destroys the structure, > 0 when the structure survives
 */
int32_t structureDamage(STRUCTURE *psStructure, unsigned damage, WEAPON_CLASS weaponClass, WEAPON_SUBCLASS weaponSubClass, unsigned impactTime, bool isDamagePerSecond, int minDamage, bool empRadiusHit)
{
	int32_t relativeDamage;

	CHECK_STRUCTURE(psStructure);

	debug(LOG_ATTACK, "structure id %d, body %d, armour %d, damage: %d",
	      psStructure->id, psStructure->body, objArmour(psStructure, weaponClass), damage);

	relativeDamage = objDamage(psStructure, nullptr, damage, psStructure->structureBody(), weaponClass, weaponSubClass, isDamagePerSecond, minDamage, empRadiusHit);

	// If the shell did sufficient damage to destroy the structure
	if (relativeDamage < 0)
	{
		debug(LOG_ATTACK, "Structure (id %d) DESTROYED", psStructure->id);
		destroyStruct(psStructure, impactTime);
	}
	else
	{
		// Survived
		CHECK_STRUCTURE(psStructure);
	}

	return relativeDamage;
}

int32_t getStructureDamage(const STRUCTURE *psStructure)
{
	CHECK_STRUCTURE(psStructure);

	unsigned maxBody = structureBodyBuilt(psStructure);

	int64_t health = (int64_t)65536 * psStructure->body / MAX(1, maxBody);
	CLIP(health, 0, 65536);

	return 65536 - health;
}

uint32_t structureBuildPointsToCompletion(const STRUCTURE & structure)
{
	if (structureHasModules(&structure)) {
		auto moduleStat = getModuleStat(&structure);

		if (moduleStat != nullptr) {
			return moduleStat->buildPoints;
		}
	}

	return structure.pStructureType->buildPoints;
}

float structureCompletionProgress(const STRUCTURE & structure)
{
	return MIN(1, structure.currentBuildPts / (float)structureBuildPointsToCompletion(structure));
}

/// Add buildPoints to the structures currentBuildPts, due to construction work by the droid
/// Also can deconstruct (demolish) a building if passed negative buildpoints
void structureBuild(STRUCTURE *psStruct, DROID *psDroid, int buildPoints, int buildRate)
{
	bool checkResearchButton = psStruct->status == SS_BUILT;  // We probably just started demolishing, if this is true.
	int prevResearchState = 0;
	if (checkResearchButton)
	{
		prevResearchState = intGetResearchState();
	}

	if (psDroid && !aiCheckAlliances(psStruct->player, psDroid->player))
	{
		// Enemy structure
		return;
	}
	else if (psStruct->pStructureType->type != REF_FACTORY_MODULE)
	{
		for (unsigned player = 0; player < MAX_PLAYERS; player++)
		{
			for (const DROID *psCurr : apsDroidLists[player])
			{
				// An enemy droid is blocking it
				if ((STRUCTURE *) orderStateObj(psCurr, DORDER_BUILD) == psStruct
				    && !aiCheckAlliances(psStruct->player, psCurr->player))
				{
					return;
				}
			}
		}
	}
	psStruct->buildRate += buildRate;  // buildRate = buildPoints/GAME_UPDATES_PER_SEC, but might be rounded up or down each tick, so can't use buildPoints to get a stable number.
	if (psStruct->currentBuildPts <= 0 && buildPoints > 0)
	{
		// Just starting to build structure, need power for it.
		bool haveEnoughPower = requestPowerFor(psStruct, structPowerToBuildOrAddNextModule(psStruct));
		if (!haveEnoughPower)
		{
			buildPoints = 0;  // No power to build.
		}
	}

	int newBuildPoints = psStruct->currentBuildPts + buildPoints;
	ASSERT(newBuildPoints <= 1 + 3 * (int)structureBuildPointsToCompletion(*psStruct), "unsigned int underflow?");
	CLIP(newBuildPoints, 0, structureBuildPointsToCompletion(*psStruct));

	if (psStruct->currentBuildPts > 0 && newBuildPoints <= 0)
	{
		// Demolished structure, return some power.
		addPower(psStruct->player, structureTotalReturn(psStruct));
	}

	int deltaBody = quantiseFraction(9 * psStruct->structureBody(), 10 * structureBuildPointsToCompletion(*psStruct), newBuildPoints, psStruct->currentBuildPts);
	psStruct->currentBuildPts = newBuildPoints;
	psStruct->body = std::max<int>(psStruct->body + deltaBody, 1);

	//check if structure is built
	if (buildPoints > 0 && psStruct->currentBuildPts >= structureBuildPointsToCompletion(*psStruct))
	{
		buildingComplete(psStruct);

		//only play the sound if selected player
		if (psDroid &&
		    psStruct->player == selectedPlayer
		    && (psDroid->order.type != DORDER_LINEBUILD
		        || map_coord(psDroid->order.pos) == map_coord(psDroid->order.pos2)))
		{
			audio_QueueTrackPos(ID_SOUND_STRUCTURE_COMPLETED,
			                    psStruct->pos.x, psStruct->pos.y, psStruct->pos.z);
			intRefreshScreen();		// update any open interface bars.
		}

		/* must reset here before the callback, droid must have DACTION_NONE
		     in order to be able to start a new built task, doubled in actionUpdateDroid() */
		if (psDroid)
		{
			// Clear all orders for helping hands. Needed for AI script which runs next frame.
			for (DROID* psIter : apsDroidLists[psDroid->player])
			{
				if ((psIter->order.type == DORDER_BUILD || psIter->order.type == DORDER_HELPBUILD || psIter->order.type == DORDER_LINEBUILD)
				    && psIter->order.psObj == psStruct
				    && (psIter->order.type != DORDER_LINEBUILD || map_coord(psIter->order.pos) == map_coord(psIter->order.pos2)))
				{
					objTrace(psIter->id, "Construction order %s complete (%d, %d -> %d, %d)", getDroidOrderName(psDroid->order.type),
					         psIter->order.pos2.x, psIter->order.pos.y, psIter->order.pos2.x, psIter->order.pos2.y);
					psIter->action = DACTION_NONE;
					psIter->order = DroidOrder(DORDER_NONE);
					setDroidActionTarget(psIter, nullptr, 0);
				}
			}

			audio_StopObjTrack(psDroid, ID_SOUND_CONSTRUCTION_LOOP);
		}
		triggerEventStructBuilt(psStruct, psDroid);
		checkPlayerBuiltHQ(psStruct);
	}
	else
	{
		STRUCT_STATES prevStatus = psStruct->status;
		psStruct->status = SS_BEING_BUILT;
		if (prevStatus == SS_BUILT)
		{
			// Starting to demolish.
			triggerEventStructDemolish(psStruct, psDroid);
			if (psStruct->player == selectedPlayer)
			{
				intRefreshScreen();
			}

			switch (psStruct->pStructureType->type)
			{
			case REF_FACTORY:
			case REF_CYBORG_FACTORY:
			case REF_VTOL_FACTORY:
			{
				if (psStruct->pFunctionality)
				{
					FACTORY *psFactory = &psStruct->pFunctionality->factory;
					if (psFactory->psCommander)
					{
						//remove the commander from the factory
						syncDebugDroid(psFactory->psCommander, '-');
						assignFactoryCommandDroid(psStruct, nullptr);
					}
				}
				break;
			}
			case REF_POWER_GEN:
				releasePowerGen(psStruct);
				break;
			case REF_RESOURCE_EXTRACTOR:
				releaseResExtractor(psStruct);
				break;
			case REF_REPAIR_FACILITY:
			{
				if (psStruct->pFunctionality)
				{
					REPAIR_FACILITY	*psRepairFac = &psStruct->pFunctionality->repairFacility;
					if (psRepairFac->psObj)
					{
						psRepairFac->psObj = nullptr;
						psRepairFac->state = RepairState::Idle;
					}
				}
				break;
			}
			case REF_REARM_PAD:
			{
				if (psStruct->pFunctionality)
				{
					REARM_PAD *psReArmPad = &psStruct->pFunctionality->rearmPad;
					if (psReArmPad->psObj)
					{
						// Possible TODO: Need to do anything with the droid? (order it to find a new place to rearm?)
						psReArmPad->psObj = nullptr;
					}
				}
				break;
			}
			default:
				break;
			}
		}
	}
	if (buildPoints < 0 && psStruct->currentBuildPts == 0)
	{
		triggerEvent(TRIGGER_OBJECT_RECYCLED, psStruct);
		removeStruct(psStruct, true);
	}

	if (checkResearchButton)
	{
		intNotifyResearchButton(prevResearchState);
	}
}

static bool structureHasModules(const STRUCTURE *psStruct)
{
	return psStruct->capacity != 0;
}

// Power returned on demolish, which is half the power taken to build the structure and any modules
static int structureTotalReturn(const STRUCTURE *psStruct)
{
	int power = psStruct->pStructureType->powerToBuild;

	const STRUCTURE_STATS* const moduleStats = getModuleStat(psStruct);

	if (nullptr != moduleStats)
	{
		power += psStruct->capacity * moduleStats->powerToBuild;
	}

	return power / 2;
}

void structureDemolish(STRUCTURE *psStruct, DROID *psDroid, int buildPoints)
{
	structureBuild(psStruct, psDroid, -buildPoints);
}

void structureRepair(STRUCTURE *psStruct, DROID *psDroid, int buildRate)
{
	int repairAmount = gameTimeAdjustedAverage(buildRate * psStruct->structureBody(), psStruct->pStructureType->buildPoints);
	/*	(droid construction power * current max hitpoints [incl. upgrades])
			/ construction power that was necessary to build structure in the first place

	=> to repair a building from 1HP to full health takes as much time as building it.
	=> if buildPoints = 1 and structureBody < buildPoints, repairAmount might get truncated to zero.
		This happens with expensive, but weak buildings like mortar pits. In this case, do nothing
		and notify the caller (read: droid) of your idleness by returning false.
	*/
	psStruct->body = clip<UDWORD>(psStruct->body + repairAmount, 0, psStruct->structureBody());
}

static void refundFactoryBuildPower(STRUCTURE *psBuilding)
{
	ASSERT_OR_RETURN(, psBuilding && psBuilding->isFactory(), "structure not a factory");
	FACTORY *psFactory = &psBuilding->pFunctionality->factory;

	if (psFactory->psSubject)
	{
		if (psFactory->buildPointsRemaining < (int)calcTemplateBuild(psFactory->psSubject))
		{
			// We started building, so give the power back that was used.
			addPower(psBuilding->player, calcTemplatePower(psFactory->psSubject));
		}

	}
}

/* Set the type of droid for a factory to build */
bool structSetManufacture(STRUCTURE *psStruct, DROID_TEMPLATE *psTempl, QUEUE_MODE mode)
{
	CHECK_STRUCTURE(psStruct);

	ASSERT_OR_RETURN(false, psStruct->isFactory(), "structure not a factory");

	/* psTempl might be NULL if the build is being cancelled in the middle */
	ASSERT_OR_RETURN(false, !psTempl
	                 || (validTemplateForFactory(psTempl, psStruct, true) && researchedTemplate(psTempl, psStruct->player, true, true))
	                 || psStruct->player == scavengerPlayer() || !bMultiPlayer,
	                 "Wrong template for player %d factory, type %d.", psStruct->player, psStruct->pStructureType->type);

	FACTORY *psFact = &psStruct->pFunctionality->factory;

	if (mode == ModeQueue)
	{
		sendStructureInfo(psStruct, STRUCTUREINFO_MANUFACTURE, psTempl);
		setStatusPendingStart(*psFact, psTempl);
		return true;  // Wait for our message before doing anything.
	}

	//assign it to the Factory
	refundFactoryBuildPower(psStruct);
	psFact->psSubject = psTempl;

	//set up the start time and build time
	if (psTempl != nullptr)
	{
		//only use this for non selectedPlayer
		if (psStruct->player != selectedPlayer)
		{
			//set quantity to produce
			psFact->productionLoops = 1;
		}

		psFact->timeStarted = ACTION_START_TIME;//gameTime;
		psFact->timeStartHold = 0;

		psFact->buildPointsRemaining = calcTemplateBuild(psTempl);
		//check for zero build time - usually caused by 'silly' data! If so, set to 1 build point - ie very fast!
		psFact->buildPointsRemaining = std::max(psFact->buildPointsRemaining, 1);
	}
	return true;
}


/*****************************************************************************/
/*
* All this wall type code is horrible, but I really can't think of a better way to do it.
*        John.
*/

enum WallOrientation
{
	WallConnectNone = 0,
	WallConnectLeft = 1,
	WallConnectRight = 2,
	WallConnectUp = 4,
	WallConnectDown = 8,
};

// Orientations are:
//
//  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
//                  |   |   |   |                   |   |   |   |
//  *  -*   *- -*-  *  -*   *- -*-  *  -*   *- -*-  *  -*   *- -*-
//                                  |   |   |   |   |   |   |   |

// IMDs are:
//
//  0   1   2   3
//      |   |   |
// -*- -*- -*- -*
//      |

// Orientations are:                   IMDs are:
// 0 1 2 3 4 5 6 7 8 9 A B C D E F     0 1 2 3
//   ╴ ╶ ─ ╵ ┘ └ ┴ ╷ ┐ ┌ ┬ │ ┤ ├ ┼     ─ ┼ ┴ ┘

static uint16_t wallDir(WallOrientation orient)
{
	const uint16_t d0 = DEG(0), d1 = DEG(90), d2 = DEG(180), d3 = DEG(270);  // d1 = rotate ccw, d3 = rotate cw
	//                    0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
	uint16_t dirs[16] = {d0, d0, d2, d0, d3, d0, d3, d0, d1, d1, d2, d2, d3, d1, d3, d0};
	return dirs[orient];
}

static uint16_t wallType(WallOrientation orient)
{
	//               0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	int types[16] = {0, 0, 0, 0, 0, 3, 3, 2, 0, 3, 3, 2, 0, 2, 2, 1};
	return types[orient];
}

// look at where other walls are to decide what type of wall to build
static WallOrientation structWallScan(bool aWallPresent[5][5], int x, int y)
{
	WallOrientation left  = aWallPresent[x - 1][y] ? WallConnectLeft  : WallConnectNone;
	WallOrientation right = aWallPresent[x + 1][y] ? WallConnectRight : WallConnectNone;
	WallOrientation up    = aWallPresent[x][y - 1] ? WallConnectUp    : WallConnectNone;
	WallOrientation down  = aWallPresent[x][y + 1] ? WallConnectDown  : WallConnectNone;
	return WallOrientation(left | right | up | down);
}

static bool isWallCombiningStructureType(STRUCTURE_STATS const *pStructureType)
{
	STRUCTURE_TYPE type = pStructureType->type;
	return type == REF_WALL ||
	       type == REF_GATE ||
	       type == REF_WALLCORNER ||
	       pStructureType->combinesWithWall;  // hardpoints and fortresses by default
}

bool isWall(STRUCTURE_TYPE type)
{
	return type == REF_WALL || type == REF_WALLCORNER;
}

bool isBuildableOnWalls(STRUCTURE_TYPE type)
{
	return type == REF_DEFENSE || type == REF_GATE;
}

static void structFindWalls(unsigned player, Vector2i map, bool aWallPresent[5][5], STRUCTURE *apsStructs[5][5])
{
	for (int y = -2; y <= 2; ++y)
		for (int x = -2; x <= 2; ++x)
		{
			STRUCTURE *psStruct = castStructure(mapTile(map.x + x, map.y + y)->psObject);
			if (psStruct != nullptr && isWallCombiningStructureType(psStruct->pStructureType) && player < MAX_PLAYERS && aiCheckAlliances(player, psStruct->player))
			{
				aWallPresent[x + 2][y + 2] = true;
				apsStructs[x + 2][y + 2] = psStruct;
			}
		}
	// add in the wall about to be built
	aWallPresent[2][2] = true;
}

static void structFindWallBlueprints(Vector2i map, bool aWallPresent[5][5])
{
	for (int y = -2; y <= 2; ++y)
		for (int x = -2; x <= 2; ++x)
		{
			STRUCTURE_STATS const *stats = getTileBlueprintStats(map.x + x, map.y + y);
			if (stats != nullptr && isWallCombiningStructureType(stats))
			{
				aWallPresent[x + 2][y + 2] = true;
			}
		}
}

static bool wallBlockingTerrainJoin(Vector2i map)
{
	MAPTILE *psTile = mapTile(map);
	return terrainType(psTile) == TER_WATER || terrainType(psTile) == TER_CLIFFFACE || psTile->psObject != nullptr;
}

static WallOrientation structWallScanTerrain(bool aWallPresent[5][5], Vector2i map)
{
	WallOrientation orientation = structWallScan(aWallPresent, 2, 2);

	if (orientation == WallConnectNone)
	{
		// If neutral, try choosing horizontal or vertical based on terrain, but don't change to corner type.
		aWallPresent[2][1] = wallBlockingTerrainJoin(map + Vector2i(0, -1));
		aWallPresent[2][3] = wallBlockingTerrainJoin(map + Vector2i(0,  1));
		aWallPresent[1][2] = wallBlockingTerrainJoin(map + Vector2i(-1,  0));
		aWallPresent[3][2] = wallBlockingTerrainJoin(map + Vector2i(1,  0));
		orientation = structWallScan(aWallPresent, 2, 2);
		if ((orientation & (WallConnectLeft | WallConnectRight)) != 0 && (orientation & (WallConnectUp | WallConnectDown)) != 0)
		{
			orientation = WallConnectNone;
		}
	}

	return orientation;
}

static WallOrientation structChooseWallTypeBlueprint(Vector2i map)
{
	bool            aWallPresent[5][5];
	STRUCTURE      *apsStructs[5][5];

	// scan around the location looking for walls
	memset(aWallPresent, 0, sizeof(aWallPresent));
	structFindWalls(selectedPlayer, map, aWallPresent, apsStructs);
	structFindWallBlueprints(map, aWallPresent);

	// finally return the type for this wall
	return structWallScanTerrain(aWallPresent, map);
}

// Choose a type of wall for a location - and update any neighbouring walls
static WallOrientation structChooseWallType(unsigned player, Vector2i map)
{
	bool		aWallPresent[5][5];
	STRUCTURE	*psStruct;
	STRUCTURE	*apsStructs[5][5];

	// scan around the location looking for walls
	memset(aWallPresent, 0, sizeof(aWallPresent));
	structFindWalls(player, map, aWallPresent, apsStructs);

	// now make sure that all the walls around this one are OK
	for (int x = 1; x <= 3; ++x)
	{
		for (int y = 1; y <= 3; ++y)
		{
			// do not look at walls diagonally from this wall
			if (((x == 2 && y != 2) ||
			     (x != 2 && y == 2)) &&
			    aWallPresent[x][y])
			{
				// figure out what type the wall currently is
				psStruct = apsStructs[x][y];
				if (psStruct->pStructureType->type != REF_WALL && psStruct->pStructureType->type != REF_GATE)
				{
					// do not need to adjust anything apart from walls
					continue;
				}

				// see what type the wall should be
				WallOrientation scanType = structWallScan(aWallPresent, x, y);

				// Got to change the wall
				if (scanType != WallConnectNone)
				{
					psStruct->pFunctionality->wall.type = wallType(scanType);
					psStruct->rot.direction = wallDir(scanType);
					psStruct->sDisplay.imd = psStruct->pStructureType->pIMD[std::min<unsigned>(psStruct->pFunctionality->wall.type, psStruct->pStructureType->pIMD.size() - 1)];
				}
			}
		}
	}

	// finally return the type for this wall
	return structWallScanTerrain(aWallPresent, map);
}


/* For now all this does is work out what height the terrain needs to be set to
An actual foundation structure may end up being placed down
The x and y passed in are the CENTRE of the structure*/
static int foundationHeight(const STRUCTURE *psStruct)
{
	StructureBounds b = getStructureBounds(psStruct);

	//check the terrain is the correct type return -1 if not

	//may also have to check that overlapping terrain can be set to the average height
	//eg water - don't want it to 'flow' into the structure if this effect is coded!

	//initialise the starting values so they get set in loop
	int foundationMin = INT32_MAX;
	int foundationMax = INT32_MIN;

	for (int breadth = 0; breadth <= b.size.y; breadth++)
	{
		for (int width = 0; width <= b.size.x; width++)
		{
			int height = map_TileHeight(b.map.x + width, b.map.y + breadth);
			foundationMin = std::min(foundationMin, height);
			foundationMax = std::max(foundationMax, height);
		}
	}
	//return the average of max/min height
	return (foundationMin + foundationMax) / 2;
}


static void buildFlatten(STRUCTURE *pStructure, int h)
{
	StructureBounds b = getStructureBounds(pStructure);

	for (int breadth = 0; breadth <= b.size.y; ++breadth)
	{
		for (int width = 0; width <= b.size.x; ++width)
		{
			setTileHeight(b.map.x + width, b.map.y + breadth, h);
			// We need to raise features on raised tiles to the new height
			if (TileHasFeature(mapTile(b.map.x + width, b.map.y + breadth)))
			{
				getTileFeature(b.map.x + width, b.map.y + breadth)->pos.z = h;
			}
		}
	}
}

static bool isPulledToTerrain(const STRUCTURE *psBuilding)
{
	STRUCTURE_TYPE type = psBuilding->pStructureType->type;
	return type == REF_DEFENSE || type == REF_GATE || type == REF_WALL || type == REF_WALLCORNER || type == REF_REARM_PAD;
}

void alignStructure(STRUCTURE *psBuilding)
{
	/* DEFENSIVE structures are pulled to the terrain */
	if (!isPulledToTerrain(psBuilding))
	{
		int mapH = foundationHeight(psBuilding);

		buildFlatten(psBuilding, mapH);
		psBuilding->pos.z = mapH;
		psBuilding->foundationDepth = psBuilding->pos.z;

		// Align surrounding structures.
		StructureBounds b = getStructureBounds(psBuilding);
		syncDebug("Flattened (%d+%d, %d+%d) to %d for %d(p%d)", b.map.x, b.size.x, b.map.y, b.size.y, mapH, psBuilding->id, psBuilding->player);
		for (int breadth = -1; breadth <= b.size.y; ++breadth)
		{
			for (int width = -1; width <= b.size.x; ++width)
			{
				STRUCTURE *neighbourStructure = castStructure(mapTile(b.map.x + width, b.map.y + breadth)->psObject);
				if (neighbourStructure != nullptr && isPulledToTerrain(neighbourStructure))
				{
					alignStructure(neighbourStructure);  // Recursive call, but will go to the else case, so will not re-recurse.
				}
			}
		}
	}
	else
	{
		// TODO: Does this need iIMDBaseShape, or is this truly visual only?
		// Sample points around the structure to find a good depth for the foundation
		iIMDBaseShape *s = psBuilding->sDisplay.imd;

		psBuilding->pos.z = TILE_MIN_HEIGHT;
		psBuilding->foundationDepth = TILE_MAX_HEIGHT;

		Vector2i dir = iSinCosR(psBuilding->rot.direction, 1);
		// Rotate s->max.{x, z} and s->min.{x, z} by angle rot.direction.
		Vector2i p1{s->max.x * dir.y - s->max.z * dir.x, s->max.x * dir.x + s->max.z * dir.y};
		Vector2i p2{s->min.x * dir.y - s->min.z * dir.x, s->min.x * dir.x + s->min.z * dir.y};

		int h1 = map_Height(psBuilding->pos.x + p1.x, psBuilding->pos.y + p2.y);
		int h2 = map_Height(psBuilding->pos.x + p1.x, psBuilding->pos.y + p1.y);
		int h3 = map_Height(psBuilding->pos.x + p2.x, psBuilding->pos.y + p1.y);
		int h4 = map_Height(psBuilding->pos.x + p2.x, psBuilding->pos.y + p2.y);
		int minH = std::min({h1, h2, h3, h4});
		int maxH = std::max({h1, h2, h3, h4});
		psBuilding->pos.z = std::max(psBuilding->pos.z, maxH);
		psBuilding->foundationDepth = std::min<float>(psBuilding->foundationDepth, minH);
		syncDebug("minH=%d,maxH=%d,pointHeight=%d", minH, maxH, psBuilding->pos.z);  // s->max is based on floats! If this causes desynchs, need to fix!
	}
}

/*Builds an instance of a Structure - the x/y passed in are in world coords. */
STRUCTURE *buildStructure(STRUCTURE_STATS *pStructureType, UDWORD x, UDWORD y, UDWORD player, bool FromSave)
{
	return buildStructureDir(pStructureType, x, y, 0, player, FromSave, generateSynchronisedObjectId());
}

STRUCTURE *buildStructureDir(STRUCTURE_STATS *pStructureType, UDWORD x, UDWORD y, uint16_t direction, UDWORD player, bool FromSave)
{
	return buildStructureDir(pStructureType, x, y, direction, player, FromSave, generateSynchronisedObjectId());
}

STRUCTURE *buildStructureDir(STRUCTURE_STATS *pStructureType, UDWORD x, UDWORD y, uint16_t direction, UDWORD player, bool FromSave, uint32_t id)
{
	STRUCTURE *psBuilding = nullptr;
	const Vector2i size = pStructureType->size(direction);

	ASSERT_OR_RETURN(nullptr, player < MAX_PLAYERS, "Cannot build structure for player %" PRIu32 " (>= MAX_PLAYERS)", player);
	ASSERT_OR_RETURN(nullptr, pStructureType && pStructureType->type != REF_DEMOLISH, "You cannot build demolition!");

	if (IsStatExpansionModule(pStructureType) == false)
	{
		SDWORD preScrollMinX = 0, preScrollMinY = 0, preScrollMaxX = 0, preScrollMaxY = 0;
		UDWORD	max = pStructureType - asStructureStats;
		int	i;

		ASSERT_OR_RETURN(nullptr, max <= numStructureStats, "Invalid structure type");

		// Don't allow more than interface limits
		if (asStructureStats[max].curCount[player] + 1 > asStructureStats[max].upgrade[player].limit)
		{
			debug(LOG_ERROR, "Player %u: Building %s could not be built due to building limits (has %u, max %u)!",
			      player, getStatsName(pStructureType), asStructureStats[max].curCount[player],
			      asStructureStats[max].upgrade[player].limit);
			return nullptr;
		}

		// snap the coords to a tile
		x = (x & ~TILE_MASK) + size.x % 2 * TILE_UNITS / 2;
		y = (y & ~TILE_MASK) + size.y % 2 * TILE_UNITS / 2;

		//check not trying to build too near the edge
		if (map_coord(x) < TOO_NEAR_EDGE || map_coord(x) > (mapWidth - TOO_NEAR_EDGE))
		{
			debug(LOG_WARNING, "attempting to build too closely to map-edge, "
			      "x coord (%u) too near edge (req. distance is %u)", x, TOO_NEAR_EDGE);
			return nullptr;
		}
		if (map_coord(y) < TOO_NEAR_EDGE || map_coord(y) > (mapHeight - TOO_NEAR_EDGE))
		{
			debug(LOG_WARNING, "attempting to build too closely to map-edge, "
			      "y coord (%u) too near edge (req. distance is %u)", y, TOO_NEAR_EDGE);
			return nullptr;
		}

		WallOrientation wallOrientation = WallConnectNone;
		if (!FromSave && isWallCombiningStructureType(pStructureType))
		{
			for (int dy = 0; dy < size.y; ++dy)
				for (int dx = 0; dx < size.x; ++dx)
				{
					Vector2i pos = map_coord(Vector2i(x, y) - size * TILE_UNITS / 2) + Vector2i(dx, dy);
					wallOrientation = structChooseWallType(player, pos);  // This makes neighbouring walls match us, even if we're a hardpoint, not a wall.
				}
		}

		// initialize the structure object
		STRUCTURE building(id, player);

		//fill in other details
		building.pStructureType = pStructureType;

		building.pos.x = x;
		building.pos.y = y;
		building.rot.direction = snapDirection(direction);
		building.rot.pitch = 0;
		building.rot.roll = 0;

		//This needs to be done before the functionality bit...
		//load into the map data and structure list if not an upgrade
		Vector2i map = map_coord(Vector2i(x, y)) - size / 2;

		//set up the imd to use for the display
		building.sDisplay.imd = pStructureType->pIMD[0];

		building.state = SAS_NORMAL;
		building.lastStateTime = gameTime;

		/* if resource extractor - need to remove oil feature first, but do not do any
		 * consistency checking here - save games do not have any feature to remove
		 * to remove when placing oil derricks! */
		if (pStructureType->type == REF_RESOURCE_EXTRACTOR)
		{
			FEATURE *psFeature = getTileFeature(map_coord(x), map_coord(y));

			if (psFeature && psFeature->psStats->subType == FEAT_OIL_RESOURCE)
			{
				if (fireOnLocation(psFeature->pos.x, psFeature->pos.y))
				{
					// Can't build on burning oil resource
					return nullptr;
				}
				// remove it from the map
				turnOffMultiMsg(true); // don't send this one!
				removeFeature(psFeature);
				turnOffMultiMsg(false);
			}
		}

		for (int tileY = map.y; tileY < map.y + size.y; ++tileY)
		{
			for (int tileX = map.x; tileX < map.x + size.x; ++tileX)
			{
				MAPTILE *psTile = mapTile(tileX, tileY);

				/* Remove any walls underneath the building. You can build defense buildings on top
				 * of walls, you see. This is not the place to test whether we own it! */
				if (isBuildableOnWalls(pStructureType->type) && TileHasWall(psTile))
				{
					removeStruct((STRUCTURE *)psTile->psObject, true);
				}
				else if (TileHasStructure(psTile) && !wzapi::scriptIsObjectQueuedForRemoval(psTile->psObject))
				{
#if defined(WZ_CC_GNU) && !defined(WZ_CC_INTEL) && !defined(WZ_CC_CLANG) && (7 <= __GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
					debug(LOG_ERROR, "Player %u (%s): is building %s at (%d, %d) but found %s already at (%d, %d)",
					      player, isHumanPlayer(player) ? "Human" : "AI", getStatsName(pStructureType), map.x, map.y,
					      getStatsName(getTileStructure(tileX, tileY)->pStructureType), tileX, tileY);
#if defined(WZ_CC_GNU) && !defined(WZ_CC_INTEL) && !defined(WZ_CC_CLANG) && (7 <= __GNUC__)
# pragma GCC diagnostic pop
#endif
					return nullptr;
				}
			}
		}
		// Emplace the structure being built in the global storage to obtain stable address.
		STRUCTURE& stableBuilding = GlobalStructContainer().emplace(std::move(building));
		psBuilding = &stableBuilding;
		for (int tileY = map.y; tileY < map.y + size.y; ++tileY)
		{
			for (int tileX = map.x; tileX < map.x + size.x; ++tileX)
			{
				// We now know the previous loop didn't return early, so it is safe to save references to `stableBuilding` now.
				MAPTILE *psTile = mapTile(tileX, tileY);
				psTile->psObject = psBuilding;

				// if it's a tall structure then flag it in the map.
				if (psBuilding->sDisplay.imd && psBuilding->sDisplay.imd->max.y > TALLOBJECT_YMAX)
				{
					auxSetBlocking(tileX, tileY, AIR_BLOCKED);
				}
			}
		}

		switch (pStructureType->type)
		{
		case REF_REARM_PAD:
			break;  // Not blocking.
		default:
			auxStructureBlocking(psBuilding);
			break;
		}

		//set up the rest of the data
		for (i = 0; i < MAX_WEAPONS; i++)
		{
			psBuilding->asWeaps[i].rot.direction = 0;
			psBuilding->asWeaps[i].rot.pitch = 0;
			psBuilding->asWeaps[i].rot.roll = 0;
			psBuilding->asWeaps[i].prevRot = psBuilding->asWeaps[i].rot;
			psBuilding->asWeaps[i].origin = ORIGIN_UNKNOWN;
			psBuilding->psTarget[i] = nullptr;
		}

		psBuilding->periodicalDamageStart = 0;
		psBuilding->periodicalDamage = 0;

		psBuilding->status = SS_BEING_BUILT;
		psBuilding->currentBuildPts = 0;

		alignStructure(psBuilding);

		/* Store the weapons */
		psBuilding->numWeaps = 0;
		if (pStructureType->numWeaps > 0)
		{
			UDWORD weapon;

			for (weapon = 0; weapon < pStructureType->numWeaps; weapon++)
			{
				if (pStructureType->psWeapStat[weapon])
				{
					psBuilding->asWeaps[weapon].lastFired = 0;
					psBuilding->asWeaps[weapon].shotsFired = 0;
					//in multiPlayer make the Las-Sats require re-loading from the start
					if (bMultiPlayer && pStructureType->psWeapStat[0]->weaponSubClass == WSC_LAS_SAT)
					{
						psBuilding->asWeaps[0].lastFired = gameTime;
					}
					psBuilding->asWeaps[weapon].nStat =	pStructureType->psWeapStat[weapon] - asWeaponStats.data();
					psBuilding->asWeaps[weapon].ammo = psBuilding->getWeaponStats(weapon)->upgrade[psBuilding->player].numRounds;
					psBuilding->numWeaps++;
				}
			}
		}
		else
		{
			if (pStructureType->psWeapStat[0])
			{
				psBuilding->asWeaps[0].lastFired = 0;
				psBuilding->asWeaps[0].shotsFired = 0;
				//in multiPlayer make the Las-Sats require re-loading from the start
				if (bMultiPlayer && pStructureType->psWeapStat[0]->weaponSubClass == WSC_LAS_SAT)
				{
					psBuilding->asWeaps[0].lastFired = gameTime;
				}
				psBuilding->asWeaps[0].nStat =	pStructureType->psWeapStat[0] - asWeaponStats.data();
				psBuilding->asWeaps[0].ammo = psBuilding->getWeaponStats(0)->upgrade[psBuilding->player].numRounds;
			}
		}

		psBuilding->resistance = (UWORD)structureResistance(pStructureType, (UBYTE)player);
		psBuilding->lastResistance = ACTION_START_TIME;

		// Do the visibility stuff before setFunctionality - so placement of DP's can work
		memset(psBuilding->seenThisTick, 0, sizeof(psBuilding->seenThisTick));

		// Structure is visible to anyone with shared vision.
		for (unsigned vPlayer = 0; vPlayer < MAX_PLAYERS; ++vPlayer)
		{
			psBuilding->visible[vPlayer] = hasSharedVision(vPlayer, player) ? UINT8_MAX : 0;
		}

		// Reveal any tiles that can be seen by the structure
		visTilesUpdate(psBuilding);

		/*if we're coming from a SAVEGAME and we're on an Expand_Limbo mission,
		any factories that were built previously for the selectedPlayer will
		have DP's in an invalid location - the scroll limits will have been
		changed to not include them. This is the only HACK I can think of to
		enable them to be loaded up. So here goes...*/
		if (FromSave && player == selectedPlayer && missionLimboExpand())
		{
			//save the current values
			preScrollMinX = scrollMinX;
			preScrollMinY = scrollMinY;
			preScrollMaxX = scrollMaxX;
			preScrollMaxY = scrollMaxY;
			//set the current values to mapWidth/mapHeight
			scrollMinX = 0;
			scrollMinY = 0;
			scrollMaxX = mapWidth;
			scrollMaxY = mapHeight;
			// NOTE: resizeRadar() may be required here, since we change scroll limits?
		}
		//set the functionality dependent on the type of structure
		if (!setFunctionality(psBuilding, pStructureType->type))
		{
			removeStructFromMap(psBuilding);
			objmemDestroy(psBuilding, false);
			//better reset these if you couldn't build the structure!
			if (FromSave && player == selectedPlayer && missionLimboExpand())
			{
				//reset the current values
				scrollMinX = preScrollMinX;
				scrollMinY = preScrollMinY;
				scrollMaxX = preScrollMaxX;
				scrollMaxY = preScrollMaxY;
				// NOTE: resizeRadar() may be required here, since we change scroll limits?
			}
			return nullptr;
		}

		//reset the scroll values if adjusted
		if (FromSave && player == selectedPlayer && missionLimboExpand())
		{
			//reset the current values
			scrollMinX = preScrollMinX;
			scrollMinY = preScrollMinY;
			scrollMaxX = preScrollMaxX;
			scrollMaxY = preScrollMaxY;
			// NOTE: resizeRadar() may be required here, since we change scroll limits?
		}

		// rotate a wall if necessary
		if (!FromSave && (pStructureType->type == REF_WALL || pStructureType->type == REF_GATE))
		{
			psBuilding->pFunctionality->wall.type = wallType(wallOrientation);
			if (wallOrientation != WallConnectNone)
			{
				psBuilding->rot.direction = wallDir(wallOrientation);
				psBuilding->sDisplay.imd = psBuilding->pStructureType->pIMD[std::min<unsigned>(psBuilding->pFunctionality->wall.type, psBuilding->pStructureType->pIMD.size() - 1)];
			}
		}

		psBuilding->body = (UWORD)psBuilding->structureBody();
		psBuilding->expectedDamage = 0;  // Begin life optimistically.

		//add the structure to the list - this enables it to be drawn whilst being built
		addStructure(psBuilding);

		asStructureStats[max].curCount[player]++;

		if (isLasSat(psBuilding->pStructureType))
		{
			psBuilding->asWeaps[0].ammo = 1; // ready to trigger the fire button
		}

		// Move any delivery points under the new structure.
		StructureBounds bounds = getStructureBounds(psBuilding);
		for (unsigned playerNum = 0; playerNum < MAX_PLAYERS; ++playerNum)
		{
			for (const STRUCTURE *psStruct : apsStructLists[playerNum])
			{
				if (!psStruct)
				{
					continue;
				}
				FLAG_POSITION *fp = nullptr;
				if (psStruct->isFactory())
				{
					fp = psStruct->pFunctionality->factory.psAssemblyPoint;
				}
				else if (psStruct->pStructureType->type == REF_REPAIR_FACILITY)
				{
					fp = psStruct->pFunctionality->repairFacility.psDeliveryPoint;
				}
				if (fp != nullptr)
				{
					Vector2i pos = map_coord(fp->coords.xy());
					if (unsigned(pos.x - bounds.map.x) < unsigned(bounds.size.x) && unsigned(pos.y - bounds.map.y) < unsigned(bounds.size.y))
					{
						// Delivery point fp is under the new structure. Need to move it.
						setAssemblyPoint(fp, fp->coords.x, fp->coords.y, playerNum, true);
					}
				}
			}
		}

		if (!FromSave)
		{
			displayConstructionCloud(psBuilding->pos);
		}
	}
	else //its an upgrade
	{
		bool		bUpgraded = false;
		int32_t         bodyDiff = 0;

		//don't create the Structure use existing one
		psBuilding = getTileStructure(map_coord(x), map_coord(y));

		if (!psBuilding)
		{
			return nullptr;
		}

		ASSERT(psBuilding->player == player, "Trying to upgrade player %u building with player %u module?", static_cast<unsigned>(psBuilding->player), player);

		int prevResearchState = intGetResearchState();

		if (pStructureType->type == REF_FACTORY_MODULE)
		{
			if (psBuilding->pStructureType->type != REF_FACTORY &&
			    psBuilding->pStructureType->type != REF_VTOL_FACTORY)
			{
				debug(LOG_INFO, "Factory module %" PRIu32 " trying to upgrade structure of type %u - ignoring", id, static_cast<unsigned>(psBuilding->pStructureType->type));
				return nullptr;
			}
			//increment the capacity and output for the owning structure
			if (psBuilding->capacity < SIZE_SUPER_HEAVY)
			{
				//store the % difference in body points before upgrading
				bodyDiff = 65536 - getStructureDamage(psBuilding);

				psBuilding->capacity++;
				bUpgraded = true;
				//put any production on hold
				holdProduction(psBuilding, ModeImmediate);
			}
		}

		if (pStructureType->type == REF_RESEARCH_MODULE)
		{
			if (psBuilding->pStructureType->type != REF_RESEARCH)
			{
				debug(LOG_INFO, "Research module %" PRIu32 " trying to upgrade structure of type %u - ignoring", id, static_cast<unsigned>(psBuilding->pStructureType->type));
				return nullptr;
			}
			//increment the capacity and research points for the owning structure
			if (psBuilding->capacity == 0)
			{
				//store the % difference in body points before upgrading
				bodyDiff = 65536 - getStructureDamage(psBuilding);

				psBuilding->capacity++;
				bUpgraded = true;
				//cancel any research - put on hold now
				if (psBuilding->pFunctionality->researchFacility.psSubject)
				{
					//cancel the topic
					holdResearch(psBuilding, ModeImmediate);
				}
			}
		}

		if (pStructureType->type == REF_POWER_MODULE)
		{
			if (psBuilding->pStructureType->type != REF_POWER_GEN)
			{
				debug(LOG_INFO, "Power module %" PRIu32 " trying to upgrade structure of type %u - ignoring", id, static_cast<unsigned>(psBuilding->pStructureType->type));
				return nullptr;
			}
			//increment the capacity and research points for the owning structure
			if (psBuilding->capacity == 0)
			{
				//store the % difference in body points before upgrading
				bodyDiff = 65536 - getStructureDamage(psBuilding);

				//increment the power output, multiplier and capacity
				psBuilding->capacity++;
				bUpgraded = true;

				//need to inform any res Extr associated that not digging until complete
				releasePowerGen(psBuilding);
			}
		}

		if (bUpgraded)
		{
			std::vector<iIMDBaseShape *> &IMDs = psBuilding->pStructureType->pIMD;
			int imdIndex = std::min<int>(psBuilding->capacity * 2, IMDs.size() - 1) - 1;  // *2-1 because even-numbered IMDs are structures, odd-numbered IMDs are just the modules, and we want just the module since we cache the fully-built part of the building in psBuilding->prebuiltImd.
			if (imdIndex < 0)
			{
				// Looks like we don't have a model for this structure's upgrade
				// Log it and default to the base model (to avoid a crash)
				debug(LOG_ERROR, "No upgraded structure model to draw.");
				imdIndex = 0;
			}
			psBuilding->prebuiltImd = psBuilding->sDisplay.imd;
			psBuilding->sDisplay.imd = IMDs[imdIndex];

			//calculate the new body points of the owning structure
			psBuilding->body = (uint64_t)psBuilding->structureBody() * bodyDiff / 65536;

			//initialise the build points
			psBuilding->currentBuildPts = 0;
			//start building again
			psBuilding->status = SS_BEING_BUILT;
			psBuilding->buildRate = 1;  // Don't abandon the structure first tick, so set to nonzero.

			if (!FromSave)
			{
				triggerEventStructureUpgradeStarted(psBuilding);

				if (psBuilding->player == selectedPlayer)
				{
					intRefreshScreen();
				}
			}
		}
		else
		{
			debug(LOG_INFO, "Did not use upgrade %" PRIu32 " trying to upgrade structure of type %u (either reached capacity or not compatible)", id, static_cast<unsigned>(psBuilding->pStructureType->type));
		}
		intNotifyResearchButton(prevResearchState);
	}
	if (pStructureType->type != REF_WALL && pStructureType->type != REF_WALLCORNER)
	{
		if (player == selectedPlayer)
		{
			scoreUpdateVar(WD_STR_BUILT);
		}
	}

	/* why is this necessary - it makes tiles under the structure visible */
	setUnderTilesVis(psBuilding, player);

	psBuilding->prevTime = gameTime - deltaGameTime;  // Structure hasn't been updated this tick, yet.
	psBuilding->time = psBuilding->prevTime - 1;      // -1, so the times are different, even before updating.

	return psBuilding;
}

optional<STRUCTURE> buildBlueprint(STRUCTURE_STATS const *psStats, Vector3i pos, uint16_t direction, unsigned moduleIndex, STRUCT_STATES state, uint8_t ownerPlayer)
{
	ASSERT_OR_RETURN(nullopt, psStats != nullptr, "No blueprint stats");
	ASSERT_OR_RETURN(nullopt, psStats->pIMD[0] != nullptr, "No blueprint model for %s", getStatsName(psStats));
	ASSERT_OR_RETURN(nullopt, ownerPlayer < MAX_PLAYERS, "invalid ownerPlayer: %" PRIu8 "", ownerPlayer);

	Rotation rot(direction, 0, 0);

	int moduleNumber = 0;
	std::vector<iIMDBaseShape *> const *pIMD = &psStats->pIMD;
	if (IsStatExpansionModule(psStats))
	{
		STRUCTURE *baseStruct = castStructure(worldTile(pos.xy())->psObject);
		if (baseStruct != nullptr)
		{
			if (moduleIndex == 0)
			{
				moduleIndex = nextModuleToBuild(baseStruct, 0);
			}
			int baseModuleNumber = moduleIndex * 2 - 1; // *2-1 because even-numbered IMDs are structures, odd-numbered IMDs are just the modules.
			std::vector<iIMDBaseShape *> const *basepIMD = &baseStruct->pStructureType->pIMD;
			if ((unsigned)baseModuleNumber < basepIMD->size())
			{
				// Draw the module.
				moduleNumber = baseModuleNumber;
				pIMD = basepIMD;
				pos = baseStruct->pos;
				rot = baseStruct->rot;
			}
		}
	}

	STRUCTURE blueprint(0, ownerPlayer);
	blueprint.state = SAS_NORMAL;
	// construct the fake structure
	blueprint.pStructureType = const_cast<STRUCTURE_STATS *>(psStats);  // Couldn't be bothered to fix const correctness everywhere.
	if (selectedPlayer < MAX_PLAYERS)
	{
		blueprint.visible[selectedPlayer] = UBYTE_MAX;
	}
	blueprint.sDisplay.imd = (*pIMD)[std::min<int>(moduleNumber, pIMD->size() - 1)];
	blueprint.pos = pos;
	blueprint.rot = rot;
	blueprint.selected = false;

	blueprint.numWeaps = 0;
	blueprint.asWeaps[0].nStat = 0;

	// give defensive structures a weapon
	if (psStats->psWeapStat[0])
	{
		blueprint.asWeaps[0].nStat = psStats->psWeapStat[0] - asWeaponStats.data();
	}
	// things with sensors or ecm (or repair facilities) need these set, even if they have no official weapon
	blueprint.numWeaps = 0;
	blueprint.asWeaps[0].lastFired = 0;
	blueprint.asWeaps[0].rot.pitch = 0;
	blueprint.asWeaps[0].rot.direction = 0;
	blueprint.asWeaps[0].rot.roll = 0;
	blueprint.asWeaps[0].prevRot = blueprint.asWeaps[0].rot;

	blueprint.expectedDamage = 0;

	// Times must be different, but don't otherwise matter.
	blueprint.time = 23;
	blueprint.prevTime = 42;

	blueprint.status = state;

	// Rotate wall if needed.
	if (blueprint.pStructureType->type == REF_WALL || blueprint.pStructureType->type == REF_GATE)
	{
		WallOrientation scanType = structChooseWallTypeBlueprint(map_coord(blueprint.pos.xy()));
		unsigned type = wallType(scanType);
		if (scanType != WallConnectNone)
		{
			blueprint.rot.direction = wallDir(scanType);
			blueprint.sDisplay.imd = blueprint.pStructureType->pIMD[std::min<unsigned>(type, blueprint.pStructureType->pIMD.size() - 1)];
		}
	}

	return blueprint;
}

static Vector2i defaultAssemblyPointPos(STRUCTURE *psBuilding)
{
	const Vector2i size = psBuilding->size() + Vector2i(1, 1);  // Adding Vector2i(1, 1) to select the middle of the tile outside the building instead of the corner.
	Vector2i pos = psBuilding->pos;
	switch (snapDirection(psBuilding->rot.direction))
	{
	case 0x0000: return pos + TILE_UNITS/2 * Vector2i( size.x,  size.y);
	case 0x4000: return pos + TILE_UNITS/2 * Vector2i( size.x, -size.y);
	case 0x8000: return pos + TILE_UNITS/2 * Vector2i(-size.x, -size.y);
	case 0xC000: return pos + TILE_UNITS/2 * Vector2i(-size.x,  size.y);
	}
	return {};  // Unreachable.
}

static bool setFunctionality(STRUCTURE *psBuilding, STRUCTURE_TYPE functionType)
{
	ASSERT_OR_RETURN(false, psBuilding != nullptr, "Invalid pointer");
	CHECK_STRUCTURE(psBuilding);

	switch (functionType)
	{
	case REF_FACTORY:
	case REF_CYBORG_FACTORY:
	case REF_VTOL_FACTORY:
	case REF_RESEARCH:
	case REF_POWER_GEN:
	case REF_RESOURCE_EXTRACTOR:
	case REF_REPAIR_FACILITY:
	case REF_REARM_PAD:
	case REF_WALL:
	case REF_GATE:
		// Allocate space for the buildings functionality
		psBuilding->pFunctionality = (FUNCTIONALITY *)calloc(1, sizeof(*psBuilding->pFunctionality));
		ASSERT_OR_RETURN(false, psBuilding != nullptr, "Out of memory");
		break;

	default:
		psBuilding->pFunctionality = nullptr;
		break;
	}

	switch (functionType)
	{
	case REF_FACTORY:
	case REF_CYBORG_FACTORY:
	case REF_VTOL_FACTORY:
		{
			FACTORY *psFactory = &psBuilding->pFunctionality->factory;

			psFactory->psSubject = nullptr;

			// Default the secondary order - AB 22/04/99
			psFactory->secondaryOrder = DSS_ARANGE_LONG | DSS_REPLEV_NEVER | DSS_ALEV_ALWAYS | DSS_HALT_GUARD;

			// Create the assembly point for the factory
			if (!createFlagPosition(&psFactory->psAssemblyPoint, psBuilding->player))
			{
				return false;
			}

			// Set the assembly point
			Vector2i pos = defaultAssemblyPointPos(psBuilding);
			setAssemblyPoint(psFactory->psAssemblyPoint, pos.x, pos.y, psBuilding->player, true);

			// Add the flag to the list
			addFlagPosition(psFactory->psAssemblyPoint);
			switch (functionType)
			{
			case REF_FACTORY:
				setFlagPositionInc(psBuilding->pFunctionality, psBuilding->player, FACTORY_FLAG);
				break;
			case REF_CYBORG_FACTORY:
				setFlagPositionInc(psBuilding->pFunctionality, psBuilding->player, CYBORG_FLAG);
				break;
			case REF_VTOL_FACTORY:
				setFlagPositionInc(psBuilding->pFunctionality, psBuilding->player, VTOL_FLAG);
				break;
			default:
				ASSERT_OR_RETURN(false, false, "Invalid factory type");
			}
			break;
		}
	case REF_POWER_GEN:
	case REF_HQ:
	case REF_REARM_PAD:
		{
			break;
		}
	case REF_RESOURCE_EXTRACTOR:
		{
			RES_EXTRACTOR *psResExtracter = &psBuilding->pFunctionality->resourceExtractor;

			// Make the structure inactive
			psResExtracter->psPowerGen = nullptr;
			break;
		}
	case REF_REPAIR_FACILITY:
		{
			REPAIR_FACILITY *psRepairFac = &psBuilding->pFunctionality->repairFacility;

			psRepairFac->psObj = nullptr;

			// Create an assembly point for repaired droids
			if (!createFlagPosition(&psRepairFac->psDeliveryPoint, psBuilding->player))
			{
				return false;
			}

			// Set the assembly point
			Vector2i pos = defaultAssemblyPointPos(psBuilding);
			setAssemblyPoint(psRepairFac->psDeliveryPoint, pos.x, pos.y, psBuilding->player, true);

			// Add the flag (triangular marker on the ground) at the delivery point
			addFlagPosition(psRepairFac->psDeliveryPoint);
			setFlagPositionInc(psBuilding->pFunctionality, psBuilding->player, REPAIR_FLAG);
			break;
		}
	// Structure types without a FUNCTIONALITY
	default:
		break;
	}

	return true;
}

static bool transferFixupFunctionality(STRUCTURE *psBuilding, STRUCTURE_TYPE functionType, UDWORD priorPlayer)
{
	ASSERT_OR_RETURN(false, psBuilding != nullptr, "Invalid pointer");
	CHECK_STRUCTURE(psBuilding);

	switch (functionType)
	{
	case REF_FACTORY:
	case REF_CYBORG_FACTORY:
	case REF_VTOL_FACTORY:
	case REF_RESEARCH:
	case REF_POWER_GEN:
	case REF_RESOURCE_EXTRACTOR:
	case REF_REPAIR_FACILITY:
	case REF_REARM_PAD:
	case REF_WALL:
	case REF_GATE:
		// Structure should already have functionality
		ASSERT_OR_RETURN(false, psBuilding->pFunctionality, "Structure does not already have functionality pointer?");
		break;

	default:
		psBuilding->pFunctionality = nullptr;
		break;
	}

	switch (functionType)
	{
	case REF_FACTORY:
	case REF_CYBORG_FACTORY:
	case REF_VTOL_FACTORY:
		{
			FACTORY *psFactory = &psBuilding->pFunctionality->factory;

			// Reset factoryNumFlag for prior player
			auto psAssemblyPoint = psFactory->psAssemblyPoint;
			if (psAssemblyPoint != nullptr)
			{
				if (psAssemblyPoint->factoryInc < factoryNumFlag[priorPlayer][psAssemblyPoint->factoryType].size())
				{
					factoryNumFlag[priorPlayer][psAssemblyPoint->factoryType][psAssemblyPoint->factoryInc] = false;
				}

				//need to cancel the repositioning of the DP if selectedPlayer and currently moving
				if (priorPlayer == selectedPlayer && psAssemblyPoint->selected)
				{
					cancelDeliveryRepos();
				}
			}

			// Transfer / fix-up factory assembly point, and number
			transferFlagPositionToPlayer(psFactory->psAssemblyPoint, priorPlayer, psBuilding->player);

			switch (functionType)
			{
			case REF_FACTORY:
				setFlagPositionInc(psBuilding->pFunctionality, psBuilding->player, FACTORY_FLAG);
				break;
			case REF_CYBORG_FACTORY:
				setFlagPositionInc(psBuilding->pFunctionality, psBuilding->player, CYBORG_FLAG);
				break;
			case REF_VTOL_FACTORY:
				setFlagPositionInc(psBuilding->pFunctionality, psBuilding->player, VTOL_FLAG);
				break;
			default:
				ASSERT_OR_RETURN(false, false, "Invalid factory type");
			}
			break;
		}
	case REF_POWER_GEN:
	case REF_HQ:
	case REF_REARM_PAD:
		{
			break;
		}
	case REF_RESOURCE_EXTRACTOR:
		{
			RES_EXTRACTOR *psResExtracter = &psBuilding->pFunctionality->resourceExtractor;

			// Make the structure inactive
			psResExtracter->psPowerGen = nullptr;
			break;
		}
	case REF_REPAIR_FACILITY:
		{
			REPAIR_FACILITY *psRepairFac = &psBuilding->pFunctionality->repairFacility;

			// POSSIBLE TODO: Do something about the group? (Or can we just keep it?)

			// Reset factoryNumFlag for prior player
			auto psAssemblyPoint = psRepairFac->psDeliveryPoint;
			if (psAssemblyPoint != nullptr)
			{
				if (psAssemblyPoint->factoryInc < factoryNumFlag[priorPlayer][psAssemblyPoint->factoryType].size())
				{
					factoryNumFlag[priorPlayer][psAssemblyPoint->factoryType][psAssemblyPoint->factoryInc] = false;
				}

				//need to cancel the repositioning of the DP if selectedPlayer and currently moving
				if (priorPlayer == selectedPlayer && psAssemblyPoint->selected)
				{
					cancelDeliveryRepos();
				}
			}

			// Transfer / fix-up factory assembly point, and number
			transferFlagPositionToPlayer(psRepairFac->psDeliveryPoint, priorPlayer, psBuilding->player);

			setFlagPositionInc(psBuilding->pFunctionality, psBuilding->player, REPAIR_FLAG);
			break;
		}
	// Structure types without a FUNCTIONALITY
	default:
		break;
	}

	return true;
}

// Set the command droid that factory production should go to
void assignFactoryCommandDroid(STRUCTURE *psStruct, DROID *psCommander)
{
	FACTORY			*psFact;
	SDWORD			factoryInc, typeFlag;

	CHECK_STRUCTURE(psStruct);
	ASSERT_OR_RETURN(, psStruct->isFactory(), "structure not a factory");

	psFact = &psStruct->pFunctionality->factory;

	switch (psStruct->pStructureType->type)
	{
	case REF_FACTORY:
		typeFlag = FACTORY_FLAG;
		break;
	case REF_VTOL_FACTORY:
		typeFlag = VTOL_FLAG;
		break;
	case REF_CYBORG_FACTORY:
		typeFlag = CYBORG_FLAG;
		break;
	default:
		ASSERT(!"unknown factory type", "unknown factory type");
		typeFlag = FACTORY_FLAG;
		break;
	}

	// removing a commander from a factory
	if (psFact->psCommander != nullptr)
	{
		if (typeFlag == FACTORY_FLAG)
		{
			secondarySetState(psFact->psCommander, DSO_CLEAR_PRODUCTION,
			                  (SECONDARY_STATE)(1 << (psFact->psAssemblyPoint->factoryInc + DSS_ASSPROD_SHIFT)));
		}
		else if (typeFlag == CYBORG_FLAG)
		{
			secondarySetState(psFact->psCommander, DSO_CLEAR_PRODUCTION,
			                  (SECONDARY_STATE)(1 << (psFact->psAssemblyPoint->factoryInc + DSS_ASSPROD_CYBORG_SHIFT)));
		}
		else
		{
			secondarySetState(psFact->psCommander, DSO_CLEAR_PRODUCTION,
			                  (SECONDARY_STATE)(1 << (psFact->psAssemblyPoint->factoryInc + DSS_ASSPROD_VTOL_SHIFT)));
		}

		psFact->psCommander = nullptr;
		// TODO: Synchronise .psCommander.
		//syncDebug("Removed commander from factory %d", psStruct->id);
		if (!missionIsOffworld())
		{
			addFlagPosition(psFact->psAssemblyPoint);	// add the assembly point back into the list
		}
		else
		{
			addFlagPositionToList(psFact->psAssemblyPoint, mission.apsFlagPosLists);
		}
	}

	if (psCommander != nullptr)
	{
		ASSERT_OR_RETURN(, !missionIsOffworld(), "cannot assign a commander to a factory when off world");

		factoryInc = psFact->psAssemblyPoint->factoryInc;

		auto& flagPosList = apsFlagPosLists[psStruct->player];
		FlagPositionList::iterator flagPosIt = flagPosList.begin(), flagPosItNext;
		while (flagPosIt != flagPosList.end())
		{
			flagPosItNext = std::next(flagPosIt);
			FLAG_POSITION* flagPos = *flagPosIt;
			if (flagPos->factoryInc == factoryInc && flagPos->factoryType == typeFlag)
			{
				if (flagPos != psFact->psAssemblyPoint)
				{
					removeFlagPosition(flagPos);
				}
				else
				{
					// need to keep the assembly point(s) for the factory
					// but remove it(the primary) from the list so it doesn't get
					// displayed
					flagPosList.erase(flagPosIt);
				}
			}
			flagPosIt = flagPosItNext;
		}
		psFact->psCommander = psCommander;
		syncDebug("Assigned commander %d to factory %d", psCommander->id, psStruct->id);
	}
}


// remove all factories from a command droid
void clearCommandDroidFactory(DROID *psDroid)
{
	ASSERT_OR_RETURN(, selectedPlayer < MAX_PLAYERS, "invalid selectedPlayer: %" PRIu32 "", selectedPlayer);

	for (STRUCTURE* psCurr : apsStructLists[selectedPlayer])
	{
		if ((psCurr->pStructureType->type == REF_FACTORY) ||
		    (psCurr->pStructureType->type == REF_CYBORG_FACTORY) ||
		    (psCurr->pStructureType->type == REF_VTOL_FACTORY))
		{
			if (psCurr->pFunctionality->factory.psCommander == psDroid)
			{
				assignFactoryCommandDroid(psCurr, nullptr);
			}
		}
	}
	for (STRUCTURE* psCurr : mission.apsStructLists[selectedPlayer])
	{
		if ((psCurr->pStructureType->type == REF_FACTORY) ||
		    (psCurr->pStructureType->type == REF_CYBORG_FACTORY) ||
		    (psCurr->pStructureType->type == REF_VTOL_FACTORY))
		{
			if (psCurr->pFunctionality->factory.psCommander == psDroid)
			{
				assignFactoryCommandDroid(psCurr, nullptr);
			}
		}
	}
}

/* Check that a tile is vacant for a droid to be placed */
static bool structClearTile(UWORD x, UWORD y, PROPULSION_TYPE propulsion)
{
	UDWORD	player;

	/* Check for a structure */
	if (fpathBlockingTile(x, y, propulsion))
	{
		debug(LOG_NEVER, "failed - blocked");
		return false;
	}

	/* Check for a droid */
	for (player = 0; player < MAX_PLAYERS; player++)
	{
		for (const DROID* psCurr : apsDroidLists[player])
		{
			if (map_coord(psCurr->pos.x) == x
			    && map_coord(psCurr->pos.y) == y)
			{
				debug(LOG_NEVER, "failed - not vacant");
				return false;
			}
		}
	}

	debug(LOG_NEVER, "succeeded");
	return true;
}

/* An auxiliary function for std::stable_sort in placeDroid */
static bool comparePlacementPoints(Vector2i a, Vector2i b)
{
	return abs(a.x) + abs(a.y) < abs(b.x) + abs(b.y);
}

/* Find a location near to a structure to start the droid of */
bool placeDroid(STRUCTURE *psStructure, const DROID_TEMPLATE * psTempl, UDWORD *droidX, UDWORD *droidY)
{
	CHECK_STRUCTURE(psStructure);

	// Find the four corners of the square
	StructureBounds bounds = getStructureBounds(psStructure);
	int xmin = std::max(bounds.map.x - 1, 0);
	int xmax = std::min(bounds.map.x + bounds.size.x, mapWidth);
	int ymin = std::max(bounds.map.y - 1, 0);
	int ymax = std::min(bounds.map.y + bounds.size.y, mapHeight);

	// Round direction to nearest 90°.
	uint16_t direction = snapDirection(psStructure->rot.direction);

	/* We sort all adjacent tiles by their Manhattan distance to the
	target droid exit tile, misplaced by (1/3, 1/4) tiles.
	Since only whole coordinates are sorted, this makes sure sorting
	is deterministic. Target coordinates, multiplied by 12 to eliminate
	floats, are stored in (sx, sy). */
	int sx, sy;

	if (direction == 0x0)
	{
		sx = 12 * (xmin + 1) + 4;
		sy = 12 * ymax + 3;
	}
	else if (direction == 0x4000)
	{
		sx = 12 * xmax + 3;
		sy = 12 * (ymax - 1) - 4;
	}
	else if (direction == 0x8000)
	{
		sx = 12 * (xmax - 1) - 4;
		sy = 12 * ymin - 3;
	}
	else
	{
		sx = 12 * xmin - 3;
		sy = 12 * (ymin + 1) + 4;
	}

	std::vector<Vector2i> tiles;
	for (int y = ymin; y <= ymax; ++y)
	{
		for (int x = xmin; x <= xmax; ++x)
		{
			if (structClearTile(x, y, psTempl->getPropulsionStats()->propulsionType))
			{
				tiles.push_back(Vector2i(12 * x - sx, 12 * y - sy));
			}
		}
	}

	if (tiles.empty())
	{
		return false;
	}

	std::sort(tiles.begin(), tiles.end(), comparePlacementPoints);

	/* Store best tile coordinates in (sx, sy),
	which are also map coordinates of its north-west corner.
	Store world coordinates of this tile's center in (wx, wy) */
	sx = (tiles[0].x + sx) / 12;
	sy = (tiles[0].y + sy) / 12;
	int wx = world_coord(sx) + TILE_UNITS / 2;
	int wy = world_coord(sy) + TILE_UNITS / 2;

	/* Finally, find world coordinates of the structure point closest to (mx, my).
	For simplicity, round to grid vertices. */
	if (2 * sx <= xmin + xmax)
	{
		wx += TILE_UNITS / 2 - 1;
	}
	if (2 * sx >= xmin + xmax)
	{
		wx -= TILE_UNITS / 2 - 1;
	}
	if (2 * sy <= ymin + ymax)
	{
		wy += TILE_UNITS / 2 - 1;
	}
	if (2 * sy >= ymin + ymax)
	{
		wy -= TILE_UNITS / 2 - 1;
	}

	*droidX = wx;
	*droidY = wy;
	return true;
}

//Set the factory secondary orders to a droid
void setFactorySecondaryState(DROID *psDroid, STRUCTURE *psStructure)
{
	CHECK_STRUCTURE(psStructure);
	ASSERT_OR_RETURN(, psStructure->isFactory(), "structure not a factory");

	if (myResponsibility(psStructure->player))
	{
		uint32_t newState = psStructure->pFunctionality->factory.secondaryOrder;
		uint32_t diff = newState ^ psDroid->secondaryOrder;
		if ((diff & DSS_ARANGE_MASK) != 0)
		{
			secondarySetState(psDroid, DSO_ATTACK_RANGE, (SECONDARY_STATE)(newState & DSS_ARANGE_MASK));
		}
		if ((diff & DSS_REPLEV_MASK) != 0)
		{
			secondarySetState(psDroid, DSO_REPAIR_LEVEL, (SECONDARY_STATE)(newState & DSS_REPLEV_MASK));
		}
		if ((diff & DSS_ALEV_MASK) != 0)
		{
			secondarySetState(psDroid, DSO_ATTACK_LEVEL, (SECONDARY_STATE)(newState & DSS_ALEV_MASK));
		}
		if ((diff & DSS_CIRCLE_MASK) != 0)
		{
			secondarySetState(psDroid, DSO_CIRCLE, (SECONDARY_STATE)(newState & DSS_CIRCLE_MASK));
		}
		if ((diff & DSS_HALT_MASK) != 0)
		{
			secondarySetState(psDroid, DSO_HALTTYPE, (SECONDARY_STATE)(newState & DSS_HALT_MASK));
		}
	}
}

/* Place a newly manufactured droid next to a factory  and then send if off
to the assembly point, returns true if droid was placed successfully */
static bool structPlaceDroid(STRUCTURE *psStructure, DROID_TEMPLATE *psTempl, DROID **ppsDroid)
{
	UDWORD			x, y;
	bool			placed;//bTemp = false;
	DROID			*psNewDroid;
	FACTORY			*psFact;
	UBYTE			factoryType;
	bool			assignCommander;

	CHECK_STRUCTURE(psStructure);

	placed = placeDroid(psStructure, psTempl, &x, &y);

	if (placed)
	{
		INITIAL_DROID_ORDERS initialOrders = {psStructure->pFunctionality->factory.secondaryOrder, psStructure->pFunctionality->factory.psAssemblyPoint->coords.x, psStructure->pFunctionality->factory.psAssemblyPoint->coords.y, psStructure->id};
		//create a droid near to the structure
		syncDebug("Placing new droid at (%d,%d)", x, y);
		turnOffMultiMsg(true);
		psNewDroid = buildDroid(psTempl, x, y, psStructure->player, false, &initialOrders, psStructure->rot);
		turnOffMultiMsg(false);
		if (!psNewDroid)
		{
			*ppsDroid = nullptr;
			return false;
		}
		psFact = &psStructure->pFunctionality->factory;
		bool hasCommander = psFact->psCommander != nullptr && myResponsibility(psStructure->player);
		// assign a group to the manufactured droid
		if (psStructure->productToGroup != UBYTE_MAX)
		{
			psNewDroid->group = psStructure->productToGroup;
			if (!hasCommander || isConstructionDroid(psNewDroid))
			{
				intGroupsChanged(psNewDroid->group); // update groups UI
				SelectGroupDroid(psNewDroid);
			}
		}
		setFactorySecondaryState(psNewDroid, psStructure);
		displayConstructionCloud(psNewDroid->pos);
		/* add the droid to the list */
		addDroid(psNewDroid, apsDroidLists);
		*ppsDroid = psNewDroid;
		if (psNewDroid->player == selectedPlayer)
		{
			audio_QueueTrack(ID_SOUND_DROID_COMPLETED);
			intRefreshScreen();	// update any interface implications.
		}

		// update the droid counts
		adjustDroidCount(psNewDroid, 1);

		// if we've built a command droid - make sure that it isn't assigned to another commander
		assignCommander = false;
		if ((psNewDroid->droidType == DROID_COMMAND) &&
		    (psFact->psCommander != nullptr))
		{
			assignFactoryCommandDroid(psStructure, nullptr);
			assignCommander = true;
		}

		if (psNewDroid->isVtol() && !psNewDroid->isTransporter())
		{
			moveToRearm(psNewDroid);
		}
		if (hasCommander)
		{
			// TODO: Should synchronise .psCommander in all cases.
			//syncDebug("Has commander.");
			if (psNewDroid->isTransporter())
			{
				// Transporters can't be assigned to commanders, due to abuse of .psGroup. Try to land on the commander instead. Hopefully the transport is heavy enough to crush the commander.
				orderDroidLoc(psNewDroid, DORDER_MOVE, psFact->psCommander->pos.x, psFact->psCommander->pos.y, ModeQueue);
			}
			else if (psNewDroid->isVtol())
			{
				orderDroidObj(psNewDroid, DORDER_FIRESUPPORT, psFact->psCommander, ModeQueue);
				//moveToRearm(psNewDroid);
			}
			else if (!psNewDroid->isConstructionDroid())
			{
				orderDroidObj(psNewDroid, DORDER_COMMANDERSUPPORT, psFact->psCommander, ModeQueue);
			}
		}
		else
		{
			//check flag against factory type
			factoryType = FACTORY_FLAG;
			if (psStructure->pStructureType->type == REF_CYBORG_FACTORY)
			{
				factoryType = CYBORG_FLAG;
			}
			else if (psStructure->pStructureType->type == REF_VTOL_FACTORY)
			{
				factoryType = VTOL_FLAG;
			}
			//find flag in question.
			const auto& flagPosList = apsFlagPosLists[psFact->psAssemblyPoint->player];
			auto psFlag = std::find_if(flagPosList.begin(), flagPosList.end(), [psFact, factoryType](FLAG_POSITION* psFlag)
			{
				return psFlag->factoryInc == psFact->psAssemblyPoint->factoryInc // correct fact.
					&& psFlag->factoryType == factoryType; // correct type
			});
			ASSERT(psFlag != flagPosList.end(), "No flag found for %s at (%d, %d)", objInfo(psStructure), psStructure->pos.x, psStructure->pos.y);
			//if vtol droid - send it to ReArm Pad if one exists
			if (psFlag != flagPosList.end() && psNewDroid->isVtol())
			{
				Vector2i pos = (*psFlag)->coords.xy();
				//find a suitable location near the delivery point
				actionVTOLLandingPos(psNewDroid, &pos);
				orderDroidLoc(psNewDroid, DORDER_MOVE, pos.x, pos.y, ModeQueue);
			}
			else if (psFlag != flagPosList.end())
			{
				orderDroidLoc(psNewDroid, DORDER_MOVE, (*psFlag)->coords.x, (*psFlag)->coords.y, ModeQueue);
			}
		}
		if (assignCommander)
		{
			assignFactoryCommandDroid(psStructure, psNewDroid);
		}
		return true;
	}
	else
	{
		syncDebug("Droid placement failed");
		*ppsDroid = nullptr;
	}
	return false;
}


static bool IsFactoryCommanderGroupFull(const FACTORY *psFactory)
{
	if (bMultiPlayer)
	{
		// TODO: Synchronise .psCommander. Have to return false here, to avoid desynch.
		return false;
	}

	unsigned int DroidsInGroup;

	// If we don't have a commander return false (group not full)
	if (psFactory->psCommander == nullptr)
	{
		return false;
	}

	// allow any number of IDF droids
	if (templateIsIDF(psFactory->psSubject) || psFactory->psSubject->getPropulsionStats()->propulsionType == PROPULSION_TYPE_LIFT)
	{
		return false;
	}

	// Get the number of droids in the commanders group
	DroidsInGroup = psFactory->psCommander->psGroup ? psFactory->psCommander->psGroup->getNumMembers() : 0;

	// if the number in group is less than the maximum allowed then return false (group not full)
	if (DroidsInGroup < cmdDroidMaxGroup(psFactory->psCommander))
	{
		return false;
	}

	// the number in group has reached the maximum
	return true;
}

// Check if a player has a certain structure. Optionally, checks if there is
// at least one that is built.
bool structureExists(int player, STRUCTURE_TYPE type, bool built, bool isMission)
{
	bool found = false;

	ASSERT_OR_RETURN(false, player >= 0, "invalid player: %d", player);
	if (player >= MAX_PLAYERS)
	{
		return false;
	}

	StructureList* pList = isMission ? &mission.apsStructLists[player] : &apsStructLists[player];
	for (const STRUCTURE *psCurr : *pList)
	{
		if (psCurr->pStructureType->type == type && (!built || (built && psCurr->status == SS_BUILT)))
		{
			found = true;
			break;
		}
	}

	return found;
}

// Disallow manufacture of units once these limits are reached,
// doesn't mean that these numbers can't be exceeded if units are
// put down in the editor or by the scripts.

void setMaxDroids(UDWORD player, int value)
{
	ASSERT_OR_RETURN(, player < MAX_PLAYERS, "player = %" PRIu32 "", player);
	droidLimit[player] = value;
}

void setMaxCommanders(UDWORD player, int value)
{
	ASSERT_OR_RETURN(, player < MAX_PLAYERS, "player = %" PRIu32 "", player);
	commanderLimit[player] = value;
}

void setMaxConstructors(UDWORD player, int value)
{
	ASSERT_OR_RETURN(, player < MAX_PLAYERS, "player = %" PRIu32 "", player);
	constructorLimit[player] = value;
}

int getMaxDroids(UDWORD player)
{
	ASSERT_OR_RETURN(0, player < MAX_PLAYERS, "player = %" PRIu32 "", player);
	return droidLimit[player];
}

int getMaxCommanders(UDWORD player)
{
	ASSERT_OR_RETURN(0, player < MAX_PLAYERS, "player = %" PRIu32 "", player);
	return commanderLimit[player];
}

int getMaxConstructors(UDWORD player)
{
	ASSERT_OR_RETURN(0, player < MAX_PLAYERS, "player = %" PRIu32 "", player);
	return constructorLimit[player];
}

bool IsPlayerDroidLimitReached(int player)
{
	int numDroids = getNumDroids(player) + getNumMissionDroids(player) + getNumTransporterDroids(player);
	return numDroids >= getMaxDroids(player);
}

// Check for max number of units reached and halt production.
static bool checkHaltOnMaxUnitsReached(STRUCTURE *psStructure, bool isMission)
{
	CHECK_STRUCTURE(psStructure);

	char limitMsg[300];
	bool isLimit = false;
	int player = psStructure->player;

	DROID_TEMPLATE *templ = psStructure->pFunctionality->factory.psSubject;

	// if the players that owns the factory has reached his (or hers) droid limit
	// then put production on hold & return - we need a message to be displayed here !!!!!!!
	if (IsPlayerDroidLimitReached(player))
	{
		isLimit = true;
		sstrcpy(limitMsg, _("Can't build any more units, Unit Limit Reached — Production Halted"));
	}
	else switch (droidTemplateType(templ))
		{
		case DROID_COMMAND:
			if (!structureExists(player, REF_COMMAND_CONTROL, true, isMission))
			{
				isLimit = true;
				ssprintf(limitMsg, _("Can't build \"%s\" without a Command Relay Center — Production Halted"), templ->name.toUtf8().c_str());
			}
			else if (getNumCommandDroids(player) >= getMaxCommanders(player))
			{
				isLimit = true;
				ssprintf(limitMsg, _("Can't build \"%s\", Commander Limit Reached — Production Halted"), templ->name.toUtf8().c_str());
			}
			break;
		case DROID_CONSTRUCT:
		case DROID_CYBORG_CONSTRUCT:
			if (getNumConstructorDroids(player) >= getMaxConstructors(player))
			{
				isLimit = true;
				ssprintf(limitMsg, _("Can't build any more \"%s\", Construction Unit Limit Reached — Production Halted"), templ->name.toUtf8().c_str());
			}
			break;
		default:
			break;
		}

	if (isLimit && player == selectedPlayer && (lastMaxUnitMessage == 0 || lastMaxUnitMessage + MAX_UNIT_MESSAGE_PAUSE <= gameTime))
	{
		addConsoleMessage(limitMsg, DEFAULT_JUSTIFY, SYSTEM_MESSAGE);
		lastMaxUnitMessage = gameTime;
	}

	return isLimit;
}

void aiUpdateRepair_handleState(STRUCTURE &station)
{
	STRUCTURE *psStructure = &station;
	const REPAIR_FACILITY	*psRepairFac = &station.pFunctionality->repairFacility;
	DROID *psDroid = (DROID*) psRepairFac->psObj;
	// apply logic for current state
	switch (psRepairFac->state)
	{
	case RepairState::Idle:
	{
		actionAlignTurret(psStructure, 0);
		return;
	}
	case RepairState::Repairing:
	{
		ASSERT_OR_RETURN(, psDroid != nullptr, "invalid droid pointer");
		psDroid->body += gameTimeAdjustedAverage(getBuildingRepairPoints(psStructure));
		psDroid->body = MIN(psDroid->originalBody, psDroid->body);
		actionTargetTurret(psStructure, (BASE_OBJECT*) psDroid, &psStructure->asWeaps[0]);
		if (psStructure->visibleForLocalDisplay() && psDroid->visibleForLocalDisplay()) // display only - does not impact simulation state
		{
			Vector3i iVecEffect;
			// add plasma repair effect whilst being repaired
			iVecEffect.x = psDroid->pos.x + (10 - rand() % 20);
			iVecEffect.y = psDroid->pos.z + (10 - rand() % 20);
			iVecEffect.z = psDroid->pos.y + (10 - rand() % 20);
			effectSetSize(100);
			addEffect(&iVecEffect, EFFECT_EXPLOSION, EXPLOSION_TYPE_SPECIFIED, true, getDisplayImdFromIndex(MI_FLAME), 0, gameTime - deltaGameTime + 1);
		}
		return;
	};
	case RepairState::Invalid:
	{
		debug(LOG_ERROR, "repair state is invalid");
		return;
	}
	}
}

RepairEvents aiUpdateRepair_obtainEvents(const STRUCTURE &station, DROID **psDroidOut)
{
	const STRUCTURE *psStructure = &station;
	const REPAIR_FACILITY	*psRepairFac = &station.pFunctionality->repairFacility;
	SDWORD	xdiff, ydiff;
	switch (psRepairFac->state)
	{
	case RepairState::Idle:
	{
		DROID *psDroid = findSomeoneToRepair(psStructure, (TILE_UNITS * 5 / 2));
		*psDroidOut = psDroid;
		return psDroid? RepairEvents::RepairTargetFound : RepairEvents::NoEvents;
	}
	case RepairState::Repairing:
	{
		const DROID *psDroid = (DROID *) psRepairFac->psObj;
		ASSERT(psDroid, "repairing a null object??");
		if (psDroid->died) return RepairEvents::UnitDied;

		xdiff = (SDWORD)psDroid->pos.x - (SDWORD)psStructure->pos.x;
		ydiff = (SDWORD)psDroid->pos.y - (SDWORD)psStructure->pos.y;
		if (xdiff * xdiff + ydiff * ydiff > (TILE_UNITS * 5 / 2) * (TILE_UNITS * 5 / 2))
		{
			return RepairEvents::UnitMovedAway;
		}
		if (psDroid->body >= psDroid->originalBody)
		{
			return RepairEvents::UnitReachedMaxHP;
		}
		return RepairEvents::NoEvents;
	}
	case RepairState::Invalid:
	{
		debug(LOG_ERROR, "Inavlid repair station (id=%i player=%i) state", station.id, station.player);
		return RepairEvents::NoEvents;
	}
	default:
	{
		debug(LOG_ERROR, "Unknown repair station (id=%i player=%i) state", station.id, station.player);
		return RepairEvents::NoEvents;
	}
	};
}

RepairState aiUpdateRepair_handleEvents(STRUCTURE &station, RepairEvents ev, DROID *found)
{
	REPAIR_FACILITY	*psRepairFac = &station.pFunctionality->repairFacility;
	const STRUCTURE *psStructure = &station;
	//don't do anything if the resistance is low in multiplayer
	if (bMultiPlayer && psStructure->resistance < (int)structureResistance(psStructure->pStructureType, psStructure->player))
	{
		objTrace(psStructure->id, "Resistance too low for repair");
		droidRepairStopped(castDroid(psRepairFac->psObj), &station);
		psRepairFac->psObj = nullptr;
		return RepairState::Idle;
	}
	switch (ev)
	{
	case RepairEvents::NoEvents:
	{
		return psRepairFac->state;
	};
	case RepairEvents::RepairTargetFound:
	{
		ASSERT(found != nullptr, "Bug! found droid, but it was null?");
		psRepairFac->psObj = found;
		droidRepairStarted(found, &station);
		return RepairState::Repairing;
	};
	case RepairEvents::UnitReachedMaxHP:
	{
		DROID *psDroid = (DROID*) psRepairFac->psObj;
		psRepairFac->psObj = nullptr;
		objTrace(psStructure->id, "Repair complete of droid %d", (int)psDroid->id);
		psDroid->body = psDroid->originalBody;
		// if completely repaired reset order
		objTrace(psDroid->id, "was fully repaired by RF");
		droidRepairStopped(psDroid, &station);
		droidWasFullyRepaired(psDroid, psRepairFac);
		// only call "secondarySetState" *after* triggering "droidWasFullyRepaired"
		// because in some cases calling it would modify primary order
		// thus, loosing information that we actually had a RTR|RTR_SPECIFIED before
		secondarySetState(psDroid, DSO_RETURN_TO_LOC, DSS_NONE);
		return RepairState::Idle;
	};
	case RepairEvents::UnitDied:
	{
		DROID *psDroid = (DROID*) psRepairFac->psObj;
		syncDebugDroid(psDroid, '-');
		droidRepairStopped(psDroid, &station);
		psRepairFac->psObj = nullptr;
		return RepairState::Idle;
	};
	case RepairEvents::UnitMovedAway:
	{
		droidRepairStopped(castDroid(psRepairFac->psObj), &station);
		psRepairFac->psObj = nullptr;
		return RepairState::Idle;
	};
	}
	debug(LOG_ERROR, "Unknown event passed");
	return RepairState::Invalid;
}

void aiUpdateRepairStation(STRUCTURE &station)
{
	DROID *found = nullptr;
	RepairEvents topEvent = aiUpdateRepair_obtainEvents(station, &found);
	RepairState nextState = aiUpdateRepair_handleEvents(station, topEvent, found);
	ASSERT(nextState != RepairState::Invalid, "Bug! invalid state received.");
	station.pFunctionality->repairFacility.state = nextState;
	aiUpdateRepair_handleState(station);
}

static void aiUpdateStructure(STRUCTURE *psStructure, bool isMission)
{
	UDWORD structureMode = 0;
	DROID *psDroid;
	BASE_OBJECT *psChosenObjs[MAX_WEAPONS] = {nullptr};
	BASE_OBJECT *psChosenObj = nullptr;
	FACTORY *psFactory;
	bool bDroidPlaced = false;
	WEAPON_STATS *psWStats;
	bool bDirect = false;
	TARGET_ORIGIN tmpOrigin = ORIGIN_UNKNOWN;

	CHECK_STRUCTURE(psStructure);

	if (psStructure->time == gameTime)
	{
		// This isn't supposed to happen, and really shouldn't be possible - if this happens, maybe a structure is being updated twice?
		int count1 = 0, count2 = 0;
		for (const STRUCTURE* s : apsStructLists[psStructure->player])
		{
			count1 += s == psStructure;
		}
		for (const STRUCTURE* s : mission.apsStructLists[psStructure->player])
		{
			count2 += s == psStructure;
		}
		debug(LOG_ERROR, "psStructure->prevTime = %u, psStructure->time = %u, gameTime = %u, count1 = %d, count2 = %d", psStructure->prevTime, psStructure->time, gameTime, count1, count2);
		--psStructure->time;
	}
	psStructure->prevTime = psStructure->time;
	psStructure->time = gameTime;
	for (UDWORD i = 0; i < MAX(1, psStructure->numWeaps); ++i)
	{
		psStructure->asWeaps[i].prevRot = psStructure->asWeaps[i].rot;
	}

	if (isMission)
	{
		switch (psStructure->pStructureType->type)
		{
		case REF_RESEARCH:
		case REF_FACTORY:
		case REF_CYBORG_FACTORY:
		case REF_VTOL_FACTORY:
			break;
		default:
			return; // nothing to do
		}
	}

	// Will go out into a building EVENT stats/text file
	/* Spin round yer sensors! */
	if (psStructure->numWeaps == 0)
	{
		if ((psStructure->asWeaps[0].nStat == 0) &&
		    (psStructure->pStructureType->type != REF_REPAIR_FACILITY))
		{

			//////
			// - radar should rotate every three seconds ... 'cause we timed it at Heathrow !
			// gameTime is in milliseconds - one rotation every 3 seconds = 1 rotation event 3000 millisecs
			psStructure->asWeaps[0].rot.direction = (uint16_t)((uint64_t)gameTime * 65536 / 3000) + ((psStructure->pos.x + psStructure->pos.y) % 10) * 6550;  // Randomize by hashing position as seed for rotating 1/10th turns. Cast wrapping intended.
			psStructure->asWeaps[0].rot.pitch = 0;
		}
	}

	/* Check lassat */
	if (isLasSat(psStructure->pStructureType)
	    && gameTime - psStructure->asWeaps[0].lastFired > weaponFirePause(*psStructure->getWeaponStats(0), psStructure->player)
	    && psStructure->asWeaps[0].ammo > 0)
	{
		if (psStructure->player == selectedPlayer)
		{
			addConsoleMessage(_("Laser Satellite is ready to fire!"), CENTRE_JUSTIFY, SYSTEM_MESSAGE);
			audio_PlayTrack(ID_SOUND_UPLINK); // This is about the best sound available for this.
		}
		triggerEventStructureReady(psStructure);
		psStructure->asWeaps[0].ammo = 0; // do not fire more than once
	}

	/* See if there is an enemy to attack */
	if (psStructure->numWeaps > 0)
	{
		//structures always update their targets
		for (UDWORD i = 0; i < psStructure->numWeaps; i++)
		{
			bDirect = proj_Direct(psStructure->getWeaponStats(i));
			if (psStructure->asWeaps[i].nStat > 0 &&
			    psStructure->getWeaponStats(i)->weaponSubClass != WSC_LAS_SAT)
			{
				if (aiChooseTarget(psStructure, &psChosenObjs[i], i, true, &tmpOrigin))
				{
					objTrace(psStructure->id, "Weapon %d is targeting %d at (%d, %d)", i, psChosenObjs[i]->id,
					         psChosenObjs[i]->pos.x, psChosenObjs[i]->pos.y);
					setStructureTarget(psStructure, psChosenObjs[i], i, tmpOrigin);
				}
				else
				{
					if (i > 0)
					{
						if (psChosenObjs[0])
						{
							objTrace(psStructure->id, "Weapon %d is supporting main weapon: %d at (%d, %d)", i,
							         psChosenObjs[0]->id, psChosenObjs[0]->pos.x, psChosenObjs[0]->pos.y);
							setStructureTarget(psStructure, psChosenObjs[0], i, tmpOrigin);
							psChosenObjs[i] = psChosenObjs[0];
						}
						else
						{
							setStructureTarget(psStructure, nullptr, i, ORIGIN_UNKNOWN);
							psChosenObjs[i] = nullptr;
						}
					}
					else
					{
						setStructureTarget(psStructure, nullptr, i, ORIGIN_UNKNOWN);
						psChosenObjs[i] = nullptr;
					}
				}

				if (psChosenObjs[i] != nullptr && !aiObjectIsProbablyDoomed(psChosenObjs[i], bDirect))
				{
					// get the weapon stat to see if there is a visible turret to rotate
					psWStats = psStructure->getWeaponStats(i);

					//if were going to shoot at something move the turret first then fire when locked on
					if (psWStats->pMountGraphic == nullptr)//no turret so lock on whatever
					{
						psStructure->asWeaps[i].rot.direction = calcDirection(psStructure->pos.x, psStructure->pos.y, psChosenObjs[i]->pos.x, psChosenObjs[i]->pos.y);
						combFire(&psStructure->asWeaps[i], psStructure, psChosenObjs[i], i);
					}
					else if (actionTargetTurret(psStructure, psChosenObjs[i], &psStructure->asWeaps[i]))
					{
						combFire(&psStructure->asWeaps[i], psStructure, psChosenObjs[i], i);
					}
				}
				else
				{
					// realign the turret
					if ((psStructure->asWeaps[i].rot.direction % DEG(90)) != 0 || psStructure->asWeaps[i].rot.pitch != 0)
					{
						actionAlignTurret(psStructure, i);
					}
				}
			}
		}
	}

	/* See if there is an enemy to attack for Sensor Towers that have weapon droids attached*/
	else if (psStructure->pStructureType->pSensor)
	{
		if (structStandardSensor(psStructure) || structVTOLSensor(psStructure) || objRadarDetector(psStructure))
		{
			if (aiChooseSensorTarget(psStructure, &psChosenObj))
			{
				objTrace(psStructure->id, "Sensing (%d)", psChosenObj->id);
				if (objRadarDetector(psStructure))
				{
					setStructureTarget(psStructure, psChosenObj, 0, ORIGIN_RADAR_DETECTOR);
				}
				else
				{
					setStructureTarget(psStructure, psChosenObj, 0, ORIGIN_SENSOR);
				}
			}
			else
			{
				setStructureTarget(psStructure, nullptr, 0, ORIGIN_UNKNOWN);
			}
			psChosenObj = psStructure->psTarget[0];
		}
		else
		{
			psChosenObj = psStructure->psTarget[0];
		}
	}
	//only interested if the Structure "does" something!
	if (psStructure->pFunctionality == nullptr)
	{
		return;
	}

	/* Process the functionality according to type
	* determine the subject stats (for research or manufacture)
	* or base object (for repair) or update power levels for resourceExtractor
	*/
	BASE_STATS *pSubject = nullptr;
	switch (psStructure->pStructureType->type)
	{
	case REF_RESEARCH:
		{
			pSubject = psStructure->pFunctionality->researchFacility.psSubject;
			structureMode = REF_RESEARCH;
			break;
		}
	case REF_FACTORY:
	case REF_CYBORG_FACTORY:
	case REF_VTOL_FACTORY:
		{
			pSubject = psStructure->pFunctionality->factory.psSubject;
			structureMode = REF_FACTORY;
			//check here to see if the factory's commander has died
			if (psStructure->pFunctionality->factory.psCommander &&
			    psStructure->pFunctionality->factory.psCommander->died)
			{
				//remove the commander from the factory
				syncDebugDroid(psStructure->pFunctionality->factory.psCommander, '-');
				assignFactoryCommandDroid(psStructure, nullptr);
			}
			break;
		}
	case REF_REPAIR_FACILITY:
		{
			aiUpdateRepairStation(*psStructure);
			break;
		}
	case REF_REARM_PAD:
		{
			REARM_PAD	*psReArmPad = &psStructure->pFunctionality->rearmPad;

			psChosenObj = psReArmPad->psObj;
			structureMode = REF_REARM_PAD;
			psDroid = nullptr;

			/* select next droid if none being rearmed*/
			if (psChosenObj == nullptr)
			{
				objTrace(psStructure->id, "Rearm pad idle - look for victim");
				for (DROID* psCurr : apsDroidLists[psStructure->player])
				{
					// move next droid waiting on ground to rearm pad
					if (vtolReadyToRearm(psCurr, psStructure) &&
					    (psChosenObj == nullptr || (((DROID *)psChosenObj)->actionStarted > psCurr->actionStarted)))
					{
						objTrace(psCurr->id, "rearm pad candidate");
						objTrace(psStructure->id, "we found %s to rearm", objInfo(psCurr));
						psChosenObj = psCurr;
					}
				}
				// None available? Try allies.
				for (int i = 0; i < MAX_PLAYERS && !psChosenObj; i++)
				{
					if (aiCheckAlliances(i, psStructure->player) && i != psStructure->player)
					{
						for (DROID* psCurr : apsDroidLists[i])
						{
							// move next droid waiting on ground to rearm pad
							if (vtolReadyToRearm(psCurr, psStructure))
							{
								psChosenObj = psCurr;
								objTrace(psCurr->id, "allied rearm pad candidate");
								objTrace(psStructure->id, "we found allied %s to rearm", objInfo(psDroid));
								break;
							}
						}
					}
				}
				psDroid = (DROID *)psChosenObj;
				if (psDroid != nullptr)
				{
					actionDroid(psDroid, DACTION_MOVETOREARMPOINT, psStructure);
				}
			}
			else
			{
				psDroid = (DROID *) psChosenObj;
				if ((psDroid->sMove.Status == MOVEINACTIVE ||
				     psDroid->sMove.Status == MOVEHOVER) &&
				    psDroid->action == DACTION_WAITFORREARM)
				{
					objTrace(psDroid->id, "supposed to go to rearm but not on our way -- fixing"); // this should never happen...
					actionDroid(psDroid, DACTION_MOVETOREARMPOINT, psStructure);
				}
			}

			// if found a droid to rearm assign it to the rearm pad
			if (psDroid != nullptr)
			{
				/* set chosen object */
				psChosenObj = psDroid;
				psReArmPad->psObj = psChosenObj;
				if (psDroid->action == DACTION_MOVETOREARMPOINT)
				{
					/* reset rearm started */
					psReArmPad->timeStarted = ACTION_START_TIME;
					psReArmPad->timeLastUpdated = 0;
				}
				auxStructureBlocking(psStructure);
			}
			else
			{
				auxStructureNonblocking(psStructure);
			}
			break;
		}
	default:
		break;
	}

	if (structureMode == REF_RESEARCH)
	{
		if (pSubject != nullptr)
		{
			incrementMultiStatsResearchPerformance(psStructure->player);
		}
		incrementMultiStatsResearchPotential(psStructure->player);
	}

	/* check subject stats (for research or manufacture) */
	if (pSubject != nullptr)
	{
		//if subject is research...
		if (structureMode == REF_RESEARCH)
		{
			RESEARCH_FACILITY *psResFacility = &psStructure->pFunctionality->researchFacility;

			//if on hold don't do anything
			if (psResFacility->timeStartHold)
			{
				delPowerRequest(psStructure);
				return;
			}

			//electronic warfare affects the functionality of some structures in multiPlayer
			if (bMultiPlayer && psStructure->resistance < (int)structureResistance(psStructure->pStructureType, psStructure->player))
			{
				return;
			}

			int researchIndex = pSubject->ref - STAT_RESEARCH;

			PLAYER_RESEARCH *pPlayerRes = &asPlayerResList[psStructure->player][researchIndex];
			//check research has not already been completed by another structure
			if (!IsResearchCompleted(pPlayerRes))
			{
				RESEARCH *pResearch = (RESEARCH *)pSubject;

				unsigned pointsToAdd = gameTimeAdjustedAverage(getBuildingResearchPoints(psStructure));
				pointsToAdd = MIN(pointsToAdd, pResearch->researchPoints - pPlayerRes->currentPoints);

				unsigned shareProgress = pPlayerRes->currentPoints;  // Share old research progress instead of new one, so it doesn't get sped up by multiple players researching.
				bool shareIsFinished = false;

				if (pointsToAdd > 0 && pPlayerRes->currentPoints == 0)
				{
					bool haveEnoughPower = requestPowerFor(psStructure, pResearch->researchPower);
					if (haveEnoughPower)
					{
						shareProgress = 1;  // Share research payment, to avoid double payment even if starting research in the same game tick.
					}
					else
					{
						pointsToAdd = 0;
					}
				}

				if (pointsToAdd > 0 && pResearch->researchPoints > 0)  // might be a "free" research
				{
					pPlayerRes->currentPoints += pointsToAdd;
				}
				syncDebug("Research at %u/%u.", pPlayerRes->currentPoints, pResearch->researchPoints);

				//check if Research is complete
				if (pPlayerRes->currentPoints >= pResearch->researchPoints)
				{
					int prevState = intGetResearchState();

					//store the last topic researched - if its the best
					if (psResFacility->psBestTopic == nullptr)
					{
						psResFacility->psBestTopic = psResFacility->psSubject;
					}
					else
					{
						if (pResearch->researchPoints > psResFacility->psBestTopic->researchPoints)
						{
							psResFacility->psBestTopic = psResFacility->psSubject;
						}
					}
					psResFacility->psSubject = nullptr;
					intResearchFinished(psStructure);
					researchResult(researchIndex, psStructure->player, true, psStructure, true);

					shareIsFinished = true;

					//check if this result has enabled another topic
					intNotifyResearchButton(prevState);
				}

				// Update allies research accordingly
				if (game.type == LEVEL_TYPE::SKIRMISH && alliancesSharedResearch(game.alliance))
				{
					for (uint8_t i = 0; i < MAX_PLAYERS; i++)
					{
						if (alliances[i][psStructure->player] == ALLIANCE_FORMED)
						{
							if (!IsResearchCompleted(&asPlayerResList[i][researchIndex]))
							{
								// Share the research for that player.
								auto &allyProgress = asPlayerResList[i][researchIndex].currentPoints;
								allyProgress = std::max(allyProgress, shareProgress);
								if (shareIsFinished)
								{
									researchResult(researchIndex, i, false, nullptr, true);
								}
							}
						}
					}
				}
			}
			else
			{
				//cancel this Structure's research since now complete
				psResFacility->psSubject = nullptr;
				intResearchFinished(psStructure);
				syncDebug("Research completed elsewhere.");
			}
		}
		//check for manufacture
		else if (structureMode == REF_FACTORY)
		{
			psFactory = &psStructure->pFunctionality->factory;

			//if on hold don't do anything
			if (psFactory->timeStartHold)
			{
				return;
			}

			//electronic warfare affects the functionality of some structures in multiPlayer
			if (bMultiPlayer && psStructure->resistance < (int)structureResistance(psStructure->pStructureType, psStructure->player))
			{
				return;
			}

			if (psFactory->timeStarted == ACTION_START_TIME)
			{
				// also need to check if a command droid's group is full

				// If the factory commanders group is full - return
				if (IsFactoryCommanderGroupFull(psFactory) || checkHaltOnMaxUnitsReached(psStructure, isMission))
				{
					return;
				}

				//set the time started
				psFactory->timeStarted = gameTime;
			}

			if (psFactory->buildPointsRemaining > 0)
			{
				int progress = gameTimeAdjustedAverage(getBuildingProductionPoints(psStructure));
				if ((unsigned)psFactory->buildPointsRemaining == calcTemplateBuild(psFactory->psSubject) && progress > 0)
				{
					// We're just starting to build, check for power.
					bool haveEnoughPower = requestPowerFor(psStructure, calcTemplatePower(psFactory->psSubject));
					if (!haveEnoughPower)
					{
						progress = 0;
					}
				}
				psFactory->buildPointsRemaining -= progress;
			}

			//check for manufacture to be complete
			if (psFactory->buildPointsRemaining <= 0 && !IsFactoryCommanderGroupFull(psFactory) && !checkHaltOnMaxUnitsReached(psStructure, isMission))
			{
				if (isMission)
				{
					// put it in the mission list
					psDroid = buildMissionDroid((DROID_TEMPLATE *)pSubject,
					                            psStructure->pos.x, psStructure->pos.y,
					                            psStructure->player);
					if (psDroid)
					{
						psDroid->secondaryOrder = psFactory->secondaryOrder;
						psDroid->secondaryOrderPending = psDroid->secondaryOrder;
						setFactorySecondaryState(psDroid, psStructure);
						setDroidBase(psDroid, psStructure);
						bDroidPlaced = true;
					}
				}
				else
				{
					// place it on the map
					bDroidPlaced = structPlaceDroid(psStructure, (DROID_TEMPLATE *)pSubject, &psDroid);
				}

				//script callback, must be called after factory was flagged as idle
				if (bDroidPlaced)
				{
					//reset the start time
					psFactory->timeStarted = ACTION_START_TIME;
					psFactory->psSubject = nullptr;

					doNextProduction(psStructure, (DROID_TEMPLATE *)pSubject, ModeImmediate);

					cbNewDroid(psStructure, psDroid);
				}
			}
		}
	}

	// check base object (for rearm)
	if (psChosenObj != nullptr)
	{
		if (structureMode == REF_REARM_PAD)
		{
			//check for rearming
			REARM_PAD	*psReArmPad = &psStructure->pFunctionality->rearmPad;
			UDWORD pointsAlreadyAdded;

			psDroid = (DROID *)psChosenObj;
			ASSERT_OR_RETURN(, psDroid != nullptr, "invalid droid pointer");
			ASSERT_OR_RETURN(, psDroid->isVtol(), "invalid droid type");

			//check hasn't died whilst waiting to be rearmed
			// also clear out any previously repaired droid
			if (psDroid->died || (psDroid->action != DACTION_MOVETOREARMPOINT && psDroid->action != DACTION_WAITDURINGREARM))
			{
				syncDebugDroid(psDroid, '-');
				psReArmPad->psObj = nullptr;
				objTrace(psDroid->id, "VTOL has wrong action or is dead");
				return;
			}
			if (psDroid->action == DACTION_WAITDURINGREARM && psDroid->sMove.Status == MOVEINACTIVE)
			{
				if (psReArmPad->timeStarted == ACTION_START_TIME)
				{
					//set the time started and last updated
					psReArmPad->timeStarted = gameTime;
					psReArmPad->timeLastUpdated = gameTime;
				}
				unsigned pointsToAdd = getBuildingRearmPoints(psStructure) * (gameTime - psReArmPad->timeStarted) / GAME_TICKS_PER_SEC;
				pointsAlreadyAdded = getBuildingRearmPoints(psStructure) * (psReArmPad->timeLastUpdated - psReArmPad->timeStarted) / GAME_TICKS_PER_SEC;
				if (pointsToAdd >= psDroid->weight) // amount required is a factor of the droid weight
				{
					// We should be fully loaded by now.
					fillVtolDroid(psDroid);
					objTrace(psDroid->id, "fully loaded");
				}
				else
				{
					for (unsigned i = 0; i < psDroid->numWeaps; i++)		// rearm one weapon at a time
					{
						// Make sure it's a rearmable weapon (and so we don't divide by zero)
						if (psDroid->asWeaps[i].usedAmmo > 0 && psDroid->getWeaponStats(i)->upgrade[psDroid->player].numRounds > 0)
						{
							// Do not "simplify" this formula.
							// It is written this way to prevent rounding errors.
							int ammoToAddThisTime =
							    pointsToAdd * getNumAttackRuns(psDroid, i) / psDroid->weight -
							    pointsAlreadyAdded * getNumAttackRuns(psDroid, i) / psDroid->weight;
							psDroid->asWeaps[i].usedAmmo -= std::min<unsigned>(ammoToAddThisTime, psDroid->asWeaps[i].usedAmmo);
							if (ammoToAddThisTime)
							{
								// reset ammo and lastFired
								psDroid->asWeaps[i].ammo = psDroid->getWeaponStats(i)->upgrade[psDroid->player].numRounds;
								psDroid->asWeaps[i].lastFired = 0;
								break;
							}
						}
					}
				}
				if (psDroid->isDamaged()) // do repairs
				{
					psDroid->body += gameTimeAdjustedAverage(getBuildingRepairPoints(psStructure));
					if (psDroid->body >= psDroid->originalBody)
					{
						psDroid->body = psDroid->originalBody;
					}
				}
				psReArmPad->timeLastUpdated = gameTime;

				//check for fully armed and fully repaired
				if (vtolHappy(psDroid))
				{
					//clear the rearm pad
					psDroid->action = DACTION_NONE;
					psReArmPad->psObj = nullptr;
					auxStructureNonblocking(psStructure);
					triggerEventDroidIdle(psDroid);
					objTrace(psDroid->id, "VTOL happy and ready for action!");
				}
			}
		}
	}
}


/** Decides whether a structure should emit smoke when it's damaged */
static bool canSmoke(const STRUCTURE *psStruct)
{
	if (psStruct->pStructureType->type == REF_WALL || psStruct->pStructureType->type == REF_WALLCORNER
	    || psStruct->status == SS_BEING_BUILT || psStruct->pStructureType->type == REF_GATE)
	{
		return (false);
	}
	else
	{
		return (true);
	}
}

static float CalcStructureSmokeInterval(float damage)
{
	return static_cast<float>((((1. - damage) + 0.1) * 10) * STRUCTURE_DAMAGE_SCALING);
}

void _syncDebugStructure(const char *function, STRUCTURE const *psStruct, char ch)
{
	if (psStruct->type != OBJ_STRUCTURE) {
		ASSERT(false, "%c Broken psStruct->type %u!", ch, psStruct->type);
		syncDebug("Broken psStruct->type %u!", psStruct->type);
		return;
	}
	int ref = 0;
	int refChr = ' ';

	// Print what the structure is producing (or repairing), too.
	switch (psStruct->pStructureType->type)
	{
	case REF_RESEARCH:
		if (psStruct->pFunctionality && psStruct->pFunctionality->researchFacility.psSubject != nullptr)
		{
			ref = psStruct->pFunctionality->researchFacility.psSubject->ref;
			refChr = 'r';
		}
		break;
	case REF_FACTORY:
	case REF_CYBORG_FACTORY:
	case REF_VTOL_FACTORY:
		if (psStruct->pFunctionality && psStruct->pFunctionality->factory.psSubject != nullptr)
		{
			ref = psStruct->pFunctionality->factory.psSubject->multiPlayerID;
			refChr = 'p';
		}
		break;
	case REF_REPAIR_FACILITY:
		if (psStruct->pFunctionality && psStruct->pFunctionality->repairFacility.psObj != nullptr)
		{
			ref = (int)psStruct->pFunctionality->repairFacility.psObj->id;
			refChr = '+';
		}
		break;
	default:
		break;
	}

	int list[] =
	{
		ch,

		(int)psStruct->id,

		psStruct->player,
		psStruct->pos.x, psStruct->pos.y, psStruct->pos.z,
		(int)psStruct->status,
		(int)psStruct->pStructureType->type, refChr, ref,
		(int)psStruct->currentBuildPts,
		(int)psStruct->body,
	};
	_syncDebugIntList(function, "%c structure%d = p%d;pos(%d,%d,%d),status%d,type%d,%c%.0d,bld%d,body%d", list, ARRAY_SIZE(list));
}

int requestOpenGate(STRUCTURE *psStructure)
{
	if (psStructure->status != SS_BUILT || psStructure->pStructureType->type != REF_GATE)
	{
		return 0;  // Can't open.
	}

	switch (psStructure->state)
	{
	case SAS_NORMAL:
		psStructure->lastStateTime = gameTime;
		psStructure->state = SAS_OPENING;
		break;
	case SAS_OPEN:
		psStructure->lastStateTime = gameTime;
		return 0;  // Already open.
	case SAS_OPENING:
		break;
	case SAS_CLOSING:
		psStructure->lastStateTime = 2 * gameTime - psStructure->lastStateTime - SAS_OPEN_SPEED;
		psStructure->state = SAS_OPENING;
		return 0; // Busy
	}

	return psStructure->lastStateTime + SAS_OPEN_SPEED - gameTime;
}

int gateCurrentOpenHeight(const STRUCTURE *psStructure, uint32_t time, int minimumStub)
{
	STRUCTURE_STATS const *psStructureStats = psStructure->pStructureType;
	if (psStructureStats->type == REF_GATE)
	{
		int height = psStructure->sDisplay.imd->max.y;
		int openHeight;
		switch (psStructure->state)
		{
		case SAS_OPEN:
			openHeight = height;
			break;
		case SAS_OPENING:
			openHeight = (height * std::max<int>(time + GAME_TICKS_PER_UPDATE - psStructure->lastStateTime, 0)) / SAS_OPEN_SPEED;
			break;
		case SAS_CLOSING:
			openHeight = height - (height * std::max<int>(time - psStructure->lastStateTime, 0)) / SAS_OPEN_SPEED;
			break;
		default:
			return 0;
		}
		return std::max(std::min(openHeight, height - minimumStub), 0);
	}
	return 0;
}

/* The main update routine for all Structures */
void structureUpdate(STRUCTURE *psBuilding, bool bMission)
{
	UDWORD widthScatter, breadthScatter;
	UDWORD emissionInterval, iPointsToAdd, iPointsRequired;
	Vector3i dv;
	int i;

	syncDebugStructure(psBuilding, '<');

	if (psBuilding->flags.test(OBJECT_FLAG_DIRTY) && !bMission)
	{
		visTilesUpdate(psBuilding);
		psBuilding->flags.set(OBJECT_FLAG_DIRTY, false);
	}

	if (psBuilding->pStructureType->type == REF_GATE)
	{
		if (psBuilding->state == SAS_OPEN && psBuilding->lastStateTime + SAS_STAY_OPEN_TIME < gameTime)
		{
			bool		found = false;

			static GridList gridList;  // static to avoid allocations.
			gridList = gridStartIterate(psBuilding->pos.x, psBuilding->pos.y, TILE_UNITS);
			for (GridIterator gi = gridList.begin(); !found && gi != gridList.end(); ++gi)
			{
				found = isDroid(*gi);
			}

			if (!found)	// no droids on our tile, safe to close
			{
				psBuilding->state = SAS_CLOSING;
				auxStructureClosedGate(psBuilding);     // closed
				psBuilding->lastStateTime = gameTime;	// reset timer
			}
		}
		else if (psBuilding->state == SAS_OPENING && psBuilding->lastStateTime + SAS_OPEN_SPEED < gameTime)
		{
			psBuilding->state = SAS_OPEN;
			auxStructureOpenGate(psBuilding);       // opened
			psBuilding->lastStateTime = gameTime;	// reset timer
		}
		else if (psBuilding->state == SAS_CLOSING && psBuilding->lastStateTime + SAS_OPEN_SPEED < gameTime)
		{
			psBuilding->state = SAS_NORMAL;
			psBuilding->lastStateTime = gameTime;	// reset timer
		}
	}
	else if (psBuilding->pStructureType->type == REF_RESOURCE_EXTRACTOR)
	{
		if (!psBuilding->pFunctionality->resourceExtractor.psPowerGen
		    && psBuilding->animationEvent == ANIM_EVENT_ACTIVE) // no power generator connected
		{
			resetObjectAnimationState(psBuilding);
		}
		else if (psBuilding->pFunctionality->resourceExtractor.psPowerGen
		         && psBuilding->animationEvent == ANIM_EVENT_NONE // we have a power generator, but no animation
		         && psBuilding->sDisplay.imd != nullptr)
		{
			psBuilding->animationEvent = ANIM_EVENT_ACTIVE;

			const iIMDBaseShape *strFirstBaseImd = psBuilding->sDisplay.imd->displayModel()->objanimpie[psBuilding->animationEvent];
			const iIMDShape *strFirstImd = (strFirstBaseImd) ? strFirstBaseImd->displayModel() : nullptr;
			if (strFirstImd != nullptr && strFirstImd->next != nullptr)
			{
				const iIMDShape *strImd = strFirstImd->next.get(); // first imd isn't animated
				psBuilding->timeAnimationStarted = gameTime + (rand() % (strImd->objanimframes * strImd->objanimtime)); // vary animation start time
			}
			else
			{
				ASSERT(strFirstImd != nullptr && strFirstImd->next != nullptr, "Unexpected objanimpie");
				psBuilding->timeAnimationStarted = gameTime;  // so start animation
			}
		}
	}

	// Remove invalid targets. This must be done each frame.
	for (i = 0; i < MAX_WEAPONS; i++)
	{
		if (psBuilding->psTarget[i] && psBuilding->psTarget[i]->died)
		{
			syncDebugObject(psBuilding->psTarget[i], '-');
			setStructureTarget(psBuilding, nullptr, i, ORIGIN_UNKNOWN);
		}
	}

	//update the manufacture/research of the building once complete
	if (psBuilding->status == SS_BUILT)
	{
		aiUpdateStructure(psBuilding, bMission);
	}

	if (psBuilding->status != SS_BUILT)
	{
		if (psBuilding->selected)
		{
			psBuilding->selected = false;
		}
	}

	if (!bMission)
	{
		if (psBuilding->status == SS_BEING_BUILT && psBuilding->buildRate == 0 && !structureHasModules(psBuilding))
		{
			if (psBuilding->pStructureType->powerToBuild == 0)
			{
				// Building is free, and not currently being built, so deconstruct slowly over 1 minute.
				psBuilding->currentBuildPts -= std::min<int>(psBuilding->currentBuildPts, gameTimeAdjustedAverage(structureBuildPointsToCompletion(*psBuilding), 60));
			}

			if (psBuilding->currentBuildPts == 0)
			{
				removeStruct(psBuilding, true);  // If giving up on building something, remove the structure (and remove it from the power queue).
			}
		}
		psBuilding->lastBuildRate = psBuilding->buildRate;
		psBuilding->buildRate = 0;  // Reset to 0, each truck building us will add to our buildRate.
	}

	/* Only add smoke if they're visible and they can 'burn' */
	if (!bMission && psBuilding->visibleForLocalDisplay() && canSmoke(psBuilding))
	{
		const int32_t damage = getStructureDamage(psBuilding);

		// Is there any damage?
		if (damage > 0.)
		{
			emissionInterval = static_cast<UDWORD>(CalcStructureSmokeInterval(damage / 65536.f));
			unsigned effectTime = std::max(gameTime - deltaGameTime + 1, psBuilding->lastEmission + emissionInterval);
			if (gameTime >= effectTime)
			{
				const Vector2i size = psBuilding->size();
				widthScatter   = size.x * TILE_UNITS / 2 / 3;
				breadthScatter = size.y * TILE_UNITS / 2 / 3;
				dv.x = psBuilding->pos.x + widthScatter - rand() % (2 * widthScatter);
				dv.z = psBuilding->pos.y + breadthScatter - rand() % (2 * breadthScatter);
				dv.y = psBuilding->pos.z;
				dv.y += (psBuilding->sDisplay.imd->max.y * 3) / 4;
				addEffect(&dv, EFFECT_SMOKE, SMOKE_TYPE_DRIFTING_HIGH, false, nullptr, 0, effectTime);
				psBuilding->lastEmission = effectTime;
			}
		}
	}

	/* Update the fire damage data */
	if (psBuilding->periodicalDamageStart != 0 && psBuilding->periodicalDamageStart != gameTime - deltaGameTime)  // -deltaGameTime, since projectiles are updated after structures.
	{
		// The periodicalDamageStart has been set, but is not from the previous tick, so we must be out of the fire.
		psBuilding->periodicalDamage = 0;  // Reset burn damage done this tick.
		// Finished burning.
		psBuilding->periodicalDamageStart = 0;
	}

	//check the resistance level of the structure
	iPointsRequired = structureResistance(psBuilding->pStructureType, psBuilding->player);
	if (psBuilding->resistance < (SWORD)iPointsRequired)
	{
		//start the resistance increase
		if (psBuilding->lastResistance == ACTION_START_TIME)
		{
			psBuilding->lastResistance = gameTime;
		}
		//increase over time if low
		if ((gameTime - psBuilding->lastResistance) > RESISTANCE_INTERVAL)
		{
			psBuilding->resistance++;

			//in multiplayer, certain structures do not function whilst low resistance
			if (bMultiPlayer)
			{
				resetResistanceLag(psBuilding);
			}

			psBuilding->lastResistance = gameTime;
			//once the resistance is back up reset the last time increased
			if (psBuilding->resistance >= (SWORD)iPointsRequired)
			{
				psBuilding->lastResistance = ACTION_START_TIME;
			}
		}
	}
	else
	{
		//if selfrepair has been researched then check the health level of the
		//structure once resistance is fully up
		iPointsRequired = psBuilding->structureBody();
		if (selfRepairEnabled(psBuilding->player) && psBuilding->body < iPointsRequired && psBuilding->status != SS_BEING_BUILT)
		{
			//start the self repair off
			if (psBuilding->lastResistance == ACTION_START_TIME)
			{
				psBuilding->lastResistance = gameTime;
			}

			/*since self repair, then add half repair points depending on the time delay for the stat*/
			iPointsToAdd = (repairPoints(asRepairStats[aDefaultRepair[
			                                 psBuilding->player]], psBuilding->player) / 4) * ((gameTime -
			                                         psBuilding->lastResistance) / asRepairStats[
			                                                 aDefaultRepair[psBuilding->player]].time);

			//add the blue flashing effect for multiPlayer
			if (bMultiPlayer && ONEINTEN && !bMission && psBuilding->sDisplay.imd)
			{
				Vector3i position;
				const Vector3f *point;
				SDWORD	realY;
				UDWORD	pointIndex;

				// since this is a visual effect, it should be based on the *display* model
				const iIMDShape *pDisplayModel = psBuilding->sDisplay.imd->displayModel();
				pointIndex = rand() % (pDisplayModel->points.size() - 1);
				point = &(pDisplayModel->points.at(pointIndex));
				position.x = static_cast<int>(psBuilding->pos.x + point->x);
				realY = static_cast<SDWORD>(structHeightScale(psBuilding) * point->y);
				position.y = psBuilding->pos.z + realY;
				position.z = static_cast<int>(psBuilding->pos.y - point->z);
				const auto psTile = mapTile(map_coord({position.x, position.y}));
				if (tileIsClearlyVisible(psTile))
				{
					effectSetSize(30);
					addEffect(&position, EFFECT_EXPLOSION, EXPLOSION_TYPE_SPECIFIED, true, getDisplayImdFromIndex(MI_PLASMA), 0, gameTime - deltaGameTime + rand() % deltaGameTime);
				}
			}

			if (iPointsToAdd)
			{
				psBuilding->body = (UWORD)(psBuilding->body + iPointsToAdd);
				psBuilding->lastResistance = gameTime;
				if (psBuilding->body > iPointsRequired)
				{
					psBuilding->body = (UWORD)iPointsRequired;
					psBuilding->lastResistance = ACTION_START_TIME;
				}
			}
		}
	}

	syncDebugStructure(psBuilding, '>');

	CHECK_STRUCTURE(psBuilding);
}

STRUCTURE::STRUCTURE(uint32_t id, unsigned player)
	: BASE_OBJECT(OBJ_STRUCTURE, id, player)
	, pStructureType(nullptr)
	, pFunctionality(nullptr)
	, buildRate(1)  // Initialise to 1 instead of 0, to make sure we don't get destroyed first tick due to inactivity.
	, lastBuildRate(0)
	, prebuiltImd(nullptr)
{
	pos = Vector3i(0, 0, 0);
	rot = Vector3i(0, 0, 0);
	capacity = 0;
}

/* Release all resources associated with a structure */
STRUCTURE::~STRUCTURE()
{
	STRUCTURE *psBuilding = this;

	// free up the space used by the functionality array
	free(psBuilding->pFunctionality);
	psBuilding->pFunctionality = nullptr;
}


/*
fills the list with Structure that can be built. There is a limit on how many can
be built at any one time. Pass back the number available.
There is now a limit of how many of each type of structure are allowed per mission
*/
std::vector<STRUCTURE_STATS *> fillStructureList(UDWORD _selectedPlayer, UDWORD limit, bool showFavorites)
{
	std::vector<STRUCTURE_STATS *> structureList;
	UDWORD			inc;
	STRUCTURE_STATS	*psBuilding;

	ASSERT_OR_RETURN(structureList, _selectedPlayer < MAX_PLAYERS, "_selectedPlayer = %" PRIu32 "", _selectedPlayer);

	// counters for current nb of buildings, max buildings, current nb modules
	int8_t researchLabCurrMax[] 	= {0, 0};
	int8_t factoriesCurrMax[] 		= {0, 0};
	int8_t vtolFactoriesCurrMax[] 	= {0, 0};
	int8_t powerGenCurrMax[]		= {0, 0};
	int8_t factoryModules 			= 0;
	int8_t powerGenModules			= 0;
	int8_t researchModules			= 0;

	//if currently on a mission can't build factory/research/power/derricks
	if (!missionIsOffworld())
	{
		for (const STRUCTURE* psCurr : apsStructLists[_selectedPlayer])
		{
			if (psCurr->pStructureType->type == REF_RESEARCH && psCurr->status == SS_BUILT)
			{
				researchModules += psCurr->capacity;
			}
			else if (psCurr->pStructureType->type == REF_FACTORY && psCurr->status == SS_BUILT)
			{
				factoryModules += psCurr->capacity;
			}
			else if (psCurr->pStructureType->type == REF_POWER_GEN && psCurr->status == SS_BUILT)
			{
				powerGenModules += psCurr->capacity;
			}
			else if (psCurr->pStructureType->type == REF_VTOL_FACTORY && psCurr->status == SS_BUILT)
			{
				// same as REF_FACTORY
				factoryModules += psCurr->capacity;
			}
		}
	}

	// find maximum allowed limits (current built numbers already available, just grab them)
	for (inc = 0; inc < numStructureStats; inc++)
	{
		if (apStructTypeLists[_selectedPlayer][inc] == AVAILABLE || (includeRedundantDesigns && apStructTypeLists[_selectedPlayer][inc] == REDUNDANT))
		{
			int8_t *counter;
			if (asStructureStats[inc].type == REF_RESEARCH)
			{
				counter = researchLabCurrMax;
			}
			else if (asStructureStats[inc].type == REF_FACTORY)
			{
				counter = factoriesCurrMax;
			}
			else if (asStructureStats[inc].type == REF_VTOL_FACTORY)
			{
				counter = vtolFactoriesCurrMax;
			}
			else if (asStructureStats[inc].type == REF_POWER_GEN)
			{
				counter = powerGenCurrMax;
			}
			else
			{
				continue;
			}
			counter[0] = asStructureStats[inc].curCount[_selectedPlayer];
			counter[1] = asStructureStats[inc].upgrade[_selectedPlayer].limit;

		}
	}

	debug(LOG_NEVER, "structures: RL %i/%i (%i), F %i/%i (%i), VF %i/%i, PG %i/%i (%i)",
					researchLabCurrMax[0], researchLabCurrMax[1], researchModules,
					factoriesCurrMax[0], factoriesCurrMax[1], factoryModules,
					vtolFactoriesCurrMax[0], vtolFactoriesCurrMax[1],
					powerGenCurrMax[0], powerGenCurrMax[1], powerGenModules);

	//set the list of Structures to build
	for (inc = 0; inc < numStructureStats; inc++)
	{
		//if the structure is flagged as available, add it to the list
		if (apStructTypeLists[_selectedPlayer][inc] == AVAILABLE || (includeRedundantDesigns && apStructTypeLists[_selectedPlayer][inc] == REDUNDANT))
		{
			//check not built the maximum allowed already
			if (asStructureStats[inc].curCount[_selectedPlayer] < asStructureStats[inc].upgrade[_selectedPlayer].limit)
			{
				psBuilding = asStructureStats + inc;

				//don't want corner wall to appear in list
				if (psBuilding->type == REF_WALLCORNER)
				{
					continue;
				}

				// Remove the demolish stat from the list for tutorial
				// tjc 4-dec-98  ...
				if (bInTutorial)
				{
					if (psBuilding->type == REF_DEMOLISH)
					{
						continue;
					}
				}

				//can't build list when offworld
				if (missionIsOffworld())
				{
					if (psBuilding->type == REF_FACTORY ||
					    psBuilding->type == REF_COMMAND_CONTROL ||
					    psBuilding->type == REF_HQ ||
					    psBuilding->type == REF_POWER_GEN ||
					    psBuilding->type == REF_RESOURCE_EXTRACTOR ||
					    psBuilding->type == REF_RESEARCH ||
					    psBuilding->type == REF_CYBORG_FACTORY ||
					    psBuilding->type == REF_VTOL_FACTORY)
					{
						continue;
					}
				}

				if (psBuilding->type == REF_RESEARCH_MODULE)
				{
					//don't add to list if Research Facility not presently built
					//or if all labs already have a module
					if (!researchLabCurrMax[0] || researchModules >= researchLabCurrMax[1])
					{
						continue;
					}
				}
				else if (psBuilding->type == REF_FACTORY_MODULE)
				{
					//don't add to list if Factory not presently built
					//or if all factories already have all possible modules
					if (!factoriesCurrMax[0] || (factoryModules >= (factoriesCurrMax[1] + vtolFactoriesCurrMax[1]) * 2))
					{
						continue;
					}
				}
				else if (psBuilding->type == REF_POWER_MODULE)
				{
					//don't add to list if Power Gen not presently built
					//or if all generators already have a module
					if (!powerGenCurrMax[0] || (powerGenModules >= powerGenCurrMax[1]))
					{
						continue;
					}
				}
				// show only favorites?
				if (showFavorites && !asStructureStats[inc].isFavorite)
				{
					continue;
				}

				debug(LOG_NEVER, "adding %s (%x)", getStatsName(psBuilding), apStructTypeLists[_selectedPlayer][inc]);
				structureList.push_back(psBuilding);
				if (structureList.size() == limit)
				{
					return structureList;
				}
			}
		}
	}
	return structureList;
}


enum STRUCTURE_PACKABILITY
{
	PACKABILITY_EMPTY = 0, PACKABILITY_DEFENSE = 1, PACKABILITY_NORMAL = 2, PACKABILITY_REPAIR = 3
};

static inline bool canPack(STRUCTURE_PACKABILITY a, STRUCTURE_PACKABILITY b)
{
	return (int)a + (int)b <= 3;  // Defense can be put next to anything except repair facilities, normal base structures can't be put next to each other, and anything goes next to empty tiles.
}

static STRUCTURE_PACKABILITY baseStructureTypePackability(STRUCTURE_TYPE type)
{
	switch (type)
	{
	case REF_DEFENSE:
	case REF_WALL:
	case REF_WALLCORNER:
	case REF_GATE:
	case REF_REARM_PAD:
	case REF_MISSILE_SILO:
		return PACKABILITY_DEFENSE;
	default:
		return PACKABILITY_NORMAL;
	case REF_REPAIR_FACILITY:
		return PACKABILITY_REPAIR;
	}
}

static STRUCTURE_PACKABILITY baseObjectPackability(BASE_OBJECT *psObject)
{
	if (psObject == nullptr)
	{
		return PACKABILITY_EMPTY;
	}
	switch (psObject->type)
	{
	case OBJ_STRUCTURE: return baseStructureTypePackability(((STRUCTURE *)psObject)->pStructureType->type);
	case OBJ_FEATURE:   return ((FEATURE *)psObject)->psStats->subType == FEAT_OIL_RESOURCE ? PACKABILITY_NORMAL : PACKABILITY_EMPTY;
	default:            return PACKABILITY_EMPTY;
	}
}

bool isBlueprintTooClose(STRUCTURE_STATS const *stats1, Vector2i pos1, uint16_t dir1, STRUCTURE_STATS const *stats2, Vector2i pos2, uint16_t dir2)
{
	if (stats1 == stats2 && pos1 == pos2 && dir1 == dir2)
	{
		return false;  // Same blueprint, so ignore it.
	}

	bool packable = canPack(baseStructureTypePackability(stats1->type), baseStructureTypePackability(stats2->type));
	int minDist = packable ? 0 : 1;
	StructureBounds b1 = getStructureBounds(stats1, pos1, dir1);
	StructureBounds b2 = getStructureBounds(stats2, pos2, dir2);
	Vector2i delta12 = b2.map - (b1.map + b1.size);
	Vector2i delta21 = b1.map - (b2.map + b2.size);
	int dist = std::max(std::max(delta12.x, delta21.x), std::max(delta12.y, delta21.y));
	return dist < minDist;
}

bool validLocation(BASE_STATS *psStats, Vector2i pos, uint16_t direction, unsigned player, bool bCheckBuildQueue)
{
	ASSERT_OR_RETURN(false, player < MAX_PLAYERS, "player (%u) >= MAX_PLAYERS", player);

	StructureBounds b = getStructureBounds(psStats, pos, direction);

	//make sure we are not too near map edge and not going to go over it
	if (b.map.x < scrollMinX + TOO_NEAR_EDGE || b.map.x + b.size.x > scrollMaxX - TOO_NEAR_EDGE ||
	    b.map.y < scrollMinY + TOO_NEAR_EDGE || b.map.y + b.size.y > scrollMaxY - TOO_NEAR_EDGE)
	{
		return false;
	}

	if (bCheckBuildQueue)
	{
		// cant place on top of a delivery point...
		for (const auto& psFlag : apsFlagPosLists[selectedPlayer])
		{
			ASSERT_OR_RETURN(false, psFlag->coords.x != ~0, "flag has invalid position");
			Vector2i flagTile = map_coord(psFlag->coords.xy());
			if (flagTile.x >= b.map.x && flagTile.x < b.map.x + b.size.x && flagTile.y >= b.map.y && flagTile.y < b.map.y + b.size.y)
			{
				return false;
			}
		}
	}

	STRUCTURE_STATS *psBuilding = castStructureStats(psStats);
	DROID_TEMPLATE *psTemplate = castDroidTemplate(psStats);
	if (psBuilding != nullptr)
	{
		for (int j = 0; j < b.size.y; ++j)
			for (int i = 0; i < b.size.x; ++i)
			{
				// Don't allow building structures (allow delivery points, though) outside visible area in single-player with debug mode off. (Why..?)
				const DebugInputManager& dbgInputManager = gInputManager.debugManager();
				if (!bMultiPlayer && !dbgInputManager.debugMappingsAllowed() && !TEST_TILE_VISIBLE(player, mapTile(b.map.x + i, b.map.y + j)))
				{
					return false;
				}
			}

		switch (psBuilding->type)
		{
		case REF_DEMOLISH:
			break;
		case NUM_DIFF_BUILDINGS:
		case REF_BRIDGE:
			ASSERT(!"invalid structure type", "Bad structure type %u", psBuilding->type);
			break;
		case REF_HQ:
		case REF_FACTORY:
		case REF_LAB:
		case REF_RESEARCH:
		case REF_POWER_GEN:
		case REF_WALL:
		case REF_WALLCORNER:
		case REF_GATE:
		case REF_DEFENSE:
		case REF_REPAIR_FACILITY:
		case REF_COMMAND_CONTROL:
		case REF_CYBORG_FACTORY:
		case REF_VTOL_FACTORY:
		case REF_GENERIC:
		case REF_REARM_PAD:
		case REF_MISSILE_SILO:
		case REF_SAT_UPLINK:
		case REF_LASSAT:
		case REF_FORTRESS:
			{
				/*need to check each tile the structure will sit on is not water*/
				for (int j = 0; j < b.size.y; ++j)
					for (int i = 0; i < b.size.x; ++i)
					{
						MAPTILE const *psTile = mapTile(b.map.x + i, b.map.y + j);
						if ((terrainType(psTile) == TER_WATER) ||
						    (terrainType(psTile) == TER_CLIFFFACE))
						{
							return false;
						}
					}
				//check not within landing zone
				for (int j = 0; j < b.size.y; ++j)
					for (int i = 0; i < b.size.x; ++i)
					{
						if (withinLandingZone(b.map.x + i, b.map.y + j))
						{
							return false;
						}
					}

				//walls/defensive structures can be built along any ground
				if (!(psBuilding->type == REF_REPAIR_FACILITY ||
				      psBuilding->type == REF_DEFENSE ||
				      psBuilding->type == REF_GATE ||
				      psBuilding->type == REF_WALL))
				{
					/*cannot build on ground that is too steep*/
					for (int j = 0; j < b.size.y; ++j)
						for (int i = 0; i < b.size.x; ++i)
						{
							int max, min;
							getTileMaxMin(b.map.x + i, b.map.y + j, &max, &min);
							if (max - min > MAX_INCLINE)
							{
								return false;
							}
						}
				}
				//don't bother checking if already found a problem
				STRUCTURE_PACKABILITY packThis = baseStructureTypePackability(psBuilding->type);

				// skirmish AIs don't build nondefensives next to anything. (route hack)
				if (packThis == PACKABILITY_NORMAL && bMultiPlayer && game.type == LEVEL_TYPE::SKIRMISH && !isHumanPlayer(player))
				{
					packThis = PACKABILITY_REPAIR;
				}

				/* need to check there is one tile between buildings */
				for (int j = -1; j < b.size.y + 1; ++j)
					for (int i = -1; i < b.size.x + 1; ++i)
					{
						//skip the actual area the structure will cover
						if (i < 0 || i >= b.size.x || j < 0 || j >= b.size.y)
						{
							BASE_OBJECT *object = mapTile(b.map.x + i, b.map.y + j)->psObject;
							STRUCTURE *structure = castStructure(object);
							if (structure != nullptr && !structure->visible[player] && !aiCheckAlliances(player, structure->player))
							{
								continue;  // Ignore structures we can't see.
							}

							STRUCTURE_PACKABILITY packObj = baseObjectPackability(object);

							if (!canPack(packThis, packObj))
							{
								return false;
							}
						}
					}
				if (psBuilding->flags & STRUCTURE_CONNECTED)
				{
					bool connection = false;
					for (int j = -1; j < b.size.y + 1; ++j)
					{
						for (int i = -1; i < b.size.x + 1; ++i)
						{
							//skip the actual area the structure will cover
							if (i < 0 || i >= b.size.x || j < 0 || j >= b.size.y)
							{
								STRUCTURE const *psStruct = getTileStructure(b.map.x + i, b.map.y + j);
								if (psStruct != nullptr && psStruct->player == player && psStruct->status == SS_BUILT)
								{
									connection = true;
									break;
								}
							}
						}
					}
					if (!connection)
					{
						return false; // needed to be connected to another building
					}
				}

				/*need to check each tile the structure will sit on*/
				for (int j = 0; j < b.size.y; ++j)
					for (int i = 0; i < b.size.x; ++i)
					{
						MAPTILE const *psTile = mapTile(b.map.x + i, b.map.y + j);
						if (TileIsKnownOccupied(psTile, player))
						{
							if (TileHasWall(psTile) && (psBuilding->type == REF_DEFENSE || psBuilding->type == REF_GATE || psBuilding->type == REF_WALL))
							{
								STRUCTURE const *psStruct = getTileStructure(b.map.x + i, b.map.y + j);
								if (psStruct != nullptr && psStruct->player != player)
								{
									return false;
								}
							}
							else
							{
								return false;
							}
						}
					}
				break;
			}
		case REF_FACTORY_MODULE:
			if (TileHasStructure(worldTile(pos)))
			{
				STRUCTURE const *psStruct = getTileStructure(map_coord(pos.x), map_coord(pos.y));
				if (psStruct && (psStruct->pStructureType->type == REF_FACTORY ||
				                 psStruct->pStructureType->type == REF_VTOL_FACTORY)
					&& psStruct->status == SS_BUILT && aiCheckAlliances(player, psStruct->player)
					&& nextModuleToBuild(psStruct, -1) > 0)
				{
					break;
				}
			}
			return false;
		case REF_RESEARCH_MODULE:
			if (TileHasStructure(worldTile(pos)))
			{
				STRUCTURE const *psStruct = getTileStructure(map_coord(pos.x), map_coord(pos.y));
				if (psStruct && psStruct->pStructureType->type == REF_RESEARCH
					&& psStruct->status == SS_BUILT
					&& aiCheckAlliances(player, psStruct->player)
					&& nextModuleToBuild(psStruct, -1) > 0)
				{
					break;
				}
			}
			return false;
		case REF_POWER_MODULE:
			if (TileHasStructure(worldTile(pos)))
			{
				STRUCTURE const *psStruct = getTileStructure(map_coord(pos.x), map_coord(pos.y));
				if (psStruct && psStruct->pStructureType->type == REF_POWER_GEN
					&& psStruct->status == SS_BUILT
					&& aiCheckAlliances(player, psStruct->player)
					&& nextModuleToBuild(psStruct, -1) > 0)
				{
					break;
				}
			}
			return false;
		case REF_RESOURCE_EXTRACTOR:
			if (TileHasFeature(worldTile(pos)))
			{
				FEATURE const *psFeat = getTileFeature(map_coord(pos.x), map_coord(pos.y));
				if (psFeat && psFeat->psStats->subType == FEAT_OIL_RESOURCE)
				{
					break;
				}
			}
			return false;
		}
		//if setting up a build queue need to check against future sites as well - AB 4/5/99
		if (ctrlShiftDown() && player == selectedPlayer && bCheckBuildQueue &&
		    anyBlueprintTooClose(psBuilding, pos, direction))
		{
			return false;
		}
	}
	else if (psTemplate != nullptr)
	{
		PROPULSION_STATS *psPropStats = psTemplate->getPropulsionStats();

		if (fpathBlockingTile(b.map.x, b.map.y, psPropStats->propulsionType))
		{
			return false;
		}
	}
	else
	{
		// not positioning a structure or droid, ie positioning a feature
		if (fpathBlockingTile(b.map.x, b.map.y, PROPULSION_TYPE_WHEELED))
		{
			return false;
		}
	}

	return true;
}


//remove a structure from the map
static void removeStructFromMap(STRUCTURE *psStruct)
{
	auxStructureNonblocking(psStruct);

	/* set tiles drawing */
	StructureBounds b = getStructureBounds(psStruct);
	for (int j = 0; j < b.size.y; ++j)
	{
		for (int i = 0; i < b.size.x; ++i)
		{
			MAPTILE *psTile = mapTile(b.map.x + i, b.map.y + j);
			if (psTile->psObject == psStruct)
			{
				psTile->psObject = nullptr;
				auxClearBlocking(b.map.x + i, b.map.y + j, AIR_BLOCKED);
			}
		}
	}
}

// remove a structure from a game without any visible effects
// bDestroy = true if the object is to be destroyed
// (for example used to change the type of wall at a location)
bool removeStruct(STRUCTURE *psDel, bool bDestroy)
{
	bool		resourceFound = false;
	FLAG_POSITION	*psAssemblyPoint = nullptr;

	ASSERT_OR_RETURN(false, psDel != nullptr, "Invalid structure pointer");

	int prevResearchState = intGetResearchState();

	if (bDestroy)
	{
		removeStructFromMap(psDel);
	}

	if (bDestroy)
	{
		//if the structure is a resource extractor, need to put the resource back in the map
		/*ONLY IF ANY POWER LEFT - HACK HACK HACK!!!! OIL POOLS NEED TO KNOW
		HOW MUCH IS THERE && NOT RES EXTRACTORS */
		if (psDel->pStructureType->type == REF_RESOURCE_EXTRACTOR)
		{
			FEATURE *psOil = buildFeature(oilResFeature, psDel->pos.x, psDel->pos.y, false);
			memcpy(psOil->seenThisTick, psDel->visible, sizeof(psOil->seenThisTick));
			resourceFound = true;
		}
	}

	if (psDel->pStructureType->type == REF_RESOURCE_EXTRACTOR)
	{
		//tell associated Power Gen
		releaseResExtractor(psDel);
		//tell keybind that this is going away (to prevent dangling pointer in kf_JumpToResourceExtractor)
		keybindInformResourceExtractorRemoved(psDel);
	}

	if (psDel->pStructureType->type == REF_POWER_GEN)
	{
		//tell associated Res Extractors
		releasePowerGen(psDel);
	}

	//check for a research topic currently under way
	if (psDel->pStructureType->type == REF_RESEARCH)
	{
		if (psDel->pFunctionality->researchFacility.psSubject)
		{
			//cancel the topic
			cancelResearch(psDel, ModeImmediate);
		}
	}

	if (psDel->pStructureType->type == REF_REPAIR_FACILITY)
	{
		if (psDel->pFunctionality && psDel->pFunctionality->repairFacility.state == RepairState::Repairing)
		{
			if (psDel->pFunctionality->repairFacility.psObj != nullptr)
			{
				droidRepairStopped(castDroid(psDel->pFunctionality->repairFacility.psObj), psDel);
			}
		}
	}

	//subtract one from the structLimits list so can build another - don't allow to go less than zero!
	if (asStructureStats[psDel->pStructureType - asStructureStats].curCount[psDel->player])
	{
		asStructureStats[psDel->pStructureType - asStructureStats].curCount[psDel->player]--;
	}

	//if it is a factory - need to reset the factoryNumFlag
	if (psDel->isFactory())
	{
		FACTORY *psFactory = &psDel->pFunctionality->factory;

		//need to initialise the production run as well
		cancelProduction(psDel, ModeImmediate);

		psAssemblyPoint = psFactory->psAssemblyPoint;
	}
	else if (psDel->pStructureType->type == REF_REPAIR_FACILITY)
	{
		psAssemblyPoint = psDel->pFunctionality->repairFacility.psDeliveryPoint;
	}

	if (psAssemblyPoint != nullptr)
	{
		if (psAssemblyPoint->factoryInc < factoryNumFlag[psDel->player][psAssemblyPoint->factoryType].size())
		{
			factoryNumFlag[psDel->player][psAssemblyPoint->factoryType][psAssemblyPoint->factoryInc] = false;
		}

		//need to cancel the repositioning of the DP if selectedPlayer and currently moving
		if (psDel->player == selectedPlayer && psAssemblyPoint->selected)
		{
			cancelDeliveryRepos();
		}
	}

	if (bDestroy)
	{
		debug(LOG_DEATH, "Killing off %s id %d (%p)", objInfo(psDel), psDel->id, static_cast<void *>(psDel));
		killStruct(psDel);
	}

	if (psDel->player == selectedPlayer)
	{
		intRefreshScreen();
	}

	delPowerRequest(psDel);

	intNotifyResearchButton(prevResearchState);

	return resourceFound;
}

/* Remove a structure */
bool destroyStruct(STRUCTURE *psDel, unsigned impactTime)
{
	UDWORD			widthScatter, breadthScatter, heightScatter;

	const unsigned          burnDurationWall    =  1000;
	const unsigned          burnDurationOilWell = 60000;
	const unsigned          burnDurationOther   = 10000;

	CHECK_STRUCTURE(psDel);
	ASSERT(gameTime - deltaGameTime <= impactTime, "Expected %u <= %u, gameTime = %u, bad impactTime", gameTime - deltaGameTime, impactTime, gameTime);

	/* Firstly, are we dealing with a wall section */
	const STRUCTURE_TYPE type = psDel->pStructureType->type;
	const bool bMinor = type == REF_WALL || type == REF_WALLCORNER;
	const bool bDerrick = type == REF_RESOURCE_EXTRACTOR;
	const bool bPowerGen = type == REF_POWER_GEN;
	unsigned burnDuration = bMinor ? burnDurationWall : bDerrick ? burnDurationOilWell : burnDurationOther;
	if (psDel->status == SS_BEING_BUILT)
	{
		burnDuration = static_cast<unsigned>(burnDuration * structureCompletionProgress(*psDel));
	}

	/* Only add if visible */
	if (psDel->visibleForLocalDisplay())
	{
		Vector3i pos;
		int      i;

		/* Set off some explosions, but not for walls */
		/* First Explosions */
		widthScatter = TILE_UNITS;
		breadthScatter = TILE_UNITS;
		heightScatter = TILE_UNITS;
		for (i = 0; i < (bMinor ? 2 : 4); ++i)  // only add two for walls - gets crazy otherwise
		{
			pos.x = psDel->pos.x + widthScatter - rand() % (2 * widthScatter);
			pos.z = psDel->pos.y + breadthScatter - rand() % (2 * breadthScatter);
			pos.y = psDel->pos.z + 32 + rand() % heightScatter;
			addEffect(&pos, EFFECT_EXPLOSION, EXPLOSION_TYPE_MEDIUM, false, nullptr, 0, impactTime);
		}

		/* Get coordinates for everybody! */
		pos.x = psDel->pos.x;
		pos.z = psDel->pos.y;  // z = y [sic] intentional
		pos.y = map_Height(pos.x, pos.z);

		// Set off a fire, provide dimensions for the fire
		if (bMinor)
		{
			effectGiveAuxVar(world_coord(psDel->pStructureType->baseWidth) / 4);
		}
		else
		{
			effectGiveAuxVar(world_coord(psDel->pStructureType->baseWidth) / 3);
		}
		/* Give a duration */
		effectGiveAuxVarSec(burnDuration);
		if (bDerrick)  // oil resources
		{
			/* Oil resources burn AND puff out smoke AND for longer*/
			addEffect(&pos, EFFECT_FIRE, FIRE_TYPE_SMOKY, false, nullptr, 0, impactTime);
		}
		else  // everything else
		{
			addEffect(&pos, EFFECT_FIRE, FIRE_TYPE_LOCALISED, false, nullptr, 0, impactTime);
		}

		/* Power stations have their own destruction sequence */
		if (bPowerGen)
		{
			addEffect(&pos, EFFECT_DESTRUCTION, DESTRUCTION_TYPE_POWER_STATION, false, nullptr, 0, impactTime);
			pos.y += SHOCK_WAVE_HEIGHT;
			addEffect(&pos, EFFECT_EXPLOSION, EXPLOSION_TYPE_SHOCKWAVE, false, nullptr, 0, impactTime);
		}
		/* As do wall sections */
		else if (bMinor)
		{
			addEffect(&pos, EFFECT_DESTRUCTION, DESTRUCTION_TYPE_WALL_SECTION, false, nullptr, 0, impactTime);
		}
		else // and everything else goes here.....
		{
			addEffect(&pos, EFFECT_DESTRUCTION, DESTRUCTION_TYPE_STRUCTURE, false, nullptr, 0, impactTime);
		}

		// shake the screen if we're near enough and it is explosive in nature
		if (clipXY(pos.x, pos.z))
		{
			switch (type)
			{
			// These are the types that would cause a explosive outcome if destoryed
			case REF_HQ:
			case REF_POWER_GEN:
			case REF_MISSILE_SILO: // for campaign
				shakeStart(1500);
				break;
			case REF_COMMAND_CONTROL:
			case REF_VTOL_FACTORY:
			case REF_CYBORG_FACTORY:
			case REF_FACTORY:
				shakeStart(750);
				break;
			case REF_RESOURCE_EXTRACTOR:
				shakeStart(400);
				break;
			default:
				break;
			}
		}

		// and add a sound effect
		audio_PlayStaticTrack(psDel->pos.x, psDel->pos.y, ID_SOUND_EXPLOSION);
	}

	// Actually set the tiles on fire - even if the effect is not visible.
	tileSetFire(psDel->pos.x, psDel->pos.y, burnDuration);

	const bool resourceFound = removeStruct(psDel, true);
	psDel->died = impactTime;

	// Leave burn marks in the ground where building once stood
	if (psDel->visibleForLocalDisplay() && !resourceFound && !bMinor)
	{
		StructureBounds b = getStructureBounds(psDel);
		for (int breadth = 0; breadth < b.size.y; ++breadth)
		{
			for (int width = 0; width < b.size.x; ++width)
			{
				MAPTILE *psTile = mapTile(b.map.x + width, b.map.y + breadth);
				if (TEST_TILE_VISIBLE_TO_SELECTEDPLAYER(psTile))
				{
					psTile->illumination /= 2;
					psTile->ambientOcclusion /= 2;
				}
			}
		}
	}

	if (bMultiPlayer)
	{
		technologyGiveAway(psDel);  // Drop an artifact, if applicable.
	}

	// updates score stats only if not wall
	if (!bMinor)
	{
		if (psDel->player == selectedPlayer)
		{
			scoreUpdateVar(WD_STR_LOST);
		}
		// only counts as a kill if structure doesn't belong to our ally
		else if (selectedPlayer < MAX_PLAYERS && !aiCheckAlliances(psDel->player, selectedPlayer))
		{
			scoreUpdateVar(WD_STR_KILLED);
		}
	}

	return true;
}


/* gets a structure stat from its name - relies on the name being unique (or it will
return the first one it finds!! */
int32_t getStructStatFromName(const WzString &name)
{
	STRUCTURE_STATS *psStat = getStructStatsFromName(name);
	if (psStat)
	{
		return psStat->index;
	}
	return -1;
}

STRUCTURE_STATS *getStructStatsFromName(const WzString &name)
{
	STRUCTURE_STATS *psStat = nullptr;
	auto it = lookupStructStatPtr.find(name);
	if (it != lookupStructStatPtr.end())
	{
		psStat = it->second;
	}
	return psStat;
}

/*check to see if the structure is 'doing' anything  - return true if idle*/
bool  STRUCTURE::isIdle() const
{
	BASE_STATS		*pSubject = nullptr;

	CHECK_STRUCTURE(this);

	if (pFunctionality == nullptr)
	{
		return true;
	}

	//determine the Subject
	switch (pStructureType->type)
	{
	case REF_RESEARCH:
		{
			pSubject = pFunctionality->researchFacility.psSubject;
			break;
		}
	case REF_FACTORY:
	case REF_CYBORG_FACTORY:
	case REF_VTOL_FACTORY:
		{
			pSubject = pFunctionality->factory.psSubject;
			break;
		}
	default:
		break;
	}

	if (pSubject != nullptr)
	{
		return false;
	}

	return true;
}


/*checks to see if a specific structure type exists -as opposed to a structure
stat type*/
bool checkSpecificStructExists(UDWORD structInc, UDWORD player)
{
	ASSERT_OR_RETURN(false, structInc < numStructureStats, "Invalid structure inc");

	for (const STRUCTURE *psStructure : apsStructLists[player])
	{
		if (psStructure->status == SS_BUILT)
		{
			if (psStructure->pStructureType->ref - STAT_STRUCTURE == structInc)
			{
				return true;
			}
		}
	}
	return false;
}


/*finds a suitable position for the assembly point based on one passed in*/
void findAssemblyPointPosition(UDWORD *pX, UDWORD *pY, UDWORD player)
{
	//set up a dummy stat pointer
	STRUCTURE_STATS     sStats;
	UDWORD              passes = 0;
	SDWORD	            i, j, startX, endX, startY, endY;

	sStats.ref = 0;
	sStats.baseWidth = 1;
	sStats.baseBreadth = 1;

	/* Initial box dimensions and set iteration count to zero */
	startX = endX = *pX;	startY = endY = *pY;
	passes = 0;

	//if the value passed in is not a valid location - find one!
	if (!validLocation(&sStats, world_coord(Vector2i(*pX, *pY)), 0, player, false))
	{
		/* Keep going until we get a tile or we exceed distance */
		while (passes < LOOK_FOR_EMPTY_TILE)
		{
			/* Process whole box */
			for (i = startX; i <= endX; i++)
			{
				for (j = startY; j <= endY; j++)
				{
					/* Test only perimeter as internal tested previous iteration */
					if (i == startX || i == endX || j == startY || j == endY)
					{
						/* Good enough? */
						if (validLocation(&sStats, world_coord(Vector2i(i, j)), 0, player, false))
						{
							/* Set exit conditions and get out NOW */
							*pX = i;
							*pY = j;
							//jump out of the loop
							return;
						}
					}
				}
			}
			/* Expand the box out in all directions - off map handled by validLocation() */
			startX--; startY--;	endX++;	endY++;	passes++;
		}
	}
	else
	{
		//the first location was valid
		return;
	}
	/* If we got this far, then we failed - passed in values will be unchanged */
	ASSERT(!"unable to find a valid location", "unable to find a valid location!");
}


/*sets the point new droids go to - x/y in world coords for a Factory
bCheck is set to true for initial placement of the Assembly Point*/
void setAssemblyPoint(FLAG_POSITION *psAssemblyPoint, UDWORD x, UDWORD y,
                      UDWORD player, bool bCheck)
{
	ASSERT_OR_RETURN(, psAssemblyPoint != nullptr, "invalid AssemblyPoint pointer");

	//check its valid
	x = map_coord(x);
	y = map_coord(y);
	if (bCheck)
	{
		findAssemblyPointPosition(&x, &y, player);
	}
	//add half a tile so the centre is in the middle of the tile
	x = world_coord(x) + TILE_UNITS / 2;
	y = world_coord(y) + TILE_UNITS / 2;

	psAssemblyPoint->coords.x = x;
	psAssemblyPoint->coords.y = y;

	// Deliv Point sits at the height of the tile it's centre is on + arbitrary amount!
	psAssemblyPoint->coords.z = map_Height(x, y) + ASSEMBLY_POINT_Z_PADDING;
}


/*sets the factory Inc for the Assembly Point*/
void setFlagPositionInc(FUNCTIONALITY *pFunctionality, UDWORD player, UBYTE factoryType)
{
	ASSERT_OR_RETURN(, player < MAX_PLAYERS, "invalid player number");

	//find the first vacant slot
	unsigned inc = std::find(factoryNumFlag[player][factoryType].begin(), factoryNumFlag[player][factoryType].end(), false) - factoryNumFlag[player][factoryType].begin();
	if (inc == factoryNumFlag[player][factoryType].size())
	{
		// first time init for this factory flag slot, set it to false
		factoryNumFlag[player][factoryType].push_back(false);
	}

	if (factoryType == REPAIR_FLAG)
	{
		// this is a special case, there are no flag numbers for this "factory"
		REPAIR_FACILITY *psRepair = &pFunctionality->repairFacility;
		psRepair->psDeliveryPoint->factoryInc = 0;
		psRepair->psDeliveryPoint->factoryType = factoryType;
		// factoryNumFlag[player][factoryType][inc] = true;
	}
	else
	{
		FACTORY *psFactory = &pFunctionality->factory;
		psFactory->psAssemblyPoint->factoryInc = inc;
		psFactory->psAssemblyPoint->factoryType = factoryType;
		factoryNumFlag[player][factoryType][inc] = true;
	}
}

STRUCTURE_STATS *structGetDemolishStat()
{
	ASSERT_OR_RETURN(nullptr, g_psStatDestroyStruct != nullptr , "Demolish stat not initialised");
	return g_psStatDestroyStruct;
}


/*sets the flag to indicate a SatUplink Exists - so draw everything!*/
void setSatUplinkExists(bool state, UDWORD player)
{
	satUplinkExists[player] = (UBYTE)state;
	if (state)
	{
		satuplinkbits |= (1 << player);
	}
	else
	{
		satuplinkbits &= ~(1 << player);
	}
}


/*returns the status of the flag*/
bool getSatUplinkExists(UDWORD player)
{
	return satUplinkExists[player];
}


/*sets the flag to indicate a Las Sat Exists - ONLY EVER WANT ONE*/
void setLasSatExists(bool state, UDWORD player)
{
	lasSatExists[player] = (UBYTE)state;
}


/* calculate muzzle base location in 3d world */
bool calcStructureMuzzleBaseLocation(const STRUCTURE *psStructure, Vector3i *muzzle, int weapon_slot)
{
	const iIMDBaseShape *psShape = psStructure->pStructureType->pIMD[0];

	CHECK_STRUCTURE(psStructure);

	if (psShape && !psShape->connectors.empty())
	{
		Vector3i barrel(0, 0, 0);

		Affine3F af;

		af.Trans(psStructure->pos.x, -psStructure->pos.z, psStructure->pos.y);

		//matrix = the center of droid
		af.RotY(psStructure->rot.direction);
		af.RotX(psStructure->rot.pitch);
		af.RotZ(-psStructure->rot.roll);
		af.Trans(psShape->connectors[weapon_slot].x, -psShape->connectors[weapon_slot].z,
		         -psShape->connectors[weapon_slot].y);//note y and z flipped


		*muzzle = (af * barrel).xzy();
		muzzle->z = -muzzle->z;
	}
	else
	{
		*muzzle = psStructure->pos + Vector3i(0, 0, psStructure->sDisplay.imd->max.y);
	}

	return true;
}

/* calculate muzzle tip location in 3d world */
bool calcStructureMuzzleLocation(const STRUCTURE *psStructure, Vector3i *muzzle, int weapon_slot)
{
	const iIMDBaseShape *psShape = psStructure->pStructureType->pIMD[0];

	CHECK_STRUCTURE(psStructure);

	if (psShape && !psShape->connectors.empty())
	{
		Vector3i barrel(0, 0, 0);
		unsigned int nWeaponStat = psStructure->asWeaps[weapon_slot].nStat;
		const iIMDBaseShape *psWeaponImd = nullptr, *psMountImd = nullptr;

		if (nWeaponStat)
		{
			psWeaponImd = asWeaponStats[nWeaponStat].pIMD;
			psMountImd = asWeaponStats[nWeaponStat].pMountGraphic;
		}

		Affine3F af;

		af.Trans(psStructure->pos.x, -psStructure->pos.z, psStructure->pos.y);

		//matrix = the center of droid
		af.RotY(psStructure->rot.direction);
		af.RotX(psStructure->rot.pitch);
		af.RotZ(-psStructure->rot.roll);
		af.Trans(psShape->connectors[weapon_slot].x, -psShape->connectors[weapon_slot].z,
		         -psShape->connectors[weapon_slot].y);//note y and z flipped

		//matrix = the weapon[slot] mount on the body
		af.RotY(psStructure->asWeaps[weapon_slot].rot.direction);  // +ve anticlockwise

		// process turret mount
		if (psMountImd && !psMountImd->connectors.empty())
		{
			af.Trans(psMountImd->connectors[0].x, -psMountImd->connectors[0].z, -psMountImd->connectors[0].y);
		}

		//matrix = the turret connector for the gun
		af.RotX(psStructure->asWeaps[weapon_slot].rot.pitch);      // +ve up

		//process the gun
		if (psWeaponImd && !psWeaponImd->connectors.empty())
		{
			unsigned int connector_num = 0;

			// which barrel is firing if model have multiple muzzle connectors?
			if (psStructure->asWeaps[weapon_slot].shotsFired && (psWeaponImd->connectors.size() > 1))
			{
				// shoot first, draw later - substract one shot to get correct results
				connector_num = (psStructure->asWeaps[weapon_slot].shotsFired - 1) % (static_cast<uint32_t>(psWeaponImd->connectors.size()));
			}

			barrel = Vector3i(psWeaponImd->connectors[connector_num].x, -psWeaponImd->connectors[connector_num].z, -psWeaponImd->connectors[connector_num].y);
		}

		*muzzle = (af * barrel).xzy();
		muzzle->z = -muzzle->z;
	}
	else
	{
		*muzzle = psStructure->pos + Vector3i(0, 0, 0 + psStructure->sDisplay.imd->max.y);
	}

	return true;
}


/*Looks through the list of structures to see if there are any inactive
resource extractors*/
void checkForResExtractors(STRUCTURE *psBuilding)
{
	ASSERT_OR_RETURN(, psBuilding->pStructureType->type == REF_POWER_GEN, "invalid structure type");

	// Find derricks, sorted by unused first, then ones attached to power generators without modules.
	typedef std::pair<int, STRUCTURE *> Derrick;
	typedef std::vector<Derrick> Derricks;
	Derricks derricks;
	derricks.reserve(NUM_POWER_MODULES + 1);
	for (STRUCTURE *currExtractor : apsExtractorLists[psBuilding->player])
	{
		RES_EXTRACTOR *resExtractor = &currExtractor->pFunctionality->resourceExtractor;

		if (currExtractor->status != SS_BUILT)
		{
			continue;  // Derrick not complete.
		}
		int priority = resExtractor->psPowerGen != nullptr ? resExtractor->psPowerGen->capacity : -1;
		//auto d = std::find_if(derricks.begin(), derricks.end(), [priority](Derrick const &v) { return v.first <= priority; });
		Derricks::iterator d = derricks.begin();
		while (d != derricks.end() && d->first <= priority)
		{
			++d;
		}
		derricks.insert(d, Derrick(priority, currExtractor));
		derricks.resize(std::min<unsigned>(derricks.size(), NUM_POWER_MODULES));  // No point remembering more derricks than this.
	}

	// Attach derricks.
	Derricks::const_iterator d = derricks.begin();
	for (int i = 0; i < NUM_POWER_MODULES; ++i)
	{
		POWER_GEN *powerGen = &psBuilding->pFunctionality->powerGenerator;
		if (powerGen->apResExtractors[i] != nullptr)
		{
			continue;  // Slot full.
		}

		int priority = psBuilding->capacity;
		if (d == derricks.end() || d->first >= priority)
		{
			continue;  // No more derricks to transfer to this power generator.
		}

		STRUCTURE *derrick = d->second;
		RES_EXTRACTOR *resExtractor = &derrick->pFunctionality->resourceExtractor;
		if (resExtractor->psPowerGen != nullptr)
		{
			informPowerGen(derrick);  // Remove the derrick from the previous power generator.
		}
		// Assign the derrick to the power generator.
		powerGen->apResExtractors[i] = derrick;
		resExtractor->psPowerGen = psBuilding;

		++d;
	}
}

uint16_t countPlayerUnusedDerricks()
{
	uint16_t total = 0;

	if (selectedPlayer >= MAX_PLAYERS) { return 0; }

	for (const STRUCTURE *psStruct : apsExtractorLists[selectedPlayer])
	{
		if (psStruct->status == SS_BUILT && psStruct->pStructureType->type == REF_RESOURCE_EXTRACTOR)
		{
			if (!psStruct->pFunctionality->resourceExtractor.psPowerGen) {
				total++;
			}
		}
	}

	return total;
}

/*Looks through the list of structures to see if there are any Power Gens
with available slots for the new Res Ext*/
void checkForPowerGen(STRUCTURE *psBuilding)
{
	ASSERT_OR_RETURN(, psBuilding->pStructureType->type == REF_RESOURCE_EXTRACTOR, "invalid structure type");

	RES_EXTRACTOR *psRE = &psBuilding->pFunctionality->resourceExtractor;
	if (psRE->psPowerGen != nullptr)
	{
		return;
	}

	// Find a power generator, if possible with a power module.
	STRUCTURE *bestPowerGen = nullptr;
	int bestSlot = 0;
	for (STRUCTURE *psCurr : apsStructLists[psBuilding->player])
	{
		if (psCurr->pStructureType->type == REF_POWER_GEN && psCurr->status == SS_BUILT)
		{
			if (bestPowerGen != nullptr && bestPowerGen->capacity >= psCurr->capacity)
			{
				continue;  // Power generator not better.
			}

			POWER_GEN *psPG = &psCurr->pFunctionality->powerGenerator;
			for (int i = 0; i < NUM_POWER_MODULES; ++i)
			{
				if (psPG->apResExtractors[i] == nullptr)
				{
					bestPowerGen = psCurr;
					bestSlot = i;
					break;
				}
			}
		}
	}

	if (bestPowerGen != nullptr)
	{
		// Attach the derrick to the power generator.
		POWER_GEN *psPG = &bestPowerGen->pFunctionality->powerGenerator;
		psPG->apResExtractors[bestSlot] = psBuilding;
		psRE->psPowerGen = bestPowerGen;
	}
}


/*initialise the slot the Resource Extractor filled in the owning Power Gen*/
void informPowerGen(STRUCTURE *psStruct)
{
	UDWORD		i;
	POWER_GEN	*psPowerGen;

	if (psStruct->pStructureType->type != REF_RESOURCE_EXTRACTOR)
	{
		ASSERT(!"invalid structure type", "invalid structure type");
		return;
	}

	//get the owning power generator
	psPowerGen = &psStruct->pFunctionality->resourceExtractor.psPowerGen->pFunctionality->powerGenerator;
	if (psPowerGen)
	{
		for (i = 0; i < NUM_POWER_MODULES; i++)
		{
			if (psPowerGen->apResExtractors[i] == psStruct)
			{
				//initialise the 'slot'
				psPowerGen->apResExtractors[i] = nullptr;
				break;
			}
		}
	}
}


/*called when a Res extractor is destroyed or runs out of power or is disconnected
adjusts the owning Power Gen so that it can link to a different Res Extractor if one
is available*/
void releaseResExtractor(STRUCTURE *psRelease)
{
	if (psRelease->pStructureType->type != REF_RESOURCE_EXTRACTOR)
	{
		ASSERT(!"invalid structure type", "Invalid structure type");
		return;
	}

	//tell associated Power Gen
	if (psRelease->pFunctionality->resourceExtractor.psPowerGen)
	{
		informPowerGen(psRelease);
	}

	psRelease->pFunctionality->resourceExtractor.psPowerGen = nullptr;

	//there may be spare resource extractors
	for (STRUCTURE* psCurr : apsExtractorLists[psRelease->player])
	{
		//check not connected and power left and built!
		if (psCurr != psRelease && psCurr->pFunctionality->resourceExtractor.psPowerGen == nullptr && psCurr->status == SS_BUILT)
		{
			checkForPowerGen(psCurr);
		}
	}
}


/*called when a Power Gen is destroyed or is disconnected
adjusts the associated Res Extractors so that they can link to different Power
Gens if any are available*/
void releasePowerGen(STRUCTURE *psRelease)
{
	POWER_GEN	*psPowerGen;
	UDWORD		i;

	if (psRelease->pStructureType->type != REF_POWER_GEN)
	{
		ASSERT(!"invalid structure type", "Invalid structure type");
		return;
	}

	psPowerGen = &psRelease->pFunctionality->powerGenerator;
	//go through list of res extractors, setting them to inactive
	for (i = 0; i < NUM_POWER_MODULES; i++)
	{
		if (psPowerGen->apResExtractors[i])
		{
			psPowerGen->apResExtractors[i]->pFunctionality->resourceExtractor.psPowerGen = nullptr;
			psPowerGen->apResExtractors[i] = nullptr;
		}
	}
	//may have a power gen with spare capacity
	for (STRUCTURE* psCurr : apsStructLists[psRelease->player])
	{
		if (psCurr->pStructureType->type == REF_POWER_GEN &&
		    psCurr != psRelease && psCurr->status == SS_BUILT)
		{
			checkForResExtractors(psCurr);
		}
	}
}


/*this is called whenever a structure has finished building*/
void buildingComplete(STRUCTURE *psBuilding)
{
	CHECK_STRUCTURE(psBuilding);

	int prevState = 0;
	if (psBuilding->pStructureType->type == REF_RESEARCH)
	{
		prevState = intGetResearchState();
	}

	psBuilding->currentBuildPts = structureBuildPointsToCompletion(*psBuilding);
	psBuilding->status = SS_BUILT;

	visTilesUpdate(psBuilding);

	if (psBuilding->prebuiltImd != nullptr)
	{
		// We finished building a module, now use the combined IMD.
		std::vector<iIMDBaseShape *> &IMDs = psBuilding->pStructureType->pIMD;
		int imdIndex = std::min<int>(numStructureModules(psBuilding) * 2, IMDs.size() - 1); // *2 because even-numbered IMDs are structures, odd-numbered IMDs are just the modules.
		psBuilding->prebuiltImd = nullptr;
		psBuilding->sDisplay.imd = IMDs[imdIndex];
	}

	switch (psBuilding->pStructureType->type)
	{
	case REF_POWER_GEN:
		checkForResExtractors(psBuilding);
		break;
	case REF_RESOURCE_EXTRACTOR:
		checkForPowerGen(psBuilding);
		break;
	case REF_RESEARCH:
		//this deals with research facilities that are upgraded whilst mid-research
		releaseResearch(psBuilding, ModeImmediate);
		intNotifyResearchButton(prevState);
		break;
	case REF_FACTORY:
	case REF_CYBORG_FACTORY:
	case REF_VTOL_FACTORY:
		//this deals with factories that are upgraded whilst mid-production
		releaseProduction(psBuilding, ModeImmediate);
		break;
	case REF_SAT_UPLINK:
		revealAll(psBuilding->player);
		break;
	case REF_GATE:
		auxStructureNonblocking(psBuilding);  // Clear outdated flags.
		auxStructureClosedGate(psBuilding);  // Don't block for the sake of allied pathfinding.
		break;
	default:
		//do nothing
		break;
	}
}


/*for a given structure, return a pointer to its module stat */
STRUCTURE_STATS *getModuleStat(const STRUCTURE *psStruct)
{
	ASSERT_OR_RETURN(nullptr, psStruct != nullptr, "Invalid structure pointer");

	switch (psStruct->pStructureType->type)
	{
	case REF_POWER_GEN:
		return &asStructureStats[powerModuleStat];
	case REF_FACTORY:
	case REF_VTOL_FACTORY:
		return &asStructureStats[factoryModuleStat];
	case REF_RESEARCH:
		return &asStructureStats[researchModuleStat];
	default:
		//no other structures can have modules attached
		return nullptr;
	}
}

/**
 * Count the artillery and VTOL droids assigned to a structure.
 */
static unsigned int countAssignedDroids(const STRUCTURE *psStructure)
{
	unsigned int num;

	CHECK_STRUCTURE(psStructure);

	// For non-debug builds
	if (psStructure == nullptr)
	{
		return 0;
	}

	if (selectedPlayer >= MAX_PLAYERS)
	{
		return 0;
	}

	num = 0;
	for (const DROID* psCurr : apsDroidLists[selectedPlayer])
	{
		if (psCurr->order.psObj
		    && psCurr->order.psObj->id == psStructure->id
		    && psCurr->player == psStructure->player)
		{
			const MOVEMENT_MODEL weapontype = psCurr->getWeaponStats(0)->movementModel;

			if (weapontype == MM_INDIRECT
			    || weapontype == MM_HOMINGINDIRECT
			    || psCurr->isVtol())
			{
				num++;
			}
		}
	}

	return num;
}

//print some info at the top of the screen dependent on the structure
void printStructureInfo(STRUCTURE *psStructure)
{
	unsigned int numConnected;
	POWER_GEN	*psPowerGen;

	ASSERT_OR_RETURN(, psStructure != nullptr, "Invalid Structure pointer");

	if (isBlueprint(psStructure))
	{
		return;  // Don't print anything about imaginary structures. Would crash, anyway.
	}

	const DebugInputManager& dbgInputManager = gInputManager.debugManager();
	switch (psStructure->pStructureType->type)
	{
	case REF_HQ:
		{
			unsigned int assigned_droids = countAssignedDroids(psStructure);
			console(ngettext("%s - %u Unit assigned - Hitpoints %d/%d", "%s - %u Units assigned - Hitpoints %d/%d", assigned_droids),
					getLocalizedStatsName(psStructure->pStructureType), assigned_droids, psStructure->body, psStructure->structureBody());
			if (dbgInputManager.debugMappingsAllowed())
			{
				// TRANSLATORS: A debug output string (user-visible if debug mode is enabled)
				console(_("ID %d - sensor range %d - ECM %d"), psStructure->id, structSensorRange(psStructure), structJammerPower(psStructure));
			}
			break;
		}
	case REF_DEFENSE:
		if (psStructure->pStructureType->pSensor != nullptr
		    && (psStructure->pStructureType->pSensor->type == STANDARD_SENSOR
		        || psStructure->pStructureType->pSensor->type == INDIRECT_CB_SENSOR
		        || psStructure->pStructureType->pSensor->type == VTOL_INTERCEPT_SENSOR
		        || psStructure->pStructureType->pSensor->type == VTOL_CB_SENSOR
		        || psStructure->pStructureType->pSensor->type == SUPER_SENSOR
		        || psStructure->pStructureType->pSensor->type == RADAR_DETECTOR_SENSOR)
		    && psStructure->pStructureType->pSensor->location == LOC_TURRET)
		{
			unsigned int assigned_droids = countAssignedDroids(psStructure);
			console(ngettext("%s - %u Unit assigned - Damage %d/%d", "%s - %u Units assigned - Hitpoints %d/%d", assigned_droids),
					getLocalizedStatsName(psStructure->pStructureType), assigned_droids, psStructure->body, psStructure->structureBody());
		}
		else
		{
			console(_("%s - Hitpoints %d/%d"), getLocalizedStatsName(psStructure->pStructureType), psStructure->body, psStructure->structureBody());
		}
		if (dbgInputManager.debugMappingsAllowed())
		{
			// TRANSLATORS: A debug output string (user-visible if debug mode is enabled)
			// "born": Time the game object was born
			// "depth": Depth of structure's foundation
			console(_("ID %d - armour %d|%d - sensor range %d - ECM %d - born %u - depth %.02f"),
			        psStructure->id, objArmour(psStructure, WC_KINETIC), objArmour(psStructure, WC_HEAT),
			        structSensorRange(psStructure), structJammerPower(psStructure), psStructure->born, psStructure->foundationDepth);
		}
		break;
	case REF_REPAIR_FACILITY:
		console(_("%s - Hitpoints %d/%d"), getLocalizedStatsName(psStructure->pStructureType), psStructure->body, psStructure->structureBody());
		if (dbgInputManager.debugMappingsAllowed())
		{
			// TRANSLATORS: A debug output string (user-visible if debug mode is enabled)
			// "state": The "RepairState" (printed as an integer, Idle = 0, Repairing = 1)
			console(_("ID %d - State %d"), psStructure->id, psStructure->pFunctionality->repairFacility.state);
		}
		break;
	case REF_RESOURCE_EXTRACTOR:
		console(_("%s - Hitpoints %d/%d"), getLocalizedStatsName(psStructure->pStructureType), psStructure->body, psStructure->structureBody());
		if (dbgInputManager.debugMappingsAllowed() && selectedPlayer < MAX_PLAYERS)
		{
			console(_("ID %d - %s"), psStructure->id, (auxTile(map_coord(psStructure->pos.x), map_coord(psStructure->pos.y), selectedPlayer) & AUXBITS_DANGER) ? "danger" : "safe");
		}
		break;
	case REF_POWER_GEN:
		psPowerGen = &psStructure->pFunctionality->powerGenerator;
		numConnected = 0;
		for (int i = 0; i < NUM_POWER_MODULES; i++)
		{
			if (psPowerGen->apResExtractors[i])
			{
				numConnected++;
			}
		}
		console(_("%s - Connected %u of %u - Hitpoints %d/%d"), getLocalizedStatsName(psStructure->pStructureType), numConnected,
		        NUM_POWER_MODULES, psStructure->body, psStructure->structureBody());
		if (dbgInputManager.debugMappingsAllowed())
		{
			// TRANSLATORS: A debug output string (user-visible if debug mode is enabled)
			console(_("ID %u - Multiplier: %u"), psStructure->id, getBuildingPowerPoints(psStructure));
		}
		break;
	case REF_CYBORG_FACTORY:
	case REF_VTOL_FACTORY:
	case REF_FACTORY:
		console(_("%s - Hitpoints %d/%d"), getLocalizedStatsName(psStructure->pStructureType), psStructure->body, psStructure->structureBody());
		if (dbgInputManager.debugMappingsAllowed())
		{
			// TRANSLATORS: A debug output string (user-visible if debug mode is enabled)
			console(_("ID %u - Production Output: %u - BuildPointsRemaining: %u - Resistance: %d / %d"), psStructure->id,
			        getBuildingProductionPoints(psStructure), psStructure->pFunctionality->factory.buildPointsRemaining,
			        psStructure->resistance, structureResistance(psStructure->pStructureType, psStructure->player));
		}
		break;
	case REF_RESEARCH:
		console(_("%s - Hitpoints %d/%d"), getLocalizedStatsName(psStructure->pStructureType), psStructure->body, psStructure->structureBody());
		if (dbgInputManager.debugMappingsAllowed())
		{
			// TRANSLATORS: A debug output string (user-visible if debug mode is enabled)
			console(_("ID %u - Research Points: %u"), psStructure->id, getBuildingResearchPoints(psStructure));
		}
		break;
	case REF_REARM_PAD:
		console(_("%s - Hitpoints %d/%d"), getLocalizedStatsName(psStructure->pStructureType), psStructure->body, psStructure->structureBody());
		if (dbgInputManager.debugMappingsAllowed())
		{
			// TRANSLATORS: A debug output string (user-visible if debug mode is enabled)
			console(_("tile %d,%d - target %s"), psStructure->pos.x / TILE_UNITS, psStructure->pos.y / TILE_UNITS,
			        objInfo(psStructure->pFunctionality->rearmPad.psObj));
		}
		break;
	default:
		console(_("%s - Hitpoints %d/%d"), getLocalizedStatsName(psStructure->pStructureType), psStructure->body, psStructure->structureBody());
		if (dbgInputManager.debugMappingsAllowed())
		{
			// TRANSLATORS: A debug output string (user-visible if debug mode is enabled)
			console(_("ID %u - sensor range %d - ECM %d"), psStructure->id, structSensorRange(psStructure), structJammerPower(psStructure));
		}
		break;
	}
}


/*Checks the template type against the factory type - returns false
if not a good combination!*/
bool validTemplateForFactory(const DROID_TEMPLATE *psTemplate, STRUCTURE *psFactory, bool complain)
{
	ASSERT_OR_RETURN(false, psTemplate, "Invalid template!");
	enum code_part level = complain ? LOG_ERROR : LOG_NEVER;

	//not in multiPlayer! - AB 26/5/99
	//ignore Transporter Droids
	if (!bMultiPlayer && isTransporter(psTemplate))
	{
		debug(level, "Cannot build transporter in campaign.");
		return false;
	}

	//check if droid is a cyborg
	if (psTemplate->droidType == DROID_CYBORG ||
	    psTemplate->droidType == DROID_CYBORG_SUPER ||
	    psTemplate->droidType == DROID_CYBORG_CONSTRUCT ||
	    psTemplate->droidType == DROID_CYBORG_REPAIR)
	{
		if (psFactory->pStructureType->type != REF_CYBORG_FACTORY)
		{
			debug(level, "Cannot build cyborg except in cyborg factory, not in %s.", objInfo(psFactory));
			return false;
		}
	}
	//check for VTOL droid
	else if (psTemplate->asParts[COMP_PROPULSION] &&
	         (psTemplate->getPropulsionStats()->propulsionType == PROPULSION_TYPE_LIFT))
	{
		if (psFactory->pStructureType->type != REF_VTOL_FACTORY)
		{
			debug(level, "Cannot build vtol except in vtol factory, not in %s.", objInfo(psFactory));
			return false;
		}
	}

	//check if cyborg factory
	if (psFactory->pStructureType->type == REF_CYBORG_FACTORY)
	{
		if (!(psTemplate->droidType == DROID_CYBORG ||
		      psTemplate->droidType == DROID_CYBORG_SUPER ||
		      psTemplate->droidType == DROID_CYBORG_CONSTRUCT ||
		      psTemplate->droidType == DROID_CYBORG_REPAIR))
		{
			debug(level, "Can only build cyborg in cyborg factory, not droidType %d in %s.",
			      psTemplate->droidType, objInfo(psFactory));
			return false;
		}
	}
	//check if vtol factory
	else if (psFactory->pStructureType->type == REF_VTOL_FACTORY)
	{
		if (!psTemplate->asParts[COMP_PROPULSION] ||
		    (psTemplate->getPropulsionStats()->propulsionType != PROPULSION_TYPE_LIFT))
		{
			debug(level, "Can only build vtol in vtol factory, not in %s.", objInfo(psFactory));
			return false;
		}
	}

	//got through all the tests...
	return true;
}

/*calculates the damage caused to the resistance levels of structures - returns
true when captured*/
bool electronicDamage(BASE_OBJECT *psTarget, UDWORD damage, UBYTE attackPlayer)
{
	STRUCTURE   *psStructure;
	DROID       *psDroid;
	bool        bCompleted = true;
	Vector3i	pos;

	ASSERT_OR_RETURN(false, attackPlayer < MAX_PLAYERS, "Invalid player id %d", (int)attackPlayer);
	ASSERT_OR_RETURN(false, psTarget != nullptr, "Target is NULL");

	//structure electronic damage
	if (psTarget->type == OBJ_STRUCTURE)
	{
		psStructure = (STRUCTURE *)psTarget;
		bCompleted = false;

		if (psStructure->pStructureType->upgrade[psStructure->player].resistance == 0)
		{
			return false;	// this structure type cannot be taken over
		}

		//if resistance is already less than 0 don't do any more
		if (psStructure->resistance < 0)
		{
			bCompleted = true;
		}
		else
		{
			//store the time it was hit
			int lastHit = psStructure->timeLastHit;
			psStructure->timeLastHit = gameTime;

			psStructure->lastHitWeapon = WSC_ELECTRONIC;

			triggerEventAttacked(psStructure, g_pProjLastAttacker, lastHit);

			psStructure->resistance = (SWORD)(psStructure->resistance - damage);

			if (psStructure->resistance < 0)
			{
				//add a console message for the selected Player
				if (psStructure->player == selectedPlayer)
				{
					console(_("%s - Electronically Damaged"),
							getLocalizedStatsName(psStructure->pStructureType));
				}
				bCompleted = true;
				//give the structure to the attacking player
				(void)giftSingleStructure(psStructure, attackPlayer);
			}
		}
	}
	//droid electronic damage
	else if (psTarget->type == OBJ_DROID)
	{
		psDroid = (DROID *)psTarget;
		bCompleted = false;
		int lastHit = psDroid->timeLastHit;
		psDroid->timeLastHit = gameTime;
		psDroid->lastHitWeapon = WSC_ELECTRONIC;

		//in multiPlayer cannot attack a Transporter with EW
		if (bMultiPlayer)
		{
			ASSERT_OR_RETURN(true, !psDroid->isTransporter(), "Cannot attack a Transporter in multiPlayer");
		}

		if (psDroid->resistance == ACTION_START_TIME)
		{
			//need to set the current resistance level since not been previously attacked (by EW)
			psDroid->resistance = psDroid->droidResistance();
		}

		if (psDroid->resistance < 0)
		{
			bCompleted = true;
		}
		else
		{
			triggerEventAttacked(psDroid, g_pProjLastAttacker, lastHit);

			psDroid->resistance = (SWORD)(psDroid->resistance - damage);

			if (psDroid->resistance <= 0)
			{
				//add a console message for the selected Player
				if (psDroid->player == selectedPlayer)
				{
					console(_("%s - Electronically Damaged"), "Unit");
				}
				bCompleted = true;

				//give the droid to the attacking player

				if (psDroid->visibleForLocalDisplay()) // display-only check for adding effect
				{
					for (int i = 0; i < 5; i++)
					{
						pos.x = psDroid->pos.x + (30 - rand() % 60);
						pos.z = psDroid->pos.y + (30 - rand() % 60);
						pos.y = psDroid->pos.z + (rand() % 8);
						effectGiveAuxVar(80);
						addEffect(&pos, EFFECT_EXPLOSION, EXPLOSION_TYPE_FLAMETHROWER, false, nullptr, 0, gameTime - deltaGameTime);
					}
				}
				if (!isDead(psDroid) && !giftSingleDroid(psDroid, attackPlayer, true, g_pProjLastAttacker->pos.xy()))
				{
					// droid limit reached, recycle
					// don't check for transporter/mission coz multiplayer only issue.
					recycleDroid(psDroid);
				}
			}
		}
	}

	return bCompleted;
}


/* EW works differently in multiplayer mode compared with single player.*/
bool validStructResistance(const STRUCTURE *psStruct)
{
	bool    bTarget = false;

	ASSERT_OR_RETURN(false, psStruct != nullptr, "Invalid structure pointer");

	if (psStruct->pStructureType->upgrade[psStruct->player].resistance != 0)
	{
		/*certain structures will only provide rewards in multiplayer so
		before they can become valid targets their resistance must be at least
		half the base value*/
		if (bMultiPlayer)
		{
			switch (psStruct->pStructureType->type)
			{
			case REF_RESEARCH:
			case REF_FACTORY:
			case REF_VTOL_FACTORY:
			case REF_CYBORG_FACTORY:
			case REF_HQ:
			case REF_REPAIR_FACILITY:
				if (psStruct->resistance >= structureResistance(psStruct->pStructureType, psStruct->player) / 2)
				{
					bTarget = true;
				}
				break;
			default:
				bTarget = true;
			}
		}
		else
		{
			bTarget = true;
		}
	}

	return bTarget;
}

unsigned structureBodyBuilt(const STRUCTURE *psStructure)
{
	unsigned maxBody = psStructure->structureBody();

	if (psStructure->status == SS_BEING_BUILT)
	{
		// Calculate the body points the structure would have, if not damaged.
		unsigned unbuiltBody = (maxBody + 9) / 10;  // See droidStartBuild() in droid.cpp.
		unsigned deltaBody = static_cast<unsigned>(maxBody * 9 * structureCompletionProgress(*psStructure) / 10);  // See structureBuild() in structure.cpp.
		maxBody = unbuiltBody + deltaBody;
	}

	return maxBody;
}

/*Access functions for the upgradeable stats of a structure*/
uint32_t STRUCTURE::structureBody() const
{
	return pStructureType->upgrade[player].hitpoints;
}

UDWORD	structureResistance(const STRUCTURE_STATS *psStats, UBYTE player)
{
	return psStats->upgrade[player].resistance;
}


/*gives the attacking player a reward based on the type of structure that has
been attacked*/
bool electronicReward(STRUCTURE *psStructure, UBYTE attackPlayer)
{
	if (!bMultiPlayer)
	{
		return false; //campaign should not give rewards (especially to the player)
	}

	ASSERT_OR_RETURN(false, attackPlayer < MAX_PLAYERS, "Invalid player id %d", (int)attackPlayer);

	bool    bRewarded = false;

	switch (psStructure->pStructureType->type)
	{
	case REF_RESEARCH:
		researchReward(psStructure->player, attackPlayer);
		bRewarded = true;
		break;
	case REF_FACTORY:
	case REF_VTOL_FACTORY:
	case REF_CYBORG_FACTORY:
		factoryReward(psStructure->player, attackPlayer);
		bRewarded = true;
		break;
	case REF_HQ:
		hqReward(psStructure->player, attackPlayer);
		if (attackPlayer == selectedPlayer)
		{
			addConsoleMessage(_("Electronic Reward - Visibility Report"),	DEFAULT_JUSTIFY, SYSTEM_MESSAGE);
		}
		bRewarded = true;
		break;
	case REF_REPAIR_FACILITY:
		repairFacilityReward(psStructure->player, attackPlayer);
		bRewarded = true;
		break;
	default:
		bRewarded = false;
	}

	return bRewarded;
}


/*find the 'best' prop/body/weapon component the losing player has and
'give' it to the reward player*/
void factoryReward(UBYTE losingPlayer, UBYTE rewardPlayer)
{
	unsigned comp = 0;

	ASSERT_OR_RETURN(, losingPlayer < MAX_PLAYERS, "Invalid losingPlayer id %d", (int)losingPlayer);
	ASSERT_OR_RETURN(, rewardPlayer < MAX_PLAYERS, "Invalid rewardPlayer id %d", (int)rewardPlayer);

	//search through the propulsions first
	for (unsigned inc = 0; inc < asPropulsionStats.size(); inc++)
	{
		if (apCompLists[losingPlayer][COMP_PROPULSION][inc] == AVAILABLE &&
		    apCompLists[rewardPlayer][COMP_PROPULSION][inc] != AVAILABLE)
		{
			if (asPropulsionStats[inc].buildPower > asPropulsionStats[comp].buildPower)
			{
				comp = inc;
			}
		}
	}
	if (comp != 0)
	{
		apCompLists[rewardPlayer][COMP_PROPULSION][comp] = AVAILABLE;
		if (rewardPlayer == selectedPlayer)
		{
			console("%s :- %s", _("Factory Reward - Propulsion"), getLocalizedStatsName(&asPropulsionStats[comp]));
		}
		return;
	}

	//haven't found a propulsion - look for a body
	for (unsigned inc = 0; inc < asBodyStats.size(); inc++)
	{
		if (apCompLists[losingPlayer][COMP_BODY][inc] == AVAILABLE &&
		    apCompLists[rewardPlayer][COMP_BODY][inc] != AVAILABLE)
		{
			if (asBodyStats[inc].buildPower > asBodyStats[comp].buildPower)
			{
				comp = inc;
			}
		}
	}
	if (comp != 0)
	{
		apCompLists[rewardPlayer][COMP_BODY][comp] = AVAILABLE;
		if (rewardPlayer == selectedPlayer)
		{
			console("%s :- %s", _("Factory Reward - Body"), getLocalizedStatsName(&asBodyStats[comp]));
		}
		return;
	}

	//haven't found a body - look for a weapon
	for (unsigned inc = 0; inc < asWeaponStats.size(); inc++)
	{
		if (apCompLists[losingPlayer][COMP_WEAPON][inc] == AVAILABLE &&
		    apCompLists[rewardPlayer][COMP_WEAPON][inc] != AVAILABLE)
		{
			if (asWeaponStats[inc].buildPower > asWeaponStats[comp].buildPower)
			{
				comp = inc;
			}
		}
	}
	if (comp != 0)
	{
		apCompLists[rewardPlayer][COMP_WEAPON][comp] = AVAILABLE;
		if (rewardPlayer == selectedPlayer)
		{
			console("%s :- %s", _("Factory Reward - Weapon"), getLocalizedStatsName(&asWeaponStats[comp]));
		}
		return;
	}

	//losing Player hasn't got anything better so don't gain anything!
	if (rewardPlayer == selectedPlayer)
	{
		addConsoleMessage(_("Factory Reward - Nothing"), DEFAULT_JUSTIFY, SYSTEM_MESSAGE);
	}
}

/*find the 'best' repair component the losing player has and
'give' it to the reward player*/
void repairFacilityReward(UBYTE losingPlayer, UBYTE rewardPlayer)
{
	unsigned comp = 0;

	ASSERT_OR_RETURN(, losingPlayer < MAX_PLAYERS, "Invalid losingPlayer id %d", (int)losingPlayer);
	ASSERT_OR_RETURN(, rewardPlayer < MAX_PLAYERS, "Invalid rewardPlayer id %d", (int)rewardPlayer);

	//search through the repair stats
	for (unsigned inc = 0; inc < asRepairStats.size(); inc++)
	{
		if (apCompLists[losingPlayer][COMP_REPAIRUNIT][inc] == AVAILABLE &&
		    apCompLists[rewardPlayer][COMP_REPAIRUNIT][inc] != AVAILABLE)
		{
			if (asRepairStats[inc].buildPower > asRepairStats[comp].buildPower)
			{
				comp = inc;
			}
		}
	}
	if (comp != 0)
	{
		apCompLists[rewardPlayer][COMP_REPAIRUNIT][comp] = AVAILABLE;
		if (rewardPlayer == selectedPlayer)
		{
			console("%s :- %s", _("Repair Facility Award - Repair"), getLocalizedStatsName(&asRepairStats[comp]));
		}
		return;
	}
	if (rewardPlayer == selectedPlayer)
	{
		addConsoleMessage(_("Repair Facility Award - Nothing"), DEFAULT_JUSTIFY, SYSTEM_MESSAGE);
	}
}


/*makes the losing players tiles/structures/features visible to the reward player*/
void hqReward(UBYTE losingPlayer, UBYTE rewardPlayer)
{
	ASSERT_OR_RETURN(, losingPlayer < MAX_PLAYERS && rewardPlayer < MAX_PLAYERS, "losingPlayer (%" PRIu8 "), rewardPlayer (%" PRIu8 ") must both be < MAXPLAYERS", losingPlayer, rewardPlayer);

	// share exploration info - pretty useless but perhaps a nice touch?
	for (int y = 0; y < mapHeight; ++y)
	{
		for (int x = 0; x < mapWidth; ++x)
		{
			MAPTILE *psTile = mapTile(x, y);
			if (TEST_TILE_VISIBLE(losingPlayer, psTile))
			{
				psTile->tileExploredBits |= alliancebits[rewardPlayer];
			}
		}
	}

	//struct
	for (int i = 0; i < MAX_PLAYERS; ++i)
	{
		for (STRUCTURE *psStruct : apsStructLists[i])
		{
			if (psStruct->visible[losingPlayer] && !psStruct->died)
			{
				psStruct->visible[rewardPlayer] = psStruct->visible[losingPlayer];
			}
		}

		//feature
		for (FEATURE *psFeat : apsFeatureLists[i])
		{
			if (psFeat->visible[losingPlayer])
			{
				psFeat->visible[rewardPlayer] = psFeat->visible[losingPlayer];
			}
		}

		//droids.
		for (DROID *psDroid : apsDroidLists[i])
		{
			if (psDroid->visible[losingPlayer] || psDroid->player == losingPlayer)
			{
				psDroid->visible[rewardPlayer] = UBYTE_MAX;
			}
		}
	}
}


// Return true if structure is a factory of any type.
//
bool STRUCTURE::isFactory() const
{
	ASSERT_OR_RETURN(false, pStructureType != nullptr, "Invalid structureType!");

	return type == OBJ_STRUCTURE && (
		pStructureType->type == REF_FACTORY ||
		pStructureType->type == REF_CYBORG_FACTORY ||
		pStructureType->type == REF_VTOL_FACTORY);
}


// Return true if flag is a delivery point for a factory.
//
bool FlagIsFactory(const FLAG_POSITION *psCurrFlag)
{
	if ((psCurrFlag->factoryType == FACTORY_FLAG) || (psCurrFlag->factoryType == CYBORG_FLAG) ||
	    (psCurrFlag->factoryType == VTOL_FLAG))
	{
		return true;
	}

	return false;
}


// Find a structure's delivery point , only if it's a factory.
// Returns NULL if not found or the structure isn't a factory.
//
FLAG_POSITION *FindFactoryDelivery(const STRUCTURE *Struct)
{
	if (Struct && Struct->isFactory())
	{
		// Find the factories delivery point.
		for (const auto& psFlag : apsFlagPosLists[Struct->player])
		{
			if (FlagIsFactory(psFlag)
				&& Struct->pFunctionality->factory.psAssemblyPoint->factoryInc == psFlag->factoryInc
				&& Struct->pFunctionality->factory.psAssemblyPoint->factoryType == psFlag->factoryType)
			{
				return psFlag;
			}
		}
	}

	return nullptr;
}


//Find the factory associated with the delivery point - returns NULL if none exist
STRUCTURE	*findDeliveryFactory(FLAG_POSITION *psDelPoint)
{
	FACTORY		*psFactory;
	REPAIR_FACILITY *psRepair;

	for (STRUCTURE* psCurr : apsStructLists[psDelPoint->player])
	{
		if (!psCurr)
		{
			continue;
		}
		if (psCurr->isFactory())
		{
			psFactory = &psCurr->pFunctionality->factory;
			if (psFactory->psAssemblyPoint->factoryInc == psDelPoint->factoryInc &&
			    psFactory->psAssemblyPoint->factoryType == psDelPoint->factoryType)
			{
				return psCurr;
			}
		}
		else if (psCurr->pStructureType->type == REF_REPAIR_FACILITY)
		{
			psRepair = &psCurr->pFunctionality->repairFacility;
			if (psRepair->psDeliveryPoint == psDelPoint)
			{
				return psCurr;
			}
		}
	}
	return nullptr;
}

/*cancels the production run for the factory and returns any power that was
accrued but not used*/
void cancelProduction(STRUCTURE *psBuilding, QUEUE_MODE mode, bool mayClearProductionRun)
{
	ASSERT_OR_RETURN(, psBuilding && psBuilding->isFactory(), "structure not a factory");

	FACTORY *psFactory = &psBuilding->pFunctionality->factory;

	if (psBuilding->player == productionPlayer && mayClearProductionRun)
	{
		//clear the production run for this factory
		if (psFactory->psAssemblyPoint->factoryInc < asProductionRun[psFactory->psAssemblyPoint->factoryType].size())
		{
			asProductionRun[psFactory->psAssemblyPoint->factoryType][psFactory->psAssemblyPoint->factoryInc].clear();
		}
		psFactory->productionLoops = 0;
	}

	if (mode == ModeQueue)
	{
		sendStructureInfo(psBuilding, STRUCTUREINFO_CANCELPRODUCTION, nullptr);
		setStatusPendingCancel(*psFactory);

		return;
	}

	//clear the factory's subject
	refundFactoryBuildPower(psBuilding);
	psFactory->psSubject = nullptr;
	delPowerRequest(psBuilding);
}


/*set a factory's production run to hold*/
void holdProduction(STRUCTURE *psBuilding, QUEUE_MODE mode)
{
	FACTORY *psFactory;

	ASSERT_OR_RETURN(, psBuilding && psBuilding->isFactory(), "structure not a factory");

	psFactory = &psBuilding->pFunctionality->factory;

	if (mode == ModeQueue)
	{
		sendStructureInfo(psBuilding, STRUCTUREINFO_HOLDPRODUCTION, nullptr);
		setStatusPendingHold(*psFactory);

		return;
	}

	if (psFactory->psSubject)
	{
		//set the time the factory was put on hold
		psFactory->timeStartHold = gameTime;
		//play audio to indicate on hold
		if (psBuilding->player == selectedPlayer)
		{
			audio_PlayTrack(ID_SOUND_WINDOWCLOSE);
		}
	}

	delPowerRequest(psBuilding);
}

/*release a factory's production run from hold*/
void releaseProduction(STRUCTURE *psBuilding, QUEUE_MODE mode)
{
	ASSERT_OR_RETURN(, psBuilding && psBuilding->isFactory(), "structure not a factory");

	FACTORY *psFactory = &psBuilding->pFunctionality->factory;

	if (mode == ModeQueue)
	{
		sendStructureInfo(psBuilding, STRUCTUREINFO_RELEASEPRODUCTION, nullptr);
		setStatusPendingRelease(*psFactory);

		return;
	}

	if (psFactory->psSubject && psFactory->timeStartHold)
	{
		//adjust the start time for the current subject
		if (psFactory->timeStarted != ACTION_START_TIME)
		{
			psFactory->timeStarted += (gameTime - psFactory->timeStartHold);
		}
		psFactory->timeStartHold = 0;
	}
}

void doNextProduction(STRUCTURE *psStructure, DROID_TEMPLATE *current, QUEUE_MODE mode)
{
	DROID_TEMPLATE *psNextTemplate = factoryProdUpdate(psStructure, current);

	if (psNextTemplate != nullptr)
	{
		structSetManufacture(psStructure, psNextTemplate, ModeQueue);  // ModeQueue instead of mode, since production lists aren't currently synchronised.
	}
	else
	{
		cancelProduction(psStructure, mode);
	}
}

bool ProductionRunEntry::operator ==(DROID_TEMPLATE *t) const
{
	return psTemplate->multiPlayerID == t->multiPlayerID;
}

/*this is called when a factory produces a droid. The Template returned is the next
one to build - if any*/
DROID_TEMPLATE *factoryProdUpdate(STRUCTURE *psStructure, DROID_TEMPLATE *psTemplate)
{
	CHECK_STRUCTURE(psStructure);
	if (psStructure->player != productionPlayer)
	{
		return nullptr;  // Production lists not currently synchronised.
	}

	FACTORY *psFactory = &psStructure->pFunctionality->factory;
	if (psFactory->psAssemblyPoint->factoryInc >= asProductionRun[psFactory->psAssemblyPoint->factoryType].size())
	{
		return nullptr;  // Don't even have a production list.
	}
	ProductionRun &productionRun = asProductionRun[psFactory->psAssemblyPoint->factoryType][psFactory->psAssemblyPoint->factoryInc];

	if (psTemplate != nullptr)
	{
		//find the entry in the array for this template
		ProductionRun::iterator entry = std::find(productionRun.begin(), productionRun.end(), psTemplate);
		if (entry != productionRun.end())
		{
			entry->built = std::min(entry->built + 1, entry->quantity);
			if (!entry->isComplete())
			{
				return psTemplate;  // Build another of the same type.
			}
			if (psFactory->productionLoops == 0)
			{
				productionRun.erase(entry);
			}
		}
	}
	//find the next template to build - this just looks for the first uncompleted run
	for (unsigned inc = 0; inc < productionRun.size(); ++inc)
	{
		if (!productionRun[inc].isComplete())
		{
			return productionRun[inc].psTemplate;
		}
	}
	// Check that we aren't looping doing nothing.
	if (productionRun.empty())
	{
		if (psFactory->productionLoops != INFINITE_PRODUCTION)
		{
			psFactory->productionLoops = 0;  // Reset number of loops, unless set to infinite.
		}
	}
	else if (psFactory->productionLoops != 0)  //If you've got here there's nothing left to build unless factory is on loop production
	{
		//reduce the loop count if not infinite
		if (psFactory->productionLoops != INFINITE_PRODUCTION)
		{
			psFactory->productionLoops--;
		}

		//need to reset the quantity built for each entry in the production list
		std::for_each(productionRun.begin(), productionRun.end(), std::mem_fn(&ProductionRunEntry::restart));

		//get the first to build again
		return productionRun[0].psTemplate;
	}
	//if got to here then nothing left to produce so clear the array
	productionRun.clear();
	return nullptr;
}


//adjust the production run for this template type
void factoryProdAdjust(STRUCTURE *psStructure, DROID_TEMPLATE *psTemplate, bool add)
{
	CHECK_STRUCTURE(psStructure);
	ASSERT_OR_RETURN(, psStructure->player == productionPlayer, "called for incorrect player");
	ASSERT_OR_RETURN(, psTemplate != nullptr, "NULL template");

	FACTORY *psFactory = &psStructure->pFunctionality->factory;
	if (psFactory->psAssemblyPoint->factoryInc >= asProductionRun[psFactory->psAssemblyPoint->factoryType].size())
	{
		asProductionRun[psFactory->psAssemblyPoint->factoryType].resize(psFactory->psAssemblyPoint->factoryInc + 1);  // Don't have a production list, create it.
	}
	ProductionRun &productionRun = asProductionRun[psFactory->psAssemblyPoint->factoryType][psFactory->psAssemblyPoint->factoryInc];

	//see if the template is already in the list
	ProductionRun::iterator entry = std::find(productionRun.begin(), productionRun.end(), psTemplate);

	if (entry != productionRun.end())
	{
		if (psFactory->productionLoops == 0)
		{
			entry->removeComplete();  // We are not looping, so remove the built droids from the list, so that quantity corresponds to the displayed number.
		}

		//adjust the prod run
		entry->quantity += add ? 1 : -1;
		entry->built = std::min(entry->built, entry->quantity);

		// Allows us to queue up more units up to MAX_IN_RUN instead of ignoring how many we have built from that queue
		// check to see if user canceled all orders in queue
		if (entry->quantity <= 0 || entry->quantity > MAX_IN_RUN)
		{
			productionRun.erase(entry);  // Entry empty, so get rid of it.
		}
	}
	else
	{
		//start off a new template
		ProductionRunEntry tmplEntry;
		tmplEntry.psTemplate = psTemplate;
		tmplEntry.quantity = add ? 1 : MAX_IN_RUN; //wrap around to max value
		tmplEntry.built = 0;
		productionRun.push_back(tmplEntry);
	}
	//if nothing is allocated then the current factory may have been cancelled
	if (productionRun.empty())
	{
		//must have cancelled everything - so tell the struct
		if (psFactory->productionLoops != INFINITE_PRODUCTION)
		{
			psFactory->productionLoops = 0;  // Reset number of loops, unless set to infinite.
		}
	}

	//need to check if this was the template that was mid-production
	if (getProduction(psStructure, FactoryGetTemplate(psFactory)).numRemaining() == 0)
	{
		doNextProduction(psStructure, FactoryGetTemplate(psFactory), ModeQueue);
	}
	else if (!StructureIsManufacturingPending(psStructure))
	{
		structSetManufacture(psStructure, psTemplate, ModeQueue);
	}

	if (StructureIsOnHoldPending(psStructure))
	{
		releaseProduction(psStructure, ModeQueue);
	}
}

/** checks the status of the production of a template
 */
ProductionRunEntry getProduction(STRUCTURE *psStructure, DROID_TEMPLATE *psTemplate)
{
	if (psStructure == nullptr || psStructure->player != productionPlayer || psTemplate == nullptr || !psStructure->isFactory())
	{
		return ProductionRunEntry();  // Not producing any NULL pointers.
	}

	FACTORY *psFactory = &psStructure->pFunctionality->factory;
	if (!(psFactory->psAssemblyPoint) || psFactory->psAssemblyPoint->factoryInc >= asProductionRun[psFactory->psAssemblyPoint->factoryType].size())
	{
		return ProductionRunEntry();  // Don't have a production list.
	}
	ProductionRun &productionRun = asProductionRun[psFactory->psAssemblyPoint->factoryType][psFactory->psAssemblyPoint->factoryInc];

	//see if the template is in the list
	ProductionRun::iterator entry = std::find(productionRun.begin(), productionRun.end(), psTemplate);
	if (entry != productionRun.end())
	{
		return *entry;
	}

	//not in the list so none being produced
	return ProductionRunEntry();
}


/*looks through a players production list to see how many command droids
are being built*/
UBYTE checkProductionForCommand(UBYTE player)
{
	unsigned quantity = 0;

	if (player == productionPlayer)
	{
		//assumes Cyborg or VTOL droids are not Command types!
		unsigned factoryType = FACTORY_FLAG;

		for (unsigned factoryInc = 0; factoryInc < factoryNumFlag[player][factoryType].size(); ++factoryInc)
		{
			//check to see if there is a factory with a production run
			if (factoryNumFlag[player][factoryType][factoryInc] && factoryInc < asProductionRun[factoryType].size())
			{
				ProductionRun &productionRun = asProductionRun[factoryType][factoryInc];
				for (unsigned inc = 0; inc < productionRun.size(); ++inc)
				{
					if (productionRun[inc].psTemplate->droidType == DROID_COMMAND)
					{
						quantity += productionRun[inc].numRemaining();
					}
				}
			}
		}
	}
	return quantity;
}


// Count number of factories assignable to a command droid.
//
UWORD countAssignableFactories(UBYTE player, UWORD factoryType)
{
	UBYTE           quantity = 0;

	ASSERT_OR_RETURN(0, player == selectedPlayer, "%s should only be called for selectedPlayer", __FUNCTION__);

	if (player >= MAX_PLAYERS) { return 0; }

	for (unsigned factoryInc = 0; factoryInc < factoryNumFlag[player][factoryType].size(); ++factoryInc)
	{
		//check to see if there is a factory
		if (factoryNumFlag[player][factoryType][factoryInc])
		{
			quantity++;
		}
	}

	return quantity;
}


// check whether a factory of a certain number and type exists
bool checkFactoryExists(UDWORD player, UDWORD factoryType, UDWORD inc)
{
	ASSERT_OR_RETURN(false, player < MAX_PLAYERS, "Invalid player");
	ASSERT_OR_RETURN(false, factoryType < NUM_FACTORY_TYPES, "Invalid factoryType");

	return inc < factoryNumFlag[player][factoryType].size() && factoryNumFlag[player][factoryType][inc];
}


//check that delivery points haven't been put down in invalid location
void checkDeliveryPoints(UDWORD version)
{
	UBYTE			inc;
	FACTORY			*psFactory;
	REPAIR_FACILITY	*psRepair;
	UDWORD					x, y;

	//find any factories
	for (inc = 0; inc < MAX_PLAYERS; inc++)
	{
		//don't bother checking selectedPlayer's - causes problems when try and use
		//validLocation since it finds that the DP is on itself! And validLocation
		//will have been called to put in down in the first place
		if (inc != selectedPlayer)
		{
			for (STRUCTURE* psStruct : apsStructLists[inc])
			{
				if (!psStruct)
				{
					continue;
				}
				if (psStruct->isFactory())
				{
					//check the DP
					psFactory = &psStruct->pFunctionality->factory;
					if (psFactory->psAssemblyPoint == nullptr)//need to add one
					{
						ASSERT_OR_RETURN(, psFactory->psAssemblyPoint != nullptr, "no delivery point for factory");
					}
					else
					{
						setAssemblyPoint(psFactory->psAssemblyPoint, psFactory->psAssemblyPoint->
						                 coords.x, psFactory->psAssemblyPoint->coords.y, inc, true);
					}
				}
				else if (psStruct->pStructureType->type == REF_REPAIR_FACILITY)
				{
					psRepair = &psStruct->pFunctionality->repairFacility;

					if (psRepair->psDeliveryPoint == nullptr)//need to add one
					{
						if (version >= VERSION_19)
						{
							ASSERT_OR_RETURN(, psRepair->psDeliveryPoint != nullptr, "no delivery point for repair facility");
						}
						else
						{
							// add an assembly point
							if (!createFlagPosition(&psRepair->psDeliveryPoint, psStruct->player))
							{
								ASSERT(!"can't create new delivery point for repair facility", "unable to create new delivery point for repair facility");
								return;
							}
							addFlagPosition(psRepair->psDeliveryPoint);
							setFlagPositionInc(psStruct->pFunctionality, psStruct->player, REPAIR_FLAG);
							//initialise the assembly point position
							x = map_coord(psStruct->pos.x + 256);
							y = map_coord(psStruct->pos.y + 256);
							// Belt and braces - shouldn't be able to build too near edge
							setAssemblyPoint(psRepair->psDeliveryPoint, world_coord(x),
							                 world_coord(y), inc, true);
						}
					}
					else//check existing one
					{
						setAssemblyPoint(psRepair->psDeliveryPoint, psRepair->psDeliveryPoint->
						                 coords.x, psRepair->psDeliveryPoint->coords.y, inc, true);
					}
				}
			}
		}
	}
}


//adjust the loop quantity for this factory
void factoryLoopAdjust(STRUCTURE *psStruct, bool add)
{
	FACTORY		*psFactory;

	ASSERT_OR_RETURN(, psStruct && psStruct->isFactory(), "structure is not a factory");
	ASSERT_OR_RETURN(, psStruct->player == selectedPlayer, "should only be called for selectedPlayer");

	psFactory = &psStruct->pFunctionality->factory;

	if (add)
	{
		//check for wrapping to infinite production
		if (psFactory->productionLoops == MAX_IN_RUN)
		{
			psFactory->productionLoops = 0;
		}
		else
		{
			//increment the count
			psFactory->productionLoops++;
			//check for limit - this caters for when on infinite production and want to wrap around
			if (psFactory->productionLoops > MAX_IN_RUN)
			{
				psFactory->productionLoops = INFINITE_PRODUCTION;
			}
		}
	}
	else
	{
		//decrement the count
		if (psFactory->productionLoops == 0)
		{
			psFactory->productionLoops = INFINITE_PRODUCTION;
		}
		else
		{
			psFactory->productionLoops--;
		}
	}
}


/*Used for determining how much of the structure to draw as being built or demolished*/
float structHeightScale(const STRUCTURE *psStruct)
{
	return MAX(structureCompletionProgress(*psStruct), 0.05f);
}


/*compares the structure sensor type with the droid weapon type to see if the
FIRE_SUPPORT order can be assigned*/
bool structSensorDroidWeapon(const STRUCTURE *psStruct, const DROID *psDroid)
{
	//another crash when nStat is marked as 0xcd... FIXME: Why is nStat not initialized properly?
	//Added a safety check: Only units with weapons can be assigned.
	if (psDroid->numWeaps > 0)
	{
		const auto* weaponStats = psDroid->getWeaponStats(0);
		//Standard Sensor Tower + indirect weapon droid (non VTOL)
		//else if (structStandardSensor(psStruct) && (psDroid->numWeaps &&
		if (structStandardSensor(psStruct) && (psDroid->asWeaps[0].nStat > 0 &&
		                                       !proj_Direct(weaponStats)) &&
		    !psDroid->isVtol())
		{
			return true;
		}
		//CB Sensor Tower + indirect weapon droid (non VTOL)
		//if (structCBSensor(psStruct) && (psDroid->numWeaps &&
		else if (structCBSensor(psStruct) && (psDroid->asWeaps[0].nStat > 0 &&
		                                      !proj_Direct(weaponStats)) &&
		         !psDroid->isVtol())
		{
			return true;
		}
		//VTOL Intercept Sensor Tower + any weapon VTOL droid
		//else if (structVTOLSensor(psStruct) && psDroid->numWeaps &&
		else if (structVTOLSensor(psStruct) && psDroid->asWeaps[0].nStat > 0 &&
		         psDroid->isVtol())
		{
			return true;
		}
		//VTOL CB Sensor Tower + any weapon VTOL droid
		//else if (structVTOLCBSensor(psStruct) && psDroid->numWeaps &&
		else if (structVTOLCBSensor(psStruct) && psDroid->asWeaps[0].nStat > 0 &&
		         psDroid->isVtol())
		{
			return true;
		}
	}

	//case not matched
	return false;
}


/*checks if the structure has a Counter Battery sensor attached - returns
true if it has*/
bool structCBSensor(const STRUCTURE *psStruct)
{
	// Super Sensor works as any type
	if (psStruct->pStructureType->pSensor
	    && (psStruct->pStructureType->pSensor->type == INDIRECT_CB_SENSOR
	        || psStruct->pStructureType->pSensor->type == SUPER_SENSOR)
	    && psStruct->pStructureType->pSensor->location == LOC_TURRET)
	{
		return true;
	}

	return false;
}


/*checks if the structure has a Standard Turret sensor attached - returns
true if it has*/
bool structStandardSensor(const STRUCTURE *psStruct)
{
	// Super Sensor works as any type
	if (psStruct->pStructureType->pSensor
	    && (psStruct->pStructureType->pSensor->type == STANDARD_SENSOR
	        || psStruct->pStructureType->pSensor->type == SUPER_SENSOR)
	    && psStruct->pStructureType->pSensor->location == LOC_TURRET)
	{
		return true;
	}

	return false;
}


/*checks if the structure has a VTOL Intercept sensor attached - returns
true if it has*/
bool structVTOLSensor(const STRUCTURE *psStruct)
{
	// Super Sensor works as any type
	if (psStruct->pStructureType->pSensor
	    && (psStruct->pStructureType->pSensor->type == VTOL_INTERCEPT_SENSOR
	        || psStruct->pStructureType->pSensor->type == SUPER_SENSOR)
	    && psStruct->pStructureType->pSensor->location == LOC_TURRET)
	{
		return true;
	}

	return false;
}


/*checks if the structure has a VTOL Counter Battery sensor attached - returns
true if it has*/
bool structVTOLCBSensor(const STRUCTURE *psStruct)
{
	// Super Sensor works as any type
	if (psStruct->pStructureType->pSensor
	    && (psStruct->pStructureType->pSensor->type == VTOL_CB_SENSOR
	        || psStruct->pStructureType->pSensor->type == SUPER_SENSOR)
	    && psStruct->pStructureType->pSensor->location == LOC_TURRET)
	{
		return true;
	}

	return false;
}


// check whether a rearm pad is clear
bool clearRearmPad(const STRUCTURE *psStruct)
{
	return psStruct->pStructureType->type == REF_REARM_PAD
	       && (psStruct->pFunctionality->rearmPad.psObj == nullptr
	           || vtolHappy((DROID *)psStruct->pFunctionality->rearmPad.psObj));
}


// return the nearest rearm pad
// psTarget can be NULL
STRUCTURE *findNearestReArmPad(DROID *psDroid, STRUCTURE *psTarget, bool bClear)
{
	STRUCTURE		*psNearest, *psTotallyClear;
	SDWORD			xdiff, ydiff, mindist, currdist, totallyDist;
	SDWORD			cx, cy;

	ASSERT_OR_RETURN(nullptr, psDroid != nullptr, "No droid was passed.");

	if (psTarget != nullptr)
	{
		if (!vtolOnRearmPad(psTarget, psDroid))
		{
			return psTarget;
		}
		cx = (SDWORD)psTarget->pos.x;
		cy = (SDWORD)psTarget->pos.y;
	}
	else
	{
		cx = (SDWORD)psDroid->pos.x;
		cy = (SDWORD)psDroid->pos.y;
	}
	mindist = SDWORD_MAX;
	totallyDist = SDWORD_MAX;
	psNearest = nullptr;
	psTotallyClear = nullptr;
	for (STRUCTURE *psStruct : apsStructLists[psDroid->player])
	{
		if (psStruct->pStructureType->type == REF_REARM_PAD && (!bClear || clearRearmPad(psStruct)))
		{
			xdiff = (SDWORD)psStruct->pos.x - cx;
			ydiff = (SDWORD)psStruct->pos.y - cy;
			currdist = xdiff * xdiff + ydiff * ydiff;
			if (bClear && !vtolOnRearmPad(psStruct, psDroid))
			{
				if (currdist < totallyDist)
				{
					totallyDist = currdist;
					psTotallyClear = psStruct;
				}
			}
			else
			{
				if (currdist < mindist)
				{
					mindist = currdist;
					psNearest = psStruct;
				}
			}
		}
	}
	if (bClear && (psTotallyClear != nullptr))
	{
		psNearest = psTotallyClear;
	}
	if (!psNearest)
	{
		objTrace(psDroid->id, "Failed to find a landing spot (%s)!", bClear ? "req clear" : "any");
	}
	return psNearest;
}


// clear a rearm pad for a droid to land on it
void ensureRearmPadClear(STRUCTURE *psStruct, DROID *psDroid)
{
	const int tx = map_coord(psStruct->pos.x);
	const int ty = map_coord(psStruct->pos.y);

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (aiCheckAlliances(psStruct->player, i))
		{
			for (DROID *psCurr : apsDroidLists[i])
			{
				if (psCurr != psDroid
				    && map_coord(psCurr->pos.x) == tx
				    && map_coord(psCurr->pos.y) == ty
				    && psCurr->isVtol())
				{
					actionDroid(psCurr, DACTION_CLEARREARMPAD, psStruct);
				}
			}
		}
	}
}


// return whether a rearm pad has a vtol on it
bool vtolOnRearmPad(const STRUCTURE *psStruct, const DROID *psDroid)
{
	SDWORD	tx, ty;

	tx = map_coord(psStruct->pos.x);
	ty = map_coord(psStruct->pos.y);

	for (const DROID* psCurr : apsDroidLists[psStruct->player])
	{
		if (psCurr != psDroid
		    && map_coord(psCurr->pos.x) == tx
		    && map_coord(psCurr->pos.y) == ty)
		{
			return true;
		}
	}

	return false;
}


/* Just returns true if the structure's present body points aren't as high as the original*/
bool	STRUCTURE::isDamaged() const
{
	return body < structureBody();
}

// give a structure from one player to another - used in Electronic Warfare
//returns pointer to the new structure
STRUCTURE *giftSingleStructure(STRUCTURE *psStructure, UBYTE attackPlayer, bool electronic_warfare)
{
	STRUCTURE           *psNewStruct;
	STRUCTURE_STATS     *psType, *psModule;
	UDWORD              x, y;
	UBYTE               capacity = 0, originalPlayer;
	SWORD               buildPoints = 0;
	bool                bPowerOn;
	UWORD               direction;

	ASSERT_OR_RETURN(nullptr, attackPlayer < MAX_PLAYERS, "attackPlayer (%" PRIu32 ") must be < MAX_PLAYERS", attackPlayer);
	CHECK_STRUCTURE(psStructure);
	visRemoveVisibility(psStructure);

	int prevState = intGetResearchState();
	bool reward = electronicReward(psStructure, attackPlayer);

	if (bMultiPlayer)
	{
		//certain structures give specific results - the rest swap sides!
		if (!electronic_warfare || !reward)
		{
			originalPlayer = psStructure->player;
			//tell the system the structure no longer exists
			(void)removeStruct(psStructure, false);

			// remove structure from one list
			removeStructureFromList(psStructure, apsStructLists);

			psStructure->selected = false;

			// change player id
			psStructure->player	= (UBYTE)attackPlayer;

			//restore the resistance value
			psStructure->resistance = (UWORD)structureResistance(psStructure->pStructureType, psStructure->player);

			// add to other list.
			addStructure(psStructure);

			//check through the 'attackPlayer' players list of droids to see if any are targetting it
			for (DROID* psCurr : apsDroidLists[attackPlayer])
			{
				if (psCurr->order.psObj == psStructure)
				{
					orderDroid(psCurr, DORDER_STOP, ModeImmediate);
					break;
				}
				for (unsigned i = 0; i < psCurr->numWeaps; ++i)
				{
					if (psCurr->psActionTarget[i] == psStructure)
					{
						orderDroid(psCurr, DORDER_STOP, ModeImmediate);
						break;
					}
				}
				//check through order list
				orderClearTargetFromDroidList(psCurr, psStructure);
			}

			//check through the 'attackPlayer' players list of structures to see if any are targetting it
			for (STRUCTURE* psStruct : apsStructLists[attackPlayer])
			{
				if (psStruct->psTarget[0] == psStructure)
				{
					setStructureTarget(psStruct, nullptr, 0, ORIGIN_UNKNOWN);
				}
			}

			transferFixupFunctionality(psStructure, psStructure->pStructureType->type, originalPlayer);

			if (psStructure->status == SS_BUILT)
			{
				buildingComplete(psStructure);
			}
			//since the structure isn't being rebuilt, the visibility code needs to be adjusted
			//make sure this structure is visible to selectedPlayer
			psStructure->visible[attackPlayer] = UINT8_MAX;
			triggerEventObjectTransfer(psStructure, originalPlayer);
		}
		intNotifyResearchButton(prevState);
		return nullptr;
	}

	//save info about the structure
	psType = psStructure->pStructureType;
	x = psStructure->pos.x;
	y = psStructure->pos.y;
	direction = psStructure->rot.direction;
	originalPlayer = psStructure->player;
	//save how complete the build process is
	if (psStructure->status == SS_BEING_BUILT)
	{
		buildPoints = psStructure->currentBuildPts;
	}
	//check module not attached
	psModule = getModuleStat(psStructure);
	//get rid of the structure
	(void)removeStruct(psStructure, true);

	//make sure power is not used to build
	bPowerOn = powerCalculated;
	powerCalculated = false;
	//build a new one for the attacking player - set last element to true so it doesn't adjust x/y
	psNewStruct = buildStructure(psType, x, y, attackPlayer, true);
	capacity = psStructure->capacity;
	if (psNewStruct)
	{
		psNewStruct->rot.direction = direction;
		if (capacity)
		{
			switch (psType->type)
			{
			case REF_POWER_GEN:
			case REF_RESEARCH:
				//build the module for powerGen and research
				buildStructure(psModule, psNewStruct->pos.x, psNewStruct->pos.y, attackPlayer, false);
				break;
			case REF_FACTORY:
			case REF_VTOL_FACTORY:
				//build the appropriate number of modules
				while (capacity)
				{
					buildStructure(psModule, psNewStruct->pos.x, psNewStruct->pos.y, attackPlayer, false);
					capacity--;
				}
				break;
			default:
				break;
			}
		}
		if (buildPoints)
		{
			psNewStruct->status = SS_BEING_BUILT;
			psNewStruct->currentBuildPts = buildPoints;
		}
		else
		{
			psNewStruct->status = SS_BUILT;
			buildingComplete(psNewStruct);
			triggerEventStructBuilt(psNewStruct, nullptr);
			checkPlayerBuiltHQ(psNewStruct);
		}

		if (!bMultiPlayer)
		{
			if (originalPlayer == selectedPlayer)
			{
				//make sure this structure is visible to selectedPlayer if the structure used to be selectedPlayers'
				ASSERT(selectedPlayer < MAX_PLAYERS, "selectedPlayer (%" PRIu32 ") must be < MAX_PLAYERS", selectedPlayer);
				psNewStruct->visible[selectedPlayer] = UBYTE_MAX;
			}
			if (!electronic_warfare || !reward)
			{
				triggerEventObjectTransfer(psNewStruct, originalPlayer);
			}
		}
	}
	powerCalculated = bPowerOn;
	intNotifyResearchButton(prevState);
	return psNewStruct;
}

/*returns the power cost to build this structure, or to add its next module */
UDWORD structPowerToBuildOrAddNextModule(const STRUCTURE *psStruct)
{
	if (psStruct->capacity)
	{
		STRUCTURE_STATS *psStats = getModuleStat(psStruct);
		ASSERT(psStats != nullptr, "getModuleStat returned null");
		if (psStats)
		{
			return psStats->powerToBuild; // return the cost to build the module
		}
	}
	// no module attached so building the base structure
	return psStruct->pStructureType->powerToBuild;
}


//for MULTIPLAYER ONLY
//this adjusts the time the relevant action started if the building is attacked by EW weapon
void resetResistanceLag(STRUCTURE *psBuilding)
{
	if (bMultiPlayer)
	{
		switch (psBuilding->pStructureType->type)
		{
		case REF_RESEARCH:
			break;
		case REF_FACTORY:
		case REF_VTOL_FACTORY:
		case REF_CYBORG_FACTORY:
			{
				FACTORY    *psFactory = &psBuilding->pFunctionality->factory;

				//if working on a unit
				if (psFactory->psSubject)
				{
					//adjust the start time for the current subject
					if (psFactory->timeStarted != ACTION_START_TIME)
					{
						psFactory->timeStarted += (gameTime - psBuilding->lastResistance);
					}
				}
			}
		default: //do nothing
			break;
		}
	}
}


/*checks the structure passed in is a Las Sat structure which is currently
selected - returns true if valid*/
bool lasSatStructSelected(const STRUCTURE *psStruct)
{
	if ((psStruct->selected || (bMultiPlayer && !isHumanPlayer(psStruct->player)))
	    && psStruct->asWeaps[0].nStat
	    && (psStruct->getWeaponStats(0)->weaponSubClass == WSC_LAS_SAT))
	{
		return true;
	}

	return false;
}

/* Call CALL_NEWDROID script callback */
void cbNewDroid(STRUCTURE *psFactory, DROID *psDroid)
{
	ASSERT_OR_RETURN(, psDroid != nullptr, "no droid assigned for CALL_NEWDROID callback");
	triggerEventDroidBuilt(psDroid, psFactory);
}

StructureBounds getStructureBounds(const STRUCTURE *object)
{
	const Vector2i size = object->size();
	const Vector2i map = map_coord(object->pos.xy()) - size / 2;
	return StructureBounds(map, size);
}

StructureBounds getStructureBounds(const STRUCTURE_STATS *stats, Vector2i pos, uint16_t direction)
{
	const Vector2i size = stats->size(direction);
	const Vector2i map = map_coord(pos) - size / 2;
	return StructureBounds(map, size);
}

void checkStructure(const STRUCTURE *psStructure, const char *const location_description, const char *function, const int recurse)
{
	if (recurse < 0)
	{
		return;
	}

	ASSERT_HELPER(psStructure != nullptr, location_description, function, "CHECK_STRUCTURE: NULL pointer");
	ASSERT_HELPER(psStructure->id != 0, location_description, function, "CHECK_STRUCTURE: Structure with ID 0");
	ASSERT_HELPER(psStructure->type == OBJ_STRUCTURE, location_description, function, "CHECK_STRUCTURE: No structure (type num %u)", (unsigned int)psStructure->type);
	ASSERT_HELPER(psStructure->player < MAX_PLAYERS, location_description, function, "CHECK_STRUCTURE: Out of bound player num (%u)", (unsigned int)psStructure->player);
	ASSERT_HELPER(psStructure->pStructureType->type < NUM_DIFF_BUILDINGS, location_description, function, "CHECK_STRUCTURE: Out of bound structure type (%u)", (unsigned int)psStructure->pStructureType->type);
	ASSERT_HELPER(psStructure->numWeaps <= MAX_WEAPONS, location_description, function, "CHECK_STRUCTURE: Out of bound weapon count (%u)", (unsigned int)psStructure->numWeaps);

	for (unsigned i = 0; i < ARRAY_SIZE(psStructure->asWeaps); ++i)
	{
		if (psStructure->psTarget[i])
		{
			checkObject(psStructure->psTarget[i], location_description, function, recurse - 1);
		}
	}
}

constexpr PHYSFS_sint64 MAX_FAVORITE_STRUCTS_FILE_SIZE = 1024 * 1024 * 2; // 2 MB seems like enough...

bool loadFavoriteStructsFile(const char* path)
{
	favoriteStructs.clear();

	// file size sanity check
	PHYSFS_Stat metaData;
	if (PHYSFS_stat(path, &metaData) != 0)
	{
		if (metaData.filesize > MAX_FAVORITE_STRUCTS_FILE_SIZE)
		{
			debug(LOG_ERROR, "%s: has too large a file size (%" PRIu64 ") - skipping load", path, static_cast<uint64_t>(metaData.filesize));
			return false;
		}
	}

	auto jsonObj = parseJsonFile(path);
	if (!jsonObj.has_value())
	{
		debug(LOG_WZ, "%s not found (or loadable)", path);
		return false;
	}

	auto& rootObj = jsonObj.value();

	try {
		uint32_t version = 0;
		auto it_version = rootObj.find("version");
		if (it_version != rootObj.end())
		{
			version = it_version->get<uint32_t>();
		}
		if (version > 1)
		{
			debug(LOG_ERROR, "%s: \"version\" (%" PRIu32 ") is newer than this version of WZ supports - ignoring file", path, version);
			return false;
		}
		auto it_structures = rootObj.find("favoriteStructures");
		if (it_structures == rootObj.end())
		{
			debug(LOG_ERROR, "%s: Missing expected \"favoriteStructures\" key", path);
			return false;
		}
		if (!it_structures->is_array())
		{
			debug(LOG_ERROR, "%s: \"favoriteStructures\" is not an array", path);
			return false;
		}
		for (size_t idx = 0; idx < it_structures->size(); idx++)
		{
			favoriteStructs.push_back(it_structures->at(idx).get<WzString>());
		}
	}
	catch (const std::exception &e) {
		debug(LOG_ERROR, "%s: Failed to parse JSON; error: %s", path, e.what());
		favoriteStructs.clear();
		return false;
	}

	return true;
}

bool writeFavoriteStructsFile(const char* path)
{
	nlohmann::json root = nlohmann::json::object();
	root["version"] = 1;

	auto structsArray = nlohmann::json::array();
	for (size_t index = 0; index < favoriteStructs.size(); ++index)
	{
		structsArray.push_back(favoriteStructs[index]);
	}
	root["favoriteStructures"] = std::move(structsArray);

	return saveJSONToFile(root, path);
}

static void parseFavoriteStructs()
{
	if (asStructureStats == nullptr)
	{
		debug(LOG_WARNING, "asStructureStats was null?");
		return;
	}

	for (unsigned i = 0; i < numStructureStats; ++i)
	{
		if (std::find(favoriteStructs.begin(), favoriteStructs.end(), asStructureStats[i].id) != favoriteStructs.end())
		{
			asStructureStats[i].isFavorite = true;
		}
		else
		{
			asStructureStats[i].isFavorite = false;
		}
	}
}

static void packFavoriteStructs()
{
	if (asStructureStats == nullptr)
	{
		debug(LOG_WARNING, "asStructureStats was null?");
		return;
	}

	favoriteStructs.clear();

	for (unsigned i = 0; i < numStructureStats; ++i)
	{
		if (asStructureStats[i].isFavorite)
		{
			if (asStructureStats[i].id.isEmpty())
			{
				ASSERT(false, "Invalid struct stats - empty id");
				continue;
			}
			favoriteStructs.push_back(asStructureStats[i].id);
		}
	}
}

// This follows the logic in droid.cpp nextModuleToBuild()
bool canStructureHaveAModuleAdded(const STRUCTURE* const structure)
{
	if (nullptr == structure || nullptr == structure->pStructureType || structure->status != SS_BUILT)
	{
		return false;
	}

	switch (structure->pStructureType->type)
	{
		case REF_FACTORY:
		case REF_CYBORG_FACTORY:
		case REF_VTOL_FACTORY:
			return structure->capacity < NUM_FACTORY_MODULES;

		case REF_POWER_GEN:
		case REF_RESEARCH:
			return structure->capacity == 0;

		default:
			return false;
	}
}

LineBuild calcLineBuild(Vector2i size, STRUCTURE_TYPE type, Vector2i worldPos, Vector2i worldPos2)
{
	ASSERT_OR_RETURN({}, size.x > 0 && size.y > 0, "Zero-size building");

	bool packed = type == REF_RESOURCE_EXTRACTOR || baseStructureTypePackability(type) <= PACKABILITY_DEFENSE;
	Vector2i tile = {TILE_UNITS, TILE_UNITS};
	Vector2i padding = packed? Vector2i{0, 0} : Vector2i{1, 1};
	Vector2i paddedSize = size + padding;
	Vector2i worldSize = world_coord(size);
	Vector2i worldPaddedSize = world_coord(paddedSize);

	LineBuild lb;
	lb.begin = round_to_nearest_tile(worldPos - worldSize/2) + worldSize/2;

	Vector2i delta = worldPos2 - lb.begin;
	Vector2i count = (abs(delta) + worldPaddedSize/2)/paddedSize + tile;
	lb.count = map_coord(std::max(count.x, count.y));
	if (lb.count <= 1)
	{
		lb.step = {0, 0};
	}
	else if (count.x > count.y)
	{
		lb.step.x = delta.x < 0? -worldPaddedSize.x : worldPaddedSize.x;
		lb.step.y = round_to_nearest_tile(delta.y/(lb.count - 1));
	}
	else
	{
		lb.step.x = round_to_nearest_tile(delta.x/(lb.count - 1));
		lb.step.y = delta.y < 0? -worldPaddedSize.y : worldPaddedSize.y;
	}

	//debug(LOG_INFO, "calcLineBuild(size: [%d, %d], packed: %d, worldPos: [%d, %d], worldPos2: [%d, %d]) -> {begin: [%d, %d], step: [%d, %d], count: %d}", size.x, size.y, packed, worldPos.x, worldPos.y, worldPos2.x, worldPos2.y, lb.begin.x, lb.begin.y, lb.step.x, lb.step.y, lb.count);

	return lb;
}

LineBuild calcLineBuild(STRUCTURE_STATS const *stats, uint16_t direction, Vector2i pos, Vector2i pos2)
{
	return calcLineBuild(stats->size(direction), stats->type, pos, pos2);
}

StructContainer& GlobalStructContainer()
{
	static StructContainer instance;
	return instance;
}
