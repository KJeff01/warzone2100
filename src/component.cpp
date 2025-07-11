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
 * @file component.c
 * Draws component objects - oh yes indeed.
*/

#include "lib/framework/frame.h"
#include "lib/ivis_opengl/piematrix.h"
#include "lib/ivis_opengl/ivisdef.h"
#include "lib/netplay/netplay.h"

#include "action.h"
#include "component.h"
#include "display3d.h"
#include "effects.h"
#include "intdisplay.h"
#include "loop.h"
#include "map.h"
#include "miscimd.h"
#include "projectile.h"
#include "transporter.h"
#include "mission.h"
#include "faction.h"
#ifndef GLM_ENABLE_EXPERIMENTAL
	#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/transform.hpp>

#define GetRadius(x) ((x)->sradius)

#define	DEFAULT_COMPONENT_TRANSLUCENCY	128
#define	DROID_EMP_SPREAD	(20 - rand()%40)

//VTOL weapon connector start
#define VTOL_CONNECTOR_START 5

static bool		leftFirst;

// Colour Lookups
// use col = MAX_PLAYERS for anycolour (see multiint.c)
bool setPlayerColour(UDWORD player, UDWORD col)
{
	if (player >= MAX_PLAYERS)
	{
		NetPlay.players[player].colour = MAX_PLAYERS;
		return true;
	}
	// Allow color values from 0 to 15
	ASSERT_OR_RETURN(false, col < 16, "Bad colour setting");
	NetPlay.players[player].colour = col;
	return true;
}

UBYTE getPlayerColour(UDWORD pl)
{
	if (pl >= MAX_PLAYERS)
	{
		return 0; // baba, or spectator slot
	}
	return NetPlay.players[pl].colour;
}

static glm::mat4 setMatrix(const Vector3i *Position, const Vector3i *Rotation, int scale)
{
	return glm::translate(glm::vec3(*Position)) *
		glm::rotate(UNDEG(DEG(Rotation->x)), glm::vec3(1.f, 0.f, 0.f)) *
		glm::rotate(UNDEG(DEG(Rotation->y)), glm::vec3(0.f, 1.f, 0.f)) *
		glm::rotate(UNDEG(DEG(Rotation->z)), glm::vec3(0.f, 0.f, 1.f)) *
		glm::scale(glm::vec3(scale / 100.f));
}

UDWORD getComponentDroidRadius(const DROID *)
{
	return 100;
}


UDWORD getComponentDroidTemplateRadius(const DROID_TEMPLATE *)
{
	return 100;
}


UDWORD getComponentRadius(const BASE_STATS *psComponent) // DISPLAY ONLY
{
	const iIMDShape *ComponentIMD = nullptr;
	const iIMDShape *MountIMD = nullptr;
	SDWORD compID;

	compID = StatIsComponent(psComponent);
	if (compID >= 0)
	{
		StatGetComponentIMD(psComponent, compID, &ComponentIMD, &MountIMD);
		if (ComponentIMD)
		{
			return GetRadius(ComponentIMD);
		}
	}

	/* VTOL bombs are only stats allowed to have NULL ComponentIMD */
	if (StatIsComponent(psComponent) != COMP_WEAPON
	    || (((const WEAPON_STATS *)psComponent)->weaponSubClass != WSC_BOMB
	        && ((const WEAPON_STATS *)psComponent)->weaponSubClass != WSC_EMP))
	{
		ASSERT(ComponentIMD, "No ComponentIMD!");
	}

	return COMPONENT_RADIUS;
}


UDWORD getResearchRadius(const BASE_STATS *Stat) // DISPLAY ONLY
{
	const iIMDShape *ResearchIMD = safeGetDisplayModelFromBase(((const RESEARCH *)Stat)->pIMD);

	if (ResearchIMD)
	{
		return GetRadius(ResearchIMD);
	}

	debug(LOG_ERROR, "ResearchPIE == NULL");

	return 100;
}


UDWORD getStructureSizeMax(const STRUCTURE *psStructure) // DISPLAY ONLY
{
	//radius based on base plate size
	return MAX(psStructure->pStructureType->baseWidth, psStructure->pStructureType->baseBreadth);
}

UDWORD getStructureStatSizeMax(const STRUCTURE_STATS *Stats) // DISPLAY ONLY
{
	//radius based on base plate size
	return MAX(Stats->baseWidth, Stats->baseBreadth);
}

UDWORD getStructureStatHeight(const STRUCTURE_STATS *psStat) // DISPLAY ONLY
{
	if (psStat->pIMD[0])
	{
		const auto pDisplayIMD = psStat->pIMD[0]->displayModel();
		return (pDisplayIMD->max.y - pDisplayIMD->min.y);
	}

	return 0;
}

static void draw_player_3d_shape(uint32_t player_index, const iIMDShape *shape, const glm::mat4 &modelMatrix)
{
	int team = getPlayerColour(player_index);
	for (const iIMDShape *imd = getFactionDisplayIMD(getPlayerFaction(player_index), shape); imd != nullptr; imd = imd->next.get())
	{
		const PIELIGHT teamcolour = imd->getTeamColourForModel(team);
		pie_Draw3DButton(imd, teamcolour, modelMatrix, glm::mat4(1.f));
	}
}

void displayIMDButton(const iIMDShape *IMDShape, const Vector3i *Rotation, const Vector3i *Position, int scale)
{
	draw_player_3d_shape(selectedPlayer, IMDShape, setMatrix(Position, Rotation, scale));
}

static void sharedStructureButton(const STRUCTURE_STATS *Stats, const iIMDBaseShape *strBaseImd, const Vector3i *Rotation, const Vector3i *Position, int scale)
{
	if (!strBaseImd) { return; }

	const iIMDBaseShape *baseImd, *mountImd[MAX_WEAPONS], *weaponImd[MAX_WEAPONS];
	Vector3i pos = *Position;

	const iIMDShape *strImd = strBaseImd->displayModel();

	if (!strImd)
	{
		return;
	}

	/* HACK HACK HACK!
	if its a 'tall thin (ie tower)' structure stat with something on the top - offset the position to show the object on top */
	if (!strImd->connectors.empty() && scale == SMALL_STRUCT_SCALE && getStructureStatHeight(Stats) > TOWER_HEIGHT)
	{
		pos.y -= 20;
	}

	const glm::mat4 matrix = setMatrix(&pos, Rotation, scale);

	/* Draw the building's base first */
	baseImd = Stats->pBaseIMD;

	if (baseImd != nullptr)
	{
		draw_player_3d_shape(selectedPlayer, baseImd->displayModel(), matrix);
	}
	draw_player_3d_shape(selectedPlayer, strImd, matrix);

	//and draw the turret
	if (!strImd->connectors.empty())
	{
		weaponImd[0] = nullptr;
		mountImd[0] = nullptr;
		for (int i = 0; i < Stats->numWeaps; i++)
		{
			weaponImd[i] = nullptr;//weapon is gun ecm or sensor
			mountImd[i] = nullptr;
		}
		//get an imd to draw on the connector priority is weapon, ECM, sensor
		//check for weapon
		//can only have the MAX_WEAPONS
		for (int i = 0; i < MAX(1, Stats->numWeaps); i++)
		{
			//can only have the one
			if (Stats->psWeapStat[i] != nullptr)
			{
				weaponImd[i] = Stats->psWeapStat[i]->pIMD;
				mountImd[i] = Stats->psWeapStat[i]->pMountGraphic;
			}

			if (weaponImd[i] == nullptr)
			{
				//check for ECM
				if (Stats->pECM != nullptr)
				{
					weaponImd[i] =  Stats->pECM->pIMD;
					mountImd[i] =  Stats->pECM->pMountGraphic;
				}
			}

			if (weaponImd[i] == nullptr)
			{
				//check for sensor
				if (Stats->pSensor != nullptr)
				{
					weaponImd[i] =  Stats->pSensor->pIMD;
					mountImd[i]  =  Stats->pSensor->pMountGraphic;
				}
			}
		}

		//draw Weapon/ECM/Sensor for structure
		if (weaponImd[0] != nullptr)
		{
			for (uint32_t i = 0; i < MAX(1, Stats->numWeaps); i++)
			{
				if (i >= strImd->connectors.size())
				{
					break;
				}
				glm::mat4 localMatrix = glm::translate(glm::vec3(strImd->connectors[i].xzy()));
				if (mountImd[i] != nullptr)
				{
					auto displayModel = mountImd[i]->displayModel();
					draw_player_3d_shape(selectedPlayer, displayModel, matrix * localMatrix);
					if (!displayModel->connectors.empty())
					{
						localMatrix *= glm::translate(glm::vec3(displayModel->connectors[0].xzy()));
					}
				}
				draw_player_3d_shape(selectedPlayer, weaponImd[i]->displayModel(), matrix * localMatrix);
				//we have a droid weapon so do we draw a muzzle flash
			}
		}
	}
}

void displayStructureButton(const STRUCTURE *psStructure, const Vector3i *rotation, const Vector3i *Position, int scale)
{
	sharedStructureButton(psStructure->pStructureType, psStructure->sDisplay.imd, rotation, Position, scale);
}

void displayStructureStatButton(const STRUCTURE_STATS *Stats, const Vector3i *rotation, const Vector3i *Position, int scale)
{
	ASSERT_OR_RETURN(, Stats != nullptr, "Stats is NULL");
	if (Stats->pIMD.empty() || Stats->pIMD[0] == nullptr)
	{
		return; // error with loaded stats/models - probably due to a mod
	}
	sharedStructureButton(Stats, Stats->pIMD[0], rotation, Position, scale);
}

// Render a component given a BASE_STATS structure.
//
void displayComponentButton(const BASE_STATS *Stat, const Vector3i *Rotation, const Vector3i *Position, int scale)
{
	const iIMDShape *ComponentIMD = nullptr;
	const iIMDShape *MountIMD = nullptr;
	int compID = StatIsComponent(Stat);

	if (compID >= 0)
	{
		StatGetComponentIMD(Stat, compID, &ComponentIMD, &MountIMD);
	}
	else
	{
		return;
	}
	glm::mat4 matrix = setMatrix(Position, Rotation, scale);

	/* VTOL bombs are only stats allowed to have NULL ComponentIMD */
	if (StatIsComponent(Stat) != COMP_WEAPON
	    || (((const WEAPON_STATS *)Stat)->weaponSubClass != WSC_BOMB
	        && ((const WEAPON_STATS *)Stat)->weaponSubClass != WSC_EMP))
	{
		ASSERT(ComponentIMD, "No ComponentIMD");
	}

	if (MountIMD)
	{
		draw_player_3d_shape(selectedPlayer, MountIMD, matrix);

		/* translate for weapon mount point */
		if (!MountIMD->connectors.empty())
		{
			matrix *= glm::translate(glm::vec3(MountIMD->connectors[0].xzy()));
		}
	}
	if (ComponentIMD)
	{
		draw_player_3d_shape(selectedPlayer, ComponentIMD, matrix);
	}
}


// Render a research item given a BASE_STATS structure.
//
void displayResearchButton(const BASE_STATS *Stat, const Vector3i *Rotation, const Vector3i *Position, int scale)
{
	ASSERT_OR_RETURN(, Stat != nullptr, "Stat is NULL");
	const iIMDBaseShape *ResearchIMD = ((const RESEARCH *)Stat)->pIMD;
	const iIMDBaseShape *MountIMD = ((const RESEARCH *)Stat)->pIMD2;

	ASSERT(ResearchIMD, "ResearchIMD is NULL");
	if (ResearchIMD)
	{
		const glm::mat4 &matrix = setMatrix(Position, Rotation, scale);

		if (MountIMD)
		{
			draw_player_3d_shape(selectedPlayer, MountIMD->displayModel(), matrix);
		}
		draw_player_3d_shape(selectedPlayer, ResearchIMD->displayModel(), matrix);
	}
}


static inline const iIMDBaseShape *getLeftPropulsionIMD(const DROID *psDroid)
{
	int bodyStat = psDroid->asBits[COMP_BODY];
	int propStat = psDroid->asBits[COMP_PROPULSION];
	return asBodyStats[bodyStat].ppIMDList[propStat * NUM_PROP_SIDES + LEFT_PROP];
}

static inline const iIMDBaseShape *getRightPropulsionIMD(const DROID *psDroid)
{
	int bodyStat = psDroid->asBits[COMP_BODY];
	int propStat = psDroid->asBits[COMP_PROPULSION];
	return asBodyStats[bodyStat].ppIMDList[propStat * NUM_PROP_SIDES + RIGHT_PROP];
}

void drawMuzzleFlash(WEAPON sWeap, const iIMDShape *weaponImd, const iIMDShape *flashImd, PIELIGHT buildingBrightness, int pieFlag, int iPieData, glm::mat4 modelMatrix, const glm::mat4 &viewMatrix, float heightAboveTerrain, UBYTE colour)
{
	if (!weaponImd || !flashImd || weaponImd->connectors.empty() || graphicsTime < sWeap.lastFired)
	{
		return;
	}

	int connector_num = 0;

	// which barrel is firing if model have multiple muzzle connectors?
	if (sWeap.shotsFired && (weaponImd->connectors.size() > 1))
	{
		// shoot first, draw later - substract one shot to get correct results
		connector_num = (sWeap.shotsFired - 1) % (static_cast<uint32_t>(weaponImd->connectors.size()));
	}

	/* Now we need to move to the end of the firing barrel */
	modelMatrix *= glm::translate(glm::vec3(weaponImd->connectors[connector_num].xzy()));
	heightAboveTerrain += weaponImd->connectors[connector_num].z;

	// assume no clan colours for muzzle effects
	if (flashImd->numFrames == 0 || flashImd->animInterval <= 0)
	{
		// no anim so display one frame for a fixed time
		if (graphicsTime >= sWeap.lastFired && graphicsTime < sWeap.lastFired + BASE_MUZZLE_FLASH_DURATION)
		{
			pie_Draw3DShape(flashImd, 0, colour, buildingBrightness, pieFlag | pie_ADDITIVE, EFFECT_MUZZLE_ADDITIVE, modelMatrix, viewMatrix, -heightAboveTerrain);
		}
	}
	else if (graphicsTime >= sWeap.lastFired)
	{
		// animated muzzle
		const int DEFAULT_ANIM_INTERVAL = 17; // A lot of PIE files specify 1, which is too small, so set something bigger as a fallback
		int animRate = MAX(flashImd->animInterval, DEFAULT_ANIM_INTERVAL);
		int frame = (graphicsTime - sWeap.lastFired) / animRate;
		if (frame < flashImd->numFrames)
		{
			pie_Draw3DShape(flashImd, frame, colour, buildingBrightness, pieFlag | pie_ADDITIVE, EFFECT_MUZZLE_ADDITIVE, modelMatrix, viewMatrix, -heightAboveTerrain);
		}
	}
}

/* Assumes matrix context is already set */
// this is able to handle multiple weapon graphics now
// removed mountRotation,they get such stuff from psObj directly now
static bool displayCompObj(const DROID *psDroid, bool bButton, const glm::mat4& modelMatrix2, const glm::mat4 &viewMatrix)
{
	iIMDBaseShape *psMoveAnim, *psStillAnim;
	SDWORD				iConnector;
	PROPULSION_STATS	*psPropStats;
	SDWORD				pieFlag, iPieData;
	SDWORD				shieldPieFlag = 0, iShieldPieData = 0;
	PIELIGHT			brightness;
	UDWORD				colour;
	size_t	i = 0;
	bool				didDrawSomething = false;

	if (!bButton && psDroid->shieldPoints > 0 && droidGetMaxShieldPoints(psDroid) > 0)
	{
		float factor = static_cast<float>(psDroid->shieldPoints) / droidGetMaxShieldPoints(psDroid);
		iShieldPieData = static_cast<SDWORD>(std::round(255.0f * factor));
		shieldPieFlag = pie_FORCELIGHT | pie_TRANSLUCENT | pie_SHIELD;
	}

	glm::mat4 modifiedModelMatrix = modelMatrix2;

	if (psDroid->timeLastHit - graphicsTime < ELEC_DAMAGE_DURATION && psDroid->lastHitWeapon == WSC_ELECTRONIC && !gamePaused())
	{
		colour = getPlayerColour(rand() % MAX_PLAYERS);
	}
	else
	{
		colour = getPlayerColour(psDroid->player);
	}

	/* get propulsion stats */
	psPropStats = psDroid->getPropulsionStats();
	ASSERT_OR_RETURN(didDrawSomething, psPropStats != nullptr, "invalid propulsion stats pointer");

	//set pieflag for button object or ingame object
	if (bButton)
	{
		pieFlag = pie_BUTTON;
		brightness = WZCOL_WHITE;
	}
	else
	{
		pieFlag = pie_SHADOW;
		brightness = pal_SetBrightness(psDroid->illumination);
		// NOTE: Beware of transporters that are offscreen, on a mission!  We should *not* be checking tiles at this point in time!
		if (!psDroid->isTransporter() && !missionIsOffworld())
		{
			MAPTILE *psTile = worldTile(psDroid->pos.x, psDroid->pos.y);
			if (psTile->jammerBits & alliancebits[psDroid->player])
			{
				pieFlag |= pie_ECM;
			}
		}
	}

	/* set default components transparent */
	if (psDroid->asBits[COMP_PROPULSION] == 0)
	{
		pieFlag  |= pie_TRANSLUCENT;
		iPieData  = DEFAULT_COMPONENT_TRANSLUCENCY;
	}
	else
	{
		iPieData = 0;
	}

	if (!bButton && psPropStats->propulsionType == PROPULSION_TYPE_PROPELLOR)
	{
		// FIXME: change when adding submarines to the game
		modifiedModelMatrix *= glm::translate(glm::vec3(0.f, -world_coord(1) / 2.3f, 0.f));
	}

	const iIMDBaseShape *psShapeProp = (leftFirst ? getLeftPropulsionIMD(psDroid) : getRightPropulsionIMD(psDroid));
	if (psShapeProp)
	{
		if (pie_Draw3DShape(psShapeProp->displayModel(), 0, colour, brightness, pieFlag, iPieData, modifiedModelMatrix, viewMatrix, -(psDroid->heightAboveMap)))
		{
			didDrawSomething = true;
		}
		if (!bButton && psDroid->shieldPoints > 0)
		{
			if (pie_Draw3DShape(psShapeProp->displayModel(), 0, colour, brightness, shieldPieFlag, iShieldPieData, modifiedModelMatrix, viewMatrix, -(psDroid->heightAboveMap)))
			{
				didDrawSomething = true;
			}
		}
	}

	/* set default components transparent */
	if (psDroid->asBits[COMP_BODY] == 0)
	{
		pieFlag  |= pie_TRANSLUCENT;
		iPieData  = DEFAULT_COMPONENT_TRANSLUCENCY;
	}
	else
	{
		pieFlag  &= ~pie_TRANSLUCENT;
		iPieData = 0;
	}

	/* Get the body graphic now*/
	const iIMDBaseShape *psBaseShapeBody = BODY_IMD(psDroid, psDroid->player);
	const iIMDShape *psShapeBody = (psBaseShapeBody) ? psBaseShapeBody->displayModel() : nullptr;
	if (psShapeBody)
	{
		const iIMDShape *strImd = psShapeBody;
		if (psDroid->droidType == DROID_PERSON)
		{
			// previously, used to scale the model by 0.75 - no longer needed with the fixed models though...
		}
		if (strImd->objanimpie[psDroid->animationEvent])
		{
			strImd = strImd->objanimpie[psDroid->animationEvent]->displayModel();
		}
		while (strImd)
		{
			if (drawShape(strImd, psDroid->timeAnimationStarted, colour, brightness, pieFlag, iPieData, modifiedModelMatrix, viewMatrix, -(psDroid->heightAboveMap)))
			{
				didDrawSomething = true;
			}
			if (!bButton && psDroid->shieldPoints > 0)
			{
				if (drawShape(strImd, psDroid->timeAnimationStarted, colour, brightness, shieldPieFlag, iShieldPieData, modifiedModelMatrix, viewMatrix, -(psDroid->heightAboveMap)))
				{
					didDrawSomething = true;
				}
			}
			strImd = strImd->next.get();
		}
	}

	/* Render animation effects based on movement or lack thereof, if any */
	const auto* bodyStats = psDroid->getBodyStats();
	psMoveAnim = bodyStats->ppMoveIMDList[psDroid->asBits[COMP_PROPULSION]];
	psStillAnim = bodyStats->ppStillIMDList[psDroid->asBits[COMP_PROPULSION]];
	if (!bButton && psMoveAnim && psDroid->sMove.Status != MOVEINACTIVE)
	{
		const iIMDShape *displayModel = psMoveAnim->displayModel();
		if (pie_Draw3DShape(displayModel, getModularScaledGraphicsTime(displayModel->animInterval, displayModel->numFrames), colour, brightness, pie_ADDITIVE, 200, modifiedModelMatrix, viewMatrix, -(psDroid->heightAboveMap)))
		{
			didDrawSomething = true;
		}
	}
	else if (!bButton && psStillAnim) // standing still
	{
		const iIMDShape *displayModel = psStillAnim->displayModel();
		if (pie_Draw3DShape(displayModel, getModularScaledGraphicsTime(displayModel->animInterval, displayModel->numFrames), colour, brightness, 0, 0, modifiedModelMatrix, viewMatrix, -(psDroid->heightAboveMap)))
		{
			didDrawSomething = true;
		}
	}

	/* set default components transparent */
	if (psDroid->asWeaps[0].nStat        == 0 &&
	    psDroid->asBits[COMP_SENSOR]     == 0 &&
	    psDroid->asBits[COMP_ECM]        == 0 &&
	    psDroid->asBits[COMP_BRAIN]      == 0 &&
	    psDroid->asBits[COMP_REPAIRUNIT] == 0 &&
	    psDroid->asBits[COMP_CONSTRUCT]  == 0)
	{
		pieFlag  |= pie_TRANSLUCENT;
		iPieData  = DEFAULT_COMPONENT_TRANSLUCENCY;
	}
	else
	{
		pieFlag  &= ~pie_TRANSLUCENT;
		iPieData = 0;
	}

	if (psShapeBody && !psShapeBody->connectors.empty())
	{
		/* vtol weapons attach to connector 2 (underneath);
		 * all others to connector 1 */
		/* VTOL's now skip the first 5 connectors(0 to 4),
		VTOL's use 5,6,7,8 etc now */
		if (psPropStats->propulsionType == PROPULSION_TYPE_LIFT && psDroid->droidType == DROID_WEAPON)
		{
			iConnector = VTOL_CONNECTOR_START;
		}
		else
		{
			iConnector = 0;
		}

		switch (psDroid->droidType)
		{
		case DROID_DEFAULT:
		case DROID_TRANSPORTER:
		case DROID_SUPERTRANSPORTER:
		case DROID_CYBORG:
		case DROID_CYBORG_SUPER:
		case DROID_WEAPON:
		case DROID_COMMAND:		// command droids have a weapon to store all the graphics
			/*	Get the mounting graphic - we've already moved to the right position
			Allegedly - all droids will have a mount graphic so this shouldn't
			fall on it's arse......*/
			/* Double check that the weapon droid actually has any */
			for (i = 0; i < psDroid->numWeaps; i++)
			{
				if ((psDroid->asWeaps[i].nStat > 0 || psDroid->droidType == DROID_DEFAULT)
				    && !psShapeBody->connectors.empty())
				{
					Rotation rot = getInterpolatedWeaponRotation(psDroid, i, graphicsTime);

					glm::mat4 localModelMatrix = modifiedModelMatrix;
					float localHeightAboveTerrain = psDroid->heightAboveMap;

					//to skip number of VTOL_CONNECTOR_START ground unit connectors
					size_t bodyConnectorIndex = (iConnector < VTOL_CONNECTOR_START) ? i : iConnector + i;
					if (bodyConnectorIndex < psShapeBody->connectors.size())
					{
						localModelMatrix *= glm::translate(glm::vec3(psShapeBody->connectors[bodyConnectorIndex].xzy()));
						localHeightAboveTerrain += psShapeBody->connectors[bodyConnectorIndex].z;
					}
					else
					{
						debug(LOG_INFO, "Model lacks sufficient connectors?: %s", psShapeBody->modelName.toUtf8().c_str());
					}
					localModelMatrix *= glm::rotate(UNDEG(-rot.direction), glm::vec3(0.f, 1.f, 0.f));

					/* vtol weapons inverted */
					if (iConnector >= VTOL_CONNECTOR_START)
					{
						//this might affect gun rotation
						localModelMatrix *= glm::rotate(UNDEG(65536 / 2), glm::vec3(0.f, 0.f, 1.f));
					}

					/* Get the mount graphic */
					const iIMDBaseShape *psBaseShape = WEAPON_MOUNT_IMD(psDroid, i);
					const iIMDShape *psShape = (psBaseShape) ? psBaseShape->displayModel() : nullptr;

					int recoilValue = getRecoil(psDroid->asWeaps[i]);
					localModelMatrix *= glm::translate(glm::vec3(0.f, 0.f, recoilValue / 3.f));

					/* Draw it */
					if (psShape)
					{
						if (pie_Draw3DShape(psShape, 0, colour, brightness, pieFlag, iPieData, localModelMatrix, viewMatrix, -localHeightAboveTerrain))
						{
							didDrawSomething = true;
						}
						if (!bButton && psDroid->shieldPoints > 0)
						{
							if (pie_Draw3DShape(psShape, 0, colour, brightness, shieldPieFlag, iShieldPieData, localModelMatrix, viewMatrix, -localHeightAboveTerrain))
							{
								didDrawSomething = true;
							}
						}
					}
					localModelMatrix *= glm::translate(glm::vec3(0, 0, recoilValue));

					/* translate for weapon mount point */
					if (psShape && !psShape->connectors.empty())
					{
						localModelMatrix *= glm::translate(glm::vec3(psShape->connectors[0].xzy()));
						localHeightAboveTerrain += psShape->connectors[0].z;
					}

					/* vtol weapons inverted */
					if (iConnector >= VTOL_CONNECTOR_START)
					{
						//pitch the barrel down
						localModelMatrix *= glm::rotate(UNDEG(-rot.pitch), glm::vec3(1.f, 0.f, 0.f));
					}
					else
					{
						//pitch the barrel up
						localModelMatrix *= glm::rotate(UNDEG(rot.pitch), glm::vec3(1.f, 0.f, 0.f));
					}

					/* Get the weapon (gun?) graphic */
					psBaseShape = WEAPON_IMD(psDroid, i);
					psShape = (psBaseShape) ? psBaseShape->displayModel() : nullptr;

					// We have a weapon so we draw it and a muzzle flash from weapon connector
					if (psShape)
					{
						if (pie_Draw3DShape(psShape, 0, colour, brightness, pieFlag, iPieData, localModelMatrix, viewMatrix, -localHeightAboveTerrain))
						{
							didDrawSomething = true;
						}
						if (!bButton && psDroid->shieldPoints > 0)
						{
							if (pie_Draw3DShape(psShape, 0, colour, brightness, shieldPieFlag, iShieldPieData, localModelMatrix, viewMatrix, -localHeightAboveTerrain))
							{
								didDrawSomething = true;
							}
						}
						auto flashBaseModel = MUZZLE_FLASH_PIE(psDroid, i);
						const iIMDShape *pMuzzleFlash = (flashBaseModel) ? flashBaseModel->displayModel() : nullptr;
						drawMuzzleFlash(psDroid->asWeaps[i], psShape, pMuzzleFlash, brightness, pieFlag, iPieData, localModelMatrix, viewMatrix, localHeightAboveTerrain);
					}
				}
			}
			break;

		case DROID_SENSOR:
		case DROID_CONSTRUCT:
		case DROID_CYBORG_CONSTRUCT:
		case DROID_ECM:
		case DROID_REPAIR:
		case DROID_CYBORG_REPAIR:
			{
				Rotation rot = getInterpolatedWeaponRotation(psDroid, 0, graphicsTime);
				iIMDBaseShape *psBaseShape = nullptr;
				iIMDBaseShape *psBaseMountShape = nullptr;

				switch (psDroid->droidType)
				{
				default:
					ASSERT(false, "Bad component type");
					break;
				case DROID_SENSOR:
					psBaseMountShape = SENSOR_MOUNT_IMD(psDroid, psDroid->player);
					/* Get the sensor graphic, assuming it's there */
					psBaseShape = SENSOR_IMD(psDroid, psDroid->player);
					break;
				case DROID_CONSTRUCT:
				case DROID_CYBORG_CONSTRUCT:
					psBaseMountShape = CONSTRUCT_MOUNT_IMD(psDroid, psDroid->player);
					/* Get the construct graphic assuming it's there */
					psBaseShape = CONSTRUCT_IMD(psDroid, psDroid->player);
					break;
				case DROID_ECM:
					psBaseMountShape = ECM_MOUNT_IMD(psDroid, psDroid->player);
					/* Get the ECM graphic assuming it's there.... */
					psBaseShape = ECM_IMD(psDroid, psDroid->player);
					break;
				case DROID_REPAIR:
				case DROID_CYBORG_REPAIR:
					psBaseMountShape = REPAIR_MOUNT_IMD(psDroid, psDroid->player);
					/* Get the Repair graphic assuming it's there.... */
					psBaseShape = REPAIR_IMD(psDroid, psDroid->player);
					break;
				}

				const iIMDShape *psShape = (psBaseShape) ? psBaseShape->displayModel() : nullptr;
				const iIMDShape *psMountShape = (psBaseMountShape) ? psBaseMountShape->displayModel() : nullptr;

				/*	Get the mounting graphic - we've already moved to the right position
				Allegedly - all droids will have a mount graphic so this shouldn't
				fall on it's arse......*/
				//sensor and cyborg and ecm uses connectors[0]

				glm::mat4 localModelMatrix = modifiedModelMatrix;
				float localHeightAboveTerrain = psDroid->heightAboveMap;
				/* vtol weapons inverted */
				if (iConnector >= VTOL_CONNECTOR_START)
				{
					//this might affect gun rotation
					localModelMatrix *= glm::rotate(UNDEG(65536 / 2), glm::vec3(0.f, 0.f, 1.f));
				}

				localModelMatrix *= glm::translate(glm::vec3(psShapeBody->connectors[0].xzy()));
				localHeightAboveTerrain += psShapeBody->connectors[0].z;

				localModelMatrix *= glm::rotate(UNDEG(-rot.direction), glm::vec3(0.f, 1.f, 0.f));
				/* Draw it */
				if (psMountShape)
				{
					if (pie_Draw3DShape(psMountShape, 0, colour, brightness, pieFlag, iPieData, localModelMatrix, viewMatrix, -localHeightAboveTerrain))
					{
						didDrawSomething = true;
					}
				}

				/* translate for construct mount point if cyborg */
				if (psDroid->isCyborg() && psMountShape && !psMountShape->connectors.empty())
				{
					localModelMatrix *= glm::translate(glm::vec3(psMountShape->connectors[0].xzy()));
					localHeightAboveTerrain += psMountShape->connectors[0].z;
				}

				/* Draw it */
				if (psShape)
				{
					if (pie_Draw3DShape(psShape, 0, colour, brightness, pieFlag, iPieData, localModelMatrix, viewMatrix, -localHeightAboveTerrain))
					{
						didDrawSomething = true;
					}

					// In repair droid case only:
					if ((psDroid->droidType == DROID_REPAIR || psDroid->droidType == DROID_CYBORG_REPAIR) &&
					    !psShape->connectors.empty() && psDroid->action == DACTION_DROIDREPAIR)
					{
						Spacetime st = interpolateObjectSpacetime(psDroid, graphicsTime);
						localModelMatrix *= glm::translate(glm::vec3(psShape->connectors[0].xzy()));
						localHeightAboveTerrain += psShape->connectors[0].z;
						localModelMatrix *= glm::translate(glm::vec3(0.f, -20.f, 0.f));

						psShape = getImdFromIndex(MI_FLAME)->displayModel();

						/* Rotate for droid */
						localModelMatrix *= glm::rotate(UNDEG(st.rot.direction), glm::vec3(0.f, 1.f, 0.f));
						localModelMatrix *= glm::rotate(UNDEG(-st.rot.pitch), glm::vec3(1.f, 0.f, 0.f));
						localModelMatrix *= glm::rotate(UNDEG(-st.rot.roll), glm::vec3(0.f, 0.f, 1.f));
						//rotate Y
						localModelMatrix *= glm::rotate(UNDEG(rot.direction), glm::vec3(0.f, 1.f, 0.f));

						localModelMatrix *= glm::rotate(UNDEG(-playerPos.r.y), glm::vec3(0.f, 1.f, 0.f));
						localModelMatrix *= glm::rotate(UNDEG(-playerPos.r.x), glm::vec3(1.f, 0.f, 0.f));

						if (pie_Draw3DShape(psShape, getModularScaledGraphicsTime(psShape->animInterval, psShape->numFrames), 0, brightness, pie_ADDITIVE, 140, localModelMatrix, viewMatrix, -localHeightAboveTerrain))
						{
							didDrawSomething = true;
						}

//						localModelMatrix *= glm::rotate(UNDEG(playerPos.r.x), glm::vec3(1.f, 0.f, 0.f)); // Not used?
//						localModelMatrix *= glm::rotate(UNDEG(playerPos.r.y), glm::vec3(0.f, 1.f, 0.f)); // Not used?
					}
				}
				break;
			}
		case DROID_PERSON:
			// no extra mounts for people
			break;
		default:
			ASSERT(!"invalid droid type", "Whoa! Weirdy type of droid found in drawComponentObject!!!");
			break;
		}
	}

	/* set default components transparent */
	if (psDroid->asBits[COMP_PROPULSION] == 0)
	{
		pieFlag  |= pie_TRANSLUCENT;
		iPieData  = DEFAULT_COMPONENT_TRANSLUCENCY;
	}
	else
	{
		pieFlag  &= ~pie_TRANSLUCENT;
		iPieData = 0;
	}

	// now render the other propulsion side
	psShapeProp = (leftFirst ? getRightPropulsionIMD(psDroid) : getLeftPropulsionIMD(psDroid));
	if (psShapeProp)
	{
		if (pie_Draw3DShape(psShapeProp->displayModel(), 0, colour, brightness, pieFlag, iPieData, modifiedModelMatrix, viewMatrix, -(psDroid->heightAboveMap)))
		{
			didDrawSomething = true;
		}
		if (!bButton && psDroid->shieldPoints > 0)
		{
			if (pie_Draw3DShape(psShapeProp->displayModel(), 0, colour, brightness, shieldPieFlag, iShieldPieData, modifiedModelMatrix, viewMatrix, -(psDroid->heightAboveMap)))
			{
				didDrawSomething = true;
			}
		}
	}

	return didDrawSomething;
}


// Render a composite droid given a DROID_TEMPLATE structure.
//
void displayComponentButtonTemplate(const DROID_TEMPLATE *psTemplate, const Vector3i *Rotation, const Vector3i *Position, int scale)
{
	const glm::mat4 matrix = setMatrix(Position, Rotation, scale);

	// Decide how to sort it.
	leftFirst = angleDelta(DEG(Rotation->y)) < 0;

	DROID Droid(0, selectedPlayer);
	memset(Droid.asBits, 0, sizeof(Droid.asBits));
	droidSetBits(psTemplate, &Droid);

	Droid.pos = Vector3i(0, 0, 0);
	Droid.rot = Vector3i(0, 0, 0);

	//draw multi component object as a button object
	displayCompObj(&Droid, true, matrix, glm::mat4(1.f));
}


// Render a composite droid given a DROID structure.
//
void displayComponentButtonObject(const DROID *psDroid, const Vector3i *Rotation, const Vector3i *Position, int scale)
{
	SDWORD		difference;

	const glm::mat4 matrix = setMatrix(Position, Rotation, scale);

	// Decide how to sort it.
	difference = Rotation->y % 360;

	leftFirst = !((difference > 0 && difference < 180) || difference < -180);

	// And render the composite object.
	//draw multi component object as a button object
	displayCompObj(psDroid, true, matrix, glm::mat4(1.f));
}

/* Assumes matrix context is already set */
// multiple turrets display removed the pointless mountRotation
void displayComponentObject(DROID *psDroid, const glm::mat4 &viewMatrix, const glm::mat4 &perspectiveViewMatrix)
{
	Vector3i position, rotation;
	Spacetime st = interpolateObjectSpacetime(psDroid, graphicsTime);

	leftFirst = angleDelta(playerPos.r.y - st.rot.direction) <= 0;

	/* Get the real position */
	position.x = st.pos.x;
	position.z = -(st.pos.y);
	position.y = st.pos.z;

	if (psDroid->isTransporter())
	{
		position.y += bobTransporterHeight();
	}

	/* Get all the pitch,roll,yaw info */
	rotation.y = -st.rot.direction;
	rotation.x = st.rot.pitch;
	rotation.z = st.rot.roll;

	/* Translate origin */
	/* Rotate for droid */
	glm::mat4 modelMatrix = glm::translate(glm::vec3(position)) *
		glm::rotate(UNDEG(rotation.y), glm::vec3(0.f, 1.f, 0.f)) *
		glm::rotate(UNDEG(rotation.x), glm::vec3(1.f, 0.f, 0.f)) *
		glm::rotate(UNDEG(rotation.z), glm::vec3(0.f, 0.f, 1.f));

	if (psDroid->timeLastHit - graphicsTime < ELEC_DAMAGE_DURATION && psDroid->lastHitWeapon == WSC_ELECTRONIC)
	{
		modelMatrix *= objectShimmy((BASE_OBJECT *) psDroid);
	}

	// now check if the projected circle is within the screen boundaries
	if(!clipDroidOnScreen(psDroid, perspectiveViewMatrix * modelMatrix, (getIsCloseDistance()) ? 150 : 25))
	{
		return;
	}

	if (psDroid->lastHitWeapon == WSC_EMP && graphicsTime - psDroid->timeLastHit < EMP_DISABLE_TIME)
	{
		Vector3i effectPosition;

		//add an effect on the droid
		effectPosition.x = st.pos.x + DROID_EMP_SPREAD;
		effectPosition.y = st.pos.z + rand() % 8;
		effectPosition.z = st.pos.y + DROID_EMP_SPREAD;
		effectGiveAuxVar(90 + rand() % 20);
		addEffect(&effectPosition, EFFECT_EXPLOSION, EXPLOSION_TYPE_PLASMA, false, nullptr, 0);
	}

	if (psDroid->visibleForLocalDisplay() == UBYTE_MAX)
	{
		//ingame not button object
		//should render 3 mounted weapons now
		if (displayCompObj(psDroid, false, modelMatrix, viewMatrix))
		{
			// did draw something to the screen - update the framenumber
			psDroid->sDisplay.frameNumber = frameGetFrameNumber();
		}

		/* set up all the screen coords stuff */
		calcScreenCoords(psDroid, perspectiveViewMatrix * modelMatrix);
	}
	else
	{
		int frame = graphicsTime / BLIP_ANIM_DURATION + psDroid->id % 8192; // de-sync the blip effect, but don't overflow the int
		if (pie_Draw3DShape(getImdFromIndex(MI_BLIP)->displayModel(), frame, 0, WZCOL_WHITE, pie_ADDITIVE, psDroid->visibleForLocalDisplay() / 2, modelMatrix, viewMatrix))
		{
			psDroid->sDisplay.frameNumber = frameGetFrameNumber();
		}
	}
}


void destroyFXDroid(DROID *psDroid, unsigned impactTime, Vector3f &velocity)
{
	for (int i = 0; i < 5; ++i)
	{
		const iIMDBaseShape *psImd = nullptr;

		int maxHorizontalScatter = TILE_UNITS / 4;
		int heightScatter = TILE_UNITS / 5;
		Vector2i horizontalScatter = iSinCosR(rand(), rand() % maxHorizontalScatter);

		Vector3i pos = (psDroid->pos + Vector3i(horizontalScatter, 16 + heightScatter)).xzy();
		switch (i)
		{
		case 0:
			switch (psDroid->droidType)
			{
			case DROID_DEFAULT:
			case DROID_CYBORG:
			case DROID_CYBORG_SUPER:
			case DROID_CYBORG_CONSTRUCT:
			case DROID_CYBORG_REPAIR:
			case DROID_WEAPON:
			case DROID_COMMAND:
				if (psDroid->numWeaps > 0)
				{
					if (psDroid->asWeaps[0].nStat > 0)
					{
						psImd = WEAPON_MOUNT_IMD(psDroid, 0);
					}
				}
				break;
			default:
				break;
			}
			break;
		case 1:
			switch (psDroid->droidType)
			{
			case DROID_DEFAULT:
			case DROID_CYBORG:
			case DROID_CYBORG_SUPER:
			case DROID_CYBORG_CONSTRUCT:
			case DROID_CYBORG_REPAIR:
			case DROID_WEAPON:
			case DROID_COMMAND:
				if (psDroid->numWeaps)
				{
					// get main weapon
					psImd = WEAPON_IMD(psDroid, 0);
				}
				break;
			default:
				break;
			}
			break;
		}
		if (psImd == nullptr)
		{
			psImd = getRandomDebrisImd();
		}
		// Tell the effect system that it needs to use this player's color for the next effect
		SetEffectForPlayer(psDroid->player);
		addEffect(&pos, EFFECT_GRAVITON, GRAVITON_TYPE_EMITTING_DR, true, psImd->displayModel(), getPlayerColour(psDroid->player), impactTime, nullptr, &velocity);
	}
}


void	compPersonToBits(DROID *psDroid, Vector3f &velocity)
{
	Vector3i position;	//,rotation,velocity;
	iIMDBaseShape	*headImd, *legsImd, *armImd, *bodyImd;
	UDWORD		col;

	if (!psDroid->visibleForLocalDisplay()) // display only - should not affect game state
	{
		/* We can't see the person or cyborg - so get out */
		return;
	}
	/* get bits pointers according to whether baba or cyborg*/
	if (psDroid->isCyborg())
	{
		// This is probably unused now, since there's a more appropriate effect for cyborgs.
		headImd = getImdFromIndex(MI_CYBORG_HEAD);
		legsImd = getImdFromIndex(MI_CYBORG_LEGS);
		armImd  = getImdFromIndex(MI_CYBORG_ARM);
		bodyImd = getImdFromIndex(MI_CYBORG_BODY);
	}
	else
	{
		headImd = getImdFromIndex(MI_BABA_HEAD);
		legsImd = getImdFromIndex(MI_BABA_LEGS);
		armImd  = getImdFromIndex(MI_BABA_ARM);
		bodyImd = getImdFromIndex(MI_BABA_BODY);
	}

	/* Get where he's at */
	position.x = psDroid->pos.x;
	position.y = psDroid->pos.z + 1;
	position.z = psDroid->pos.y;

	/* Tell about player colour */
	col = getPlayerColour(psDroid->player);

	addEffect(&position, EFFECT_GRAVITON, GRAVITON_TYPE_GIBLET, true, headImd->displayModel(), col, gameTime - deltaGameTime + 1, nullptr, &velocity);
	addEffect(&position, EFFECT_GRAVITON, GRAVITON_TYPE_GIBLET, true, legsImd->displayModel(), col, gameTime - deltaGameTime + 1, nullptr, &velocity);
	addEffect(&position, EFFECT_GRAVITON, GRAVITON_TYPE_GIBLET, true, armImd->displayModel(), col, gameTime - deltaGameTime + 1, nullptr, &velocity);
	addEffect(&position, EFFECT_GRAVITON, GRAVITON_TYPE_GIBLET, true, bodyImd->displayModel(), col, gameTime - deltaGameTime + 1, nullptr, &velocity);
}


SDWORD	rescaleButtonObject(SDWORD radius, SDWORD baseScale, SDWORD baseRadius)
{
	SDWORD newScale;
	newScale = 100 * baseRadius;
	newScale /= radius;
	if (baseScale > 0)
	{
		newScale += baseScale;
		newScale /= 2;
	}
	return newScale;
}
