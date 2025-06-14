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

#ifndef __INCLUDED_LIB_SOUND_AUDIO_ID_H__
#define __INCLUDED_LIB_SOUND_AUDIO_ID_H__

enum INGAME_AUDIO
{
	NO_SOUND = -1,

	/* Beeps */

	ID_SOUND_WINDOWCLOSE,
	ID_SOUND_WINDOWOPEN,
	ID_SOUND_BUTTON_CLICK_3,
	ID_SOUND_SELECT,
	ID_SOUND_BUTTON_CLICK_5,
	FE_AUDIO_MESSAGEEND,
	ID_SOUND_ZOOM_ON_RADAR,
	ID_SOUND_BUILD_FAIL,
	ID_SOUND_MESSAGEEND,
	ID_SOUND_GAME_SHUTDOWN,

	/* Design */

	ID_SOUND_TURRET_SELECTED,
	ID_SOUND_BODY_SELECTED,
	ID_SOUND_PROPULSION_SELECTED,
	ID_SOUND_DESIGN_COMPLETED,

	/* Structures */

	ID_SOUND_CONSTRUCTION_STARTED,
	ID_SOUND_STRUCTURE_COMPLETED,
	ID_SOUND_STRUCTURE_UNDER_ATTACK,
	ID_SOUND_STRUCTURE_REPAIR_IN_PROGRESS,
	ID_SOUND_STRUCTURE_DEMOLISHED,

	/* Power */

	ID_SOUND_POWER_GENERATOR_UNDER_ATTACK,
	ID_SOUND_POWER_GENERATOR_DESTROYED,
	ID_SOUND_POWER_LOW,
	ID_SOUND_RESOURCE_HERE,
	ID_SOUND_DERRICK_UNDER_ATTACK,
	ID_SOUND_DERRICK_DESTROYED,
	ID_SOUND_RESOURCE_DEPLETED,
	ID_SOUND_POWER_TRANSFER_IN_PROGRESS,
	ID_SOUND_POWER_GENERATOR_REQUIRED,

	/* Research */

	ID_SOUND_RESEARCH_FACILITY_REQUIRED,
	ID_SOUND_ARTIFACT,
	ID_SOUND_ARTIFACT_RECOVERED,
	ID_SOUND_NEW_RESEARCH_PROJ_AVAILABLE,
	ID_SOUND_NEW_STRUCTURE_AVAILABLE,
	ID_SOUND_NEW_COMPONENT_AVAILABLE,
	ID_SOUND_NEW_CYBORG_AVAILABLE,
	ID_SOUND_RESEARCH_COMPLETED,
	ID_SOUND_MAJOR_RESEARCH,
	ID_SOUND_STRUCTURE_RESEARCH_COMPLETED,
	ID_SOUND_POWER_RESEARCH_COMPLETED,
	ID_SOUND_COMPUTER_RESEARCH_COMPLETED,
	ID_SOUND_VEHICLE_RESEARCH_COMPLETED,
	ID_SOUND_SYSTEMS_RESEARCH_COMPLETED,
	ID_SOUND_WEAPON_RESEARCH_COMPLETED,
	ID_SOUND_CYBORG_RESEARCH_COMPLETED,

	/* Production */

	ID_SOUND_PRODUCTION_STARTED,
	ID_SOUND_DROID_COMPLETED,
	ID_SOUND_PRODUCTION_PAUSED,
	ID_SOUND_PRODUCTION_CANCELLED,
	ID_SOUND_DELIVERY_POINT_ASSIGNED,
	ID_SOUND_DELIVERY_POINT_ASSIGNED_TO,

	/* Repair */

	ID_SOUND_UNIT_REPAIRED,

	/* Detection */

	ID_SOUND_SCAVENGERS_DETECTED,
	ID_SOUND_SCAVENGER_BASE_DETECTED,
	ID_SOUND_SCAVENGER_OUTPOST_DETECTED,
	ID_SOUND_POWER_RESOURCE,
	ID_SOUND_ARTEFACT_DISC,
	ID_SOUND_ENEMY_UNIT_DETECTED,
	ID_SOUND_ENEMY_BASE_DETECTED,
	ID_SOUND_ALLY_DETECTED,
	ID_SOUND_ENEMY_TRANSPORT_DETECTED,
	ID_SOUND_ENEMY_LZ_DETECTED,
	ID_SOUND_FRIENDLY_LZ_DETECTED,
	ID_SOUND_NEXUS_TOWER_DETECTED,
	ID_SOUND_NEXUS_TURRET_DETECTED,
	ID_SOUND_NEXUS_UNIT_DETECTED,
	ID_SOUND_ENEMY_BATTERY_DETECTED,
	ID_SOUND_ENEMY_VTOLS_DETECTED,

	/* Status */

	ID_SOUND_SCAVENGER_BASE,
	ID_SOUND_SCAVENGER_OUTPOST,
	ID_SOUND_SCAVENGER_OUTPOST_ERADICATED,
	ID_SOUND_SCAVENGER_BASE_ERADICATED,
	ID_SOUND_ENEMY_BASE,
	ID_SOUND_ENEMY_BASE_ERADICATED,
	ID_SOUND_INCOMING_ENEMY_TRANSPORT,
	ID_SOUND_ENEMY_LZ,
	ID_SOUND_LZ1,
	ID_SOUND_LZ2,

	/* Combat */

	ID_SOUND_UNIT_UNDER_ATTACK,
	ID_SOUND_UNIT_DESTROYED,
	ID_SOUND_UNIT_RETREATING,
	ID_SOUND_UNIT_RETURNING_FOR_REPAIR,

	/* Artillery Batteries */
	ID_SOUND_ASSIGNED_TO_SENSOR,
	ID_SOUND_SENSOR_LOCKED_ON,
	ID_SOUND_ASSIGNED_TO_COUNTER_RADAR,
	ID_SOUND_ENEMY_BATTERY_LOCATED,
	ID_SOUND_BATTERY_FIRING_COUNTER_ATTACK,

	/* Vtols */

	ID_SOUND_INTERCEPTORS_LAUNCHED,
	ID_SOUND_REARMING,
	ID_SOUND_VTOLS_ENGAGING,
	ID_SOUND_ASSIGNED,
	ID_SOUND_INTERCEPTORS_ASSIGNED,

	/* Command Console */

	ID_SOUND_COMMAND_CONSOLE_ACTIVATED,
	ID_SOUND_SHORT_RANGE,
	ID_SOUND_LONG_RANGE,
	ID_SOUND_OPTIMUM_RANGE,
	ID_SOUND_RETREAT_AT_MEDIUM_DAMAGE,
	ID_SOUND_RETREAT_AT_HEAVY_DAMAGE,
	ID_SOUND_NO_RETREAT,
	ID_SOUND_FIRE_AT_WILL,
	ID_SOUND_RETURN_FIRE,
	ID_SOUND_CEASEFIRE,
	ID_SOUND_HOLD_POSITION,
	ID_SOUND_GUARD,
	ID_SOUND_PURSUE,
	ID_SOUND_PATROL,
	ID_SOUND_RETURN_TO_LZ,
	ID_SOUND_RECYCLING,
	ID_SOUND_SCATTER,

	/* Tutorial Stuff */

	ID_SOUND_NOT_POSSIBLE_TRY_AGAIN,
	ID_SOUND_NO,
	ID_SOUND_THAT_IS_INCORRECT,
	ID_SOUND_WELL_DONE,
	ID_SOUND_EXCELLENT,

	/* Group and Commander Assignment */

	ID_SOUND_ASSIGNED_TO_COMMANDER,
	ID_SOUND_GROUP_REPORTING,
	ID_SOUND_COMMANDER_REPORTING,

	/* Routing */


	ID_SOUND_ROUTE_OBSTRUCTED,
	ID_SOUND_NO_ROUTE_AVAILABLE,

	/* Transports and LZS */

	ID_SOUND_REINFORCEMENTS_AVAILABLE,
	ID_SOUND_REINFORCEMENTS_IN_TRANSIT,
	ID_SOUND_TRANSPORT_LANDING,
	ID_SOUND_TRANSPORT_UNDER_ATTACK,
	ID_SOUND_TRANSPORT_REPAIRING,
	ID_SOUND_LZ_COMPROMISED,
	ID_SOUND_LZ_CLEAR,
	ID_SOUND_TRANSPORT_RETURNING_TO_BASE,
	ID_SOUND_TRANSPORT_UNABLE_TO_LAND,

	/* Mission Messages */

	ID_SOUND_MISSION_OBJECTIVE,
	ID_SOUND_MISSION_UPDATE,
	ID_SOUND_WARZONE_PAUSED,
	ID_SOUND_WARZONE_ACTIVE,
	ID_SOUND_MISSION_RESULTS,
	ID_SOUND_RESEARCH_STOLEN,
	ID_SOUND_TECHNOLOGY_TAKEN,
	ID_SOUND_INCOMING_TRANSMISSION,
	ID_SOUND_INCOMING_INTELLIGENCE_REPORT,
	ID_SOUND_MISSION_FAILED,
	ID_SOUND_MISSION_SUCCESSFUL,
	ID_SOUND_OBJECTIVE_ACCOMPLISHED,
	ID_SOUND_OBJECTIVE_FAILED,
	ID_SOUND_MISSION_TIMER_ACTIVATED,
	ID_SOUND_10_MINUTES_REMAINING,
	ID_SOUND_5_MINUTES_REMAINING,
	ID_SOUND_3_MINUTES_REMAINING,
	ID_SOUND_2_MINUTES_REMAINING,
	ID_SOUND_1_MINUTE_REMAINING,
	ID_SOUND_UNIT_CAPTURED,
	ID_SOUND_SYSTEM_FAILURE_IMMINENT,
	ID_SOUND_YOU_ARE_DEFEATED,
	ID_SOUND_MISSILE_CODES_DECIPHERED,
	ID_SOUND_1ST_MISSILE_CODES_DECIPHERED,
	ID_SOUND_2ND_MISSILE_CODES_DECIPHERED,
	ID_SOUND_3RD_MISSILE_CODES_DECIPHERED,
	ID_SOUND_MISSILE_CODES_CRACKED,
	ID_SOUND_ENTERING_WARZONE,
	ID_ALLIANCE_ACC,
	ID_ALLIANCE_BRO,
	ID_ALLIANCE_OFF,
	ID_CLAN_ENTER,
	ID_CLAN_EXIT,
	ID_GIFT,
	ID_POWER_TRANSMIT,
	ID_SENSOR_DOWNLOAD,
	ID_TECHNOLOGY_TRANSFER,
	ID_UNITS_TRANSFER,

	/* Group and Commander Voices - Male */
	ID_SOUND_GROUP,
	ID_SOUND_GROUP_0,
	ID_SOUND_GROUP_1,
	ID_SOUND_GROUP_2,
	ID_SOUND_GROUP_3,
	ID_SOUND_GROUP_4,
	ID_SOUND_GROUP_5,
	ID_SOUND_GROUP_6,
	ID_SOUND_GROUP_7,
	ID_SOUND_GROUP_8,
	ID_SOUND_GROUP_9,
	ID_SOUND_REPORTING,
	ID_SOUND_COMMANDER,
	ID_SOUND_COM_SCAVS_DETECTED,
	ID_SOUND_COM_SCAV_BASE_DETECTED,
	ID_SOUND_COM_SCAV_OUTPOST_DETECTED,
	ID_SOUND_COM_RESOURCE_DETECTED,
	ID_SOUND_COM_ARTEFACT_DETECTED,
	ID_SOUND_COM_ENEMY_DETECTED,
	ID_SOUND_COM_ENEMY_BASE_DETECTED,
	ID_SOUND_COM_ALLY_DETECTED,
	ID_SOUND_COM_ENEMY_TRANSPORT_DETECTED,
	ID_SOUND_COM_ENEMY_LZ_DETECTED,
	ID_SOUND_COM_FRIENDLY_LZ_DETECTED,
	ID_SOUND_COM_NEXUS_TOWER_DETECTED,
	ID_SOUND_COM_NEXUS_TURRET_DETECTED,
	ID_SOUND_COM_NEXUS_DETECTED,
	ID_SOUND_COM_ENEMY_BATTERY_DETECTED,
	ID_SOUND_COM_ENEMY_VTOLS_DETECTED,
	ID_SOUND_COM_ROUTE_OBSTRUCTED,
	ID_SOUND_COM_NO_ROUTE_AVAILABLE,
	ID_SOUND_COM_UNABLE_TO_COMPLY,
	ID_SOUND_COM_RETURNING_FOR_REPAIR,
	ID_SOUND_COM_HEADING_FOR_RALLY_POINT,

	/* Radio Clicks */

	ID_SOUND_RADIOCLICK_1,
	ID_SOUND_RADIOCLICK_2,
	ID_SOUND_RADIOCLICK_3,
	ID_SOUND_RADIOCLICK_4,
	ID_SOUND_RADIOCLICK_5,
	ID_SOUND_RADIOCLICK_6,

	/* Transport Pilots */
	ID_SOUND_APPROACHING_LZ,
	ID_SOUND_ALRIGHT_BOYS,
	ID_SOUND_GREEN_LIGHT_IN_5,
	ID_SOUND_GREEN_LIGHT_IN_4,
	ID_SOUND_GREEN_LIGHT_IN_3,
	ID_SOUND_GREEN_LIGHT_IN_2,
	ID_SOUND_GO_GO_GO,
	ID_SOUND_PREPARE_FOR_DUST_OFF,

	/* VTol Pilots */
	/* Ver-1 */
	ID_SOUND_ENEMY_LOCATED1,
	ID_SOUND_ON_OUR_WAY1,
	ID_SOUND_RETURNING_TO_BASE1,
	ID_SOUND_LOCKED_ON1,
	ID_SOUND_COMMENCING_ATTACK_RUN1,
	ID_SOUND_ABORTING_ATTACK_RUN1,

	/* Ver-2 */
	ID_SOUND_ENEMY_LOCATED2,
	ID_SOUND_ON_OUR_WAY2,
	ID_SOUND_RETURNING_TO_BASE2,
	ID_SOUND_LOCKED_ON2,
	ID_SOUND_COMMENCING_ATTACK_RUN2,
	ID_SOUND_ABORTING_ATTACK_RUN2,

	/* Ver-3 */
	ID_SOUND_ENEMY_LOCATED3,
	ID_SOUND_ON_OUR_WAY3,
	ID_SOUND_RETURNING_TO_BASE3,
	ID_SOUND_LOCKED_ON3,
	ID_SOUND_COMMENCING_ATTACK_RUN3,
	ID_SOUND_ABORTING_ATTACK_RUN3,

	/* The Collective */
	ID_SOUND_COLL_CLEANSE_AND_DESTROY,
	ID_SOUND_COLL_DESTROYING_BIOLOGICALS,
	ID_SOUND_COLL_ATTACK,
	ID_SOUND_COLL_FIRE,
	ID_SOUND_COLL_ENEMY_DETECTED,
	ID_SOUND_COLL_ENGAGING,
	ID_SOUND_COLL_STARTING_ATTACK_RUN,
	ID_SOUND_COLL_DIE,
	ID_SOUND_COLL_INTERCEPT_AND_DESTROY,
	ID_SOUND_COLL_ENEMY_DESTROYED,

	/* SFX */

	/* Weapon Sounds */

	ID_SOUND_ROCKET,
	ID_SOUND_ROTARY_LASER,
	ID_SOUND_GAUSSGUN,
	ID_SOUND_LARGE_CANNON,
	ID_SOUND_SMALL_CANNON,
	ID_SOUND_MEDIUM_CANNON,
	ID_SOUND_FLAME_THROWER,
	ID_SOUND_PULSE_LASER,
	ID_SOUND_BEAM_LASER,
	ID_SOUND_MORTAR,
	ID_SOUND_HOWITZ_FLIGHT,
	ID_SOUND_BABA_MG_1,
	ID_SOUND_BABA_MG_2,
	ID_SOUND_BABA_MG_3,
	ID_SOUND_BABA_MG_HEAVY,
	ID_SOUND_BABA_MG_TOWER,
	ID_SOUND_SPLASH,
	ID_SOUND_ASSAULT_MG,
	ID_SOUND_RAPID_CANNON,
	ID_SOUND_HIVEL_CANNON,
	ID_SOUND_NEXUS_TOWER,

	/* Construction sounds */
	ID_SOUND_WELD_1,
	ID_SOUND_WELD_2,
	ID_SOUND_CONSTRUCTION_START,
	ID_SOUND_CONSTRUCTION_LOOP,
	ID_SOUND_CONSTRUCTION_1,
	ID_SOUND_CONSTRUCTION_2,
	ID_SOUND_CONSTRUCTION_3,
	ID_SOUND_CONSTRUCTION_4,

	/* Explosions */

	ID_SOUND_EXPLOSION_SMALL,
	ID_SOUND_EXPLOSION_LASER,
	ID_SOUND_EXPLOSION,
	ID_SOUND_EXPLOSION_ANTITANK,
	ID_SOUND_RICOCHET_1,
	ID_SOUND_RICOCHET_2,
	ID_SOUND_RICOCHET_3,
	ID_SOUND_BARB_SQUISH,
	ID_SOUND_BUILDING_FALL,
	ID_SOUND_NEXUS_EXPLOSION,

	/* Droid Engine Noises */
	ID_SOUND_CONSTRUCTOR_MOVEOFF,
	ID_SOUND_CONSTRUCTOR_MOVE,
	ID_SOUND_CONSTRUCTOR_SHUTDOWN,

	/* Transports */
	ID_SOUND_BLIMP_FLIGHT,
	ID_SOUND_BLIMP_IDLE,
	ID_SOUND_BLIMP_LAND,
	ID_SOUND_BLIMP_TAKE_OFF,

	/* Vtols */
	ID_SOUND_VTOL_LAND,
	ID_SOUND_VTOL_OFF,
	ID_SOUND_VTOL_MOVE,

	/* Treads */
	ID_SOUND_TREAD,

	/* Hovers */
	ID_SOUND_HOVER_MOVE,
	ID_SOUND_HOVER_START,
	ID_SOUND_HOVER_STOP,

	/* Cyborgs */
	ID_SOUND_CYBORG_MOVE,

	/* Buildings */

	ID_SOUND_OIL_PUMP_2,
	ID_SOUND_POWER_HUM,
	ID_SOUND_POWER_SPARK,
	ID_SOUND_STEAM,
	ID_SOUND_ECM_TOWER,
	ID_SOUND_FIRE_ROAR,

	/* Misc */

	ID_SOUND_HELP,
	ID_SOUND_BARB_SCREAM,
	ID_SOUND_BARB_SCREAM2,
	ID_SOUND_BARB_SCREAM3,
	ID_SOUND_OF_SILENCE,
	ID_SOUND_SHIELD_HIT,

	/* Extra */

	ID_SOUND_LANDING_ZONE,
	ID_SOUND_SATELLITE_UPLINK,
	ID_SOUND_NASDA_CENTRAL,
	ID_SOUND_NUCLEAR_REACTOR,
	ID_SOUND_SAM_SITE,
	ID_SOUND_MISSILE_SILO,
	ID_SOUND_MISSILE_NME_DETECTED,
	ID_SOUND_STRUCTURE_CAPTURED,
	ID_SOUND_CIVILIAN_RESCUED,
	ID_SOUND_CIVILIANS_RESCUED,
	ID_SOUND_UNITS_RESCUED,
	ID_SOUND_GROUP_RESCUED,
	ID_SOUND_GROUP_CAPTURED,
	ID_SOUND_OBJECTIVE_CAPTURED,
	ID_SOUND_OBJECTIVE_DESTROYED,
	ID_SOUND_STRUCTURE_INFECTED,
	ID_SOUND_GROUP_INFECTED,
	ID_SOUND_OUT_OF_TIME,
	ID_SOUND_ENEMY_ESCAPED,
	ID_SOUND_ENEMY_ESCAPING,
	ID_SOUND_ENEMY_TRANSPORT_LANDING,
	ID_SOUND_TEAM_ALPHA_ERADICATED,
	ID_SOUND_TEAM_BETA_ERADICATED,
	ID_SOUND_TEAM_GAMMA_ERADICATED,
	ID_SOUND_TEAM_ALPHA_RESCUED,
	ID_SOUND_TEAM_BETA_RESCUED,
	ID_SOUND_TEAM_GAMMA_RESCUED,
	ID_SOUND_LASER_SATELLITE_FIRING,
	ID_SOUND_INCOMING_LASER_SAT_STRIKE,

	/* Nexus */

	ID_SOUND_NEXUS_DEFENCES_ABSORBED,
	ID_SOUND_NEXUS_DEFENCES_NEUTRALISED,
	ID_SOUND_NEXUS_LAUGH1,
	ID_SOUND_NEXUS_LAUGH2,
	ID_SOUND_NEXUS_LAUGH3,
	ID_SOUND_NEXUS_PRODUCTION_COMPLETED,
	ID_SOUND_NEXUS_RESEARCH_ABSORBED,
	ID_SOUND_NEXUS_STRUCTURE_ABSORBED,
	ID_SOUND_NEXUS_STRUCTURE_NEUTRALISED,
	ID_SOUND_NEXUS_SYNAPTIC_LINK,
	ID_SOUND_NEXUS_UNIT_ABSORBED,
	ID_SOUND_NEXUS_UNIT_NEUTRALISED,

	/* multiplayer sfx */

	ID_SOUND_CYBORG_GROUND,
	ID_SOUND_CYBORG_HEAVY,
	ID_SOUND_EMP,
	ID_SOUND_LASER_HEAVY,
	ID_SOUND_PLASMA_FLAMER,
	ID_SOUND_PARTICLE_GUN,
	ID_SOUND_UPLINK,

	/* added for upgrade - 2/9/99 AB*/
	ID_SOUND_LAS_SAT_COUNTDOWN,

	ID_SOUND_BEACON,

	/* Last ID */
	ID_SOUND_NEXT,  // Thanks to this dummy we don't have to redefine ID_MAX_SOUND every time in terms of the preceding enum value
	ID_MAX_SOUND = ID_SOUND_NEXT - 1,
};

INGAME_AUDIO audio_GetIDFromStr(const char *pWavStr);

#endif // __INCLUDED_LIB_SOUND_AUDIO_ID_H__
