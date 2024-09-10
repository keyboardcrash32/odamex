// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 2006-2021 by The Odamex Team.
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
//  Server table
//
//-----------------------------------------------------------------------------

#include "odalaunch.h"

#include "gui/server_table.h"

#include "FL/fl_draw.H"

/******************************************************************************/

enum column_e
{
	COL_ADDRESS,
	COL_SERVERNAME,
	COL_GAMETYPE,
	COL_WADS,
	COL_MAP,
	COL_PLAYERS,
	COL_PING
};

static const int MAX_COLUMNS = COL_PING + 1;

static const char* HEADER_STRINGS[] = {"Address", "Server Name", "Gametype", "WADs",
                                       "Map",     "Players",     "Ping"};

/******************************************************************************/

ServerTable::ServerTable(int X, int Y, int W, int H, const char* l)
    : Fl_Table_Row(X, Y, W, H, l)
{
	type(SELECT_SINGLE);
	cols(MAX_COLUMNS);
	col_header(1);
	col_resize(1);
}

/******************************************************************************/

void ServerTable::draw_cell(TableContext context, int R, int C, int X, int Y, int W,
                            int H)
{
	switch (context)
	{
	case CONTEXT_STARTPAGE:
		DB_GetServerList(m_servers);
		rows(int(m_servers.size()));
		break;
	case CONTEXT_ENDPAGE:
		break;
	case CONTEXT_ROW_HEADER:
		break;
	case CONTEXT_COL_HEADER:
		if (C >= ARRAY_LENGTH(::HEADER_STRINGS))
			break;

		fl_push_clip(X, Y, W, H);
		{
			fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, ::FL_BACKGROUND_COLOR);
			fl_font(::FL_HELVETICA_BOLD, 14);
			fl_color(::FL_BLACK);
			fl_draw(::HEADER_STRINGS[C], X, Y, W, H, ::FL_ALIGN_CENTER, 0, 0);
		}
		fl_pop_clip();
		break;
	case CONTEXT_CELL:
		if (C >= ARRAY_LENGTH(::HEADER_STRINGS))
			break;

		fl_push_clip(X, Y, W, H);
		{
			if (row_selected(R))
				fl_color(::FL_BLUE);
			else
				fl_color(::FL_WHITE);

			fl_rectf(X, Y, W, H);

			fl_font(::FL_HELVETICA, 14);
			fl_color(::FL_BLACK);

			switch (C)
			{
			case COL_ADDRESS:
				fl_draw(m_servers[R].address.c_str(), X, Y, W, H, ::FL_ALIGN_LEFT, 0, 0);
				break;
			case COL_SERVERNAME:
				fl_draw(m_servers[R].servername.c_str(), X, Y, W, H, ::FL_ALIGN_LEFT, 0,
				        0);
				break;
			case COL_GAMETYPE:
				fl_draw(m_servers[R].gametype.c_str(), X, Y, W, H, ::FL_ALIGN_LEFT, 0, 0);
				break;
			case COL_WADS:
				fl_draw(m_servers[R].Wads().c_str(), X, Y, W, H, ::FL_ALIGN_LEFT, 0, 0);
				break;
			case COL_MAP:
				fl_draw(m_servers[R].map.c_str(), X, Y, W, H, ::FL_ALIGN_LEFT, 0, 0);
				break;
			case COL_PLAYERS:
				fl_draw(m_servers[R].players.c_str(), X, Y, W, H, ::FL_ALIGN_LEFT, 0, 0);
				break;
			case COL_PING:
				fl_draw(m_servers[R].ping.c_str(), X, Y, W, H, ::FL_ALIGN_LEFT, 0, 0);
				break;
			default:
				break;
			}
		}
		fl_pop_clip();
		break;
	case CONTEXT_RC_RESIZE:
		break;
	}
}

/******************************************************************************/

std::string ServerTable::getSelectedAddress()
{
	int row_top, col_left, row_bot, col_right;
	get_selection(row_top, col_left, row_bot, col_right);
	if (row_top == -1)
	{
		// No server selected.
		return "";
	}
	else if (row_top >= m_servers.size())
	{
		// Row does not map to a server.
		return "";
	}

	return m_servers[row_top].address;
}
