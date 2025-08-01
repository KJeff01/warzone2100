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
/*
 * Multijoin.c
 *
 * Alex Lee, pumpkin studios, bath,
 *
 * Stuff to handle the comings and goings of players.
 */

#include <physfs.h>

#include "lib/framework/frame.h"
#include "lib/framework/strres.h"
#include "lib/framework/math_ext.h"
#include "lib/framework/object_list_iteration.h"

#include "lib/gamelib/gtime.h"
#include "lib/ivis_opengl/textdraw.h"
#include "lib/netplay/netplay.h"
#include "lib/sound/audio.h"
#include "lib/sound/audio_id.h"

#include "multijoin.h"

#include "objmem.h"
#include "statsdef.h"
#include "droiddef.h"
#include "game.h"
#include "projectile.h"
#include "droid.h"
#include "map.h"
#include "levels.h"
#include "power.h"
#include "game.h"					// for loading maps
#include "message.h"				// for clearing game messages
#include "order.h"
#include "console.h"
#include "orderdef.h"				// for droid_order_data
#include "hci.h"
#include "component.h"
#include "research.h"
#include "wrappers.h"
#include "intimage.h"
#include "data.h"
#include "activity.h"
#include "main.h"					// for GetGameMode

#include "multimenu.h"
#include "multiplay.h"
#include "multirecv.h"
#include "multiint.h"
#include "multistat.h"
#include "multigifts.h"
#include "multivote.h"
#include "qtscript.h"
#include "clparse.h"
#include "multilobbycommands.h"
#include "stdinreader.h"

// ////////////////////////////////////////////////////////////////////////////
// Local Functions

static void resetMultiVisibility(UDWORD player);
void destroyPlayerResources(UDWORD player, bool quietly);

//////////////////////////////////////////////////////////////////////////////
/*
** when a remote player leaves an arena game do this!
**
** @param player -- the one we need to clear
** @param quietly -- true means without any visible effects
*/
void clearPlayer(UDWORD player, bool quietly)
{
	ASSERT_OR_RETURN(, player < MAX_CONNECTED_PLAYERS, "Invalid player: %" PRIu32 "", player);

	ASSERT(player < NetPlay.playerReferences.size(), "Invalid player: %" PRIu32 "", player);
	NetPlay.playerReferences[player]->disconnect();
	NetPlay.playerReferences[player] = std::make_shared<PlayerReference>(player);

	debug(LOG_NET, "R.I.P. %s (%u). quietly is %s", getPlayerName(player), player, quietly ? "true" : "false");

	ingame.LagCounter[player] = 0;
	ingame.DesyncCounter[player] = 0;
	ingame.JoiningInProgress[player] = false;	// if they never joined, reset the flag
	ingame.DataIntegrity[player] = false;
	ingame.hostChatPermissions[player] = false;
	ingame.lastSentPlayerDataCheck2[player].reset();

	if (player >= MAX_PLAYERS)
	{
		return; // no more to do
	}

	destroyPlayerResources(player, quietly);
}

void destroyPlayerResources(UDWORD player, bool quietly)
{
	UDWORD			i;

	if (player >= MAX_PLAYERS)
	{
		return; // no more to do
	}

	for (i = 0; i < MAX_PLAYERS; i++)				// remove alliances
	{
		// Never remove a player's self-alliance, as the player can be selected and units added via the debug menu
		// even after they have left, and this would lead to them firing on each other.
		if (i != player)
		{
			alliances[player][i] = ALLIANCE_BROKEN;
			alliances[i][player] = ALLIANCE_BROKEN;
			alliancebits[i] &= ~(1 << player);
			alliancebits[player] &= ~(1 << i);
		}
	}

	debug(LOG_DEATH, "killing off all droids for player %d", player);
	// delete all droids
	mutating_list_iterate(apsDroidLists[player], [quietly](DROID* d)
	{
		if (quietly)			// don't show effects
		{
			killDroid(d);
		}
		else				// show effects
		{
			destroyDroid(d, gameTime);
		}
		return IterationResult::CONTINUE_ITERATION;
	});

	debug(LOG_DEATH, "killing off all structures for player %d", player);
	// delete all structs
	mutating_list_iterate(apsStructLists[player], [quietly](STRUCTURE* psStruct)
	{
		// FIXME: look why destroyStruct() doesn't put back the feature like removeStruct() does
		if (quietly || psStruct->pStructureType->type == REF_RESOURCE_EXTRACTOR)		// don't show effects
		{
			removeStruct(psStruct, true);
		}
		else			// show effects
		{
			destroyStruct(psStruct, gameTime);
		}
		return IterationResult::CONTINUE_ITERATION;
	});

	return;
}

static bool destroyMatchingStructs(UDWORD player, std::function<bool (STRUCTURE *)> cmp, bool quietly)
{
	bool destroyedAnyStructs = false;
	mutating_list_iterate(apsStructLists[player], [quietly, &cmp, &destroyedAnyStructs](STRUCTURE* psStruct)
	{
		if (cmp(psStruct))
		{
			// FIXME: look why destroyStruct() doesn't put back the feature like removeStruct() does
			if (quietly || psStruct->pStructureType->type == REF_RESOURCE_EXTRACTOR)		// don't show effects
			{
				removeStruct(psStruct, true);
			}
			else			// show effects
			{
				destroyStruct(psStruct, gameTime);
			}
			destroyedAnyStructs = true;
		}
		return IterationResult::CONTINUE_ITERATION;
	});
	return destroyedAnyStructs;
}

bool splitResourcesAmongTeam(UDWORD player)
{
	auto team = NetPlay.players[player].team;

	// Build a list of team members who are still around
	std::vector<uint32_t> possibleTargets;
	for (uint32_t i = 0; i < MAX_PLAYERS; i++)
	{
		if (i != player
			&& i != scavengerSlot()										// ...not scavenger player
			&& NetPlay.players[i].team == team							// ...belonging to the same team
			&& aiCheckAlliances(i, player)								// ...the alliance hasn't been broken
			// && NetPlay.players[i].difficulty != AIDifficulty::DISABLED	// ...not disabled // NOTE: Can't do this check as the host may set difficulty == DISABLED for slots before clients do, leading to sync issues, so for now (instead) check for human players only...
			&& isHumanPlayer(i)											// ... is a human
			&& !NetPlay.players[i].isSpectator							// ... not spectator
			)
		{
			possibleTargets.push_back(i);
		}
	}

	if (possibleTargets.empty())
	{
		// no valid targets for resources...
		return false;
	}

	// Distribute power evenly
	auto powerPerTarget = getPower(player) / static_cast<int32_t>(possibleTargets.size());
	for (auto to : possibleTargets)
	{
		addPower(to, powerPerTarget);
	}
	setPower(player, 0);

	// Distribute the player's additional unit limits
	auto additionalDroidsLimitPerTarget = getMaxDroids(player) / static_cast<int>(possibleTargets.size());
	auto additionalCommandersLimitPerTarget = getMaxCommanders(player) / static_cast<int>(possibleTargets.size());
	auto additionalConstructorsLimitPerTarget = getMaxConstructors(player) / static_cast<int>(possibleTargets.size());
	for (auto to : possibleTargets)
	{
		setMaxDroids(to, getMaxDroids(to) + additionalDroidsLimitPerTarget);
		setMaxCommanders(to, getMaxCommanders(to) + additionalCommandersLimitPerTarget);
		setMaxConstructors(to, getMaxConstructors(to) + additionalConstructorsLimitPerTarget);
	}

	// Distribute droids between targets as evenly as possible
	struct PlayerItemsReceived
	{
		uint32_t player = 0;
		uint32_t itemsRecv = 0;
	};
	std::vector<PlayerItemsReceived> droidsGiftedPerTarget;
	for (auto to : possibleTargets)
	{
		droidsGiftedPerTarget.push_back(PlayerItemsReceived{to, 0});
	}
	auto incrRecvItem = [&](size_t idx) {
		droidsGiftedPerTarget[idx].itemsRecv += 1;
		std::stable_sort(droidsGiftedPerTarget.begin(), droidsGiftedPerTarget.end(), [](const PlayerItemsReceived& a, const PlayerItemsReceived& b) -> bool {
			return a.itemsRecv < b.itemsRecv;
		});
	};
	mutating_list_iterate(apsDroidLists[player], [&droidsGiftedPerTarget, &incrRecvItem](DROID* d)
	{
		bool transferredDroid = false;
		if (!isDead(d))
		{
			for (size_t i = 0; i < droidsGiftedPerTarget.size(); ++i)
			{
				if (giftSingleDroid(d, droidsGiftedPerTarget[i].player, false))
				{
					transferredDroid = true;
					incrRecvItem(i);
					break;
				}
				// if we can't gift this droid to this player, try again with the next player in priority queue
			}
		}

		if (!transferredDroid)
		{
			destroyDroid(d, gameTime);
		}
		return IterationResult::CONTINUE_ITERATION;
	});

	auto distributeMatchingStructs = [&](std::function<bool (STRUCTURE *)> cmp)
	{
		std::vector<PlayerItemsReceived> structsGiftedPerTarget;
		for (auto to : possibleTargets)
		{
			structsGiftedPerTarget.push_back(PlayerItemsReceived{to, 0});
		}
		auto incrRecvStruct = [&](size_t idx) {
			structsGiftedPerTarget[idx].itemsRecv += 1;
			std::stable_sort(structsGiftedPerTarget.begin(), structsGiftedPerTarget.end(), [](const PlayerItemsReceived& a, const PlayerItemsReceived& b) -> bool {
				return a.itemsRecv < b.itemsRecv;
			});
		};

		mutating_list_iterate(apsStructLists[player], [&cmp, &structsGiftedPerTarget, &incrRecvStruct](STRUCTURE* psStruct)
		{
			if (psStruct && cmp(psStruct))
			{
				giftSingleStructure(psStruct, structsGiftedPerTarget.front().player, false);
				incrRecvStruct(0);
			}
			return IterationResult::CONTINUE_ITERATION;
		});
	};

	// Distribute key structures
	distributeMatchingStructs([](STRUCTURE *psStruct) { return psStruct->pStructureType->type == REF_RESOURCE_EXTRACTOR; });
	distributeMatchingStructs([](STRUCTURE *psStruct) { return psStruct->pStructureType->type == REF_POWER_GEN; });
	if (alliancesSharedResearch(game.alliance))
	{
		distributeMatchingStructs([](STRUCTURE *psStruct) { return psStruct->pStructureType->type == REF_RESEARCH; });
	}
	else
	{
		// destroy the research centers in unshared research mode
		destroyMatchingStructs(player, [](STRUCTURE *psStruct) { return psStruct->pStructureType->type == REF_RESEARCH; }, false);
	}
	distributeMatchingStructs([](STRUCTURE *psStruct) { return psStruct->pStructureType->type == REF_COMMAND_CONTROL; });
	distributeMatchingStructs([](STRUCTURE *psStruct) { return psStruct->isFactory(); });

	return true;
}

void handlePlayerLeftInGame(UDWORD player)
{
	ASSERT_OR_RETURN(, player < MAX_CONNECTED_PLAYERS, "Invalid player: %" PRIu32 "", player);

	bool leftWhilePlayer = NetPlay.players[player].isSpectator;

	ASSERT(player < NetPlay.playerReferences.size(), "Invalid player: %" PRIu32 "", player);
	NetPlay.playerReferences[player]->disconnect();
	NetPlay.playerReferences[player] = std::make_shared<PlayerReference>(player);

	debug(LOG_NET, "R.I.P. %s (%u).", getPlayerName(player), player);

	ingame.LagCounter[player] = 0;
	ingame.DesyncCounter[player] = 0;
	ingame.JoiningInProgress[player] = false;	// if they never joined, reset the flag
	ingame.PendingDisconnect[player] = false;
	ingame.DataIntegrity[player] = false;
	ingame.lastSentPlayerDataCheck2[player].reset();

	if (leftWhilePlayer)
	{
		ingame.playerLeftGameTime[player] = gameTime;
	}

	if (player >= MAX_PLAYERS)
	{
		return; // no more to do
	}

	PLAYER_LEAVE_MODE mode = game.playerLeaveMode;
	switch (mode)
	{
		case PLAYER_LEAVE_MODE::DESTROY_RESOURCES:
			destroyPlayerResources(player, false);
			break;
		case PLAYER_LEAVE_MODE::SPLIT_WITH_TEAM:
			if (!splitResourcesAmongTeam(player))
			{
				// no valid targets to split resources among
				// instead, destroy the player
				destroyPlayerResources(player, false);
			}
			break;
	}
}

// Reset visibility, so a new player can't see the old stuff!!
static void resetMultiVisibility(UDWORD player)
{
	UDWORD		owned;

	if (player >= MAX_PLAYERS)
	{
		return;
	}

	for (owned = 0 ; owned < MAX_PLAYERS ; owned++)		// for each player
	{
		if (owned != player)								// done reset own stuff..
		{
			//droids
			for (DROID* pDroid : apsDroidLists[owned])
			{
				pDroid->visible[player] = false;
			}

			//structures
			for (STRUCTURE* pStruct : apsStructLists[owned])
			{
				pStruct->visible[player] = false;
			}

		}
	}
	return;
}

static void sendPlayerLeft(uint32_t playerIndex)
{
	ASSERT_OR_RETURN(, NetPlay.isHost, "Only host should call this.");

	uint32_t forcedPlayerIndex = whosResponsible(playerIndex);
	NETQUEUE(*netQueueType)(unsigned) = forcedPlayerIndex != selectedPlayer ? NETgameQueueForced : NETgameQueue;
	auto w = NETbeginEncode(netQueueType(forcedPlayerIndex), GAME_PLAYER_LEFT);
	NETuint32_t(w, playerIndex);
	NETend(w);
}

static void addConsolePlayerLeftMessage(unsigned playerIndex)
{
	if (selectedPlayer != playerIndex)
	{
		std::string msg = astringf(_("%s has Left the Game"), getPlayerName(playerIndex));
		addConsoleMessage(msg.c_str(), DEFAULT_JUSTIFY, SYSTEM_MESSAGE);
	}
}

static void addConsolePlayerJoinMessage(unsigned playerIndex)
{
	if (selectedPlayer != playerIndex)
	{
		std::string msg = astringf(_("%s joined the Game"), getPlayerName(playerIndex));
		if ((game.blindMode != BLIND_MODE::NONE) && NetPlay.isHost && (NetPlay.hostPlayer >= MAX_PLAYER_SLOTS))
		{
			msg += " ";
			msg += astringf(_("(codename: %s)"), getPlayerGenericName(playerIndex));
		}
		addConsoleMessage(msg.c_str(), DEFAULT_JUSTIFY, SYSTEM_MESSAGE);
	}
}

void recvPlayerLeft(NETQUEUE queue)
{
	uint32_t playerIndex = 0;
	auto r = NETbeginDecode(queue, GAME_PLAYER_LEFT);
	NETuint32_t(r, playerIndex);
	NETend(r);

	addConsolePlayerLeftMessage(playerIndex);

	if (whosResponsible(playerIndex) != queue.index)
	{
		return;
	}

	turnOffMultiMsg(true);
	handlePlayerLeftInGame(playerIndex);
	turnOffMultiMsg(false);
	if (!ingame.TimeEveryoneIsInGame.has_value()) // If game hasn't actually started
	{
		clearPlayerMultiStats(playerIndex); // local only
	}
	NetPlay.players[playerIndex].allocated = false;

	NETsetPlayerConnectionStatus(CONNECTIONSTATUS_PLAYER_DROPPED, playerIndex);
	cancelOrDismissKickVote(playerIndex);

	debug(LOG_INFO, "** player %u has dropped, in-game! (gameTime: %" PRIu32 ")", playerIndex, gameTime);

	// fire script callback to reassign skirmish players.
	if (GetGameMode() == GS_NORMAL)
	{
		triggerEventPlayerLeft(playerIndex);
	}

	ActivityManager::instance().updateMultiplayGameData(game, ingame, NETGameIsLocked());

	wz_command_interface_output_room_status_json();
}

// ////////////////////////////////////////////////////////////////////////////
// A remote player has left the game
bool MultiPlayerLeave(UDWORD playerIndex)
{
	if (playerIndex >= MAX_CONNECTED_PLAYERS)
	{
		ASSERT(false, "Bad player number");
		return false;
	}

	if (NetPlay.isHost)
	{
		multiClearHostRequestMoveToPlayer(playerIndex);
		multiSyncResetPlayerChallenge(playerIndex);
		resetMultiOptionPrefValues(playerIndex);
	}

	NETlogEntry("Player leaving game", SYNC_FLAG, playerIndex);
	debug(LOG_NET, "** Player %u [%s], has left the game at game time %u.", playerIndex, getPlayerName(playerIndex), gameTime);

	ingame.muteChat[playerIndex] = false;

	if (wz_command_interface_enabled())
	{
		// WZEVENT: playerLeft: <playerIdx> <gameTime> <b64pubkey> <hash> <V|?> <b64name> <ip>
		const auto& identity = getOutputPlayerIdentity(playerIndex);
		std::string playerPublicKeyB64 = base64Encode(identity.toBytes(EcKey::Public));
		std::string playerIdentityHash = identity.publicHashString();
		std::string playerVerifiedStatus = (ingame.VerifiedIdentity[playerIndex]) ? "V" : "?";
		std::string playerName = getPlayerName(playerIndex);
		std::string playerNameB64 = base64Encode(std::vector<unsigned char>(playerName.begin(), playerName.end()));
		wz_command_interface_output("WZEVENT: playerLeft: %" PRIu32 " %" PRIu32 " %s %s %s %s %s\n", playerIndex, gameTime, playerPublicKeyB64.c_str(), playerIdentityHash.c_str(), playerVerifiedStatus.c_str(), playerNameB64.c_str(), NetPlay.players[playerIndex].IPtextAddress);
	}

	if (ingame.localJoiningInProgress)
	{
		addConsolePlayerLeftMessage(playerIndex);
		clearPlayer(playerIndex, false);
		clearPlayerMultiStats(playerIndex); // local only
		NetPlay.players[playerIndex].difficulty = AIDifficulty::DISABLED;
	}
	else if (NetPlay.isHost)  // If hosting, and game has started (not in pre-game lobby screen, that is).
	{
		sendPlayerLeft(playerIndex);

		if (bDisplayMultiJoiningStatus) // if still waiting for players to load *or* waiting for game to start...
		{
			auto w = NETbeginEncode(NETbroadcastQueue(), NET_PLAYER_DROPPED);
			NETuint32_t(w, playerIndex);
			NETend(w);
			// only set ingame.JoiningInProgress[player_id] to false
			// when the game starts, it will handle the GAME_PLAYER_LEFT message in their queue properly
			ingame.JoiningInProgress[playerIndex] = false;
			ingame.PendingDisconnect[playerIndex] = true; // used as a UI indicator that a disconnect will be processed in the future
		}
	}

	if (NetPlay.players[playerIndex].wzFiles && NetPlay.players[playerIndex].fileSendInProgress())
	{
		char buf[256];

		ssprintf(buf, _("File transfer has been aborted for %d.") , playerIndex);
		addConsoleMessage(buf, DEFAULT_JUSTIFY, SYSTEM_MESSAGE);
		debug(LOG_INFO, "=== File has been aborted for %d ===", playerIndex);
		NetPlay.players[playerIndex].wzFiles->clear();
	}

	if (widgGetFromID(psWScreen, IDRET_FORM))
	{
		if (playerIndex < MAX_PLAYERS) // only play audio when *player* slots drop (ignore spectator slots)
		{
			audio_QueueTrack(ID_CLAN_EXIT);
		}
	}

	netPlayersUpdated = true;
	return true;
}

// ////////////////////////////////////////////////////////////////////////////
// A Remote Player has joined the game.
bool MultiPlayerJoin(UDWORD playerIndex, optional<EcKey::Key> verifiedJoinIdentity)
{
	if (widgGetFromID(psWScreen, IDRET_FORM))	// if ingame.
	{
		audio_QueueTrack(ID_CLAN_ENTER);
	}

	if (widgGetFromID(psWScreen, MULTIOP_PLAYERS))	// if in multimenu.
	{
		if (!multiRequestUp && (NetPlay.isHost || ingame.localJoiningInProgress))
		{
			netPlayersUpdated = true;	// update the player box.
		}
	}

	if (NetPlay.isHost)		// host responsible for welcoming this player.
	{
		// if we've already received a request from this player don't reallocate.
		if (ingame.JoiningInProgress[playerIndex])
		{
			return true;
		}
		ASSERT(NetPlay.playercount <= MAX_PLAYERS, "Too many players!");
		ASSERT(GetGameMode() != GS_NORMAL, "A player joined after the game started??");

		// setup data for this player, then broadcast it to the other players.
		setupNewPlayer(playerIndex);						// setup all the guff for that player.
		if (verifiedJoinIdentity.has_value())
		{
			multiStatsSetVerifiedIdentityFromJoin(playerIndex, verifiedJoinIdentity.value());
		}
		sendOptions();
		// if skirmish and game full, then kick...
		if (NetPlay.playercount > game.maxPlayers)
		{
			kickPlayer(playerIndex, _("The game is already full."), ERROR_FULL, false);
		}
		// send everyone's stats to the new guy
		{
			int i;

			for (i = 0; i < MAX_CONNECTED_PLAYERS; i++)
			{
				if (NetPlay.players[i].allocated)
				{
					sendMultiStats(i);
				}
			}
		}
		if (lobby_slashcommands_enabled())
		{
			// Inform the new player that this lobby has slash commands enabled.
			sendRoomSystemMessageToSingleReceiver("Lobby slash commands enabled. Type " LOBBY_COMMAND_PREFIX "help to see details.", playerIndex, true);
		}
	}
	addConsolePlayerJoinMessage(playerIndex);
	return true;
}

bool sendDataCheck()
{
	int i = 0;

	auto w = NETbeginEncode(NETnetQueue(NetPlay.hostPlayer), NET_DATA_CHECK);		// only need to send to HOST
	for (i = 0; i < DATA_MAXDATA; i++)
	{
		NETuint32_t(w, DataHash[i]);
	}
	NETend(w);
	debug(LOG_NET, "sent hash to host");
	return true;
}

bool recvDataCheck(NETQUEUE queue)
{
	int i = 0;
	uint32_t player = queue.index;
	uint32_t tempBuffer[DATA_MAXDATA] = {0};

	if (!NetPlay.isHost)				// only host should act
	{
		ASSERT(false, "Host only routine detected for client!");
		return false;
	}

	auto r = NETbeginDecode(queue, NET_DATA_CHECK);
	for (i = 0; i < DATA_MAXDATA; i++)
	{
		NETuint32_t(r, tempBuffer[i]);
	}
	NETend(r);

	if (player >= MAX_CONNECTED_PLAYERS) // invalid player number.
	{
		debug(LOG_ERROR, "invalid player number (%u) detected.", player);
		return false;
	}

	if (whosResponsible(player) != queue.index)
	{
		HandleBadParam("NET_DATA_CHECK given incorrect params.", player, queue.index);
		return false;
	}

	debug(LOG_NET, "** Received NET_DATA_CHECK from player %u", player);

	if (NetPlay.isHost)
	{
		if (memcmp(DataHash, tempBuffer, sizeof(DataHash)))
		{
			char msg[256] = {'\0'};

			for (i = 0; DataHash[i] == tempBuffer[i]; ++i)
			{
			}

			snprintf(msg, sizeof(msg), _("%s (%u) has an incompatible mod, and has been kicked."), getPlayerName(player), player);
			sendInGameSystemMessage(msg);
			addConsoleMessage(msg, LEFT_JUSTIFY, NOTIFY_MESSAGE);

			kickPlayer(player, _("Your data doesn't match the host's!"), ERROR_WRONGDATA, false);
			debug(LOG_ERROR, "%s (%u) has an incompatible mod. ([%d] got %x, expected %x)", getPlayerName(player), player, i, tempBuffer[i], DataHash[i]);

			return false;
		}
		else
		{
			debug(LOG_NET, "DataCheck message received and verified for player %s (slot=%u)", getPlayerName(player), player);
			ingame.DataIntegrity[player] = true;
		}
	}
	return true;
}
// ////////////////////////////////////////////////////////////////////////////
// Setup Stuff for a new player.
void setupNewPlayer(UDWORD player)
{
	ASSERT_HOST_ONLY(return);
	ASSERT_OR_RETURN(, player < MAX_CONNECTED_PLAYERS, "Invalid player: %" PRIu32 "", player);

	ingame.PingTimes[player] = 0;					// Reset ping time
	ingame.LagCounter[player] = 0;
	ingame.DesyncCounter[player] = 0;
	ingame.VerifiedIdentity[player] = false;
	ingame.JoiningInProgress[player] = true;			// Note that player is now joining
	ingame.joinTimes[player] = std::chrono::steady_clock::now();
	ingame.PendingDisconnect[player] = false;
	ingame.DataIntegrity[player] = false;
	ingame.hostChatPermissions[player] = (NetPlay.bComms) ? NETgetDefaultMPHostFreeChatPreference() : true;
	ingame.lastSentPlayerDataCheck2[player].reset();
	ingame.muteChat[player] = false;
	ingame.lastReadyTimes[player].reset();
	if (multiplayPlayersCanCheckReady())
	{
		ingame.lastNotReadyTimes[player] = ingame.joinTimes[player];
	}
	else
	{
		ingame.lastNotReadyTimes[player].reset();
	}
	ingame.secondsNotReady[player] = 0;
	ingame.playerLeftGameTime[player].reset();
	multiSyncResetPlayerChallenge(player);

	resetMultiVisibility(player);						// set visibility flags.

	setMultiStats(player, getMultiStats(player), true);  // get the players score

	if (selectedPlayer != player)
	{
		char buf[255];
		ssprintf(buf, _("%s is joining the game"), getPlayerName(player));
		addConsoleMessage(buf, DEFAULT_JUSTIFY, SYSTEM_MESSAGE);
	}
}


// While not the perfect place for this, it has to do when a HOST joins (hosts) game
// unfortunately, we don't get the message until after the setup is done.
void ShowMOTD()
{
	char buf[250] = { '\0' };
	// when HOST joins the game, show server MOTD message first
	addConsoleMessage(_("Server message:"), DEFAULT_JUSTIFY, NOTIFY_MESSAGE);
	if (NetPlay.MOTD)
	{
		addConsoleMessage(NetPlay.MOTD, DEFAULT_JUSTIFY, NOTIFY_MESSAGE);
	}
	else
	{
		ssprintf(buf, "%s", "Null message");
		addConsoleMessage(buf, DEFAULT_JUSTIFY, NOTIFY_MESSAGE);
	}
	if (NetPlay.HaveUpgrade)
	{
		audio_PlayBuildFailedOnce();
		ssprintf(buf, "%s", _("There is an update to the game, please visit https://wz2100.net to download new version."));
		addConsoleMessage(buf, DEFAULT_JUSTIFY, NOTIFY_MESSAGE);
	}
	else
	{
		audio_PlayTrack(FE_AUDIO_MESSAGEEND);
	}

}
