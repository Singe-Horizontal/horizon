/***************************************************
 *       _   _            _                        *
 *      | | | |          (_)                       *
 *      | |_| | ___  _ __ _ _______  _ __          *
 *      |  _  |/ _ \| '__| |_  / _ \| '_  \        *
 *      | | | | (_) | |  | |/ / (_) | | | |        *
 *      \_| |_/\___/|_|  |_/___\___/|_| |_|        *
 ***************************************************
 * This file is part of Horizon (c).
 *
 * Copyright (c) 2019 Sagun K. (sagunxp@gmail.com).
 * Copyright (c) 2019 Horizon Dev Team.
 *
 * Base Author - Sagun K. (sagunxp@gmail.com)
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 **************************************************/

#ifndef HORIZON_ZONE_GAME_GRIDDEFINITIONS_HPP
#define HORIZON_ZONE_GAME_GRIDDEFINITIONS_HPP

#include "Core/Structures/TypeList.hpp"
#include "Server/Zone/Game/Map/Coordinates.hpp"

namespace Horizon
{
	namespace Zone
	{
		namespace Entities
		{
			class Player;
			class NPC;
			class Elemental;
			class Homunculus;
			class Mercenary;
			class Pet;
			class Monster;
			class Skill;
		}
	}
}

template <class ZONE_OBJECT_TYPES>
class GridHolder;

template <int16_t MAX_COORDINATES>
class Coordinates;

template<class OBJECT_TYPES>
class GridReferenceContainer;

// Max Cells in a map are 416.
#define MAX_CELLS_PER_GRID 32
#define MAX_GRIDS_PER_MAP 14
#define MAX_CELLS_PER_MAP MAX_CELLS_PER_GRID * MAX_GRIDS_PER_MAP

#define entity_ns(e) Horizon::Zone::Entities::e
typedef TYPELIST_8(entity_ns(Player),
	entity_ns(NPC),
	entity_ns(Elemental),
	entity_ns(Homunculus),
	entity_ns(Mercenary),
	entity_ns(Pet),
	entity_ns(Monster),
	entity_ns(Skill)) AllEntityTypes;
typedef GridHolder<AllEntityTypes> GridHolderType;
#undef entity_ns

typedef Coordinates<MAX_GRIDS_PER_MAP> GridCoords;
typedef Coordinates<MAX_CELLS_PER_MAP> MapCoords;

typedef GridReferenceContainer<AllEntityTypes> MapEntityContainer;

#endif /* HORIZON_ZONE_GAME_GRIDDEFINITIONS_HPP */
