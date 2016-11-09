#include "SelectNetEventManager.h"
#include <stdio.h>
#include "socketlib/socketlib.h"
SelectNetEventManager::SelectNetEventManager() {

}
SelectNetEventManager::~SelectNetEventManager() {

}
void SelectNetEventManager::run() {
	
	struct timeval timeout;

    memset(&timeout,0,sizeof(struct timeval));
    timeout.tv_usec = 16000;

    int hsock = setup_fdset();
    if(Socket::select(hsock, &m_fdset, NULL, NULL, &timeout) < 0)
    	return;

	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while(it != m_net_drivers.end()) {
		INetDriver *driver = *it;
		if(FD_ISSET(driver->getListenerSocket(), &m_fdset)) {
			driver->tick(&m_fdset);
		} else {
			driver->think(&m_fdset);
		}
		it++;
	}
}
int SelectNetEventManager::setup_fdset() {
	FD_ZERO(&m_fdset);
	int hsock = -1;
	std::vector<INetDriver *>::iterator it = m_net_drivers.begin();
	while(it != m_net_drivers.end()) {
		INetDriver *driver = *it;
		//add listen socket to fd
		int sd = driver->getListenerSocket();
		if(sd > hsock)
			hsock = sd;
		FD_SET(sd, &m_fdset);

		//add clients to fd
		hsock = driver->setup_fdset(&m_fdset);
		it++;
	}
	return ++hsock;
}