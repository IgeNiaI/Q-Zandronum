//-----------------------------------------------------------------------------
//
// Zandronum-tspg Source
// Copyright (C) 2018 Sean Baggaley
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Zandronum Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: tspg_dnsbl.cpp
//
//-----------------------------------------------------------------------------
#include "tspg_dnsbl.h"

#ifdef _WIN32
void TSPG_InitDNSBL() {}
void TSPG_DeinitDNSBL() {}
void TSPG_TickDNSBL() {}
void TSPG_CheckIPAgainstBlocklists(NETADDRESS_s& address) {}
#else
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "sv_ban.h"
#include "c_dispatch.h"
#include "c_console.h"
#include "json.hpp"

using json = nlohmann::json;

CVAR(Bool, tspg_dnsbl_enable, false, TSPG_NOSET);
CVAR(String, tspg_dnsbl_socket, "tspg-dnsbl.sock", TSPG_NOSET);

static int sock;
static bool isConnected = false;

static void parseResponse(json&);
static void kickIp(NETADDRESS_s&);
static void onDisconnect();
static bool attemptConnection();
static bool sendFail = false;

void TSPG_InitDNSBL()
{
	if (!tspg_dnsbl_enable)
	{
		return;
	}

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	{
		Printf("Failed to create socket.\n");
		return;
	}

	const char *socketAddress = tspg_dnsbl_socket.GetGenericRep(CVAR_String).String;

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socketAddress, sizeof(addr.sun_path) - 1);

	int flags = fcntl(sock, F_GETFL, 0);
	fcntl(sock, F_SETFL, flags | O_NONBLOCK);

	if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			// do nothing
		}
		else
		{
			Printf("Failed to connect to socket %s: (%d) %s.\n", socketAddress, errno, strerror(errno));
			return;
		}
	}

	isConnected = true;
	sendFail = false;
}

void TSPG_DeinitDNSBL()
{
	if (!tspg_dnsbl_enable || !isConnected)
	{
		return;
	}

	close(sock);
	isConnected = false;
}

void onDisconnect()
{
	isConnected = false;
	close(sock);
}

void TSPG_CheckIPAgainstBlocklists(NETADDRESS_s& address)
{
	if (!tspg_dnsbl_enable)
	{
		return;
	}

	if (!attemptConnection())
	{
		return;
	}

	std::string addrStr(address.ToStringNoPort());

	json req;
	req["type"] = "check";
	req["address"] = addrStr;

	std::string reqstr = req.dump();

	int sendflags = 0;
#ifdef __linux__
	sendflags |= MSG_NOSIGNAL;
#endif
	int bytes = send(sock, reqstr.c_str(), reqstr.length(), sendflags);
	if (bytes == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			// nothing to do?
		}
		else
		{
			Printf("TSPG_CheckIPAgainstBlocklists: error while sending request for address %s: (%d) %s\n", address.ToString(), errno, strerror(errno));
			onDisconnect();

			// Try again - it forces us to reconnect
			sendFail = true;
			if (!sendFail)
			{
				TSPG_CheckIPAgainstBlocklists(address);
			}
		}
	}
}

void TSPG_TickDNSBL()
{
	if (!isConnected || !tspg_dnsbl_enable)
	{
		return;
	}

	char buf[8192];
	memset(&buf, 0, sizeof(buf));
	int bytes;

	while ((bytes = recv(sock, buf, sizeof(buf), 0)) > 0)
	{
		try
		{
			json res = json::parse(buf);
			parseResponse(res);
		}
		catch (json::parse_error e)
		{
			Printf("Error while parsing response: %s.\n", e.what());
		}
	}

	if (bytes == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
		{
			// nothing to do?
		}
		else
		{
			Printf("TSPG_CheckIPAgainstBlocklists: error while receiving response: (%d) %s\n", errno, strerror(errno));
			onDisconnect();
			return;
		}
	}

	// tmp?
	TSPG_KickBannedPlayers();
}

void parseResponse(json& res)
{
	std::string type = res["type"];
	if (type == "ban")
	{
		std::string address = res["address"];
		std::string reason = res["reason"];
		time_t tExpiration = SERVERBAN_ParseBanLength("perm");

		char buf[64];
		snprintf(buf, sizeof(buf), "An IP masking service (VPN) has been detected. Please disable it before connecting (%s)", reason.c_str());

		std::string unused;
		SERVERBAN_GetBanList()->addEntry(address.c_str(), nullptr, buf, unused, tExpiration);

		NETADDRESS_s netaddr;
		netaddr.LoadFromString(address.c_str());
		kickIp(netaddr);

		Printf("[TSPG] %s an IP masking service (VPN) has been detected and has been automatically banned (%s)\n", address.c_str(), reason.c_str());
	}
}

void kickIp(NETADDRESS_s& addr)
{
	for (ULONG ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++)
	{
		if (SERVER_GetClient(ulIdx)->State == CLS_FREE)
			continue;

		if (SERVER_GetClient(ulIdx)->Address.CompareNoPort(addr))
		{
			char buf[512];
			snprintf(buf, sizeof(buf), "An IP masking service (VPN) has been detected. Please disable it before connecting.");

			SERVER_KickPlayer(ulIdx, buf);
		}
	}
}

bool attemptConnection()
{
	if (!isConnected)
	{
		TSPG_InitDNSBL();
	}

	return isConnected;
}
#endif