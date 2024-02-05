#define _CRT_SECURE_NO_WARNINGS                 // turns of deprecated warnings
#define _WINSOCK_DEPRECATED_NO_WARNINGS 
#include <iostream>
#include "Server.h"
#include <string>

void StartServer(void);
void RegisterClient(void);

// server parameters
uint16_t port = 0;
int numConnections = 0;
int sendCapacity = 0;

// console command parameter
char consoleCommand = '~';

bool serverRunning = false;


Server server = Server();

int main() {

    WSADATA wsadata;
    WSAStartup(WINSOCK_VERSION, &wsadata);

    int choice = 0;

    do {
        std::cout << "Welcome to the Server Rafael's Server Chat!" << std::endl;
        std::cout << "1. Start Server (start listening for connections)" << std::endl;
        std::cout << "2. Shutdown Server and Application." << std::endl;
        std::cin >> choice;
    } while (choice != 1 && choice != 2);

    if (choice == 1) {
        StartServer();
    } 

    if (choice == 2) {
        return WSACleanup();
    }

    while (serverRunning) {
        // sending console command and max num of connections
        server.serverRun(numConnections, consoleCommand, sendCapacity);
    }

    return 0;
}

void StartServer(void) {
    
    std::cout << "Please specify the port you would like init()" << std::endl;
    std::cin >> port;

    server.init(port);

    
    if (server.status == SUCCESS) {
        std::cout << "Success on init() the server!" << std::endl;
        serverRunning = true;

        std::cout << "How many connections would like in the server" << std::endl;
        std::cin >> numConnections;
        // check if it is num, if it is -> next

        std::cout << "Please enter the message capacity (max 255)" << std::endl;
        std::cin >> sendCapacity;

        std::cout << "Please enter the command character desired! (example: ~, /, !)" << std::endl;
        std::cin >> consoleCommand;

    }
    else {
        std::cout << "Failure on init() the server!" << std::endl;
    }
}

// isInteger method to validated user input will be necessary later on