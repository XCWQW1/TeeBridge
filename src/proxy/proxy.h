#pragma once

#include "base/system.h"
#include <functional>
#include <unordered_map>

using UdpPacketHook = std::function<bool(const void *pData, int size, const NETADDR &from, const NETADDR &to, bool isFromClient)>;

class TeeBridge {
public:
    TeeBridge(const NETADDR &listenAddr, const NETADDR &targetAddr);
    ~TeeBridge();

    void Run();
    void SetPacketHook(const UdpPacketHook &hook);

private:
    NETADDR m_ListenAddr;
    NETADDR m_TargetAddr;

    NETSOCKET m_ProxySocket; // 监听客户端
    NETSOCKET m_ServerSocket; // 连接服务器

    UdpPacketHook m_PacketHook;

    void HandleClientPacket();  // 处理客户端发来的包
    void HandleServerPacket();  // 处理服务器返回的包
};
