/* $Id$ */

/** @file network_server.cpp Server part of the network protocol. */

#ifdef ENABLE_NETWORK

#include "../stdafx.h"
#include "../openttd.h" // XXX StringID
#include "../debug.h"
#include "../strings_func.h"
#include "network_internal.h"
#include "core/tcp.h"
#include "../vehicle_base.h"
#include "../vehicle_func.h"
#include "../date_func.h"
#include "network_server.h"
#include "network_udp.h"
#include "../console_func.h"
#include "../command_func.h"
#include "../saveload.h"
#include "../station_base.h"
#include "../variables.h"
#include "../genworld.h"
#include "../core/alloc_func.hpp"
#include "../fileio_func.h"
#include "../string_func.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../company_gui.h"
#include "../settings_type.h"

#include "table/strings.h"

// This file handles all the server-commands

static void NetworkHandleCommandQueue(NetworkTCPSocketHandler* cs);

// **********
// Sending functions
//   DEF_SERVER_SEND_COMMAND has parameter: NetworkTCPSocketHandler *cs
// **********

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_CLIENT_INFO)(NetworkTCPSocketHandler *cs, NetworkClientInfo *ci)
{
	//
	// Packet: SERVER_CLIENT_INFO
	// Function: Sends info about a client
	// Data:
	//    uint16:  The index of the client (always unique on a server. 1 = server)
	//    uint8:  As which company the client is playing
	//    String: The name of the client
	//

	if (ci->client_index != NETWORK_EMPTY_INDEX) {
		Packet *p = NetworkSend_Init(PACKET_SERVER_CLIENT_INFO);
		p->Send_uint16(ci->client_index);
		p->Send_uint8 (ci->client_playas);
		p->Send_string(ci->client_name);

		cs->Send_Packet(p);
	}
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_COMPANY_INFO)
{
//
	// Packet: SERVER_COMPANY_INFO
	// Function: Sends info about the companies
	// Data:
	//

	int i;

	Company *company;
	Packet *p;

	byte active = ActiveCompanyCount();

	if (active == 0) {
		p = NetworkSend_Init(PACKET_SERVER_COMPANY_INFO);

		p->Send_uint8 (NETWORK_COMPANY_INFO_VERSION);
		p->Send_uint8 (active);

		cs->Send_Packet(p);
		return;
	}

	NetworkPopulateCompanyInfo();

	FOR_ALL_COMPANIES(company) {
		p = NetworkSend_Init(PACKET_SERVER_COMPANY_INFO);

		p->Send_uint8 (NETWORK_COMPANY_INFO_VERSION);
		p->Send_uint8 (active);
		p->Send_uint8 (company->index);

		p->Send_string(_network_company_info[company->index].company_name);
		p->Send_uint32(_network_company_info[company->index].inaugurated_year);
		p->Send_uint64(_network_company_info[company->index].company_value);
		p->Send_uint64(_network_company_info[company->index].money);
		p->Send_uint64(_network_company_info[company->index].income);
		p->Send_uint16(_network_company_info[company->index].performance);

		/* Send 1 if there is a passord for the company else send 0 */
		p->Send_bool(!StrEmpty(_network_company_info[company->index].password));

		for (i = 0; i < NETWORK_VEHICLE_TYPES; i++) {
			p->Send_uint16(_network_company_info[company->index].num_vehicle[i]);
		}

		for (i = 0; i < NETWORK_STATION_TYPES; i++) {
			p->Send_uint16(_network_company_info[company->index].num_station[i]);
		}

		if (StrEmpty(_network_company_info[company->index].clients)) {
			p->Send_string("<none>");
		} else {
			p->Send_string(_network_company_info[company->index].clients);
		}

		cs->Send_Packet(p);
	}

	p = NetworkSend_Init(PACKET_SERVER_COMPANY_INFO);

	p->Send_uint8 (NETWORK_COMPANY_INFO_VERSION);
	p->Send_uint8 (0);

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR)(NetworkTCPSocketHandler *cs, NetworkErrorCode error)
{
	//
	// Packet: SERVER_ERROR
	// Function: The client made an error
	// Data:
	//    uint8:  ErrorID (see network_data.h, NetworkErrorCode)
	//

	char str[100];
	Packet *p = NetworkSend_Init(PACKET_SERVER_ERROR);

	p->Send_uint8(error);
	cs->Send_Packet(p);

	GetNetworkErrorMsg(str, error, lastof(str));

	// Only send when the current client was in game
	if (cs->status > STATUS_AUTH) {
		NetworkTCPSocketHandler *new_cs;
		char client_name[NETWORK_CLIENT_NAME_LENGTH];

		NetworkGetClientName(client_name, sizeof(client_name), cs);

		DEBUG(net, 1, "'%s' made an error and has been disconnected. Reason: '%s'", client_name, str);

		NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, "%s", str);

		FOR_ALL_CLIENTS(new_cs) {
			if (new_cs->status > STATUS_AUTH && new_cs != cs) {
				// Some errors we filter to a more general error. Clients don't have to know the real
				//  reason a joining failed.
				if (error == NETWORK_ERROR_NOT_AUTHORIZED || error == NETWORK_ERROR_NOT_EXPECTED || error == NETWORK_ERROR_WRONG_REVISION)
					error = NETWORK_ERROR_ILLEGAL_PACKET;

				SEND_COMMAND(PACKET_SERVER_ERROR_QUIT)(new_cs, cs->index, error);
			}
		}
	} else {
		DEBUG(net, 1, "Client %d made an error and has been disconnected. Reason: '%s'", cs->index, str);
	}

	cs->has_quit = true;

	// Make sure the data get's there before we close the connection
	cs->Send_Packets();

	// The client made a mistake, so drop his connection now!
	NetworkCloseClient(cs);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_CHECK_NEWGRFS)(NetworkTCPSocketHandler *cs)
{
	//
	// Packet: PACKET_SERVER_CHECK_NEWGRFS
	// Function: Sends info about the used GRFs to the client
	// Data:
	//      uint8:  Amount of GRFs
	//    And then for each GRF:
	//      uint32: GRF ID
	// 16 * uint8:  MD5 checksum of the GRF
	//

	Packet *p = NetworkSend_Init(PACKET_SERVER_CHECK_NEWGRFS);
	const GRFConfig *c;
	uint grf_count = 0;

	for (c = _grfconfig; c != NULL; c = c->next) {
		if (!HasBit(c->flags, GCF_STATIC)) grf_count++;
	}

	p->Send_uint8 (grf_count);
	for (c = _grfconfig; c != NULL; c = c->next) {
		if (!HasBit(c->flags, GCF_STATIC)) cs->Send_GRFIdentifier(p, c);
	}

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_NEED_PASSWORD)(NetworkTCPSocketHandler *cs, NetworkPasswordType type)
{
	//
	// Packet: SERVER_NEED_PASSWORD
	// Function: Indication to the client that the server needs a password
	// Data:
	//    uint8:  Type of password
	//

	/* Invalid packet when status is AUTH or higher */
	if (cs->status >= STATUS_AUTH) return;

	cs->status = STATUS_AUTHORIZING;

	Packet *p = NetworkSend_Init(PACKET_SERVER_NEED_PASSWORD);
	p->Send_uint8(type);
	p->Send_uint32(_settings_game.game_creation.generation_seed);
	p->Send_string(_settings_client.network.network_id);
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_WELCOME)
{
	//
	// Packet: SERVER_WELCOME
	// Function: The client is joined and ready to receive his map
	// Data:
	//    uint16:  Own ClientID
	//

	Packet *p;
	NetworkTCPSocketHandler *new_cs;

	// Invalid packet when status is AUTH or higher
	if (cs->status >= STATUS_AUTH) return;

	cs->status = STATUS_AUTH;
	_network_game_info.clients_on++;

	p = NetworkSend_Init(PACKET_SERVER_WELCOME);
	p->Send_uint16(cs->index);
	p->Send_uint32(_settings_game.game_creation.generation_seed);
	p->Send_string(_settings_client.network.network_id);
	cs->Send_Packet(p);

		// Transmit info about all the active clients
	FOR_ALL_CLIENTS(new_cs) {
		if (new_cs != cs && new_cs->status > STATUS_AUTH)
			SEND_COMMAND(PACKET_SERVER_CLIENT_INFO)(cs, DEREF_CLIENT_INFO(new_cs));
	}
	// Also send the info of the server
	SEND_COMMAND(PACKET_SERVER_CLIENT_INFO)(cs, NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX));
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_WAIT)
{
	//
	// Packet: PACKET_SERVER_WAIT
	// Function: The client can not receive the map at the moment because
	//             someone else is already receiving the map
	// Data:
	//    uint8:  Clients awaiting map
	//
	int waiting = 0;
	NetworkTCPSocketHandler *new_cs;
	Packet *p;

	// Count how many clients are waiting in the queue
	FOR_ALL_CLIENTS(new_cs) {
		if (new_cs->status == STATUS_MAP_WAIT) waiting++;
	}

	p = NetworkSend_Init(PACKET_SERVER_WAIT);
	p->Send_uint8(waiting);
	cs->Send_Packet(p);
}

// This sends the map to the client
DEF_SERVER_SEND_COMMAND(PACKET_SERVER_MAP)
{
	//
	// Packet: SERVER_MAP
	// Function: Sends the map to the client, or a part of it (it is splitted in
	//   a lot of multiple packets)
	// Data:
	//    uint8:  packet-type (MAP_PACKET_START, MAP_PACKET_NORMAL and MAP_PACKET_END)
	//  if MAP_PACKET_START:
	//    uint32: The current FrameCounter
	//  if MAP_PACKET_NORMAL:
	//    piece of the map (till max-size of packet)
	//  if MAP_PACKET_END:
	//    nothing
	//

	static FILE *file_pointer;
	static uint sent_packets; // How many packets we did send succecfully last time

	if (cs->status < STATUS_AUTH) {
		// Illegal call, return error and ignore the packet
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_AUTHORIZED);
		return;
	}

	if (cs->status == STATUS_AUTH) {
		const char *filename = "network_server.tmp";
		Packet *p;

		// Make a dump of the current game
		if (SaveOrLoad(filename, SL_SAVE, AUTOSAVE_DIR) != SL_OK) usererror("network savedump failed");

		file_pointer = FioFOpenFile(filename, "rb", AUTOSAVE_DIR);
		fseek(file_pointer, 0, SEEK_END);

		if (ftell(file_pointer) == 0) usererror("network savedump failed - zero sized savegame?");

		// Now send the _frame_counter and how many packets are coming
		p = NetworkSend_Init(PACKET_SERVER_MAP);
		p->Send_uint8 (MAP_PACKET_START);
		p->Send_uint32(_frame_counter);
		p->Send_uint32(ftell(file_pointer));
		cs->Send_Packet(p);

		fseek(file_pointer, 0, SEEK_SET);

		sent_packets = 4; // We start with trying 4 packets

		cs->status = STATUS_MAP;
		/* Mark the start of download */
		cs->last_frame = _frame_counter;
		cs->last_frame_server = _frame_counter;
	}

	if (cs->status == STATUS_MAP) {
		uint i;
		int res;
		for (i = 0; i < sent_packets; i++) {
			Packet *p = NetworkSend_Init(PACKET_SERVER_MAP);
			p->Send_uint8(MAP_PACKET_NORMAL);
			res = (int)fread(p->buffer + p->size, 1, SEND_MTU - p->size, file_pointer);

			if (ferror(file_pointer)) usererror("Error reading temporary network savegame!");

			p->size += res;
			cs->Send_Packet(p);
			if (feof(file_pointer)) {
				// Done reading!
				Packet *p = NetworkSend_Init(PACKET_SERVER_MAP);
				p->Send_uint8(MAP_PACKET_END);
				cs->Send_Packet(p);

				// Set the status to DONE_MAP, no we will wait for the client
				//  to send it is ready (maybe that happens like never ;))
				cs->status = STATUS_DONE_MAP;
				fclose(file_pointer);

				{
					NetworkTCPSocketHandler *new_cs;
					bool new_map_client = false;
					// Check if there is a client waiting for receiving the map
					//  and start sending him the map
					FOR_ALL_CLIENTS(new_cs) {
						if (new_cs->status == STATUS_MAP_WAIT) {
							// Check if we already have a new client to send the map to
							if (!new_map_client) {
								// If not, this client will get the map
								new_cs->status = STATUS_AUTH;
								new_map_client = true;
								SEND_COMMAND(PACKET_SERVER_MAP)(new_cs);
							} else {
								// Else, send the other clients how many clients are in front of them
								SEND_COMMAND(PACKET_SERVER_WAIT)(new_cs);
							}
						}
					}
				}

				// There is no more data, so break the for
				break;
			}
		}

		// Send all packets (forced) and check if we have send it all
		cs->Send_Packets();
		if (cs->IsPacketQueueEmpty()) {
			// All are sent, increase the sent_packets
			sent_packets *= 2;
		} else {
			// Not everything is sent, decrease the sent_packets
			if (sent_packets > 1) sent_packets /= 2;
		}
	}
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_JOIN)(NetworkTCPSocketHandler *cs, uint16 client_index)
{
	//
	// Packet: SERVER_JOIN
	// Function: A client is joined (all active clients receive this after a
	//     PACKET_CLIENT_MAP_OK) Mostly what directly follows is a
	//     PACKET_SERVER_CLIENT_INFO
	// Data:
	//    uint16:  Client-Index
	//

	Packet *p = NetworkSend_Init(PACKET_SERVER_JOIN);

	p->Send_uint16(client_index);

	cs->Send_Packet(p);
}


DEF_SERVER_SEND_COMMAND(PACKET_SERVER_FRAME)
{
	//
	// Packet: SERVER_FRAME
	// Function: Sends the current frame-counter to the client
	// Data:
	//    uint32: Frame Counter
	//    uint32: Frame Counter Max (how far may the client walk before the server?)
	//    [uint32: general-seed-1]
	//    [uint32: general-seed-2]
	//      (last two depends on compile-settings, and are not default settings)
	//

	Packet *p = NetworkSend_Init(PACKET_SERVER_FRAME);
	p->Send_uint32(_frame_counter);
	p->Send_uint32(_frame_counter_max);
#ifdef ENABLE_NETWORK_SYNC_EVERY_FRAME
	p->Send_uint32(_sync_seed_1);
#ifdef NETWORK_SEND_DOUBLE_SEED
	p->Send_uint32(_sync_seed_2);
#endif
#endif
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_SYNC)
{
	//
	// Packet: SERVER_SYNC
	// Function: Sends a sync-check to the client
	// Data:
	//    uint32: Frame Counter
	//    uint32: General-seed-1
	//    [uint32: general-seed-2]
	//      (last one depends on compile-settings, and are not default settings)
	//

	Packet *p = NetworkSend_Init(PACKET_SERVER_SYNC);
	p->Send_uint32(_frame_counter);
	p->Send_uint32(_sync_seed_1);

#ifdef NETWORK_SEND_DOUBLE_SEED
	p->Send_uint32(_sync_seed_2);
#endif
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_COMMAND)(NetworkTCPSocketHandler *cs, CommandPacket *cp)
{
	//
	// Packet: SERVER_COMMAND
	// Function: Sends a DoCommand to the client
	// Data:
	//    uint8:  CompanyID (0..MAX_COMPANIES-1)
	//    uint32: CommandID (see command.h)
	//    uint32: P1 (free variables used in DoCommand)
	//    uint32: P2
	//    uint32: Tile
	//    string: text
	//    uint8:  CallBackID (see callback_table.c)
	//    uint32: Frame of execution
	//

	Packet *p = NetworkSend_Init(PACKET_SERVER_COMMAND);

	p->Send_uint8 (cp->company);
	p->Send_uint32(cp->cmd);
	p->Send_uint32(cp->p1);
	p->Send_uint32(cp->p2);
	p->Send_uint32(cp->tile);
	p->Send_string(cp->text);
	p->Send_uint8 (cp->callback);
	p->Send_uint32(cp->frame);
	p->Send_bool  (cp->my_cmd);

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_CHAT)(NetworkTCPSocketHandler *cs, NetworkAction action, uint16 client_index, bool self_send, const char *msg)
{
	//
	// Packet: SERVER_CHAT
	// Function: Sends a chat-packet to the client
	// Data:
	//    uint8:  ActionID (see network_data.h, NetworkAction)
	//    uint16:  Client-index
	//    String: Message (max NETWORK_CHAT_LENGTH)
	//

	Packet *p = NetworkSend_Init(PACKET_SERVER_CHAT);

	p->Send_uint8 (action);
	p->Send_uint16(client_index);
	p->Send_bool  (self_send);
	p->Send_string(msg);

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_ERROR_QUIT)(NetworkTCPSocketHandler *cs, uint16 client_index, NetworkErrorCode errorno)
{
	//
	// Packet: SERVER_ERROR_QUIT
	// Function: One of the clients made an error and is quiting the game
	//      This packet informs the other clients of that.
	// Data:
	//    uint16:  Client-index
	//    uint8:  ErrorID (see network_data.h, NetworkErrorCode)
	//

	Packet *p = NetworkSend_Init(PACKET_SERVER_ERROR_QUIT);

	p->Send_uint16(client_index);
	p->Send_uint8 (errorno);

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_QUIT)(NetworkTCPSocketHandler *cs, uint16 client_index, const char *leavemsg)
{
	//
	// Packet: SERVER_ERROR_QUIT
	// Function: A client left the game, and this packets informs the other clients
	//      of that.
	// Data:
	//    uint16:  Client-index
	//    String: leave-message
	//

	Packet *p = NetworkSend_Init(PACKET_SERVER_QUIT);

	p->Send_uint16(client_index);
	p->Send_string(leavemsg);

	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_SHUTDOWN)
{
	//
	// Packet: SERVER_SHUTDOWN
	// Function: Let the clients know that the server is closing
	// Data:
	//     <none>
	//

	Packet *p = NetworkSend_Init(PACKET_SERVER_SHUTDOWN);
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND(PACKET_SERVER_NEWGAME)
{
	//
	// Packet: PACKET_SERVER_NEWGAME
	// Function: Let the clients know that the server is loading a new map
	// Data:
	//     <none>
	//

	Packet *p = NetworkSend_Init(PACKET_SERVER_NEWGAME);
	cs->Send_Packet(p);
}

DEF_SERVER_SEND_COMMAND_PARAM(PACKET_SERVER_RCON)(NetworkTCPSocketHandler *cs, uint16 color, const char *command)
{
	Packet *p = NetworkSend_Init(PACKET_SERVER_RCON);

	p->Send_uint16(color);
	p->Send_string(command);
	cs->Send_Packet(p);
}

// **********
// Receiving functions
//   DEF_SERVER_RECEIVE_COMMAND has parameter: NetworkTCPSocketHandler *cs, Packet *p
// **********

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_INFO)
{
	SEND_COMMAND(PACKET_SERVER_COMPANY_INFO)(cs);
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED)
{
	if (cs->status != STATUS_INACTIVE) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		return;
	}

	NetworkClientInfo *ci = DEREF_CLIENT_INFO(cs);

	/* We now want a password from the client else we do not allow him in! */
	if (!StrEmpty(_settings_client.network.server_password)) {
		SEND_COMMAND(PACKET_SERVER_NEED_PASSWORD)(cs, NETWORK_GAME_PASSWORD);
	} else {
		if (IsValidCompanyID(ci->client_playas) && _network_company_info[ci->client_playas].password[0] != '\0') {
			SEND_COMMAND(PACKET_SERVER_NEED_PASSWORD)(cs, NETWORK_COMPANY_PASSWORD);
		} else {
			SEND_COMMAND(PACKET_SERVER_WELCOME)(cs);
		}
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_JOIN)
{
	if (cs->status != STATUS_INACTIVE) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		return;
	}

	char name[NETWORK_CLIENT_NAME_LENGTH];
	char unique_id[NETWORK_UNIQUE_ID_LENGTH];
	NetworkClientInfo *ci;
	CompanyID playas;
	NetworkLanguage client_lang;
	char client_revision[NETWORK_REVISION_LENGTH];

	p->Recv_string(client_revision, sizeof(client_revision));

	// Check if the client has revision control enabled
	if (!IsNetworkCompatibleVersion(client_revision)) {
		// Different revisions!!
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_WRONG_REVISION);
		return;
	}

	p->Recv_string(name, sizeof(name));
	playas = (Owner)p->Recv_uint8();
	client_lang = (NetworkLanguage)p->Recv_uint8();
	p->Recv_string(unique_id, sizeof(unique_id));

	if (cs->has_quit) return;

	// join another company does not affect these values
	switch (playas) {
		case COMPANY_NEW_COMPANY: /* New company */
			if (ActiveCompanyCount() >= _settings_client.network.max_companies) {
				SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_FULL);
				return;
			}
			break;
		case COMPANY_SPECTATOR: /* Spectator */
			if (NetworkSpectatorCount() >= _settings_client.network.max_spectators) {
				SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_FULL);
				return;
			}
			break;
		default: /* Join another company (companies 1-8 (index 0-7)) */
			if (!IsValidCompanyID(playas)) {
				SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_COMPANY_MISMATCH);
				return;
			}
			break;
	}

	// We need a valid name.. make it Player
	if (StrEmpty(name)) strecpy(name, "Player", lastof(name));

	if (!NetworkFindName(name)) { // Change name if duplicate
		// We could not create a name for this client
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NAME_IN_USE);
		return;
	}

	ci = DEREF_CLIENT_INFO(cs);

	strecpy(ci->client_name, name, lastof(ci->client_name));
	strecpy(ci->unique_id, unique_id, lastof(ci->unique_id));
	ci->client_playas = playas;
	ci->client_lang = client_lang;

	/* Make sure companies to which people try to join are not autocleaned */
	if (IsValidCompanyID(playas)) _network_company_info[playas].months_empty = 0;

	if (_grfconfig == NULL) {
		RECEIVE_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED)(cs, NULL);
	} else {
		SEND_COMMAND(PACKET_SERVER_CHECK_NEWGRFS)(cs);
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_PASSWORD)
{
	NetworkPasswordType type;
	char password[NETWORK_PASSWORD_LENGTH];
	const NetworkClientInfo *ci;

	type = (NetworkPasswordType)p->Recv_uint8();
	p->Recv_string(password, sizeof(password));

	if (cs->status == STATUS_AUTHORIZING && type == NETWORK_GAME_PASSWORD) {
		// Check game-password
		if (strcmp(password, _settings_client.network.server_password) != 0) {
			// Password is invalid
			SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_WRONG_PASSWORD);
			return;
		}

		ci = DEREF_CLIENT_INFO(cs);

		if (IsValidCompanyID(ci->client_playas) && _network_company_info[ci->client_playas].password[0] != '\0') {
			SEND_COMMAND(PACKET_SERVER_NEED_PASSWORD)(cs, NETWORK_COMPANY_PASSWORD);
			return;
		}

		// Valid password, allow user
		SEND_COMMAND(PACKET_SERVER_WELCOME)(cs);
		return;
	} else if (cs->status == STATUS_AUTHORIZING && type == NETWORK_COMPANY_PASSWORD) {
		ci = DEREF_CLIENT_INFO(cs);

		if (strcmp(password, _network_company_info[ci->client_playas].password) != 0) {
			// Password is invalid
			SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_WRONG_PASSWORD);
			return;
		}

		SEND_COMMAND(PACKET_SERVER_WELCOME)(cs);
		return;
	}


	SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
	return;
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_GETMAP)
{
	NetworkTCPSocketHandler *new_cs;

	// The client was never joined.. so this is impossible, right?
	//  Ignore the packet, give the client a warning, and close his connection
	if (cs->status < STATUS_AUTH || cs->has_quit) {
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_AUTHORIZED);
		return;
	}

	// Check if someone else is receiving the map
	FOR_ALL_CLIENTS(new_cs) {
		if (new_cs->status == STATUS_MAP) {
			// Tell the new client to wait
			cs->status = STATUS_MAP_WAIT;
			SEND_COMMAND(PACKET_SERVER_WAIT)(cs);
			return;
		}
	}

	// We receive a request to upload the map.. give it to the client!
	SEND_COMMAND(PACKET_SERVER_MAP)(cs);
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_MAP_OK)
{
	// Client has the map, now start syncing
	if (cs->status == STATUS_DONE_MAP && !cs->has_quit) {
		char client_name[NETWORK_CLIENT_NAME_LENGTH];
		NetworkTCPSocketHandler *new_cs;

		NetworkGetClientName(client_name, sizeof(client_name), cs);

		NetworkTextMessage(NETWORK_ACTION_JOIN, CC_DEFAULT, false, client_name, "");

		// Mark the client as pre-active, and wait for an ACK
		//  so we know he is done loading and in sync with us
		cs->status = STATUS_PRE_ACTIVE;
		NetworkHandleCommandQueue(cs);
		SEND_COMMAND(PACKET_SERVER_FRAME)(cs);
		SEND_COMMAND(PACKET_SERVER_SYNC)(cs);

		// This is the frame the client receives
		//  we need it later on to make sure the client is not too slow
		cs->last_frame = _frame_counter;
		cs->last_frame_server = _frame_counter;

		FOR_ALL_CLIENTS(new_cs) {
			if (new_cs->status > STATUS_AUTH) {
				SEND_COMMAND(PACKET_SERVER_CLIENT_INFO)(new_cs, DEREF_CLIENT_INFO(cs));
				SEND_COMMAND(PACKET_SERVER_JOIN)(new_cs, cs->index);
			}
		}

		if (_settings_client.network.pause_on_join) {
			/* Now pause the game till the client is in sync */
			DoCommandP(0, 1, 0, NULL, CMD_PAUSE);

			NetworkServerSendChat(NETWORK_ACTION_SERVER_MESSAGE, DESTTYPE_BROADCAST, 0, "Game paused (incoming client)", NETWORK_SERVER_INDEX);
		}
	} else {
		// Wrong status for this packet, give a warning to client, and close connection
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
	}
}

/** Enforce the command flags.
 * Eg a server-only command can only be executed by a server, etc.
 * @param *cp the commandpacket that is going to be checked
 * @param *ci client information for debugging output to console
 */
static bool CheckCommandFlags(const CommandPacket *cp, const NetworkClientInfo *ci)
{
	byte flags = GetCommandFlags(cp->cmd);

	if (flags & CMD_SERVER && ci->client_index != NETWORK_SERVER_INDEX) {
		IConsolePrintF(CC_ERROR, "WARNING: server only command from client %d (IP: %s), kicking...", ci->client_index, GetClientIP(ci));
		return false;
	}

	if (flags & CMD_OFFLINE) {
		IConsolePrintF(CC_ERROR, "WARNING: offline only command from client %d (IP: %s), kicking...", ci->client_index, GetClientIP(ci));
		return false;
	}

	if (cp->cmd != CMD_COMPANY_CTRL && !IsValidCompanyID(cp->company) && ci->client_index != NETWORK_SERVER_INDEX) {
		IConsolePrintF(CC_ERROR, "WARNING: spectator issueing command from client %d (IP: %s), kicking...", ci->client_index, GetClientIP(ci));
		return false;
	}

	return true;
}

/** The client has done a command and wants us to handle it
 * @param *cs the connected client that has sent the command
 * @param *p the packet in which the command was sent
 */
DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_COMMAND)
{
	NetworkTCPSocketHandler *new_cs;
	const NetworkClientInfo *ci;
	byte callback;

	// The client was never joined.. so this is impossible, right?
	//  Ignore the packet, give the client a warning, and close his connection
	if (cs->status < STATUS_DONE_MAP || cs->has_quit) {
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		return;
	}

	CommandPacket *cp = MallocT<CommandPacket>(1);
	cp->company = (CompanyID)p->Recv_uint8();
	cp->cmd     = p->Recv_uint32();
	cp->p1      = p->Recv_uint32();
	cp->p2      = p->Recv_uint32();
	cp->tile    = p->Recv_uint32();
	p->Recv_string(cp->text, lengthof(cp->text));

	callback = p->Recv_uint8();

	if (cs->has_quit) {
		free(cp);
		return;
	}

	ci = DEREF_CLIENT_INFO(cs);

	/* Check if cp->cmd is valid */
	if (!IsValidCommand(cp->cmd)) {
		IConsolePrintF(CC_ERROR, "WARNING: invalid command from client %d (IP: %s).", ci->client_index, GetClientIP(ci));
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		free(cp);
		return;
	}

	if (!CheckCommandFlags(cp, ci)) {
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_KICKED);
		free(cp);
		return;
	}

	/** Only CMD_COMPANY_CTRL is always allowed, for the rest, playas needs
	 * to match the company in the packet. If it doesn't, the client has done
	 * something pretty naughty (or a bug), and will be kicked
	 */
	if (!(cp->cmd == CMD_COMPANY_CTRL && cp->p1 == 0 && ci->client_playas == COMPANY_NEW_COMPANY) && ci->client_playas != cp->company) {
		IConsolePrintF(CC_ERROR, "WARNING: client %d (IP: %s) tried to execute a command as company %d, kicking...",
		               ci->client_playas + 1, GetClientIP(ci), cp->company + 1);
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_COMPANY_MISMATCH);
		free(cp);
		return;
	}

	/** @todo CMD_COMPANY_CTRL with p1 = 0 announces a new company to the server. To give the
	 * company the correct ID, the server injects p2 and executes the command. Any other p1
	 * is prohibited. Pretty ugly and should be redone together with its function.
	 * @see CmdCompanyCtrl()
	 */
	if (cp->cmd == CMD_COMPANY_CTRL) {
		if (cp->p1 != 0) {
			SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_CHEATER);
			free(cp);
			return;
		}

		/* XXX - Execute the command as a valid company. Normally this would be done by a
		 * spectator, but that is not allowed any commands. So do an impersonation. The drawback
		 * of this is that the first company's last_built_tile is also updated... */
		cp->company = OWNER_BEGIN;
		cp->p2 = cs - _clients; // XXX - UGLY! p2 is mis-used to get the client-id in CmdCompanyCtrl
	}

	// The frame can be executed in the same frame as the next frame-packet
	//  That frame just before that frame is saved in _frame_counter_max
	cp->frame = _frame_counter_max + 1;
	cp->next  = NULL;

	// Queue the command for the clients (are send at the end of the frame
	//   if they can handle it ;))
	FOR_ALL_CLIENTS(new_cs) {
		if (new_cs->status >= STATUS_MAP) {
			// Callbacks are only send back to the client who sent them in the
			//  first place. This filters that out.
			cp->callback = (new_cs != cs) ? 0 : callback;
			cp->my_cmd = (new_cs == cs);
			NetworkAddCommandQueue(new_cs, cp);
		}
	}

	cp->callback = 0;
	cp->my_cmd = false;
	// Queue the command on the server
	if (_local_command_queue == NULL) {
		_local_command_queue = cp;
	} else {
		// Find last packet
		CommandPacket *c = _local_command_queue;
		while (c->next != NULL) c = c->next;
		c->next = cp;
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_ERROR)
{
	// This packets means a client noticed an error and is reporting this
	//  to us. Display the error and report it to the other clients
	NetworkTCPSocketHandler *new_cs;
	char str[100];
	char client_name[NETWORK_CLIENT_NAME_LENGTH];
	NetworkErrorCode errorno = (NetworkErrorCode)p->Recv_uint8();

	// The client was never joined.. thank the client for the packet, but ignore it
	if (cs->status < STATUS_DONE_MAP || cs->has_quit) {
		cs->has_quit = true;
		return;
	}

	NetworkGetClientName(client_name, sizeof(client_name), cs);

	GetNetworkErrorMsg(str, errorno, lastof(str));

	DEBUG(net, 2, "'%s' reported an error and is closing its connection (%s)", client_name, str);

	NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, "%s", str);

	FOR_ALL_CLIENTS(new_cs) {
		if (new_cs->status > STATUS_AUTH) {
			SEND_COMMAND(PACKET_SERVER_ERROR_QUIT)(new_cs, cs->index, errorno);
		}
	}

	cs->has_quit = true;
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_QUIT)
{
	// The client wants to leave. Display this and report it to the other
	//  clients.
	NetworkTCPSocketHandler *new_cs;
	char str[100];
	char client_name[NETWORK_CLIENT_NAME_LENGTH];

	// The client was never joined.. thank the client for the packet, but ignore it
	if (cs->status < STATUS_DONE_MAP || cs->has_quit) {
		cs->has_quit = true;
		return;
	}

	p->Recv_string(str, lengthof(str));

	NetworkGetClientName(client_name, sizeof(client_name), cs);

	NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, client_name, "%s", str);

	FOR_ALL_CLIENTS(new_cs) {
		if (new_cs->status > STATUS_AUTH) {
			SEND_COMMAND(PACKET_SERVER_QUIT)(new_cs, cs->index, str);
		}
	}

	cs->has_quit = true;
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_ACK)
{
	if (cs->status < STATUS_AUTH) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_AUTHORIZED);
		return;
	}

	uint32 frame = p->Recv_uint32();

	/* The client is trying to catch up with the server */
	if (cs->status == STATUS_PRE_ACTIVE) {
		/* The client is not yet catched up? */
		if (frame + DAY_TICKS < _frame_counter) return;

		/* Now he is! Unpause the game */
		cs->status = STATUS_ACTIVE;

		if (_settings_client.network.pause_on_join) {
			DoCommandP(0, 0, 0, NULL, CMD_PAUSE);
			NetworkServerSendChat(NETWORK_ACTION_SERVER_MESSAGE, DESTTYPE_BROADCAST, 0, "Game unpaused (client connected)", NETWORK_SERVER_INDEX);
		}

		CheckMinActiveClients();

		/* Execute script for, e.g. MOTD */
		IConsoleCmdExec("exec scripts/on_server_connect.scr 0");
	}

	// The client received the frame, make note of it
	cs->last_frame = frame;
	// With those 2 values we can calculate the lag realtime
	cs->last_frame_server = _frame_counter;
}



void NetworkServerSendChat(NetworkAction action, DestType desttype, int dest, const char *msg, uint16 from_index)
{
	NetworkTCPSocketHandler *cs;
	const NetworkClientInfo *ci, *ci_own, *ci_to;

	switch (desttype) {
	case DESTTYPE_CLIENT:
		/* Are we sending to the server? */
		if (dest == NETWORK_SERVER_INDEX) {
			ci = NetworkFindClientInfoFromIndex(from_index);
			/* Display the text locally, and that is it */
			if (ci != NULL)
				NetworkTextMessage(action, (ConsoleColour)GetDrawStringCompanyColor(ci->client_playas), false, ci->client_name, "%s", msg);
		} else {
			/* Else find the client to send the message to */
			FOR_ALL_CLIENTS(cs) {
				if (cs->index == dest) {
					SEND_COMMAND(PACKET_SERVER_CHAT)(cs, action, from_index, false, msg);
					break;
				}
			}
		}

		// Display the message locally (so you know you have sent it)
		if (from_index != dest) {
			if (from_index == NETWORK_SERVER_INDEX) {
				ci = NetworkFindClientInfoFromIndex(from_index);
				ci_to = NetworkFindClientInfoFromIndex(dest);
				if (ci != NULL && ci_to != NULL)
					NetworkTextMessage(action, (ConsoleColour)GetDrawStringCompanyColor(ci->client_playas), true, ci_to->client_name, "%s", msg);
			} else {
				FOR_ALL_CLIENTS(cs) {
					if (cs->index == from_index) {
						SEND_COMMAND(PACKET_SERVER_CHAT)(cs, action, dest, true, msg);
						break;
					}
				}
			}
		}
		break;
	case DESTTYPE_TEAM: {
		bool show_local = true; // If this is false, the message is already displayed
														// on the client who did sent it.
		/* Find all clients that belong to this company */
		ci_to = NULL;
		FOR_ALL_CLIENTS(cs) {
			ci = DEREF_CLIENT_INFO(cs);
			if (ci->client_playas == dest) {
				SEND_COMMAND(PACKET_SERVER_CHAT)(cs, action, from_index, false, msg);
				if (cs->index == from_index) show_local = false;
				ci_to = ci; // Remember a client that is in the company for company-name
			}
		}

		ci = NetworkFindClientInfoFromIndex(from_index);
		ci_own = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);
		if (ci != NULL && ci_own != NULL && ci_own->client_playas == dest) {
			NetworkTextMessage(action, (ConsoleColour)GetDrawStringCompanyColor(ci->client_playas), false, ci->client_name, "%s", msg);
			if (from_index == NETWORK_SERVER_INDEX) show_local = false;
			ci_to = ci_own;
		}

		/* There is no such client */
		if (ci_to == NULL) break;

		// Display the message locally (so you know you have sent it)
		if (ci != NULL && show_local) {
			if (from_index == NETWORK_SERVER_INDEX) {
				char name[NETWORK_NAME_LENGTH];
				StringID str = IsValidCompanyID(ci_to->client_playas) ? STR_COMPANY_NAME : STR_NETWORK_SPECTATORS;
				SetDParam(0, ci_to->client_playas);
				GetString(name, str, lastof(name));
				NetworkTextMessage(action, (ConsoleColour)GetDrawStringCompanyColor(ci_own->client_playas), true, name, "%s", msg);
			} else {
				FOR_ALL_CLIENTS(cs) {
					if (cs->index == from_index) {
						SEND_COMMAND(PACKET_SERVER_CHAT)(cs, action, ci_to->client_index, true, msg);
					}
				}
			}
		}
		}
		break;
	default:
		DEBUG(net, 0, "[server] received unknown chat destination type %d. Doing broadcast instead", desttype);
		/* fall-through to next case */
	case DESTTYPE_BROADCAST:
		FOR_ALL_CLIENTS(cs) {
			SEND_COMMAND(PACKET_SERVER_CHAT)(cs, action, from_index, false, msg);
		}
		ci = NetworkFindClientInfoFromIndex(from_index);
		if (ci != NULL)
			NetworkTextMessage(action, (ConsoleColour)GetDrawStringCompanyColor(ci->client_playas), false, ci->client_name, "%s", msg);
		break;
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_CHAT)
{
	if (cs->status < STATUS_AUTH) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_AUTHORIZED);
		return;
	}

	NetworkAction action = (NetworkAction)p->Recv_uint8();
	DestType desttype = (DestType)p->Recv_uint8();
	int dest = p->Recv_uint16();
	char msg[NETWORK_CHAT_LENGTH];

	p->Recv_string(msg, NETWORK_CHAT_LENGTH);

	const NetworkClientInfo *ci = DEREF_CLIENT_INFO(cs);
	switch (action) {
		case NETWORK_ACTION_GIVE_MONEY:
			if (!IsValidCompanyID(ci->client_playas)) break;
			/* Fall-through */
		case NETWORK_ACTION_CHAT:
		case NETWORK_ACTION_CHAT_CLIENT:
		case NETWORK_ACTION_CHAT_COMPANY:
			NetworkServerSendChat(action, desttype, dest, msg, cs->index);
			break;
		default:
			IConsolePrintF(CC_ERROR, "WARNING: invalid chat action from client %d (IP: %s).", ci->client_index, GetClientIP(ci));
			SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
			break;
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_SET_PASSWORD)
{
	if (cs->status != STATUS_ACTIVE) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		return;
	}

	char password[NETWORK_PASSWORD_LENGTH];
	const NetworkClientInfo *ci;

	p->Recv_string(password, sizeof(password));
	ci = DEREF_CLIENT_INFO(cs);

	if (IsValidCompanyID(ci->client_playas)) {
		strecpy(_network_company_info[ci->client_playas].password, password, lastof(_network_company_info[0].password));
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_SET_NAME)
{
	if (cs->status != STATUS_ACTIVE) {
		/* Illegal call, return error and ignore the packet */
		SEND_COMMAND(PACKET_SERVER_ERROR)(cs, NETWORK_ERROR_NOT_EXPECTED);
		return;
	}

	char client_name[NETWORK_CLIENT_NAME_LENGTH];
	NetworkClientInfo *ci;

	p->Recv_string(client_name, sizeof(client_name));
	ci = DEREF_CLIENT_INFO(cs);

	if (cs->has_quit) return;

	if (ci != NULL) {
		// Display change
		if (NetworkFindName(client_name)) {
			NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, CC_DEFAULT, false, ci->client_name, "%s", client_name);
			strecpy(ci->client_name, client_name, lastof(ci->client_name));
			NetworkUpdateClientInfo(ci->client_index);
		}
	}
}

DEF_SERVER_RECEIVE_COMMAND(PACKET_CLIENT_RCON)
{
	char pass[NETWORK_PASSWORD_LENGTH];
	char command[NETWORK_RCONCOMMAND_LENGTH];

	if (StrEmpty(_settings_client.network.rcon_password)) return;

	p->Recv_string(pass, sizeof(pass));
	p->Recv_string(command, sizeof(command));

	if (strcmp(pass, _settings_client.network.rcon_password) != 0) {
		DEBUG(net, 0, "[rcon] wrong password from client-id %d", cs->index);
		return;
	}

	DEBUG(net, 0, "[rcon] client-id %d executed: '%s'", cs->index, command);

	_redirect_console_to_client = cs->index;
	IConsoleCmdExec(command);
	_redirect_console_to_client = 0;
	return;
}

// The layout for the receive-functions by the server
typedef void NetworkServerPacket(NetworkTCPSocketHandler *cs, Packet *p);


// This array matches PacketType. At an incoming
//  packet it is matches against this array
//  and that way the right function to handle that
//  packet is found.
static NetworkServerPacket* const _network_server_packet[] = {
	NULL, /*PACKET_SERVER_FULL,*/
	NULL, /*PACKET_SERVER_BANNED,*/
	RECEIVE_COMMAND(PACKET_CLIENT_JOIN),
	NULL, /*PACKET_SERVER_ERROR,*/
	RECEIVE_COMMAND(PACKET_CLIENT_COMPANY_INFO),
	NULL, /*PACKET_SERVER_COMPANY_INFO,*/
	NULL, /*PACKET_SERVER_CLIENT_INFO,*/
	NULL, /*PACKET_SERVER_NEED_PASSWORD,*/
	RECEIVE_COMMAND(PACKET_CLIENT_PASSWORD),
	NULL, /*PACKET_SERVER_WELCOME,*/
	RECEIVE_COMMAND(PACKET_CLIENT_GETMAP),
	NULL, /*PACKET_SERVER_WAIT,*/
	NULL, /*PACKET_SERVER_MAP,*/
	RECEIVE_COMMAND(PACKET_CLIENT_MAP_OK),
	NULL, /*PACKET_SERVER_JOIN,*/
	NULL, /*PACKET_SERVER_FRAME,*/
	NULL, /*PACKET_SERVER_SYNC,*/
	RECEIVE_COMMAND(PACKET_CLIENT_ACK),
	RECEIVE_COMMAND(PACKET_CLIENT_COMMAND),
	NULL, /*PACKET_SERVER_COMMAND,*/
	RECEIVE_COMMAND(PACKET_CLIENT_CHAT),
	NULL, /*PACKET_SERVER_CHAT,*/
	RECEIVE_COMMAND(PACKET_CLIENT_SET_PASSWORD),
	RECEIVE_COMMAND(PACKET_CLIENT_SET_NAME),
	RECEIVE_COMMAND(PACKET_CLIENT_QUIT),
	RECEIVE_COMMAND(PACKET_CLIENT_ERROR),
	NULL, /*PACKET_SERVER_QUIT,*/
	NULL, /*PACKET_SERVER_ERROR_QUIT,*/
	NULL, /*PACKET_SERVER_SHUTDOWN,*/
	NULL, /*PACKET_SERVER_NEWGAME,*/
	NULL, /*PACKET_SERVER_RCON,*/
	RECEIVE_COMMAND(PACKET_CLIENT_RCON),
	NULL, /*PACKET_CLIENT_CHECK_NEWGRFS,*/
	RECEIVE_COMMAND(PACKET_CLIENT_NEWGRFS_CHECKED),
};

// If this fails, check the array above with network_data.h
assert_compile(lengthof(_network_server_packet) == PACKET_END);

// This update the company_info-stuff
void NetworkPopulateCompanyInfo()
{
	char password[NETWORK_PASSWORD_LENGTH];
	const Company *c;
	const Vehicle *v;
	const Station *s;
	NetworkTCPSocketHandler *cs;
	const NetworkClientInfo *ci;
	uint i;
	uint16 months_empty;

	for (CompanyID cid = COMPANY_FIRST; cid < MAX_COMPANIES; cid++) {
		if (!IsValidCompanyID(cid)) memset(&_network_company_info[cid], 0, sizeof(NetworkCompanyInfo));
	}

	FOR_ALL_COMPANIES(c) {
		// Clean the info but not the password
		strecpy(password, _network_company_info[c->index].password, lastof(password));
		months_empty = _network_company_info[c->index].months_empty;
		memset(&_network_company_info[c->index], 0, sizeof(NetworkCompanyInfo));
		_network_company_info[c->index].months_empty = months_empty;
		strecpy(_network_company_info[c->index].password, password, lastof(_network_company_info[c->index].password));

		// Grap the company name
		SetDParam(0, c->index);
		GetString(_network_company_info[c->index].company_name, STR_COMPANY_NAME, lastof(_network_company_info[c->index].company_name));

		// Check the income
		if (_cur_year - 1 == c->inaugurated_year) {
			// The company is here just 1 year, so display [2], else display[1]
			for (i = 0; i < lengthof(c->yearly_expenses[2]); i++) {
				_network_company_info[c->index].income -= c->yearly_expenses[2][i];
			}
		} else {
			for (i = 0; i < lengthof(c->yearly_expenses[1]); i++) {
				_network_company_info[c->index].income -= c->yearly_expenses[1][i];
			}
		}

		// Set some general stuff
		_network_company_info[c->index].inaugurated_year = c->inaugurated_year;
		_network_company_info[c->index].company_value = c->old_economy[0].company_value;
		_network_company_info[c->index].money = c->money;
		_network_company_info[c->index].performance = c->old_economy[0].performance_history;
	}

	// Go through all vehicles and count the type of vehicles
	FOR_ALL_VEHICLES(v) {
		if (!IsValidCompanyID(v->owner) || !v->IsPrimaryVehicle()) continue;
		byte type = 0;
		switch (v->type) {
			case VEH_TRAIN: type = 0; break;
			case VEH_ROAD: type = (v->cargo_type != CT_PASSENGERS) ? 1 : 2; break;
			case VEH_AIRCRAFT: type = 3; break;
			case VEH_SHIP: type = 4; break;
			default: continue;
		}
		_network_company_info[v->owner].num_vehicle[type]++;
	}

	// Go through all stations and count the types of stations
	FOR_ALL_STATIONS(s) {
		if (IsValidCompanyID(s->owner)) {
			NetworkCompanyInfo *npi = &_network_company_info[s->owner];

			if (s->facilities & FACIL_TRAIN)      npi->num_station[0]++;
			if (s->facilities & FACIL_TRUCK_STOP) npi->num_station[1]++;
			if (s->facilities & FACIL_BUS_STOP)   npi->num_station[2]++;
			if (s->facilities & FACIL_AIRPORT)    npi->num_station[3]++;
			if (s->facilities & FACIL_DOCK)       npi->num_station[4]++;
		}
	}

	ci = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);
	// Register local company (if not dedicated)
	if (ci != NULL && IsValidCompanyID(ci->client_playas))
		strecpy(_network_company_info[ci->client_playas].clients, ci->client_name, lastof(_network_company_info[0].clients));

	FOR_ALL_CLIENTS(cs) {
		char client_name[NETWORK_CLIENT_NAME_LENGTH];

		NetworkGetClientName(client_name, sizeof(client_name), cs);

		ci = DEREF_CLIENT_INFO(cs);
		if (ci != NULL && IsValidCompanyID(ci->client_playas)) {
			if (!StrEmpty(_network_company_info[ci->client_playas].clients)) {
				strecat(_network_company_info[ci->client_playas].clients, ", ", lastof(_network_company_info[0].clients));
			}

			strecat(_network_company_info[ci->client_playas].clients, client_name, lastof(_network_company_info[0].clients));
		}
	}
}

// Send a packet to all clients with updated info about this client_index
void NetworkUpdateClientInfo(uint16 client_index)
{
	NetworkTCPSocketHandler *cs;
	NetworkClientInfo *ci = NetworkFindClientInfoFromIndex(client_index);

	if (ci == NULL) return;

	FOR_ALL_CLIENTS(cs) {
		SEND_COMMAND(PACKET_SERVER_CLIENT_INFO)(cs, ci);
	}
}

/* Check if we want to restart the map */
static void NetworkCheckRestartMap()
{
	if (_settings_client.network.restart_game_year != 0 && _cur_year >= _settings_client.network.restart_game_year) {
		DEBUG(net, 0, "Auto-restarting map. Year %d reached", _cur_year);

		StartNewGameWithoutGUI(GENERATE_NEW_SEED);
	}
}

/* Check if the server has autoclean_companies activated
    Two things happen:
      1) If a company is not protected, it is closed after 1 year (for example)
      2) If a company is protected, protection is disabled after 3 years (for example)
           (and item 1. happens a year later) */
static void NetworkAutoCleanCompanies()
{
	NetworkTCPSocketHandler *cs;
	const NetworkClientInfo *ci;
	const Company *c;
	bool clients_in_company[MAX_COMPANIES];

	if (!_settings_client.network.autoclean_companies) return;

	memset(clients_in_company, 0, sizeof(clients_in_company));

	/* Detect the active companies */
	FOR_ALL_CLIENTS(cs) {
		ci = DEREF_CLIENT_INFO(cs);
		if (IsValidCompanyID(ci->client_playas)) clients_in_company[ci->client_playas] = true;
	}

	if (!_network_dedicated) {
		ci = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);
		if (IsValidCompanyID(ci->client_playas)) clients_in_company[ci->client_playas] = true;
	}

	/* Go through all the comapnies */
	FOR_ALL_COMPANIES(c) {
		/* Skip the non-active once */
		if (c->is_ai) continue;

		if (!clients_in_company[c->index]) {
			/* The company is empty for one month more */
			_network_company_info[c->index].months_empty++;

			/* Is the company empty for autoclean_unprotected-months, and is there no protection? */
			if (_settings_client.network.autoclean_unprotected != 0 && _network_company_info[c->index].months_empty > _settings_client.network.autoclean_unprotected && StrEmpty(_network_company_info[c->index].password)) {
				/* Shut the company down */
				DoCommandP(0, 2, c->index, NULL, CMD_COMPANY_CTRL);
				IConsolePrintF(CC_DEFAULT, "Auto-cleaned company #%d", c->index + 1);
			}
			/* Is the company empty for autoclean_protected-months, and there is a protection? */
			if (_settings_client.network.autoclean_protected != 0 && _network_company_info[c->index].months_empty > _settings_client.network.autoclean_protected && !StrEmpty(_network_company_info[c->index].password)) {
				/* Unprotect the company */
				_network_company_info[c->index].password[0] = '\0';
				IConsolePrintF(CC_DEFAULT, "Auto-removed protection from company #%d", c->index + 1);
				_network_company_info[c->index].months_empty = 0;
			}
		} else {
			/* It is not empty, reset the date */
			_network_company_info[c->index].months_empty = 0;
		}
	}
}

// This function changes new_name to a name that is unique (by adding #1 ...)
//  and it returns true if that succeeded.
bool NetworkFindName(char new_name[NETWORK_CLIENT_NAME_LENGTH])
{
	NetworkTCPSocketHandler *new_cs;
	bool found_name = false;
	byte number = 0;
	char original_name[NETWORK_CLIENT_NAME_LENGTH];

	// We use NETWORK_CLIENT_NAME_LENGTH in here, because new_name is really a pointer
	ttd_strlcpy(original_name, new_name, NETWORK_CLIENT_NAME_LENGTH);

	while (!found_name) {
		const NetworkClientInfo *ci;

		found_name = true;
		FOR_ALL_CLIENTS(new_cs) {
			ci = DEREF_CLIENT_INFO(new_cs);
			if (strcmp(ci->client_name, new_name) == 0) {
				// Name already in use
				found_name = false;
				break;
			}
		}
		// Check if it is the same as the server-name
		ci = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);
		if (ci != NULL) {
			if (strcmp(ci->client_name, new_name) == 0) found_name = false; // name already in use
		}

		if (!found_name) {
			// Try a new name (<name> #1, <name> #2, and so on)

			// Stop if we tried for more than 50 times..
			if (number++ > 50) break;
			snprintf(new_name, NETWORK_CLIENT_NAME_LENGTH, "%s #%d", original_name, number);
		}
	}

	return found_name;
}

// Reads a packet from the stream
bool NetworkServer_ReadPackets(NetworkTCPSocketHandler *cs)
{
	Packet *p;
	NetworkRecvStatus res;
	while ((p = cs->Recv_Packet(&res)) != NULL) {
		byte type = p->Recv_uint8();
		if (type < PACKET_END && _network_server_packet[type] != NULL && !cs->has_quit) {
			_network_server_packet[type](cs, p);
		} else {
			DEBUG(net, 0, "[server] received invalid packet type %d", type);
		}
		delete p;
	}

	return true;
}

// Handle the local command-queue
static void NetworkHandleCommandQueue(NetworkTCPSocketHandler* cs)
{
	CommandPacket *cp;

	while ( (cp = cs->command_queue) != NULL) {
		SEND_COMMAND(PACKET_SERVER_COMMAND)(cs, cp);

		cs->command_queue = cp->next;
		free(cp);
	}
}

// This is called every tick if this is a _network_server
void NetworkServer_Tick(bool send_frame)
{
	NetworkTCPSocketHandler *cs;
#ifndef ENABLE_NETWORK_SYNC_EVERY_FRAME
	bool send_sync = false;
#endif

#ifndef ENABLE_NETWORK_SYNC_EVERY_FRAME
	if (_frame_counter >= _last_sync_frame + _settings_client.network.sync_freq) {
		_last_sync_frame = _frame_counter;
		send_sync = true;
	}
#endif

	// Now we are done with the frame, inform the clients that they can
	//  do their frame!
	FOR_ALL_CLIENTS(cs) {
		// Check if the speed of the client is what we can expect from a client
		if (cs->status == STATUS_ACTIVE) {
			// 1 lag-point per day
			int lag = NetworkCalculateLag(cs) / DAY_TICKS;
			if (lag > 0) {
				if (lag > 3) {
					// Client did still not report in after 4 game-day, drop him
					//  (that is, the 3 of above, + 1 before any lag is counted)
					IConsolePrintF(CC_ERROR,"Client #%d is dropped because the client did not respond for more than 4 game-days", cs->index);
					NetworkCloseClient(cs);
					continue;
				}

				// Report once per time we detect the lag
				if (cs->lag_test == 0) {
					IConsolePrintF(CC_WARNING,"[%d] Client #%d is slow, try increasing *net_frame_freq to a higher value!", _frame_counter, cs->index);
					cs->lag_test = 1;
				}
			} else {
				cs->lag_test = 0;
			}
		} else if (cs->status == STATUS_PRE_ACTIVE) {
			int lag = NetworkCalculateLag(cs);
			if (lag > _settings_client.network.max_join_time) {
				IConsolePrintF(CC_ERROR,"Client #%d is dropped because it took longer than %d ticks for him to join", cs->index, _settings_client.network.max_join_time);
				NetworkCloseClient(cs);
			}
		} else if (cs->status == STATUS_INACTIVE) {
			int lag = NetworkCalculateLag(cs);
			if (lag > 4 * DAY_TICKS) {
				IConsolePrintF(CC_ERROR,"Client #%d is dropped because it took longer than %d ticks to start the joining process", cs->index, 4 * DAY_TICKS);
				NetworkCloseClient(cs);
			}
		}

		if (cs->status >= STATUS_PRE_ACTIVE) {
			// Check if we can send command, and if we have anything in the queue
			NetworkHandleCommandQueue(cs);

			// Send an updated _frame_counter_max to the client
			if (send_frame) SEND_COMMAND(PACKET_SERVER_FRAME)(cs);

#ifndef ENABLE_NETWORK_SYNC_EVERY_FRAME
			// Send a sync-check packet
			if (send_sync) SEND_COMMAND(PACKET_SERVER_SYNC)(cs);
#endif
		}
	}

	/* See if we need to advertise */
	NetworkUDPAdvertise();
}

void NetworkServerYearlyLoop()
{
	NetworkCheckRestartMap();
}

void NetworkServerMonthlyLoop()
{
	NetworkAutoCleanCompanies();
}

void NetworkServerChangeOwner(Owner current_owner, Owner new_owner)
{
	/* The server has to handle all administrative issues, for example
	 * updating and notifying all clients of what has happened */
	NetworkTCPSocketHandler *cs;
	NetworkClientInfo *ci = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);

	/* The server has just changed from owner */
	if (current_owner == ci->client_playas) {
		ci->client_playas = new_owner;
		NetworkUpdateClientInfo(NETWORK_SERVER_INDEX);
	}

	/* Find all clients that were in control of this company, and mark them as new_owner */
	FOR_ALL_CLIENTS(cs) {
		ci = DEREF_CLIENT_INFO(cs);
		if (current_owner == ci->client_playas) {
			ci->client_playas = new_owner;
			NetworkUpdateClientInfo(ci->client_index);
		}
	}
}

const char* GetClientIP(const NetworkClientInfo* ci)
{
	struct in_addr addr;

	addr.s_addr = ci->client_ip;
	return inet_ntoa(addr);
}

void NetworkServerShowStatusToConsole()
{
	static const char* const stat_str[] = {
		"inactive",
		"authorizing",
		"authorized",
		"waiting",
		"loading map",
		"map done",
		"ready",
		"active"
	};

	NetworkTCPSocketHandler *cs;
	FOR_ALL_CLIENTS(cs) {
		int lag = NetworkCalculateLag(cs);
		const NetworkClientInfo *ci = DEREF_CLIENT_INFO(cs);
		const char* status;

		status = (cs->status < (ptrdiff_t)lengthof(stat_str) ? stat_str[cs->status] : "unknown");
		IConsolePrintF(CC_INFO, "Client #%1d  name: '%s'  status: '%s'  frame-lag: %3d  company: %1d  IP: %s  unique-id: '%s'",
			cs->index, ci->client_name, status, lag,
			ci->client_playas + (IsValidCompanyID(ci->client_playas) ? 1 : 0),
			GetClientIP(ci), ci->unique_id);
	}
}

void NetworkServerSendRcon(uint16 client_index, ConsoleColour colour_code, const char *string)
{
	SEND_COMMAND(PACKET_SERVER_RCON)(NetworkFindClientStateFromIndex(client_index), colour_code, string);
}

void NetworkServerSendError(uint16 client_index, NetworkErrorCode error)
{
	SEND_COMMAND(PACKET_SERVER_ERROR)(NetworkFindClientStateFromIndex(client_index), error);
}

bool NetworkCompanyHasClients(CompanyID company)
{
	const NetworkTCPSocketHandler *cs;
	const NetworkClientInfo *ci;
	FOR_ALL_CLIENTS(cs) {
		ci = DEREF_CLIENT_INFO(cs);
		if (ci->client_playas == company) return true;
	}
	return false;
}
#endif /* ENABLE_NETWORK */
