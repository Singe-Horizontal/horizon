/***************************************************
 *       _   _            _                        *
 *      | | | |          (_)                       *
 *      | |_| | ___  _ __ _ _______  _ __          *
 *      |  _  |/ _ \| '__| |_  / _ \| '_  \        *
 *      | | | | (_) | |  | |/ / (_) | | | |        *
 *      \_| |_/\___/|_|  |_/___\___/|_| |_|        *
 ***************************************************
 * This file is part of Horizon (c).
 * Copyright (c) 2020- Horizon Dev Team.
 *
 * Base Author - Sagun Khosla. (sagunxp@gmail.com)
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

#include "CharClientInterface.hpp"

#include "Server/Zone/Definitions/EntityDefinitions.hpp"
#include "Server/Zone/Definitions/ItemDefinitions.hpp"

#include "Server/Zone/Game/Entities/Player/Assets/Inventory.hpp"
#include "Server/Zone/Game/Entities/Player/Player.hpp"
#include "Server/Zone/Game/Entities/Traits/Status.hpp"
#include "Server/Zone/Game/Map/Map.hpp"
#include "Server/Zone/Game/Map/MapManager.hpp"
#include "Server/Zone/Game/Map/MapContainerThread.hpp"
#include "Server/Zone/Game/StaticDB/SkillDB.hpp"
#include "Server/Zone/Session/ZoneSession.hpp"
#include "Server/Zone/SocketMgr/ClientSocketMgr.hpp"
#include "Server/Zone/Zone.hpp"

using namespace Horizon::Zone;

ZoneClientInterface::ZoneClientInterface(std::shared_ptr<ZoneSession> s)
: ClientInterface(s), _chat_room(s), _trade(s), _party(s), _guild(s), _friend(s), _quest(s), 
  _auction(s), _mail(s), _clan(s), _party_booking(s)
{
	
}

ZoneClientInterface::~ZoneClientInterface()
{
}
bool ZoneClientInterface::login(uint32_t account_id, uint32_t char_id, uint32_t auth_code, uint32_t client_time, uint8_t gender)
{	
	mysqlx::Session session = sZone->database_pool()->get_connection();
	mysqlx::RowResult rr = session.sql("SELECT `current_server` FROM `session_data` WHERE `game_account_id` = ? AND `auth_code` = ?")
		.bind(account_id, auth_code)
		.execute();
	mysqlx::Row r = rr.fetchOne();

	if (r.isNull()) {
		HLog(error) << "Login error! Session data for game account " << account_id << " and authentication code " << auth_code << " does not exist.";
		return false;
	}

	std::string current_server = r[0].get<std::string>();
	if (current_server.compare("Z") == 0) { // Already on Zone.
		ZC_REFUSE_ENTER pkt(get_session());
		pkt.deliver(ZONE_SERV_ERROR_REJECT);
		return false;
	}

	mysqlx::RowResult rr2 = session.sql("SELECT `gender`, `group_id` FROM `game_accounts` WHERE `id` = ?")
		.bind(account_id)
		.execute();
	mysqlx::Row r2 = rr2.fetchOne();
	
	if (r2.isNull()) {
		HLog(error) << "Login error! Game account with id " << account_id << " does not exist.";
		return false;
	}
	
	session.sql("UPDATE `session_data` SET `current_server` = ? WHERE `game_account_id` = ? AND `auth_code` = ?")
		.bind("Z", account_id, auth_code)
		.execute();

	std::shared_ptr<Horizon::Zone::Entities::Player> pl = std::make_shared<Horizon::Zone::Entities::Player>(get_session(), account_id);
	
	pl->create(char_id, r2[0].get<std::string>(), r2[1].get<int>());

	ZC_AID zc_aid(get_session());
	zc_aid.deliver(pl->guid());
	
	ZC_ACCEPT_ENTER2 zc_ae2(get_session());
	zc_ae2.deliver(pl->map_coords().x(), pl->map_coords().y(), DIR_SOUTH, pl->character()._font); // edit third argument to saved font.
	
	get_session()->set_player(pl);
	
	ClientSocktMgr->set_socket_for_removal(get_session()->get_socket());
	
	sZone->database_pool()->release_connection(std::move(session));
	
	HLog(info) << "Player (" << pl->guid() << ") " << pl->name() << " has logged in.";
	return true;
}

/**
 * All cleanups are managed in ZoneSocket::perform_cleanup
 */
bool ZoneClientInterface::restart(uint8_t type)
{
	ZC_RESTART_ACK rpkt(get_session());
	
	switch (type) {
		case 0:
			HLog(info) << "Character is being respawned.";
			break;
		default:
			rpkt.deliver(type);
			HLog(info) << "Character has moved to the character server.";
			break;
	}
	
	return true;
}

/**
 * Character
 */
void ZoneClientInterface::request_name_by_char_id(int char_id)
{

}
void ZoneClientInterface::invite_baby(int account_id)
{

}
void ZoneClientInterface::pvpinfo(int character_id, int account_id)
{

}
bool ZoneClientInterface::disconnect(int8_t type)
{
	ZC_ACK_REQ_DISCONNECT pkt(get_session());
	
	HLog(debug) << "ZoneClientInterface::disconnect: Type :" << type;

	pkt.deliver(type); // 0 => Quit, 1 => Wait for 10 seconds
	
	MapMgr->manage_session_in_map(SESSION_ACTION_LOGOUT_AND_REMOVE, get_session()->get_map_name(), get_session());
	return true;
}
bool ZoneClientInterface::update_session(int32_t account_id)
{
	mysqlx::Session session = sZone->database_pool()->get_connection();
	mysqlx::RowResult rr = session.sql("SELECT `current_server` FROM `session_data` WHERE `id` = ?")
		.bind(account_id)
		.execute();
	mysqlx::Row r = rr.fetchOne();

	HLog(debug) << "Updating session from I.P. address " << get_session()->get_socket()->remote_ip_address();

	if (r.isNull()) {
		ZC_ACK_REQ_DISCONNECT pkt(get_session());
		HLog(warning) << "Invalid connection for account with ID " << account_id << ", session wasn't found.";
		pkt.deliver(0);
		return false;
	}

	std::string current_server = r[0].get<std::string>();

	if (current_server.compare("C") != 0) {
		ZC_ACK_REQ_DISCONNECT pkt(get_session());
		HLog(warning) << "Invalid connection for account with ID " << account_id << ", session wasn't found.";
		pkt.deliver(0);
		return false;
	}
	
	session.sql("UPDATE `session_data` SET `last_update` = ? WHERE `game_account_id` = ?")
		.bind(account_id, std::time(nullptr))
		.execute();

	sZone->database_pool()->release_connection(std::move(session));
	return true;
}
bool ZoneClientInterface::walk_to_coordinates(uint16_t x, uint16_t y, uint8_t dir)
{
	if (get_session()->player() == nullptr)
		return false;

	get_session()->player()->walk_to_coordinates(x, y);
	return true;
}
bool ZoneClientInterface::notify_time()
{
	ZC_NOTIFY_TIME pkt(get_session());
	pkt.deliver();
	return true;
}


bool ZoneClientInterface::notify_entity_name(uint32_t guid)
{
	std::shared_ptr<Entity> entity = get_session()->player()->get_nearby_entity(guid);
	
	if (entity == nullptr)
		return false;

#if (CLIENT_TYPE == 'M' && PACKET_VERSION >= 20150225) \
	|| (CLIENT_TYPE == 'R' && PACKET_VERSION >= 20141126) \
	|| (CLIENT_TYPE == 'Z')
	ZC_ACK_REQNAMEALL2 req(get_session());
	req.deliver(guid, std::string(entity->name()) , "", "", "", 0);
#else
	ZC_ACK_REQNAMEALL req(get_session());
	req.deliver(guid, std::string(entity->name()), "", "", "");
#endif
	
	return true;
}
bool ZoneClientInterface::stop_attack()
{
	get_session()->player()->stop_attack();
	return true;
}

entity_viewport_entry ZoneClientInterface::create_viewport_entry(std::shared_ptr<Entity> entity)
{
	entity_viewport_entry entry;

	if (entity == nullptr)
		return entry;
	
	std::shared_ptr<Horizon::Zone::Traits::Status> status = entity->status();

	if (status == nullptr)
		return entry;

	entry.guid = entity->guid();
	entry.unit_type = entity->type();
	entry.speed = status->movement_speed()->total();
	entry.body_state = 0;
	entry.health_state = 0;
	entry.effect_state = 0;
	entry.job_id = entity->job_id();
	entry.hair_style_id = status->hair_style()->get();
	entry.hair_color_id = status->hair_color()->get();
	entry.robe_id = status->robe_sprite()->get();
	entry.guild_id = 0;
	entry.guild_emblem_version = 0;
	entry.honor = entity->type() == ENTITY_PLAYER ? status->honor()->total() : 0;
	entry.virtue = entity->type() == ENTITY_PLAYER ? status->virtue()->total() : 0;
	entry.in_pk_mode = 0;
	entry.current_x = entity->map_coords().x();
	entry.current_y = entity->map_coords().y();
	entry.current_dir = entity->direction();
	
	if (entity->is_walking()) {
		entry.to_x = entity->dest_coords().x();
		entry.to_y = entity->dest_coords().y();
	}
	
	entry.posture = entity->posture();
	entry.base_level = status->base_level()->total();
	entry.font = 1;

	if (status->current_hp()->total() < status->max_hp()->total()) {
		entry.max_hp = status->max_hp()->total();
		entry.hp = status->current_hp()->total();
	}
	else {
		entry.max_hp = -1;
		entry.hp = -1;
	}
	
	entry.is_boss = 0;
	entry.body_style_id = 0;
	std::strncpy(entry.name, entity->name().c_str(), entity->name().size());
	
	switch (entry.unit_type)
	{
		case ENTITY_PLAYER:
			entry.character_id = entity->downcast<Horizon::Zone::Entities::Player>()->character()._character_id;
			entry.x_size = entry.y_size = 0;
			entry.gender = entity->downcast<Horizon::Zone::Entities::Player>()->character()._gender;
			break;
		case ENTITY_NPC:
		default:
			entry.x_size = entry.y_size = 0;
			break;
	}
	
	return entry;
}
bool ZoneClientInterface::notify_player_movement(MapCoords from, MapCoords to)
{
	ZC_NOTIFY_PLAYERMOVE pkt(get_session());
	pkt.deliver(from.x(), from.y(), to.x(), to.y());
	return true;
}
bool ZoneClientInterface::notify_movement_stop(int32_t guid, int16_t x, int16_t y)
{
	ZC_STOPMOVE pkt(get_session());
	pkt.deliver(guid, x, y);
	return true;
}
bool ZoneClientInterface::notify_viewport_add_entity(entity_viewport_entry entry)
{
#if PACKET_VERSION >= 20150513
	ZC_NOTIFY_STANDENTRY11 pkt(get_session());
	pkt.deliver(entry);
#endif
	return true;
}
bool ZoneClientInterface::notify_viewport_spawn_entity(entity_viewport_entry entry)
{
	ZC_NOTIFY_NEWENTRY11 pkt(get_session());
	pkt.deliver(entry);
	return true;
}
bool ZoneClientInterface::notify_viewport_moving_entity(entity_viewport_entry entry)
{
#if PACKET_VERSION >= 20150513
	ZC_NOTIFY_MOVEENTRY11 pkt(get_session());
	pkt.deliver(entry);
#endif
	return true;
}
bool ZoneClientInterface::notify_entity_move(int32_t guid, MapCoords from, MapCoords to)
{
	ZC_NOTIFY_MOVE pkt(get_session());
	pkt.deliver(guid, from.x(), from.y(), to.x(), to.y());
	return true;
}
bool ZoneClientInterface::notify_viewport_remove_entity(std::shared_ptr<Entity> entity, entity_viewport_notification_type type)
{
	if (entity == nullptr)
		 return false;

	ZC_NOTIFY_VANISH pkt(get_session());
	pkt.deliver(entity->guid(), type);
	return true;
}
bool ZoneClientInterface::notify_initial_status()
{
	if (get_session()->player() == nullptr)
		return false;

	std::shared_ptr<Horizon::Zone::Traits::Status> status = get_session()->player()->status();

	if (status == nullptr)
		return false;

	ZC_STATUS zcs(get_session());
	zc_status_data data;
	
	data.status_points = status->status_point()->get_base();
	data.strength = status->strength()->get_base();
	data.strength_req_stats = status->strength_cost()->get_base();
	data.agility = status->agility()->get_base();
	data.agility_req_stats = status->agility_cost()->get_base();
	data.vitality = status->vitality()->get_base();
	data.vitality_req_stats = status->vitality_cost()->get_base();
	data.intelligence = status->intelligence()->get_base();
	data.intelligence_req_stats = status->intelligence_cost()->get_base();
	data.dexterity = status->dexterity()->get_base();
	data.dexterity_req_stats = status->dexterity_cost()->get_base();
	data.luck = status->luck()->get_base();
	data.luck_req_stats = status->luck_cost()->get_base();
	data.status_atk = status->status_atk()->total();
	data.equip_atk = status->equip_atk()->total();
	data.status_matk = status->status_matk()->total();
	data.equip_matk = 0;
	data.soft_def = status->soft_def()->total();
	data.hard_def = 0;
	data.soft_mdef = status->soft_mdef()->total();
	data.hard_mdef = 0;
	data.hit = status->hit()->total();
	data.flee = status->flee()->total();
	data.perfect_dodge = 0;
	data.critical = status->crit()->total() / 10;
	data.attack_speed = status->attack_speed()->total();
	data.plus_aspd = 0;
	
	zcs.deliver(data);

	return true;
}
bool ZoneClientInterface::notify_appearance_update(entity_appearance_type type, int32_t value, int32_t value2)
{
	ZC_SPRITE_CHANGE2 pkt(get_session());
	pkt.deliver(get_session()->player()->guid(), type, value, value2);
	return true;
}

// 0x00b0
bool ZoneClientInterface::notify_compound_attribute_update(status_point_type type, int32_t value)
{
	ZC_PAR_CHANGE pkt(get_session());
	pkt.deliver(type, value);
	return true;
}

// 0x00bc
bool ZoneClientInterface::notify_status_attribute_update(status_point_type type, int32_t value, bool success)
{
	ZC_STATUS_CHANGE_ACK pkt(get_session());
	pkt.deliver(type, value, success);
	return true;
}

// 0x00be
bool ZoneClientInterface::notify_required_attribute_update(status_point_type type, int32_t value)
{
	ZC_STATUS_CHANGE pkt(get_session());
	pkt.deliver(type, value);
	return true;
}

// 0x0121
bool ZoneClientInterface::notify_cart_weight_update()
{
	return true;
}

// 0x013a
bool ZoneClientInterface::notify_attack_range_update(int32_t value)
{
	ZC_ATTACK_RANGE pkt(get_session());
	pkt.deliver(value);
	return true;

}

// 0x0acb
bool ZoneClientInterface::notify_experience_update(status_point_type type, int32_t value)
{

#if CLIENT_TYPE == 'Z' || \
	(CLIENT_TYPE == 'M' && PACKET_VERSION >= 20170906) || \
	(CLIENT_TYPE == 'R' && PACKET_VERSION >= 20170830)
	ZC_LONGLONGPAR_CHANGE pkt(get_session());
#else
	notify_compound_attribute_update(type, value);
#endif
	pkt.deliver(type, value);
	return true;
}

// 0x00b1
bool ZoneClientInterface::notify_zeny_update()
{
	return true;
}
bool ZoneClientInterface::increase_status_point(status_point_type type, uint8_t amount)
{
	std::shared_ptr<Horizon::Zone::Entities::Player> pl = get_session()->player();

	if (pl == nullptr)
		return false;

	pl->status()->increase_status_point(type, amount);
	return true;
}
void ZoneClientInterface::register_baby(int account_id, int character_id, cz_join_baby_reply_type response)
{

}
void ZoneClientInterface::npc_weapon_refine(int32_t inventory_index)
{

}
void ZoneClientInterface::npc_next_dialog(int32_t npc_guid)
{

}
void ZoneClientInterface::npc_contact(int32_t guid)
{

}
void ZoneClientInterface::npc_close(int32_t npc_guid)
{

}
void ZoneClientInterface::notify_npc_dialog(uint32_t npc_guid, std::string dialog)
{
	uint32_t current_npc_guid = get_session()->player()->npc_contact_guid();
	
}
void ZoneClientInterface::notify_npc_next_dialog(uint32_t npc_guid)
{
	ZC_WAIT_DIALOG pkt(get_session());
	pkt.deliver(npc_guid);
}
void ZoneClientInterface::notify_npc_close_dialog(uint32_t npc_guid)
{
	ZC_CLOSE_DIALOG pkt(get_session());
	pkt.deliver(npc_guid);
}
void ZoneClientInterface::notify_npc_menu_list(uint32_t npc_guid, std::string const &menu)
{
	ZC_MENU_LIST pkt(get_session());
	pkt.deliver(npc_guid, menu);
}
void ZoneClientInterface::npc_select_deal_type(int guid, cz_ack_select_dealtype deal_type)
{

}
void ZoneClientInterface::npc_select_menu(int guid, int choice)
{

}
void ZoneClientInterface::npc_input(int guid, int value)
{

}
void ZoneClientInterface::npc_input(int guid, std::string value)
{

}
void ZoneClientInterface::npc_purchase_items(std::vector<cz_pc_purchase_itemlist> items)
{

}
void ZoneClientInterface::npc_sell_items(std::vector<cz_pc_sell_itemlist> items)
{

}
void ZoneClientInterface::progress_bar_completed()
{

}
void ZoneClientInterface::vending_purchase_items(int account_id, std::vector<cz_pc_purchase_itemlist> items)
{

}
void ZoneClientInterface::vending_purchase_items(int account_id, int unique_id, std::vector<cz_pc_purchase_itemlist> items)
{

}
void ZoneClientInterface::broadcast(std::string message)
{

}
void ZoneClientInterface::broadcast_local(std::string message)
{

}

/**
 * Storage
 */
void ZoneClientInterface::storage_check_password(std::string password)
{

}
void ZoneClientInterface::storage_change_password(std::string password, std::string new_password)
{

}
bool ZoneClientInterface::notify_move_to_map(std::string map_name, int16_t x, int16_t y)
{
	ZC_NPCACK_MAPMOVE pkt(get_session());
	pkt.deliver(map_name, x, y);
	return true;
}
bool ZoneClientInterface::notify_chat(std::string message)
{
	ZC_NOTIFY_PLAYERCHAT pkt(get_session());
	pkt.deliver(message);
	return true;
}
void ZoneClientInterface::parse_chat_message(std::string message)
{
	ZC_NOTIFY_PLAYERCHAT notify_player_chat(get_session());
	ZC_NOTIFY_CHAT notify_chat(get_session());

	HLog(debug) << get_session()->player()->name() << ": " << message;

	int guid = get_session()->player()->guid();
	int msg_first_char = get_session()->player()->name().size() + 3;

	if (message[msg_first_char] == '@') {
		get_session()->player()->map()->container()->get_lua_manager()->player()->perform_command_from_player(get_session()->player(), &message[msg_first_char + 1]);
		return;
	}

	ByteBuffer buf;

	notify_chat._packet_length = 8 + message.size();
	notify_chat._guid = guid;
	notify_chat._message = message;
	buf = notify_chat.serialize();
	get_session()->player()->notify_in_area(buf, GRID_NOTIFY_AREA_WOS);
	notify_player_chat.deliver(message);
}
void ZoneClientInterface::map_enter()
{
	get_session()->player()->on_map_enter();
}
bool ZoneClientInterface::notify_map_properties(zc_map_properties p)
{
	ZC_MAPPROPERTY_R2 pkt(get_session());
	pkt.deliver(p);
	return true;
}
void ZoneClientInterface::whisper_message(const char *name, int32_t name_length, const char *message, int32_t message_length)
{
	// validate name
	// only restriction is that the name must be zero-terminated
	if (name[name_length] != '\0') {
		HLog(warning) << "ZoneClientInterface::whisper_message: Player '" << get_session()->player()->name() << "' sent an unterminated name!\n";
		return;
	}
	// validate name
	// only restriction is that the name must be zero-terminated
	if (message[message_length] != '\0') {
		HLog(warning) << "ZoneClientInterface::whisper_message: Player '" << get_session()->player()->name() << "' sent an unterminated message!\n";
		return;
	}

	HLog(debug) << name << " : " << message;

	std::shared_ptr<Horizon::Zone::Entities::Player> player = MapMgr->find_player(name);

	ZC_ACK_WHISPER02 pkt(get_session());
	if (player != nullptr)
		pkt.deliver(WRT_SUCCESS, player->character()._character_id);
	else
		pkt.deliver(WRT_RECIPIENT_OFFLINE, 0);

	ZC_WHISPER pkt2(player->get_session());
	pkt2.deliver(get_session()->player()->name(), message, player->account()._group_id >= 99 ? true : false);
}
void ZoneClientInterface::use_item(int16_t inventory_index, int32_t guid)
{
	get_session()->player()->inventory()->use_item(inventory_index, guid);
}
void ZoneClientInterface::equip_item(int16_t inventory_index, int16_t equip_location_mask)
{
	get_session()->player()->inventory()->equip_item(inventory_index, equip_location_mask);
}
void ZoneClientInterface::unequip_item(int16_t inventory_index)
{
	get_session()->player()->inventory()->unequip_item(inventory_index);
}
void ZoneClientInterface::pickup_item(int guid)
{

}
void ZoneClientInterface::throw_item(int16_t inventory_index, int16_t amount)
{

}
void ZoneClientInterface::move_item_from_inventory_to_cart(int16_t inventory_index, int amount)
{

}
void ZoneClientInterface::move_item_from_inventory_to_storage(int16_t inventory_index, int amount)
{

}
void ZoneClientInterface::move_item_from_cart_to_inventory(int16_t inventory_index, int amount)
{

}
void ZoneClientInterface::move_item_from_cart_to_storage(int16_t inventory_index, int amount)
{

}
void ZoneClientInterface::move_item_from_storage_to_inventory(int16_t inventory_index, int amount)
{

}
void ZoneClientInterface::move_item_from_storage_to_cart(int16_t inventory_index, int amount)
{

}
void ZoneClientInterface::display_item_card_composition(int card_index, int equip_index)
{

}
void ZoneClientInterface::identify_item(int inventory_index)
{
	
}
void ZoneClientInterface::repair_item(int inventory_index, int item_id, int refine, int card1, int card2, int card3, int card4)
{

}
bool ZoneClientInterface::notify_pickup_item(item_entry_data id, int16_t amount, item_inventory_addition_notif_type result)
{
	ZC_ITEM_PICKUP_ACK_V7 pkt(get_session());
	pkt.deliver(id, amount, result);
	return true;
}
bool ZoneClientInterface::notify_normal_item_list(std::vector<std::shared_ptr<const item_entry_data>> const &items)
{
	ZC_INVENTORY_ITEMLIST_NORMAL_V5 pkt(get_session());
	pkt.deliver(items);
	return true;
}
bool ZoneClientInterface::notify_equipment_item_list(std::vector<std::shared_ptr<const item_entry_data>> const &items)
{
	ZC_INVENTORY_ITEMLIST_EQUIP_V6 pkt(get_session());
	pkt.deliver(items);
	return true;
}
bool ZoneClientInterface::notify_throw_item(int16_t inventory_index, int16_t amount)
{
	ZC_ITEM_THROW_ACK pkt(get_session());
	pkt.deliver(inventory_index, amount);
	return true;
}
bool ZoneClientInterface::notify_inventory_move_failed(int16_t inventory_index, bool silent)
{
	ZC_INVENTORY_MOVE_FAILED pkt(get_session());
	pkt.deliver(inventory_index, silent);
	return true;
}
bool ZoneClientInterface::notify_delete_item(int16_t inventory_index, int16_t amount, item_deletion_reason_type reason)
{
	ZC_DELETE_ITEM_FROM_BODY pkt(get_session());
	pkt.deliver(inventory_index, amount, reason);
	return true;
}
bool ZoneClientInterface::notify_bind_on_equip(int16_t inventory_index)
{
	ZC_NOTIFY_BIND_ON_EQUIP pkt(get_session());
	pkt.deliver(inventory_index);
	return true;
}
bool ZoneClientInterface::notify_use_item(std::shared_ptr<item_entry_data> inv_item, bool success)
{
	ZC_USE_ITEM_ACK pkt(get_session());
	pkt.deliver(inv_item->inventory_index, inv_item->amount, success);
	return true;
}
bool ZoneClientInterface::notify_equip_item(std::shared_ptr<const item_entry_data> item, item_equip_result_type result)
{
	ZC_ACK_WEAR_EQUIP_V5 pkt(get_session());
	pkt.deliver(item->inventory_index, item->current_equip_location_mask, item->sprite_id, result);
	return true;
}
bool ZoneClientInterface::notify_unequip_item(std::shared_ptr<const item_entry_data> item, item_unequip_result_type result)
{
	ZC_ACK_TAKEOFF_EQUIP_V5 pkt(get_session());
	pkt.deliver(item->inventory_index, item->current_equip_location_mask, result);
	return true;
}
bool ZoneClientInterface::notify_equip_arrow(std::shared_ptr<const item_entry_data> item)
{
	ZC_EQUIP_ARROW pkt(get_session());
	pkt.deliver(item->inventory_index);
	return true;
}
bool ZoneClientInterface::notify_action_failure(int16_t message_type)
{
	ZC_ACTION_FAILURE pkt(get_session());
	pkt.deliver(message_type);
	return true;
}
bool ZoneClientInterface::notify_item_composition(int16_t item_inventory_index, int16_t card_inventory_index, zc_ack_item_composition_result_type result)
{
	ZC_ACK_ITEMCOMPOSITION pkt(get_session());
	pkt.deliver(item_inventory_index, card_inventory_index, result);
	return true;
}
bool ZoneClientInterface::notify_identified_item(int16_t inventory_index, zc_ack_item_identify_result_type result)
{
	ZC_ACK_ITEMIDENTIFY pkt(get_session());
	pkt.deliver(inventory_index, result);
	return true;
}
bool ZoneClientInterface::notify_item_refining(zc_ack_itemrefining_result_type result, int16_t inventory_index, int16_t refine_lv)
{
	ZC_ACK_ITEMREFINING pkt(get_session());
	pkt.deliver(result, inventory_index, refine_lv);
	return true;
}
bool ZoneClientInterface::notify_item_repair(int inventory_index, zc_ack_itemrepair_result_type result)
{
	ZC_ACK_ITEMREPAIR pkt(get_session());
	pkt.deliver(inventory_index, result);
	return true;
}
bool ZoneClientInterface::notify_item_merge(int inventory_index, int amount, zc_ack_merge_item_reason_type reason)
{
	ZC_ACK_MERGE_ITEM pkt(get_session());
	pkt.deliver(inventory_index, amount, reason);
	return true;
}
void ZoneClientInterface::upgrade_skill_level(int16_t skill_id)
{
	ZC_SKILLINFO_UPDATE pkt(get_session());
	
	std::shared_ptr<skill_learnt_info> ls = get_session()->player()->get_learnt_skill(skill_id);

	if (ls == nullptr) {
		skill_learnt_info s;
		s.skill_id = skill_id;
		s.level = 1;
		s.learn_type = SKILL_LEARN_PERMANENT;
		ls = std::make_shared<skill_learnt_info>(s);
		get_session()->player()->add_learnt_skill(ls);
	} else {
		ls->level += 1;
	}

	std::shared_ptr<const skill_config_data> skd = SkillDB->get_skill_by_id(ls->skill_id);

	bool upgradeable = false;

	job_class_type job_id = (job_class_type) get_session()->player()->job_id();
	std::shared_ptr<const skill_tree_config> stc = SkillDB->get_skill_tree_skill_id_by_job_id(job_id, ls->skill_id);

	if (stc == nullptr) {
		HLog(error) << "Skill ID " << ls->skill_id << " not found for Job ID " << (int32_t) job_id << ".";
		return;
	}

	upgradeable = (ls->level < stc->max_level) ? true : false;

	get_session()->player()->status()->skill_point()->sub_base(1);

	pkt.deliver(skill_id, ls->level, skd->sp_cost[ls->level], skd->use_range[ls->level], upgradeable);
}
bool ZoneClientInterface::notify_learnt_skill_list()
{
	const std::map<uint16_t, std::shared_ptr<skill_learnt_info>> &learnt_skills = get_session()->player()->get_learnt_skills();
	std::vector<zc_skill_info_data> sidv;

	for (auto sp : learnt_skills) {
		const std::shared_ptr<skill_learnt_info> ls = sp.second;

		if (ls == nullptr) {
			HLog(error) << "Learnt skill was a nullptr.";
			continue;
		}

		zc_skill_info_data si;

		std::shared_ptr<const skill_config_data> skd = SkillDB->get_skill_by_id(ls->skill_id);
		
		if (skd == nullptr) {
			HLog(error) << "Tried to send data for unknown skill with ID " << ls->skill_id;
			continue;
		}

		si.skill_id = ls->skill_id;
		si.skill_type = skd->primary_type;
		si.level = ls->level;

		si.sp_cost = ls->level ? skd->sp_cost[ls->level] : 0;
		si.range = ls->level ? skd->use_range[ls->level] : 0;
		
#if (CLIENT_VERSION == 'R' && PACKET_VERSION >= 20190807) || \
	(CLIENT_VERSION == 'Z' && PACKET_VERSION >= 20190918)
		si.level2 = ls->level;
#else
		strncpy(si.name, skd->name.c_str(), MAX_SKILL_NAME_LENGTH);
#endif

		if (ls->learn_type == SKILL_LEARN_PERMANENT) {
			job_class_type job_id = (job_class_type) get_session()->player()->job_id();
			std::shared_ptr<const skill_tree_config> stc = SkillDB->get_skill_tree_skill_id_by_job_id(job_id, ls->skill_id);

			if (stc == nullptr) {
				HLog(error) << "Skill ID " << ls->skill_id << " not found for Job ID " << (int32_t) job_id << ".";
				continue;
			}

			si.upgradeable = (ls->level < stc->max_level) ? 1 : 0;
		}

		sidv.push_back(si);
	}

	ZC_SKILLINFO_LIST pkt(get_session());
	pkt.deliver(sidv);

	return true;
}
void ZoneClientInterface::use_skill_on_target(int16_t skill_lv, int16_t skill_id, int target_guid)
{
}
void ZoneClientInterface::use_ground_skill(int16_t skill_lv, int16_t skill_id, int16_t pos_x, int16_t pos_y)
{
}
void ZoneClientInterface::use_ground_skill(int16_t skill_lv, int16_t skill_id, int16_t pos_x, int16_t pos_y, std::string contents)
{
}
void ZoneClientInterface::action_request(int32_t target_guid, player_action_type action)
{
	bool continuous = false;
	switch(action)
	{
		case PLAYER_ACT_SIT:
		case PLAYER_ACT_STAND:
		{
			std::shared_ptr<const skill_config_data> sk = SkillDB->get_skill_by_name("NV_BASIC");
			get_session()->player()->perform_skill(sk->skill_id, 3);
			break;
		}
		case PLAYER_ACT_ATTACK_REPEAT:
		{
			continuous = true;
		}
		case PLAYER_ACT_ATTACK:
		{
			std::shared_ptr<Entity> target = get_session()->player()->get_nearby_entity(target_guid);
			get_session()->player()->attack(target, continuous);
			break;
		}
		default:
			break;
	};
	
}
bool ZoneClientInterface::notify_action(player_action_type action)
{
	ZC_NOTIFY_ACT na(get_session());
	na.deliver((int8_t) action);
	return true;
}
bool ZoneClientInterface::notify_status_change(int16_t si_type, int32_t guid, int8_t state, int32_t time_remaining, int32_t val1, int32_t val2, int32_t val3)
{
	ZC_MSG_STATE_CHANGE2 sc(get_session());

	if (time_remaining < 0)
		time_remaining = 9999;

	sc.deliver(si_type, guid, state, time_remaining, val1, val2, val3);
	return true;
}
bool ZoneClientInterface::notify_status_change_end(int16_t status_index, int32_t guid, int8_t state)
{
	ZC_MSG_STATE_CHANGE sce(get_session());
	sce.deliver(status_index, guid, state);
	return true;
}
bool ZoneClientInterface::notify_skill_fail(int16_t skill_id, int32_t message_type, int32_t item_id, skill_use_fail_cause_type cause)
{
	ZC_ACK_TOUSESKILL pkt(get_session());
	pkt.deliver(skill_id, message_type, item_id, cause);
	return true;
}
bool ZoneClientInterface::notify_damage(int guid, int target_guid, int start_time, int delay_skill, int delay_damage, int damage, bool is_sp_damaged, int number_of_hits, int8_t action_type, int left_damage)
{
	ZC_NOTIFY_ACT3 pkt(get_session());

	pkt._guid = guid;
	pkt._target_guid = target_guid; 
	pkt._start_time = start_time;
	pkt._delay_skill = delay_skill;
	pkt._delay_damage = delay_damage;
	pkt._damage = damage;
	pkt._is_sp_damaged = is_sp_damaged;
	pkt._number_of_hits = number_of_hits;
	pkt._action_type = action_type;
	pkt._left_damage = left_damage; 

	get_session()->player()->notify_in_area(pkt.serialize(), GRID_NOTIFY_AREA);

	return true;
}
void ZoneClientInterface::open_vend_shop(int account_id)
{

}
void ZoneClientInterface::close_vending()
{
	
}
void ZoneClientInterface::open_buying_store(int account_id)
{

}
void ZoneClientInterface::close_buying_store()
{
	
}
void ZoneClientInterface::start_vending(std::string shop_name, std::vector<cz_req_openstore_itemlist> items)
{

}
void ZoneClientInterface::start_buying_store(std::string store_name, int zeny_limit, std::vector<cz_req_open_buying_store_itemlist> items)
{

}
void ZoneClientInterface::sell_to_buying_store(int account_id, int store_id, std::vector<cz_req_trade_buying_store_itemlist> items)
{

}
bool ZoneClientInterface::notify_buying_store_itemlist(int account_id, int store_id, int zeny_limit, std::vector<zc_ack_itemlist_buying_store> items)
{
	ZC_ACK_ITEMLIST_BUYING_STORE pkt(get_session());
	pkt.deliver(account_id, store_id, zeny_limit, items);
	return true;
}
bool ZoneClientInterface::notify_open_vending(zc_ack_openstore2_result_type result)
{
	ZC_ACK_OPENSTORE2 pkt(get_session());
	pkt.deliver(result);
	return true;
}
/**
 * Misc
 */
void ZoneClientInterface::emotion(int type)
{

}
void ZoneClientInterface::user_count()
{

}
void ZoneClientInterface::ignore_list()
{
	
}

/**
 * Class Specific 
 */

void ZoneClientInterface::star_gladiator_feel_save(cz_agree_starplace_type type)
{

}
void ZoneClientInterface::novice_explosion_spirits()
{

}
void ZoneClientInterface::novice_doridori()
{

}
void ZoneClientInterface::remember_warppoint()
{

}
void ZoneClientInterface::produce_item(int item_id, std::vector<int16_t> material_ids)
{

}
void ZoneClientInterface::remove_cart()
{

}
void ZoneClientInterface::change_cart(int16_t num)
{

}
void ZoneClientInterface::make_arrow(int16_t item_id)
{

}
void ZoneClientInterface::make_item(cz_req_makingitem_type type, int16_t item_id)
{

}

/**
 * Ranking 
 */
void ZoneClientInterface::ranking(cz_req_ranking_type type)
{

}
void ZoneClientInterface::ranking_alchemist()
{

}
void ZoneClientInterface::ranking_blacksmith()
{

}
void ZoneClientInterface::ranking_pk()
{

}
bool ZoneClientInterface::notify_ranking(cz_req_ranking_type type, std::vector<zc_ack_ranking_info> info, int ranking_points)
{
	ZC_ACK_RANKING pkt(get_session());
	pkt.deliver(type, info, ranking_points);
	return true;
}
bool ZoneClientInterface::notify_pvp_points()
{
	//int char_id; int account_id; int win_point; int lose_point; int point;
	//ZC_ACK_PVPPOINT pkt(get_session());
	//pkt.deliver(char_id, account_id, win_point, lose_point, point);
	return true;
}
void ZoneClientInterface::setting_effects(int setting)
{

}
/**
 * Battleground 
 */
void ZoneClientInterface::message(std::string message)
{

}
void ZoneClientInterface::blocking_play_cancel()
{
	ZC_ALT_PING pkt(get_session());
	pkt.deliver();
	// TODO: should also send ZC_SPECIALPOPUP for clients >= 20221005 based on mapflag.
}
void ZoneClientInterface::client_version(int version)
{

}
void ZoneClientInterface::change_direction(int head_direction, int body_direction)
{

}
void ZoneClientInterface::change_effect_state(int effect_state)
{

}
void ZoneClientInterface::change_map_type(int x, int y, bool walkable)
{

}
void ZoneClientInterface::bargain_sale_tool_close()
{

}
void ZoneClientInterface::searchstore_close()
{

}
void ZoneClientInterface::command_mercenary(int type, cz_command_mercenary_command_type command)
{

}
void ZoneClientInterface::auto_revive()
{
	
}
void ZoneClientInterface::command_pet(cz_command_pet_type command)
{

}
void ZoneClientInterface::pet_act_emotion(cz_pet_act_emotion_type emotion)
{

}
void ZoneClientInterface::pet_evolve(int evolved_pet_egg_id, std::vector<cz_pet_evolution_itemlist> items)
{
	
}
void ZoneClientInterface::view_equipment(int account_id)
{
	
}
void ZoneClientInterface::set_config(cz_config_type config, bool setting)
{
	switch(config)
	{
	case CZ_CONFIG_OPEN_EQUIPMENT_WINDOW:
		break;
	case CZ_CONFIG_CALL:
		break;
	case CZ_CONFIG_PET_AUTOFEED:
		break;
	case CZ_CONFIG_HOMUNCULUS_AUTOFEED:
		break;
	default:
		break;
	}
}
void ZoneClientInterface::rename_homunculus(std::string name)
{

}
void ZoneClientInterface::rename_pet(std::string name)
{

}
void ZoneClientInterface::request_action(int guid, int target_id, int action)
{

}
void ZoneClientInterface::move_homunculus_to_coordinates(int guid, int x, int y, int dir)
{

}
void ZoneClientInterface::move_homunculus_to_master(int guid)
{
	
}


/**
 * Administration
 */
void ZoneClientInterface::disconnect_all_players()
{

}
void ZoneClientInterface::disconnect_account(int account_id)
{

}
void ZoneClientInterface::create(std::string create)
{

}
void ZoneClientInterface::move_to_map(std::string map_name, int16_t x, int16_t y)
{

}
void ZoneClientInterface::recall(std::string username)
{

}
void ZoneClientInterface::recall(int account_id)
{

}


void ZoneClientInterface::summon(std::string char_name)
{

}
void ZoneClientInterface::warp_to(int account_id)
{

}
void ZoneClientInterface::request_username(int account_id)
{

}
void ZoneClientInterface::adjust_manner_by_name(std::string name)
{

}
void ZoneClientInterface::give_manner_point(int account_id, cz_req_give_manner_point_type type, int value)
{

}
void ZoneClientInterface::check_status(std::string name)
{

}
bool ZoneClientInterface::notify_give_manner_point(zc_ack_give_manner_point_result_type result)
{
	ZC_ACK_GIVE_MANNER_POINT pkt(get_session());
	pkt.deliver(result);
	return true;
}

/**
 * Instancing 
 */
void ZoneClientInterface::memorial_dungeon_command(cz_memorial_dungeon_command_type command)
{

}

/**
 * Cash Point Store 
 */
void ZoneClientInterface::cash_point_purchase(int kafra_points, std::vector<cz_pc_buy_cash_point_item> items)
{

}

/*
 * Private Airship
 */
void ZoneClientInterface::private_airship_request(std::string map_name, int item_id)
{

}