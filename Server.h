#pragma once

//#include "platform.h"
#include "definitions.h"
#include <iostream>
#include <fstream>       // For file operations
#include <sstream>       // For string stream operations
#include <string> 
#include <unordered_map>

#define INET_ADDRSTRLEN 16 

class Server
{
private: 
	
	SOCKET ComSocket;

	// multiplexing
	SOCKET listenSocket = INVALID_SOCKET;
	fd_set masterSet;
	bool serverActive = false;

	// the hashmap that is the result of parsing the users.txt file 
	std::unordered_map<std::string, std::string> credentials;
	std::unordered_map<SOCKET, std::string> userSockets; // i am adding the sockets in this map with the credentials, so i can differ them

	int sendCapacity = 0;
	std::string fileName;

	uint16_t serverPort;

	char serverIP[INET_ADDRSTRLEN];


public:
	
	int status;

	int init(uint16_t port);

	void serverRun(int numConnections, char commandChar, int sendCap);
	int sendMessage(SOCKET sock, char* data, int32_t length);
	int readMessage(SOCKET sock, char* buffer, int32_t size);

	void ClientCommands(char buffer[], SOCKET clientSocket);

	// commands
	bool HandleRegisterCommand(const std::string& command, SOCKET sock);
	bool HandleLoginCommand(const std::string& command, SOCKET clientSocket);

	void WhisperClient(const std::string& command, SOCKET sock);

	std::unordered_map<std::string, std::string> LoadCredentials();
	void OnClientDisconnect(SOCKET clientSocket);

	void GetList(SOCKET sock);
	void DisplayInfo();

	void StartLog(std::string logMessage);
	std::string getDateTime();
	void GetLog(SOCKET clientSocket);

	// phase 3
	void BroadcastServerInfo(void);

	void stop();

};