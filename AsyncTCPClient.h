#ifndef __ASYNCTCPCLIENT_H
#define __ASYNCTCPCLIENT_H

typedef void (*AsyncTCPClientCallback)(TCPClient *client, int status);

class AsyncTCPClient : public TCPClient {
public:
	AsyncTCPClient();
	virtual ~AsyncTCPClient();

	virtual void asyncConnect(IPAddress ip, uint16_t port, AsyncTCPClientCallback callback, network_interface_t=0);

	virtual void asyncConnect(const char *host, uint16_t port, AsyncTCPClientCallback callback, network_interface_t=0);

	static void setup();
	static void loop();
};


#endif /* __ASYNCTCPCLIENT_H */

