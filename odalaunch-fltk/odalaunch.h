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
//  Global defines.
//
//-----------------------------------------------------------------------------

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//
// ARRAY_LENGTH
//
// Safely counts the number of items in an C array.
//
// https://www.drdobbs.com/cpp/counting-array-elements-at-compile-time/197800525?pgno=1
//
#define ARRAY_LENGTH(arr)                                                  \
	(0 * sizeof(reinterpret_cast<const ::Bad_arg_to_ARRAY_LENGTH*>(arr)) + \
	 0 * sizeof(::Bad_arg_to_ARRAY_LENGTH::check_type((arr), &(arr))) +    \
	 sizeof(arr) / sizeof((arr)[0]))

struct Bad_arg_to_ARRAY_LENGTH
{
	class Is_pointer; // incomplete
	class Is_array
	{
	};
	template <typename T>
	static Is_pointer check_type(const T*, const T* const*);
	static Is_array check_type(const void*, const void*);
};
