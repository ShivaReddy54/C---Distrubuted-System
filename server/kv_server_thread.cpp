#include <iostream>
#include <unordered_map>
#include <sstream>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

unordered_map<string, string> store;

// 🔥 thread function
DWORD WINAPI handle_client(LPVOID param) {
    SOCKET client_socket = (SOCKET)param;

    char buffer[1024] = {0};
    int bytes = recv(client_socket, buffer, 1024, 0);

    if (bytes <= 0) {
        closesocket(client_socket);
        return 0;
    }

    string input(buffer);
    stringstream ss(input);

    string command, key, value;
    ss >> command >> key;

    string response;

    if (command == "SET") {
        ss >> value;
        store[key] = value;
        response = "OK\n";
    }
    else if (command == "GET") {
        if (store.find(key) != store.end()) {
            response = store[key] + "\n";
        } else {
            response = "NOT_FOUND\n";
        }
    }
    else {
        response = "INVALID_COMMAND\n";
    }

    send(client_socket, response.c_str(), response.size(), 0);

    closesocket(client_socket);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: kv_server <port>\n";
        return 1;
    }

    int port = stoi(argv[1]);

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, SOMAXCONN);

    cout << "KV Server running on port " << port << endl;

    while (true) {
        SOCKET client_socket = accept(server_fd, NULL, NULL);

        // 🔥 create new thread
        CreateThread(
            NULL,
            0,
            handle_client,
            (LPVOID)client_socket,
            0,
            NULL
        );
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}