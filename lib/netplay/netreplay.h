/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2010  Warzone 2100 Project

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
#ifndef _NETREPLAY_H
#define _NETREPLAY_H

#include "lib/framework/frame.h"

#include "netplay.h"


std::string NETreplaySaveStart(std::string const& subdir, ReplayOptionsHandler const &optionsHandler, int maxReplaysSaved, bool appendPlayerToFilename = false);
bool NETreplaySaveStop(ReplayOptionsHandler const &optionsHandler);
void NETreplaySaveNetMessage(NetMessage const *message, uint8_t player);

bool NETreplayLoadStart(std::string const &filename, ReplayOptionsHandler& optionsHandler, uint32_t& output_replayFormatVer);
bool NETreplayLoadNetMessage(std::unique_ptr<NetMessage> &message, uint8_t &player);
bool NETreplayLoadStop();

#endif // _NETREPLAY_H
