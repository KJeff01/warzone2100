include("script/campaign/libcampaign.js");
include("script/campaign/templates.js");

const GUIDE_STRUCT_BUILT_DELAY_TIME = 100;
const mis_playerRes = [
	"R-Wpn-MG1Mk1", "R-Vehicle-Body01", "R-Sys-Spade1Mk1", "R-Vehicle-Prop-Wheels",
];

const mis_scavengerRes = [
	"R-Wpn-MG-Damage01", "R-Wpn-MG-ROF01", "R-Wpn-Flamer-Range01-ScavReduce",
];

// CLASSIC: No research.

// Handlers for guide topics

function cam1A_doAddHQBuiltTopics()
{
	addGuideTopic("wz2100::structures::factory");
	addGuideTopic("wz2100::units::designing", SHOWTOPIC_FIRSTADD);
}

function cam1A_doAddFactoryBuiltTopics()
{
	addGuideTopic("wz2100::units::designing");
	addGuideTopic("wz2100::structures::rallypoint");
	addGuideTopic("wz2100::structures::factory");
	addGuideTopic("wz2100::units::building", SHOWTOPIC_FIRSTADD);
}

function cam1A_doAddResearchFacilityBuiltTopics()
{
	addGuideTopic("wz2100::structures::researchfacility", SHOWTOPIC_FIRSTADD);
}

function cam1A_doAddOilDerrickBuiltTopics()
{
	addGuideTopic("wz2100::general::power");
	addGuideTopic("wz2100::structures::oilderrick");
	addGuideTopic("wz2100::structures::powergenerator", SHOWTOPIC_FIRSTADD);
}

// Player zero's droid enters area next to first oil patch.
camAreaEvent("launchScavAttack", function(droid)
{
	camPlayVideos([cam_sounds.incoming.incomingIntelligenceReport, {video: "MB1A_MSG", type: MISS_MSG}]);
	hackAddMessage("C1A_OBJ1", PROX_MSG, CAM_HUMAN_PLAYER, false);
	// Send scavengers on war path if triggered above.
	camManageGroup(camMakeGroup("scavAttack1", ENEMIES), CAM_ORDER_ATTACK, {
		pos: camMakePos("playerBase"),
		fallback: camMakePos("retreat1"),
		morale: 50
	});
	// Activate mission timer, unlike the original campaign.
	if (!tweakOptions.classicTimers && difficulty <= MEDIUM)
	{
		camSetMissionTimer(camChangeOnDiff(camHoursToSeconds(1)));
	}
});

function runAway()
{
	const oilPatch = getObject("oilPatch");
	const droids = enumRange(oilPatch.x, oilPatch.y, 7, CAM_SCAV_7, false);
	camManageGroup(camMakeGroup(droids), CAM_ORDER_ATTACK, {
		pos: camMakePos("scavAttack1"),
		fallback: camMakePos("retreat1"),
		morale: 20 // Will run away after losing a few people.
	});
}

function doAmbush()
{
	camManageGroup(camMakeGroup("roadblockArea"), CAM_ORDER_ATTACK, {
		pos: camMakePos("oilPatch"),
		fallback: camMakePos("retreat2"),
		morale: 50 // Will mostly die.
	});
}

// Area with the radar blip just before the first scavenger outpost.
camAreaEvent("scavAttack1", function(droid)
{
	hackRemoveMessage("C1A_OBJ1", PROX_MSG, CAM_HUMAN_PLAYER);
	queue("runAway", camSecondsToMilliseconds(1));
	queue("doAmbush", camSecondsToMilliseconds(5));
});

// Road between first outpost and base two.
camAreaEvent("roadblockArea", function(droid)
{
	camEnableFactory("base2Factory");
});

// Scavengers hiding in the split canyon area between base two and three.
function raidAttack()
{
	if (!camClassicMode())
	{
		camCompleteRequiredResearch(mis_scavengerRes, CAM_SCAV_6);
		camCompleteRequiredResearch(mis_scavengerRes, CAM_SCAV_7);
	}
	camManageGroup(camMakeGroup("raidTrigger", ENEMIES), CAM_ORDER_ATTACK, {
		pos: camMakePos("scavBase3Cleanup")
	});
	camManageGroup(camMakeGroup("raidGroup", ENEMIES), CAM_ORDER_ATTACK, {
		pos: camMakePos("scavBase3Cleanup")
	});
	camManageGroup(camMakeGroup("scavBase3Cleanup", ENEMIES), CAM_ORDER_DEFEND, {
		pos: camMakePos("scavBase3Cleanup")
	});
	camEnableFactory("base3Factory");
}

// Wait for player to get close to the split canyon and attack, if not already.
camAreaEvent("raidTrigger", function(droid)
{
	camCallOnce("raidAttack");
});

// Or, they built on base two's oil patch instead. Initiate a surprise attack.
// (Also handles queuing guide topics for structures built)
function eventStructureBuilt(structure, droid)
{
	if (structure.player === CAM_HUMAN_PLAYER)
	{
		if (structure.stattype === FACTORY)
		{
			queue("cam1A_doAddFactoryBuiltTopics", GUIDE_STRUCT_BUILT_DELAY_TIME);
		}
		else if (structure.stattype === RESEARCH_LAB)
		{
			queue("cam1A_doAddResearchFacilityBuiltTopics", GUIDE_STRUCT_BUILT_DELAY_TIME);
		}
		else if (structure.stattype === RESOURCE_EXTRACTOR)
		{
			// Is it in the base two area?
			const objs = enumArea("scavBase2Cleanup", CAM_HUMAN_PLAYER);
			for (let i = 0, l = objs.length; i < l; ++i)
			{
				const obj = objs[i];
				if (obj.type === STRUCTURE && obj.stattype === RESOURCE_EXTRACTOR)
				{
					camCallOnce("raidAttack");
					break;
				}
			}

			// Add the oil derrick topic
			queue("cam1A_doAddOilDerrickBuiltTopics", GUIDE_STRUCT_BUILT_DELAY_TIME);
		}
		else if (structure.stattype === HQ)
		{
			queue("cam1A_doAddHQBuiltTopics", GUIDE_STRUCT_BUILT_DELAY_TIME);
		}
	}
}

camAreaEvent("factoryTrigger", function(droid)
{
	camEnableFactory("base4Factory");
});

function camEnemyBaseEliminated_scavGroup1()
{
	camEnableFactory("base2Factory");
}

function camEnemyBaseEliminated_scavGroup2()
{
	queue("camDetectEnemyBase", camSecondsToMilliseconds(2), "scavGroup3");
}

function enableBaseStructures()
{
	const structs = [
		cam_base_structures.commandCenter, cam_base_structures.powerGenerator,
		cam_base_structures.derrick, cam_base_structures.researchLab,
		cam_base_structures.factory,
	];

	for (let i = 0; i < structs.length; ++i)
	{
		enableStructure(structs[i], CAM_HUMAN_PLAYER);
	}
}

function cam1A_doNeedPowerTopics()
{
	// inform the user about power (and the need to build an oil derrick)
	addGuideTopic("wz2100::structures::oilderrick");
	addGuideTopic("wz2100::general::power", SHOWTOPIC_FIRSTADD);
}

function eventDroidBuilt(droid, structure)
{
	if (!camDef(structure)) // "clone wars" cheat
	{
		return;
	}
	if (droid.player === CAM_HUMAN_PLAYER)
	{
		// inform the user about power (and the need to build an oil derrick)
		camCallOnce("cam1A_doNeedPowerTopics");
	}
}

function doAddHQGuideTopic()
{
	addGuideTopic("wz2100::structures::hq", SHOWTOPIC_FIRSTADD);
}

function eventStartLevel()
{
	const PLAYER_POWER = 1300;
	const startPos = getObject("startPosition");
	const lz = getObject("landingZone");

	camSetStandardWinLossConditions(CAM_VICTORY_STANDARD, cam_levels.alpha2);

	centreView(startPos.x, startPos.y);
	setNoGoArea(lz.x, lz.y, lz.x2, lz.y2, CAM_HUMAN_PLAYER);

	setAlliance(CAM_SCAV_6, CAM_SCAV_7, true);

	enableBaseStructures();
	camCompleteRequiredResearch(mis_playerRes, CAM_HUMAN_PLAYER);

	if (camClassicMode())
	{
		setPower(PLAYER_POWER, CAM_HUMAN_PLAYER);

		camSetArtifacts({
			"base1ArtifactPos": { tech: "R-Wpn-MG-Damage01" },
			"base2Factory": { tech: "R-Wpn-Flamer01Mk1" },
			"base3Factory": { tech: "R-Defense-Tower01" },
			"base4Factory": { tech: "R-Sys-Engineering01" },
		});
	}
	else
	{
		if (difficulty === HARD)
		{
			setPower(600, CAM_HUMAN_PLAYER);
		}
		else if (difficulty >= INSANE)
		{
			setPower(300, CAM_HUMAN_PLAYER);
		}
		else
		{
			setPower(PLAYER_POWER, CAM_HUMAN_PLAYER);
		}

		completeResearch("R-Wpn-Flamer-Range01-ScavReduce", CAM_SCAV_6);
		completeResearch("R-Wpn-Flamer-Range01-ScavReduce", CAM_SCAV_7);
		if (difficulty >= HARD)
		{
			camCompleteRequiredResearch(mis_scavengerRes, CAM_SCAV_6);
			camCompleteRequiredResearch(mis_scavengerRes, CAM_SCAV_7);
		}
		if (difficulty >= INSANE)
		{
			completeResearch("R-Wpn-Flamer-Range01-ScavReduce-Undo", CAM_SCAV_6);
			completeResearch("R-Wpn-Flamer-Range01-ScavReduce-Undo", CAM_SCAV_7);
		}

		camSetArtifacts({
			"base1ArtifactPos": { tech: ["R-Wpn-MG-Damage01", "R-Sys-Engineering01"] },
			"base2Factory": { tech: ["R-Wpn-Flamer01Mk1", "R-Sys-MobileRepairTurret01"] },
			"base3Factory": { tech: "R-Wpn-MG-Damage02" },
			"base4Factory": { tech: "R-Wpn-MG-ROF01" },
		});
	}

	// Give player briefing.
	camPlayVideos({video: "CMB1_MSG", type: CAMP_MSG, immediate: false});

	if (tweakOptions.classicTimers)
	{
		camSetMissionTimer(-1);
	}
	else
	{
		if (difficulty === HARD)
		{
			camSetMissionTimer(camMinutesToSeconds(40));
		}
		else if (difficulty >= INSANE)
		{
			camSetMissionTimer(camMinutesToSeconds(30));
		}
		else
		{
			camSetMissionTimer(-1); // will start mission timer later
		}
	}

	// Feed libcampaign.js with data to do the rest.
	camSetEnemyBases({
		"scavGroup1": {
			cleanup: "scavBase1Cleanup",
			detectMsg: "C1A_BASE0",
			detectSnd: cam_sounds.baseDetection.scavengerOutpostDetected,
			eliminateSnd: cam_sounds.baseElimination.scavengerOutpostEradicated
		},
		"scavGroup2": {
			cleanup: "scavBase2Cleanup",
			detectMsg: "C1A_BASE1",
			detectSnd: cam_sounds.baseDetection.scavengerBaseDetected,
			eliminateSnd: cam_sounds.baseElimination.scavengerBaseEradicated
		},
		"scavGroup3": {
			cleanup: "scavBase3Cleanup",
			detectMsg: "C1A_BASE2",
			detectSnd: cam_sounds.baseDetection.scavengerBaseDetected,
			eliminateSnd: cam_sounds.baseElimination.scavengerBaseEradicated
		},
		"scavGroup4": {
			cleanup: "scavBase4Cleanup",
			detectMsg: "C1A_BASE3",
			detectSnd: cam_sounds.baseDetection.scavengerBaseDetected,
			eliminateSnd: cam_sounds.baseElimination.scavengerBaseEradicated
		},
	});

	camSetFactories({
		"base2Factory": {
			assembly: "base2Assembly",
			order: CAM_ORDER_ATTACK,
			data: { pos: "playerBase" },
			groupSize: 3,
			maxSize: 3,
			throttle: camChangeOnDiff(camSecondsToMilliseconds((difficulty <= MEDIUM) ? 24 : 18)),
			templates: [ cTempl.trike, cTempl.bloke ]
		},
		"base3Factory": {
			assembly: "base3Assembly",
			order: CAM_ORDER_ATTACK,
			data: { pos: "playerBase" },
			groupSize: 4,
			maxSize: 4,
			throttle: camChangeOnDiff(camSecondsToMilliseconds((difficulty <= MEDIUM) ? 20 : 14)),
			templates: [ cTempl.bloke, cTempl.buggy, cTempl.bloke ]
		},
		"base4Factory": {
			assembly: "base4Assembly",
			order: CAM_ORDER_ATTACK,
			data: { pos: "playerBase" },
			groupSize: 4,
			maxSize: 4,
			throttle: camChangeOnDiff(camSecondsToMilliseconds((difficulty <= MEDIUM) ? 16 : 12)),
			templates: [ cTempl.bjeep, cTempl.bloke, cTempl.trike, cTempl.bloke ]
		},
	});

	queue("doAddHQGuideTopic", camSecondsToMilliseconds(3));
}
