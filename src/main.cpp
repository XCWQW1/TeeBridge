#include "base/logger.h"
#include "proxy/proxy.h"
#include "base/system.h"

static bool MyPacketHook(const void *pData, int size, NETADDR *pFrom, NETADDR *pTo, bool isFromClient)
{
	char FromStr[NETADDR_MAXSTRSIZE];
	char ToStr[NETADDR_MAXSTRSIZE];
	net_addr_str(pFrom, FromStr, sizeof(FromStr), true);
	net_addr_str(pTo, ToStr, sizeof(ToStr), true);

	log_info("hook", "Packet: %s -> %s, size: %d", FromStr, ToStr, size);

	// 示例：拦截特定 IP 的请求
	if (pFrom->ip[0] == 192 && pFrom->ip[1] == 168 && pFrom->ip[2] == 1 && pFrom->ip[3] == 100)
	{
		log_info("hook", "Blocked packet from 192.168.1.100");
		return true; // 丢弃包
	}

	return false; // 允许包通过
}

int main(int argc, const char **argv)
{
	CCmdlineFix CmdlineFix(&argc, &argv);
	log_set_global_logger_default();
	if(secure_random_init() != 0)
	{
		log_error("secure", "could not initialize secure RNG");
		return -1;
	}
	net_init();

	// 监听本地 8303，转发到 8304
	NETADDR ListenAddr, TargetAddr;
	net_addr_from_str(&ListenAddr, "127.0.0.1:8303");
	net_addr_from_str(&TargetAddr, "127.0.0.1:8304");

	TeeBridge proxy(ListenAddr, TargetAddr);
	// proxy.SetPacketHook(MyPacketHook); // 设置 hook
	proxy.Run();
	return 0;
}