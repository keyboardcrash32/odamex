// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2006-2020 by The Odamex Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//   Setup a game, startup stuff.
//
//-----------------------------------------------------------------------------

#pragma once

class OString;
class ResourceId;

// NOT called by W_Ticker. Fixme.
//
// [RH] The only parameter used is mapname, so I removed playermask and skill.
//		On September 1, 1998, I added the position to indicate which set
//		of single-player start spots should be spawned in the level.
void P_SetupLevel (const OString& mapname, int position);
void P_TranslateLineDef(line_t* ld, maplinedef_t* mld);

// Called by startup code.
void P_Init();

void P_SetTransferHeightBlends(side_t* sd, const mapsidedef_t* msd);
void P_SetTextureNoErr(ResourceId* res_id_ptr, unsigned int *color, const OString& texture_name);
