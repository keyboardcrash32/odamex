// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 2000-2006 by Sergey Makovkin (CSDoom .62).
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
//	SV_RPROTO
//
//-----------------------------------------------------------------------------


#include "odamex.h"

#include "p_local.h"
#include "sv_main.h"
#include "huffman.h"
#include "i_net.h"

#ifdef SIMULATE_LATENCY
#include <thread>
#include <chrono>
#endif

QWORD I_MSTime (void);

EXTERN_CVAR (log_packetdebug)
#ifdef SIMULATE_LATENCY
EXTERN_CVAR (sv_latency)
#endif

buf_t plain(MAX_UDP_PACKET); // denis - todo - call_terms destroys these statics on quit
buf_t sendd(MAX_UDP_PACKET);

const static size_t PACKET_FLAG_INDEX = sizeof(uint32_t);
const static size_t PACKET_MESSAGE_INDEX = PACKET_FLAG_INDEX + 1;
const static size_t PACKET_HEADER_SIZE = PACKET_MESSAGE_INDEX;
const static size_t PACKET_OLD_MASK = 0xFF;

//
// CompressPacket
//
// [Russell] - reason this was failing is because of huffman routines, so just
// use minilzo for now (cuts a packet size down by roughly 45%), huffman is the
// if 0'd sections
//
// [AM] Cleaned the old huffman calls for code clarity sake.
//
static void CompressPacket(buf_t& send, const size_t reserved, client_t* cl)
{
	if (plain.maxsize() < send.maxsize())
	{
		plain.resize(send.maxsize());
	}

	plain.setcursize(send.size());
	memcpy(plain.ptr(), send.ptr(), send.size());

	byte method = 0;
	if (MSG_CompressMinilzo(send, reserved, 0))
	{
		// Successful compression, set the compression flag bit.
		method |= SVF_COMPRESSED;
	}

	send.ptr()[PACKET_FLAG_INDEX] |= method;
	DPrintf("CompressPacket %x " "zu" "\n", method, send.size());
}

#ifdef SIMULATE_LATENCY
struct DelaySend
{
public:
	DelaySend(buf_t& data, player_t* pl)
	{
		m_data = data;
		m_pl = pl;
		m_tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(sv_latency);
	}
	std::chrono::time_point<std::chrono::steady_clock> m_tp;
	buf_t m_data;
	player_t* m_pl;
};

std::queue<DelaySend> m_delayQueue;
bool m_delayThreadCreated = false;
void SV_DelayLoop()
{
	for (;;)
	{
		while (m_delayQueue.size())
		{
			int sendgametic = gametic;
			auto item = &m_delayQueue.front();

			while (std::chrono::steady_clock::now() < item->m_tp)
				std::this_thread::sleep_for(std::chrono::milliseconds(1));

			NET_SendPacket(item->m_data, item->m_pl->client.address);
			m_delayQueue.pop();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void SV_SendPacketDelayed(buf_t& packet, player_t& pl)
{
	if (!m_delayThreadCreated)
	{
		std::thread tr(SV_DelayLoop);
		tr.detach();
		m_delayThreadCreated = true;
	}
	m_delayQueue.push(DelaySend(packet, &pl));
}
#endif

/**
 * @brief Send all queued packets for a client.
 * 
 * @param client Client to send packets for.
 * @return True if at least one packet was sent.
 */
bool SV_SendQueuedPackets(client_t& client)
{
	// Only measure time at this point, otherwise a server under load could
	// get stuck in an infinite loop by endlessly resending reliable packets
	// if it takes longer than the timeout to assemble a single packet
	// Unlikely, but better to be safe than sorry.
	const dtime_t time = I_MSTime();

	// Number of packets that can contain reliable packets.
	constexpr size_t reliableWindow = 2;

	size_t count = 0;
	for (;;)
	{
		if (!client.msg.writePacket(client.netbuf, time, count < reliableWindow))
			break;

		// [AM] Most of the hard work of assembling the packet is done in the client.
		//      struct.  Here, we just set some flags and send the netbuf.
		if (client.netbuf.size() <= PACKET_HEADER_SIZE)
		{
			// Can't send a packet this small.
			break;
		}

		// compress the packet, but not the sequence id
		if (client.netbuf.size() > PACKET_HEADER_SIZE)
		{
			CompressPacket(client.netbuf, PACKET_HEADER_SIZE, &client);
		}

		if (log_packetdebug)
		{
			// [AM] TODO: What can we put here?
		}

#ifdef SIMULATE_LATENCY
		SV_SendPacketDelayed(sendd, pl);
#else
		NET_SendPacket(client.netbuf, client.address);
#endif
		count += 1;
	}

	return count > 0;
}

//
// SV_AcknowledgePacket
//
void SV_AcknowledgePacket(player_t &player)
{
	client_t& cl = *player.client;

	const uint32_t packetAck = MSG_ReadULong();
	const uint32_t packetAckBits = MSG_ReadULong();
	const bool connected = cl.msg.clientAck(packetAck, packetAckBits);

	if (!connected)
	{
		// [AM] Finish our connection sequence.
		SV_ConnectClient2(player);
	}
}

VERSION_CONTROL (sv_rproto_cpp, "$Id$")
