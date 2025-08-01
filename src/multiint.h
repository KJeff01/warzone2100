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
/** @file
 *  Interface defines/externs for warzone frontend.
 */

#ifndef __INCLUDED_SRC_MULTIINT_H__
#define __INCLUDED_SRC_MULTIINT_H__

#include "lib/ivis_opengl/ivisdef.h"
#include "lib/netplay/netplay.h"
#include "lib/widget/widgbase.h"
#include "lib/widget/form.h"
#include "lib/widget/button.h"
#include <functional>
#include <vector>
#include <set>
#include "lib/framework/wzstring.h"
#include "titleui/multiplayer.h"
#include "faction.h"

#define MAX_LEN_AI_NAME   40
#define AI_CUSTOM        127
#define AI_OPEN           -2
#define AI_CLOSED         -1
#define AI_NOT_FOUND     -99
#define DEFAULT_SKIRMISH_AI_SCRIPT_NAME "nexus.js"

// WzMultiplayerOptionsTitleUI is in titleui.h to prevent dependency explosions

struct WzMultiButton : public W_BUTTON
{
	WzMultiButton() : W_BUTTON() {}

	void display(int xOffset, int yOffset) override;

	AtlasImage imNormal;
	AtlasImage imDown;
	unsigned doHighlight;
	unsigned tc;
	uint8_t alpha = 255;
	unsigned downStateMask = WBUT_DOWN | WBUT_LOCK | WBUT_CLICKLOCK;
	unsigned greyStateMask = WBUT_DISABLE;
	optional<bool> drawBlueBorder;
};

void calcBackdropLayoutForMultiplayerOptionsTitleUI(WIDGET *psWidget);
void readAIs();	///< step 1, load AI definition files
void loadMultiScripts();	///< step 2, load the actual AI scripts
const char *getAIName(int player);	///< only run this -after- readAIs() is called
const std::vector<WzString> getAINames();
int matchAIbyName(const char* name);	///< only run this -after- readAIs() is called

struct AIDATA
{
	AIDATA() : name{0}, js{0}, tip{0}, difficultyTips{0}, assigned(0) {}
	char name[MAX_LEN_AI_NAME];
	char js[MAX_LEN_AI_NAME];
	char tip[255 + 128];            ///< may contain optional AI tournament data
	char difficultyTips[5][255];    ///< optional difficulty level info
	int assigned;                   ///< How many AIs have we assigned of this type
};
const std::vector<AIDATA>& getAIData();

struct MultiplayOptionsLocked
{
	bool scavengers;
	bool alliances;
	bool teams;
	bool power;
	bool difficulty;
	bool ai;
	bool position;
	bool bases;
	bool spectators;
};
const MultiplayOptionsLocked& getLockedOptions();

const char* getDifficultyListStr(size_t idx);
size_t getDifficultyListCount();
int difficultyIcon(int difficulty);

std::set<uint32_t> validPlayerIdxTargetsForPlayerPositionMove(uint32_t player);

bool isHostOrAdmin();
bool isPlayerHostOrAdmin(uint32_t playerIdx);
bool isSpectatorOnlySlot(UDWORD playerIdx);

void printBlindModeHelpMessagesToConsole();

/**
 * Checks if all players are on the same team. If so, return that team; if not, return -1;
 * if there are no players, return team MAX_PLAYERS.
 */
int allPlayersOnSameTeam(int except);

bool multiplayPlayersCanCheckReady();
void handlePossiblePlayersCanCheckReadyChange(bool previousPlayersCanCheckReadyValue);

bool multiplayPlayersReady();
bool multiplayIsStartingGame();

bool sendReadyRequest(UBYTE player, bool bReady);

LOBBY_ERROR_TYPES getLobbyError();
void setLobbyError(LOBBY_ERROR_TYPES error_type);

/**
 * Updates structure limit flags. Flags indicate which structures are disabled.
 */
void updateStructureDisabledFlags();

void intDisplayFeBox(WIDGET *psWidget, UDWORD xOffset, UDWORD yOffset);
void intDisplayFeBox_Spectator(WIDGET *psWidget, UDWORD xOffset, UDWORD yOffset);

std::shared_ptr<W_BUTTON> addMultiBut(WIDGET &parent, UDWORD id, UDWORD x, UDWORD y, UDWORD width, UDWORD height, const char *tipres, UDWORD norm, UDWORD down, UDWORD hi, unsigned tc = MAX_PLAYERS, uint8_t alpha = 255);
/**
 * @deprecated use `addMultiBut(WIDGET &parent, UDWORD id, UDWORD x, UDWORD y, UDWORD width, UDWORD height, const char *tipres, UDWORD norm, UDWORD down, UDWORD hi, unsigned tc = MAX_PLAYERS, uint8_t alpha = 255)` instead
 **/
std::shared_ptr<W_BUTTON> addMultiBut(const std::shared_ptr<W_SCREEN> &screen, UDWORD formid, UDWORD id, UDWORD x, UDWORD y, UDWORD width, UDWORD height, const char *tipres, UDWORD norm, UDWORD down, UDWORD hi, unsigned tc = MAX_PLAYERS, uint8_t alpha = 255);
std::shared_ptr<WzMultiButton> makeMultiBut(UDWORD id, UDWORD width, UDWORD height, const char *tipres, UDWORD norm, UDWORD down, UDWORD hi, unsigned tc, uint8_t alpha = 255);

AtlasImage mpwidgetGetFrontHighlightImage(AtlasImage image);
bool changeColour(unsigned player, int col, uint32_t responsibleIdx);

extern char sPlayer[128];
extern bool multiintDisableLobbyRefresh; // gamefind
extern std::string defaultSkirmishAI;

void frontendCycleAIs();
void setDefaultSkirmishAI(const std::string& name);
std::string getDefaultSkirmishAI(const bool& displayNameOnly=false);

void kickPlayer(uint32_t player_id, const char *reason, LOBBY_ERROR_TYPES type, bool banPlayer = false);
void displayKickReasonPopup(const std::string &reason);
void loadMapPreview(bool hideInterface);

bool changeReadyStatus(UBYTE player, bool bReady);
WzString formatGameName(WzString name);
void sendRoomSystemMessage(char const *text);
void sendRoomNotifyMessage(char const *text);
void sendRoomSystemMessageToSingleReceiver(char const *text, uint32_t receiver, bool skipLocalDisplay = false);
void displayRoomSystemMessage(char const *text);
void displayRoomNotifyMessage(char const *text);
void displayLobbyDisabledNotification();

void multiLobbyHandleHostOptionsChanges(const std::array<bool, MAX_CONNECTED_PLAYERS>& priorHostChatPermissions);

void multiLobbyRandomizeOptions();

bool SendColourRequest(UBYTE player, UBYTE col);

void handleAutoReadyRequest();

void multiClearHostRequestMoveToPlayer(uint32_t playerIdx);

bool autoBalancePlayersCmd();

// ////////////////////////////////////////////////////////////////
// CONNECTION SCREEN

#define CON_TYPESID_START	10105
#define CON_TYPESID_END		10128

#define CON_SETTINGS		10130
#define CON_SETTINGS_LABEL	10131
#define CON_SETTINGSX		220 + D_W
#define	CON_SETTINGSY		190 + D_H
#define CON_SETTINGSWIDTH	200
#define CON_SETTINGSHEIGHT	100

#define CON_OK				10101
#define CON_OKX				CON_SETTINGSWIDTH-MULTIOP_OKW*2-13
#define CON_OKY				CON_SETTINGSHEIGHT-MULTIOP_OKH-3

#define CON_CANCEL			10102

#define CON_PHONE			10132
#define CON_PHONEX			20

#define CON_IP				10133
#define CON_IPX				20
#define CON_IPY				45

#define CON_IP_CANCEL		10134

#define CON_SPECTATOR_BOX	10135

//for clients
#define CON_PASSWORD		10139
#define CON_PASSWORDYES		10141
#define CON_PASSWORDNO		10142


// ////////////////////////////////////////////////////////////////
// GAME FIND SCREEN

#define GAMES_GAMEHEADER	10200
#define GAMES_GAMELIST		10201
#define GAMES_MAX           100
#define GAMES_GAMEWIDTH		525
#define GAMES_GAMEHEIGHT	28
// We can have a max of 4 icons for status, current icon size if 36x25.
#define GAMES_STATUS_START 378
#define GAMES_GAMENAME_START 2
#define GAMES_MAPNAME_START 168
#define GAMES_MODNAME_START 168 + 6		// indent a bit
#define GAMES_PLAYERS_START 342

// ////////////////////////////////////////////////////////////////
// GAME OPTIONS SCREEN

#define MULTIOP_PLAYERS			10231
#define MULTIOP_PLAYERSX		315
#define MULTIOP_PLAYERSY		1
#define MULTIOP_PLAYER_START	102350		//list of players
#define MULTIOP_PLAYER_END		102381
#define MULTIOP_PLAYERSW		298
#define MULTIOP_PLAYERS_TABS	10232
#define MULTIOP_PLAYERS_TABS_H	24
#define MULTIOP_PLAYERSH		(384 + MULTIOP_PLAYERS_TABS_H + 1)
#define MULTIOP_BLIND_WAITING_ROOM	10233

#define MULTIOP_ROW_WIDTH		298

//Team chooser
#define MULTIOP_TEAMS_START		102310			//List of teams
#define MULTIOP_TEAMS_END		102341
#define MULTIOP_TEAMSWIDTH		28
#define	MULTIOP_TEAMSHEIGHT		38

#define MULTIOP_TEAMCHOOSER_FORM		102800
#define MULTIOP_TEAMCHOOSER				102810
#define MULTIOP_TEAMCHOOSER_END     	102841
#define MULTIOP_TEAMCHOOSER_KICK		10289
#define MULTIOP_TEAMCHOOSER_SPECTATOR	10288
#define MULTIOP_TEAMCHOOSER_BAN			10287

#define MULTIOP_INLINE_OVERLAY_ROOT_FRM	10286

// 'Ready' button
#define MULTIOP_READY_FORM_ID		102900
#define MULTIOP_READY_START         (MULTIOP_READY_FORM_ID + MAX_CONNECTED_PLAYERS + 1)
#define	MULTIOP_READY_END           (MULTIOP_READY_START + MAX_CONNECTED_PLAYERS - 1)
#define MULTIOP_READY_WIDTH			41
#define MULTIOP_READY_HEIGHT		38

#define MULTIOP_PLAYERWIDTH		282
#define	MULTIOP_PLAYERHEIGHT	38

#define MULTIOP_OPTIONS			10250
#define MULTIOP_OPTIONSX		32
#define MULTIOP_OPTIONSY		1
#define MULTIOP_OPTIONSW		284
#define MULTIOP_OPTIONSH		MULTIOP_PLAYERSH

#define MULTIOP_EDITBOXW		201
#define	MULTIOP_EDITBOXH		30

#define MULTIOP_SEARCHBOXH		15

#define	MULTIOP_BLUEFORMW		231

#define	MROW1					6

#define MCOL0					45
#define MCOL1					(MCOL0+26+10)	// rem 10 for 4 lines.
#define MCOL2					(MCOL1+38)
#define MCOL3					(MCOL2+38)
#define MCOL4					(MCOL3+38)

#define MULTIOP_PNAME_ICON		10252
#define MULTIOP_PNAME			10253
#define MULTIOP_MAP				10259

#define MULTIOP_REFRESH			10275

#define MULTIOP_FILTER_TOGGLE   30277

#define MULTIOP_STRUCTLIMITS	21277	// we are using 10277 already

#define MULTIOP_CANCELX			6
#define MULTIOP_CANCELY			6

#define MULTIOP_CHATBOX			10278
#define MULTIOP_CHATBOXX		MULTIOP_OPTIONSX
#define MULTIOP_CHATBOXY		MULTIOP_PLAYERSH
#define MULTIOP_CHATBOXW		((MULTIOP_PLAYERSX+MULTIOP_PLAYERSW) - MULTIOP_OPTIONSX)

#define MULTIOP_CONSOLEBOX		0x1A001		// TODO: these should be enums!
#define MULTIOP_CONSOLEBOXX		MULTIOP_OPTIONSX
#define MULTIOP_CONSOLEBOXY		422
#define MULTIOP_CONSOLEBOXW		((MULTIOP_PLAYERSX + MULTIOP_PLAYERSW) - MULTIOP_OPTIONSX)
#define MULTIOP_CONSOLEBOXH		64

#define MULTIOP_CHATEDIT		10279
#define MULTIOP_CHATEDITX		4
#define MULTIOP_CHATEDITH		20
#define	MULTIOP_CHATEDITW		(MULTIOP_CHATBOXW - 8)

#define MULTIOP_COLCHOOSER_FORM         10280
#define MULTIOP_COLCHOOSER              102711 //10281
#define MULTIOP_COLCHOOSER_END          102742 //10288

#define MULTIOP_COLOUR_START		10332
#define MULTIOP_COLOUR_END		(MULTIOP_COLOUR_START + MAX_PLAYERS)
#define MULTIOP_COLOUR_WIDTH		29

#define MULTIOP_AI_FORM			(MULTIOP_COLOUR_END + 1)
#define MULTIOP_AI_START		(MULTIOP_AI_FORM + 1)
#define MULTIOP_AI_END			(MULTIOP_AI_START * MAX_PLAYERS)
#define MULTIOP_AI_OPEN			(MULTIOP_AI_END + 1)
#define MULTIOP_AI_CLOSED		(MULTIOP_AI_END + 2)
#define MULTIOP_AI_SPECTATOR	(MULTIOP_AI_END + 3)

#define MULTIOP_DIFFICULTY_INIT_START	(MULTIOP_AI_END + 4)
#define	MULTIOP_DIFFICULTY_INIT_END	(MULTIOP_DIFFICULTY_INIT_START + MAX_PLAYERS)

#define MULTIOP_DIFFICULTY_CHOOSE_START	(MULTIOP_DIFFICULTY_INIT_END + 1)
#define MULTIOP_DIFFICULTY_CHOOSE_END	(MULTIOP_DIFFICULTY_CHOOSE_START + MAX_PLAYERS)

#define MULTIOP_ADD_SPECTATOR_SLOTS	(MULTIOP_DIFFICULTY_CHOOSE_END + 1)

#define MULTIOP_FACTION_START		(MULTIOP_ADD_SPECTATOR_SLOTS + 100000)
#define MULTIOP_FACTION_END		(MULTIOP_FACTION_START + MAX_PLAYERS)
#define MULTIOP_FACTION_WIDTH		31
#define MULTIOP_FACCHOOSER		(MULTIOP_FACTION_END + 1)
#define MULTIOP_FACCHOOSER_END		(MULTIOP_FACCHOOSER + NUM_FACTIONS)
#define MULTIOP_FACCHOOSER_FORM		(MULTIOP_FACCHOOSER_END+1)


// ///////////////////////////////
// Many Button Variations..

#define	CON_NAMEBOXWIDTH		CON_SETTINGSWIDTH-CON_PHONEX
#define	CON_NAMEBOXHEIGHT		15

#define MULTIOP_OKW			37
#define MULTIOP_OKH			24

#define MULTIOP_BUTW			35
#define MULTIOP_BUTH			24

#endif // __INCLUDED_SRC_MULTIINT_H__
