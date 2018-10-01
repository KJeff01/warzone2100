
include("script/campaign/libcampaign.js");
include("script/campaign/templates.js");

const SCAVS = 7; //Scav player number
const SCAVENGER_RES = [
	"R-Wpn-Flamer-Damage01", "R-Wpn-Flamer-Range01", "R-Wpn-MG-Damage02", "R-Wpn-MG-ROF01",
];

//Ambush player from scav base - triggered from middle path
camAreaEvent("scavBaseTrigger", function()
{
	var ambushGroup = camMakeGroup(enumArea("eastScavs", SCAVS, false));
	camManageGroup(ambushGroup, CAM_ORDER_DEFEND, {
		pos: camMakePos("artifactLocation")
	});
});

//Moves west scavs units closer to the base - triggered from right path
camAreaEvent("ambush1Trigger", function()
{
	var ambushGroup = camMakeGroup(enumArea("westScavs", SCAVS, false));
	camManageGroup(ambushGroup, CAM_ORDER_DEFEND, {
		pos: camMakePos("ambush1")
	});
});

//Sends some units towards player LZ - triggered from left path
camAreaEvent("ambush2Trigger", function()
{
	var ambushGroup = camMakeGroup(enumArea("northWestScavs", SCAVS, false));
	camManageGroup(ambushGroup, CAM_ORDER_DEFEND, {
		pos: camMakePos("ambush2")
	});
});

function eventPickup(feature, droid)
{
	if (droid.player === CAM_HUMAN_PLAYER && feature.stattype === ARTIFACT)
	{
		hackRemoveMessage("C1-1_OBJ1", PROX_MSG, CAM_HUMAN_PLAYER);
	}
}

//Mission setup stuff
function eventStartLevel()
{
	camSetStandardWinLossConditions(CAM_VICTORY_OFFWORLD, "SUB_1_2S", {
		area: "RTLZ",
		message: "C1-1_LZ",
		reinforcements: -1, //No reinforcements
		retlz: true
	});

	var startpos = getObject("startPosition");
	var lz = getObject("landingZone"); //player lz
	var tent = getObject("transporterEntry");
	var text = getObject("transporterExit");
	centreView(startpos.x, startpos.y);
	setNoGoArea(lz.x, lz.y, lz.x2, lz.y2, CAM_HUMAN_PLAYER);
	startTransporterEntry(tent.x, tent.y, CAM_HUMAN_PLAYER);
	setTransporterExit(text.x, text.y, CAM_HUMAN_PLAYER);

	camCompleteRequiredResearch(SCAVENGER_RES, SCAVS);

	camUpgradeOnMapTemplates(cTempl.bloke, cTempl.blokeheavy, SCAVS);
	camUpgradeOnMapTemplates(cTempl.trike, cTempl.triketwin, SCAVS);
	camUpgradeOnMapTemplates(cTempl.buggy, cTempl.buggytwin, SCAVS);
	camUpgradeOnMapTemplates(cTempl.bjeep, cTempl.bjeeptwin, SCAVS);

	//Get rid of the already existing crate and replace with another
	camSafeRemoveObject("artifact1", false);
	camSetArtifacts({
		"scavFactory1": { tech: "R-Wpn-MG3Mk1" }, //Heavy machine gun
	});

	camSetFactories({
		"scavFactory1": {
			assembly: "Assembly",
			order: CAM_ORDER_ATTACK,
			data: {
				regroup: false,
				repair: 66,
				count: -1,
			},
			groupSize: 4,
			throttle: camChangeOnDiff(24000),
			templates: [ cTempl.trikeheavy, cTempl.blokeheavy, cTempl.buggyheavy, cTempl.bjeepheavy ]
		},
	});

	camEnableFactory("scavFactory1");
	camPlayVideos("FLIGHT");
	hackAddMessage("C1-1_OBJ1", PROX_MSG, CAM_HUMAN_PLAYER, true);

}
