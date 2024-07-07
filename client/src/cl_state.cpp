
#include "odamex.h"

#include "cl_state.h"

#include "i_system.h"
#include "md5.h"
#include "ocircularbuffer.h"

//------------------------------------------------------------------------------

class ClientStateImpl final : public ClientState
{
	std::string m_passwordHash;
	netadr_t m_currentAddress = {};
	netadr_t m_lastAddress = {};
	dtime_t m_lastConnectTime = 0;
	bool m_connected = false;
	uint64_t m_connectID = 0;
	OCircularBuffer<uint32_t, BIT(10)>
	    m_ackedPackets;                     // [LM] Sequence IDs of acked packets.
	uint32_t m_recentPacketID = UINT32_MAX; // [LM] Most recently acked packet ID.

  public:
	virtual void reset() override;
	virtual const netadr_t& getAddress() override { return m_currentAddress; }
	virtual const std::string& getPasswordHash() override { return m_passwordHash; }
	virtual bool startConnect(const std::string& strAddress,
	                          const std::string& strPassword) override;
	virtual bool startReconnect() override;
	virtual bool canRetryConnect() override;
	virtual bool isConnecting() override;
	virtual bool isConnected() override;
	virtual bool isValidAddress(const netadr_t& cAddress) override;
	virtual void ack(const uint32_t id) override;
	virtual uint32_t getRecentAck() override;
	virtual uint32_t getAckBits() override;
	virtual void onGotServerInfo() override;
	virtual void onSentConnect() override;
	virtual void onConnected() override;
	virtual void onDisconnect() override;
};

//------------------------------------------------------------------------------

void ClientStateImpl::reset()
{
	m_passwordHash = "";
	memset(&m_currentAddress, 0x00, sizeof(m_currentAddress));
	memset(&m_lastAddress, 0x00, sizeof(m_lastAddress));
	m_lastConnectTime = 0;
	m_connected = false;
	m_connectID = 0;
	m_ackedPackets = {};
	m_recentPacketID = UINT32_MAX;
}

//------------------------------------------------------------------------------

bool ClientStateImpl::startConnect(const std::string& strAddress,
                                   const std::string& strPassword)
{
	// [Russell] - Passworded servers
	if (!strPassword.empty())
	{
		m_passwordHash = MD5SUM(strPassword.c_str());
	}
	else
	{
		m_passwordHash = "";
	}

	if (!NET_StringToAdr(strAddress.c_str(), &m_currentAddress))
	{
		memset(&m_currentAddress, 0, sizeof(m_currentAddress));
		return false;
	}

	if (!m_currentAddress.port)
	{
		I_SetPort(m_currentAddress, SERVERPORT);
	}

	m_lastAddress = m_currentAddress;
	m_lastConnectTime = I_MSTime();
	return true;
}

//------------------------------------------------------------------------------

bool ClientStateImpl::startReconnect()
{
	if (m_lastAddress.ip[0] == 0)
	{
		return false;
	}

	m_currentAddress = m_lastAddress;
	m_lastConnectTime = I_MSTime();
	return true;
}

//------------------------------------------------------------------------------

bool ClientStateImpl::canRetryConnect()
{
	return m_lastConnectTime == 0 || I_MSTime() - m_lastConnectTime > 1000;
}

//------------------------------------------------------------------------------

bool ClientStateImpl::isConnecting()
{
	return !m_connected && m_currentAddress.ip[0] != 0;
}

//------------------------------------------------------------------------------

bool ClientStateImpl::isConnected()
{
	return m_connected && m_currentAddress.ip[0] != 0;
}

//------------------------------------------------------------------------------

bool ClientStateImpl::isValidAddress(const netadr_t& cAddress)
{
	return NET_CompareAdr(cAddress, m_currentAddress);
}

//------------------------------------------------------------------------------

void ClientStateImpl::ack(const uint32_t id)
{
	// Compare id relative to the current packet ID, to avoid wrapping weirdness.
	if (int32_t(id - m_recentPacketID) > 0)
		m_recentPacketID = id;

	m_ackedPackets[id] = id;
}

//------------------------------------------------------------------------------

uint32_t ClientStateImpl::getRecentAck()
{
	return m_recentPacketID;
}

//------------------------------------------------------------------------------

uint32_t ClientStateImpl::getAckBits()
{
	uint32_t bits = 0;
	uint32_t id = m_recentPacketID - 1;

	for (size_t i = 0; i < 32; i++)
	{
		if (m_ackedPackets[id] == id)
		{
			bits |= BIT(i);
		}
		id -= 1;
	}

	return bits;
}

//------------------------------------------------------------------------------

void ClientStateImpl::onGotServerInfo()
{
	m_lastConnectTime = 0;
}

//------------------------------------------------------------------------------

void ClientStateImpl::onSentConnect()
{
	m_lastConnectTime = I_MSTime();
}

//------------------------------------------------------------------------------

void ClientStateImpl::onConnected()
{
	m_connected = true;
	m_lastConnectTime = 0;
	m_ackedPackets = {};
	m_recentPacketID = UINT32_MAX;
}

//------------------------------------------------------------------------------

void ClientStateImpl::onDisconnect()
{
	memset(&m_currentAddress, 0, sizeof(m_currentAddress));
	m_connected = false;
	m_lastConnectTime = 0;
}

//------------------------------------------------------------------------------

ClientState& ClientState::get()
{
	static ClientStateImpl clientState;
	return clientState;
}
