#include "proxy.h"
#include <engine/shared/packer.h>

std::atomic<int> TeeBridge::m_NextFakeClientId{1};

ProxyClient::ProxyClient(TeeBridge *bridge, int realClientId, const NETADDR &targetAddr) :
	m_Bridge(bridge), m_RealClientId(realClientId), m_TargetAddr(targetAddr)
{
	NETADDR bindAddr;
	mem_zero(&bindAddr, sizeof(bindAddr));
	bindAddr.type = NETTYPE_ALL;
	m_Client.Open(bindAddr);
	ConnectToServer();
}

ProxyClient::~ProxyClient()
{
	m_Client.Close();
}

void ProxyClient::ConnectToServer()
{
	dbg_msg("ProxyClient", "Connecting to real server...");

	if(m_TargetAddr.port == 0)
		m_TargetAddr.port = 8303;

	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(&m_TargetAddr, aAddrStr, sizeof(aAddrStr), true);
	dbg_msg("ProxyClient", "Connecting to real server at %s", aAddrStr);

	m_Sixup = (m_TargetAddr.type & NETTYPE_TW7) != 0;

	NETADDR aConnectAddrs[1] = {m_TargetAddr};
	if(m_Sixup)
	{
		dbg_msg("ProxyClient", "Using Connect7() for TW7/Sixup protocol");
		m_Client.Connect7(aConnectAddrs, 1);
	}
	else
	{
		dbg_msg("ProxyClient", "Using Connect() for TW6 protocol");
		m_Client.Connect(aConnectAddrs, 1);
	}

	m_Client.RefreshStun();
}

void ProxyClient::SendToServer(CNetChunk chunk)
{
	m_Client.Send(&chunk);
}

bool ProxyClient::RecvFromServer(CNetChunk &chunk, SECURITY_TOKEN token)
{
	bool result = m_Client.Recv(&chunk, &token, false);
	if(result)
	{
		dbg_msg("ProxyClient", "Received packet from real server (size: %d)", chunk.m_DataSize);
	}
	return result;
}

void ProxyClient::Update()
{
	m_Client.Update();
	switch(m_Client.State())
	{
	case NET_CONNSTATE_ONLINE:
		dbg_msg("ProxyClient", "State: OLINE");
		break;
	case NET_CONNSTATE_OFFLINE:
		dbg_msg("ProxyClient", "State: OFFLINE");
		break;
	case NETSTATE_CONNECTING:
		dbg_msg("ProxyClient", "State: CONNECTING");
		break;
	default:
		dbg_msg("ProxyClient", "State: UNKNOWN (%d)", m_Client.State());
		break;
	}
}

TeeBridge::TeeBridge(const NETADDR &listenAddr, const NETADDR &targetAddr) :
	m_TargetAddr(targetAddr)
{
	m_Server.Open(listenAddr, &m_NetBan, 64, 4);
	m_Server.SetCallbacks(OnNewRealClient, OnNewRealClientNoAuth, OnRealClientRejoin, OnDeleteRealClient, this);
	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(&listenAddr, aAddrStr, sizeof(aAddrStr), true);
	dbg_msg("Proxy", "Listening %s", aAddrStr);
}

void TeeBridge::Run()
{
	CNetChunk chunk;
	SECURITY_TOKEN token;

	while(true)
	{
		m_Server.Update();

		if(m_Server.Recv(&chunk, &token))
		{
			HandleRealClientPacket(chunk, token);
		}

		for(auto &[fakeClientId, proxyClient] : m_ProxyClients)
		{
			proxyClient->Update();
			while(proxyClient->RecvFromServer(chunk, token))
			{
				HandleServerPacket(chunk, token, fakeClientId);
			}
		}
	}
}

int TeeBridge::OnNewRealClient(int realClientId, void *pUser, bool)
{
	auto *bridge = static_cast<TeeBridge *>(pUser);
	auto proxyClient = std::make_unique<ProxyClient>(bridge, realClientId, bridge->m_TargetAddr);
	int fakeClientId = GenerateUniqueFakeClientId();
	bridge->m_ProxyClients[fakeClientId] = std::move(proxyClient);
	bridge->m_FakeToRealClientMap[fakeClientId] = realClientId;
	return 0;
}

int TeeBridge::OnNewRealClientNoAuth(int realClientId, void *pUser)
{
	return OnNewRealClient(realClientId, pUser, false);
}

int TeeBridge::OnRealClientRejoin(int realClientId, void *pUser)
{
	return 0;
}

int TeeBridge::OnDeleteRealClient(int realClientId, const char *, void *pUser)
{
	auto *bridge = static_cast<TeeBridge *>(pUser);
	for(auto it = bridge->m_ProxyClients.begin(); it != bridge->m_ProxyClients.end(); ++it)
	{
		if(it->second->GetRealClientId() == realClientId)
		{
			bridge->m_ProxyClients.erase(it);
			break;
		}
	}
	return 0;
}

void TeeBridge::HandleRealClientPacket(CNetChunk chunk, SECURITY_TOKEN token)
{
	dbg_msg("TeeBridge", "Received packet from real client (size: %d)", chunk.m_DataSize);

	ParseAndPrintChat(chunk);

	int realClientId = chunk.m_ClientId;
	auto it = m_ProxyClients.find(realClientId);
	if(it != m_ProxyClients.end())
	{
		dbg_msg("TeeBridge", "Forwarding packet to fake client %d", realClientId);
		it->second->SendToServer(chunk);
	}
}

void TeeBridge::HandleServerPacket(CNetChunk chunk, SECURITY_TOKEN token, int fakeClientId)
{
	auto it = m_FakeToRealClientMap.find(fakeClientId);
	if(it != m_FakeToRealClientMap.end())
	{
		chunk.m_ClientId = it->second; // 替换为真实客户端 ID
		m_Server.Send(&chunk); // 转发给真实客户端
	}
}

void TeeBridge::ParseAndPrintChat(CNetChunk chunk)
{
	CUnpacker u;
	u.Reset((unsigned char *)chunk.m_pData, chunk.m_DataSize);
	int msg = u.GetInt() >> 1;
	if(msg == 1 || msg == 6)
		dbg_msg("Chat", "%s", u.GetString(CUnpacker::SANITIZE_CC));
}

int TeeBridge::GenerateUniqueFakeClientId()
{
	return m_NextFakeClientId++;
}
