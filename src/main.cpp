#include "base/logger.h"
#include "proxy/proxy.h"
#include "base/system.h"

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
	CNetBase::Init();

	NETADDR listenAddr = {NETTYPE_IPV4, {127,0,0,1}, 8303};
	NETADDR targetAddr = {NETTYPE_IPV4, {127,0,0,1}, 8304};

	TeeBridge proxy(listenAddr, targetAddr);
	proxy.Run();
	return 0;
}