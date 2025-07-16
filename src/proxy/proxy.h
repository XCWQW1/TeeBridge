#pragma once

#include <atomic>
#include <memory>
#include <unordered_map>

#include "netban.h"
#include "uuid_manager.h"
#include <engine/shared/network.h>

class ProxyClient;
class TeeBridge {
public:
	TeeBridge(const NETADDR& listenAddr, const NETADDR& targetAddr);
	~TeeBridge() = default;
	void Run();

	NETADDR m_TargetAddr;
private:
	static int OnNewRealClient(int realClientId, void* pUser, bool sixup);
	static int OnNewRealClientNoAuth(int realClientId, void* pUser);
	static int OnRealClientRejoin(int realClientId, void* pUser);
	static int OnDeleteRealClient(int realClientId, const char* pReason, void* pUser);

	void HandleRealClientPacket(CNetChunk chunk, SECURITY_TOKEN token);
	void HandleServerPacket(CNetChunk chunk, SECURITY_TOKEN token, int fakeClientId);

	void ParseAndPrintChat(CNetChunk chunk);
	static int GenerateUniqueFakeClientId();

	CNetServer m_Server;
	CNetBan m_NetBan;

	std::unordered_map<int, std::unique_ptr<ProxyClient>> m_ProxyClients;
	std::unordered_map<int, int> m_FakeToRealClientMap;

	static std::atomic<int> m_NextFakeClientId;
};

class ProxyClient {
public:
	ProxyClient(TeeBridge* bridge, int realClientId, const NETADDR& targetAddr);
	~ProxyClient();

	void ConnectToServer();
	void SendToServer(CNetChunk chunk);
	bool RecvFromServer(CNetChunk& chunk, SECURITY_TOKEN token);
	void Update();

	int GetRealClientId() const { return m_RealClientId; }

private:
	TeeBridge* m_Bridge;
	int m_RealClientId;
	CNetClient m_Client;
	NETADDR m_TargetAddr;
	CUuid m_ConnectionId;
	char m_aPassword[128];
	bool m_Sixup;
};
