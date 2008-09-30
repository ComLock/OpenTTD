/* $Id$ */

/** @file network_internal.h Variables and function used internally. */

#ifndef NETWORK_FUNC_H
#define NETWORK_FUNC_H

#ifdef ENABLE_NETWORK

#include "network_type.h"
#include "../console_type.h"

extern NetworkServerGameInfo _network_game_info;
extern NetworkCompanyInfo _network_company_info[MAX_COMPANIES];
extern NetworkClientInfo _network_client_info[MAX_CLIENT_INFO];

extern uint16 _network_own_client_index;
extern uint16 _redirect_console_to_client;
extern bool _network_need_advertise;
extern uint32 _network_last_advertise_frame;
extern uint8 _network_reconnect;
extern char *_network_host_list[10];
extern char *_network_ban_list[25];

byte NetworkSpectatorCount();
void CheckMinActiveClients();
void NetworkUpdateClientName();
bool NetworkCompanyHasClients(CompanyID company);
bool NetworkChangeCompanyPassword(byte argc, char *argv[]);
void NetworkReboot();
void NetworkDisconnect();
void NetworkGameLoop();
void NetworkUDPGameLoop();
void NetworkUDPCloseAll();
void ParseConnectionString(const char **company, const char **port, char *connection_string);
void NetworkStartDebugLog(const char *hostname, uint16 port);
void NetworkPopulateCompanyInfo();

void NetworkUpdateClientInfo(uint16 client_index);
bool NetworkClientConnectGame(const char *host, uint16 port);
void NetworkClientSendRcon(const char *password, const char *command);
void NetworkClientSendChat(NetworkAction action, DestType type, int dest, const char *msg);
void NetworkClientSetPassword();

/*** Commands ran by the server ***/
void NetworkServerMonthlyLoop();
void NetworkServerYearlyLoop();
void NetworkServerChangeOwner(Owner current_owner, Owner new_owner);
void NetworkServerShowStatusToConsole();
bool NetworkServerStart();

NetworkClientInfo *NetworkFindClientInfoFromIndex(uint16 client_index);
NetworkClientInfo *NetworkFindClientInfoFromIP(const char *ip);
const char* GetClientIP(const NetworkClientInfo *ci);

void NetworkServerSendRcon(uint16 client_index, ConsoleColour colour_code, const char *string);
void NetworkServerSendError(uint16 client_index, NetworkErrorCode error);
void NetworkServerSendChat(NetworkAction action, DestType type, int dest, const char *msg, uint16 from_index);

void NetworkInitChatMessage();
void CDECL NetworkAddChatMessage(uint16 color, uint8 duration, const char *message, ...);
void NetworkUndrawChatMessage();
void NetworkChatMessageDailyLoop();

#define FOR_ALL_ACTIVE_CLIENT_INFOS(ci) for (ci = _network_client_info; ci != endof(_network_client_info); ci++) if (ci->client_index != NETWORK_EMPTY_INDEX)

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_FUNC_H */
