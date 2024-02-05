#define _CRT_SECURE_NO_WARNINGS 
#define _WINSOCK_DEPRECATED_NO_WARNINGS // this is for ntoa
#include "Server.h"
#include <winsock2.h>
#include <iostream>
#include <fstream>       // For file operations
#include <sstream>       // For string stream operations
#include <string>  
#include <unordered_map> // for the hashmap
#include <ctime>



#pragma comment(lib,"Ws2_32.lib")


int Server::init(uint16_t port)
{
	WSADATA wsaData;
	int result;

	serverPort = port;

	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0)
	{
		std::cerr << "WSAStartup failed: " << result << std::endl;
		return SETUP_ERROR;
	}

	listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSocket == INVALID_SOCKET)
	{
		std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
		WSACleanup();
		return SETUP_ERROR;
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(port);

	result = bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if (result == SOCKET_ERROR)
	{
		std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
		closesocket(listenSocket);
		WSACleanup();
		return BIND_ERROR;
	}

	result = listen(listenSocket, SOMAXCONN);
	if (result == SOCKET_ERROR)
	{
		std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
		closesocket(listenSocket);
		WSACleanup();
		return CONNECT_ERROR;
	}

	// we are grabbing server ip here to be able to broadcast!
	int addrLength = sizeof(serverAddr);
	if (getsockname(listenSocket, (SOCKADDR*)&serverAddr, &addrLength) != SOCKET_ERROR)
	{
		
		serverIP[INET_ADDRSTRLEN];
		strcpy(serverIP, inet_ntoa(serverAddr.sin_addr));

		// printing the ip for logging/testing purposes
		std::cout << "Server IP: " << serverIP << std::endl;
	}

	return SUCCESS;
}


void Server::serverRun(int numConnections, char commandChar, int sendCap) {

	sendCapacity = sendCap;
	fileName = getDateTime() + "_chatlog.txt";

	FD_ZERO(&masterSet);
	FD_SET(listenSocket, &masterSet);

	unsigned int broadcastInterval = 5000; // broadcast every 5000 iterations, i wonder if i would eventually get out of int range (uint is 4294967295), should 
										   // be kinda fast... right?
	unsigned int counter = 0;


	while (true) {
		fd_set readySet = masterSet;

		timeval timeout = { 0, 0 };

		int readyFD = select(0, &readySet, NULL, NULL, &timeout);

		if (readyFD == SOCKET_ERROR) {
			std::cerr << "Select failed with error: " << WSAGetLastError() << std::endl;
			break;
		}

		// Perform the broadcast at intervals of 5000 loops. when it hits, reset the number to 0 
		if (counter >= broadcastInterval) {
			BroadcastServerInfo(); 
			counter = 0; // resetting the counter so it wouldnt run out of range
		}

		counter++;

		for (u_int i = 0; i < readySet.fd_count; i++) {
			SOCKET sock = readySet.fd_array[i];
			if (FD_ISSET(sock, &readySet)) {
				if (sock == listenSocket) {

					// new connection
					sockaddr_in clientAddress;
					int clientAddressLen = sizeof(clientAddress);
					SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddress, &clientAddressLen);
					if (clientSocket == INVALID_SOCKET) {
						std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
						continue;
					}
					
					// check if we have reached maximum number of connections
					if (masterSet.fd_count - 1 < numConnections) {
						FD_SET(clientSocket, &masterSet);
						std::cout << "New connection accepted." << std::endl;
					}
					else {
						std::cerr << "Maximum number of connections reached. New connection refused." << std::endl;
						closesocket(clientSocket);
					}
				}
				else {
					char buffer[1024];
					ZeroMemory(buffer, sizeof(buffer));

					//int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
					int bytesReceived = readMessage(sock, buffer, sendCapacity);

					if (bytesReceived > 0) {

						// currently working as intended.
						if (buffer[0] == commandChar) {
							ClientCommands(buffer, sock);
							continue;
						}

						// if not logged, not able to type
						// find returns the end if none is find
						if (userSockets.find(sock) == userSockets.end()) {
							std::string msg = "[SERVER] Please Login before sending messages.";
							sendMessage(sock, const_cast<char*>(msg.c_str()), sendCapacity);
							continue;
						}

						for (u_int j = 0; j < masterSet.fd_count; j++) {
							SOCKET outSock = masterSet.fd_array[j];
							// Don't send the message back to the server socket or the sender
							if (outSock != listenSocket && outSock != sock) {

								// something is rcvd, save it in log
								std::string rcvdMessage = userSockets[sock] + ": " + buffer;

								if (!rcvdMessage.empty()) {
									if (rcvdMessage.size() > 3) {

										std::cout << rcvdMessage << std::endl;
										StartLog(rcvdMessage);
									}
								}
								
								int result = sendMessage(outSock, buffer, sendCapacity); // 255 is the max, user picks the value

							}
						}
					}
					else if (bytesReceived == 0) {
						// Connection is closing
						// 
						// add name maybe?
						std::cout << "Client disconnected." << std::endl;
						closesocket(sock);
						FD_CLR(sock, &masterSet);
					}
					else {
						// An error occurred
						std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
						closesocket(sock);
						FD_CLR(sock, &masterSet);
					}
				}
			}
		}
	}
}

void Server::WhisperClient(const std::string& command, SOCKET sock) {

	std::istringstream iss(command);
	std::string cmd, usernameDestination, whisperMessage;

	if (!std::getline(iss, cmd, ':')) {
		std::cerr << "Failed to read command." << std::endl;
		return;
	}

	// Read the username part
	if (!std::getline(iss, usernameDestination, ':')) {
		std::cerr << "Failed to read username." << std::endl;
		return;
	}

	if (!std::getline(iss, whisperMessage, ':')) {
		std::cerr << "Failed to read password." << std::endl;
		return;
	}

	SOCKET destSocket;
	// find if there is online user that is the usernameDestination
	// then we send it
	for (const auto& pair : userSockets) {
		if (pair.second == usernameDestination) {
			destSocket = pair.first; // Return the socket (the key) associated with the username
		}
	}

	sendMessage(destSocket, const_cast<char*>(whisperMessage.c_str()), sendCapacity);

}

void Server::BroadcastServerInfo() {
	

	//create socket
	SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udpSocket == INVALID_SOCKET) {
		std::cerr << "Error creating UDP socket: " << WSAGetLastError() << std::endl;
		return;
	}

	// setsockopt we are enabling broadcast
	BOOL broadcastPermission = true;
	if (setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastPermission, sizeof(broadcastPermission)) == SOCKET_ERROR) {
		std::cerr << "Error setting broadcast permission: " << WSAGetLastError() << std::endl;
		closesocket(udpSocket);
		return;
	}

	sockaddr_in broadcastAddr;
	memset(&broadcastAddr, 0, sizeof(broadcastAddr));
	broadcastAddr.sin_family = AF_INET;
	broadcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST); // broadcast address (default btw)
	broadcastAddr.sin_port = htons(serverPort); // the port we want to broadcast

	// Prepare the message to broadcast
	char broadcastMessage[256];
	snprintf(broadcastMessage, sizeof(broadcastMessage), "Server IP: %s, Port: %d", serverIP, serverPort);

	// broadcast
	if (sendto(udpSocket, broadcastMessage, strlen(broadcastMessage), 0, (sockaddr*)&broadcastAddr, sizeof(broadcastAddr)) == SOCKET_ERROR) {
		std::cerr << "Error sending broadcast: " << WSAGetLastError() << std::endl;
	}
	else {
		// std::cout << "Broadcast message sent." << std::endl; LMAO HAD TO REMOVE THIS, IT SENT 1500 MESSAGES PER SECOND
	}

	closesocket(udpSocket);
}



// literally equal to client
int Server::sendMessage(SOCKET sock, char* data, int32_t length)
{
	if (status == SHUTDOWN) {
		return DISCONNECT;
	}

	// If length is less than 0 or greater than 255, returns PARAMETER_ERROR.
	if (length < 0 || length > 255)
	{
		return PARAMETER_ERROR;
	}

	char* aux = new char[length + 1]; // aux arrayh 
	aux[0] = length;

	for (int32_t i = 1; i < length; i++) {
		aux[i] = data[i - 1];
	}

	if (length > 0) {
		aux[length] = '\0'; // fixes the aux array
	}

	if (send(sock, aux, length + 1, 0) < 1) // is either shutdown or disconnect < 1 
	{
		if (status != SHUTDOWN) { // status will only be shutdown on graceful disconnect
			return DISCONNECT;
		}
		else {
			return SHUTDOWN;
		}
	}

	delete[] aux;
	return SUCCESS;
}

int Server::readMessage(SOCKET sock, char* buffer, int32_t size)
{

	int bytesReceived = 0;
	int SizeOrError = 0;
	int messageLength = 0;
	recv(sock, (char*)&messageLength, sizeof(char), 0);

	if (messageLength > size)
	{
		return PARAMETER_ERROR; //same as client
	}

	if (messageLength < 1)
	{
		if (status != SHUTDOWN) {
			return DISCONNECT;
		}
		else {
			return SHUTDOWN;
		}
	}

	for (int32_t i = 0; i < size; i++) { // initialize buffer
		buffer[i] = '\0';
	}

	while (bytesReceived < messageLength)
	{
		SizeOrError = recv(sock, buffer + bytesReceived, messageLength - bytesReceived, 0);
		if (SizeOrError < 1)
		{
			if (status != SHUTDOWN) {
				return DISCONNECT;
			}
			else {
				return SHUTDOWN;
			}
		}
		bytesReceived += SizeOrError;
	}
	return bytesReceived;
}

// needs to deal with 4 commands currently.
// - getlist
// - whisper
// - register
// - login
// - exit
void Server::ClientCommands(char buffer[], SOCKET clientSocket) {

	std::string temp = "";

	for (int i = 0; i < strlen(buffer); i++)
	{
		if (i == 0) {
			continue;
		}
		temp = temp + buffer[i];
	}

	if (temp.empty()) {
		std::cerr << "[ClientCommands] Empty command string." << std::endl;
		return;
	}

	std::istringstream iss(temp);
	std::string firstWord;

	if (std::getline(iss, firstWord, ':')) {
		std::cout << "[ClientCommands] Username and Password successfully parsed." << std::endl;
	}
	else {
		std::cerr << "Error reading first word." << std::endl;
	}

	if (firstWord == "getlist" && userSockets.find(clientSocket) != userSockets.end()) {
		GetList(clientSocket);
	}
	else if (firstWord == "register") {

		// returns bool
		if (HandleRegisterCommand(temp, clientSocket)) {
			std::string message = "[SERVER] Registration Successful.";

			sendMessage(clientSocket, const_cast<char*>(message.c_str()), sendCapacity);
		}
		else {

			std::string message = "[SERVER] Registration Failed.";

			sendMessage(clientSocket, const_cast<char*>(message.c_str()), sendCapacity);
		}
	}
	else if (firstWord == "login" ) {
		
		// returns bool
		if (HandleLoginCommand(temp, clientSocket)) {
			std::string message = "[SERVER] Login Successful.";

			sendMessage(clientSocket, const_cast<char*>(message.c_str()), sendCapacity);
		}
		else {

			std::string message = "[SERVER] Login Failed.";

			sendMessage(clientSocket, const_cast<char*>(message.c_str()), sendCapacity);
		}
	} 
	else if (firstWord == "getlog" && userSockets.find(clientSocket) != userSockets.end()) {
		GetLog(clientSocket);
	}
	else if (firstWord == "exit" && userSockets.find(clientSocket) != userSockets.end()){
		OnClientDisconnect(clientSocket);
	}
	else if (firstWord == "send" && userSockets.find(clientSocket) != userSockets.end()) {
		WhisperClient(temp, clientSocket);
	}
}

// the list of logged users is already done; its the hashmap userSockets!
void Server::GetList(SOCKET sock) {

	if (userSockets.size() <= 0) {
		std::cerr << "[GetList] There are no users currently authenticated." << std::endl;
		return;
	}

	std::string firstMessage = "[SERVER] List of currently authenticated users: ";
	std::cerr << firstMessage << std::endl;
	std::string user;

	sendMessage(sock, const_cast<char*>(firstMessage.c_str()), sendCapacity);

	int index = 0;
	for (const auto& pair : userSockets) {
		index++;
		std::cerr << std::to_string(index) + ": " + pair.second << std::endl;
	
		user = std::to_string(index) + ". " + pair.second;
		sendMessage(sock, const_cast<char*>(user.c_str()), sendCapacity);
	}
}

void Server::StartLog(std::string logMessage) {

	std::string dateTime = getDateTime();
	
	std::ofstream file(fileName, std::ios::app);
	if (file.is_open()) {
		file << "[" << dateTime << "] " << logMessage << std::endl;
		file.close();
		std::cerr << "[StartLog] Successfully logged in chatLog.txt file. " << std::endl;
	}
	else {
		std::cerr << "[StartLog] Failed to log in chatLog.txt file. " << std::endl;
	}
}

void Server::GetLog(SOCKET clientSocket) {

	std::ifstream file(fileName); // Replace with your log file path
	std::string line;

	if (!file.is_open()) {
		std::cerr << "Error opening file." << std::endl;
		return;
	}
	
	std::string temp = "[SERVER]~getlog from this session: ";

	sendMessage(clientSocket, const_cast<char*>(temp.c_str()), sendCapacity);

	while (std::getline(file, line)) {
		sendMessage(clientSocket, const_cast<char*>(line.c_str()), sendCapacity); 
	}

	file.close();
}

std::string Server::getDateTime() {

	std::time_t now = std::time(nullptr);
	std::tm* now_tm = std::localtime(&now);

	char buffer[80];

	// year month day hour minute second
	std::strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", now_tm);

	return std::string(buffer);
}

bool Server::HandleRegisterCommand(const std::string& command, SOCKET sock) {
	std::istringstream iss(command);
	std::string cmd, username, password;

	credentials = LoadCredentials();

	//if (credentials.find(username) != credentials.end()) {
	//	std::cerr << "[HandleRegisterCommand] Register account failed; this username already exists." << std::endl;
	//	std::string temp = "[SERVER] Username already exists.";
	//	sendMessage(sock, const_cast<char*>(temp.c_str()), sendCapacity);

	//	return false;
	//}

	// Read the command part
	if (!std::getline(iss, cmd, ':')) {
		std::cerr << "Failed to read command." << std::endl;
		return false;
	}

	// Read the username part
	if (!std::getline(iss, username, ':')) {
		std::cerr << "Failed to read username." << std::endl;
		return false;
	}

	if (!std::getline(iss, password, ':')) {
		std::cerr << "Failed to read password." << std::endl;
		return false;
	}

	if (credentials.find(username) != credentials.end()) {
		std::cerr << "[HandleRegisterCommand] Register account failed; this username already exists." << std::endl;
		std::string temp = "[SERVER] Username already exists.";
		sendMessage(sock, const_cast<char*>(temp.c_str()), sendCapacity);

		return false;
	}

	if (cmd == "register") {
		// Save username and password to a file
		std::ofstream file("users.txt", std::ios::app); // Append mode
		if (file.is_open()) {
			file << username << " " << password << std::endl;
			file.close();
			std::cerr << "[HandleRegisterCommand] Successfully saved registration in users.txt file. " << std::endl;
			return true;
		}
		else {
			std::cerr << "[HandleRegisterCommand] Unable to open users.txt file. " << std::endl;
			return false;
		}
	}
}

bool Server::HandleLoginCommand(const std::string& command, SOCKET clientSocket) {

	std::istringstream iss(command);
	std::string cmd, username, password;


	credentials = LoadCredentials();

	// Read the command part
	if (!std::getline(iss, cmd, ':')) {
		std::cerr << "Failed to read command." << std::endl;
		return false;
	}

	// Read the username part
	if (!std::getline(iss, username, ':')) {
		std::cerr << "Failed to read username." << std::endl;
		return false;
	}

	if (!std::getline(iss, password, ':')) {
		std::cerr << "Failed to read password." << std::endl;
		return false;
	}

		for (const auto& pair : userSockets) {
			if (pair.second == username) {
				std::string temp = "[SERVER] This account is already logged in";
				sendMessage(clientSocket, const_cast<char*>(temp.c_str()), sendCapacity);
				return false;
			}
		}

	auto it = credentials.find(username);
	if (it != credentials.end() && it->second == password) {
		std::cerr << "[HandleLoginCommand] Login successful." << std::endl;

		userSockets[clientSocket] = username;

		return true; 
	}


	std::cerr << "[HandleLoginCommand] Login failed; No username/password combo found." << std::endl;
	return false; // no match found
}

// the list must be updated when someone connects AND disconnects
void Server::OnClientDisconnect(SOCKET clientSocket) {

	if (userSockets.find(clientSocket) != userSockets.end()) {
		userSockets.erase(clientSocket);

		std::string temp = "[SERVER] Succesfully logged out.";
		sendMessage(clientSocket, const_cast<char*>(temp.c_str()), sendCapacity);
	}
	else {
		std::string tempTwo = "[SERVER] You are not logged in.";
		sendMessage(clientSocket, const_cast<char*>(tempTwo.c_str()), sendCapacity);
	}
	
	FD_CLR(clientSocket, &masterSet);
	closesocket(clientSocket); // Close the socket
}

std::unordered_map<std::string, std::string> Server::LoadCredentials() {
	std::unordered_map<std::string, std::string> credentials;
	std::ifstream file("users.txt");
	std::string line, username, password;

	if (!file.is_open()) {
		std::cerr << "[LoadCredentials] Unable to open file." << std::endl;
		return credentials;
	}

	while (std::getline(file, line)) {
		std::istringstream iss(line);
		if (iss >> username >> password) {
			credentials[username] = password;
		}
	}

	return credentials;
}

void Server::stop()
{
	status = SHUTDOWN;

	shutdown(listenSocket, SD_BOTH);
	closesocket(listenSocket);

	shutdown(ComSocket, SD_BOTH);
	closesocket(ComSocket);

}