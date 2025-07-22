#include "proxy.h"
#include "base/logger.h"
#include "base/system.h"

#include <iostream>
#include <memory>
#include <unordered_map>

using ClientMap = std::unordered_map<NETADDR, NETADDR, std::hash<NETADDR>>;

TeeBridge::TeeBridge(const NETADDR &listenAddr, const NETADDR &targetAddr)
    : m_ListenAddr(listenAddr), m_TargetAddr(targetAddr)
{
    // 创建监听 socket（客户端连接）
    m_ProxySocket = net_udp_create(m_ListenAddr);
    if (!m_ProxySocket)
    {
        log_error("proxy", "Failed to create proxy socket");
        return;
    }

    // 创建连接服务器的 socket（任意本地地址）
    NETADDR BindAddr;
    mem_zero(&BindAddr, sizeof(BindAddr));
    BindAddr.type = m_TargetAddr.type;
    m_ServerSocket = net_udp_create(BindAddr);
    if (!m_ServerSocket)
    {
        log_error("proxy", "Failed to create server socket");
        return;
    }

    char ListenStr[NETADDR_MAXSTRSIZE];
    char TargetStr[NETADDR_MAXSTRSIZE];
    net_addr_str(&m_ListenAddr, ListenStr, sizeof(ListenStr), true);
    net_addr_str(&m_TargetAddr, TargetStr, sizeof(TargetStr), true);
    log_info("proxy", "UDP Proxy started: %s -> %s", ListenStr, TargetStr);
}

TeeBridge::~TeeBridge()
{
    if(m_ProxySocket)
        net_udp_close(m_ProxySocket);
}

void TeeBridge::SetPacketHook(const UdpPacketHook &hook)
{
    m_PacketHook = hook;
}

void TeeBridge::Run()
{
    while (true)
    {
        HandleClientPacket();  // 监听客户端发来的数据
        HandleServerPacket();  // 监听服务器返回的数据

        thread_yield();
    }
}

void TeeBridge::HandleClientPacket()
{
    unsigned char buffer[1500];
    unsigned char *pData = buffer;
    NETADDR fromAddr;
    int size = net_udp_recv(m_ProxySocket, &fromAddr, &pData);

    if (size <= 0)
        return;

    log_info("proxy", "Client -> Proxy (size: %d)", size);

    if (m_PacketHook && m_PacketHook(buffer, size, fromAddr, m_TargetAddr, true))
    {
        log_info("proxy", "Packet dropped by hook");
        return;
    }

    // 维护客户端到服务器的映射
    static ClientMap clientMap;
    clientMap[fromAddr] = m_TargetAddr;

    // 转发到服务器
    if (net_udp_send(m_ServerSocket, &m_TargetAddr, buffer, size) != size)
    {
        log_error("proxy", "Failed to send to server");
    }
}

void TeeBridge::HandleServerPacket()
{
    unsigned char buffer[1500];
    unsigned char *pData = buffer;
    NETADDR fromAddr;
    int size = net_udp_recv(m_ServerSocket, &fromAddr, &pData);

    if (size <= 0)
        return;

    log_info("proxy", "Server -> Proxy (size: %d)", size);

    if (m_PacketHook && m_PacketHook(buffer, size, fromAddr, m_ListenAddr, false))
    {
        log_info("proxy", "Packet dropped by hook");
        return;
    }

    // 查找对应的客户端地址
    static ClientMap clientMap;
    auto it = clientMap.find(fromAddr);
    if (it != clientMap.end())
    {
        const NETADDR &clientAddr = it->second;

        // 转发给客户端
        if (net_udp_send(m_ProxySocket, &clientAddr, buffer, size) != size)
        {
            log_error("proxy", "Failed to send to client");
        }
    }
    else
    {
        log_warn("proxy", "No client found for server response");
    }
}
