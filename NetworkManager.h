#pragma once
#undef UNICODE

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

class NetworkManager {
public:
	NetworkManager(std::string hostName, int port);
	NetworkManager::~NetworkManager();
	int initWinSock();
	int NetworkManager::acceptClient();
	int NetworkManager::sendData(unsigned char * buffer, int length);
	int NetworkManager::shutdownSocket();

private:
	SOCKET m_ClientSocket;
	std::string m_HostName;
	int m_Port;
};