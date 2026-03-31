#include <iostream>
#include <sstream>
#include <vector>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

vector<int> server_ports = {5001, 5002};

int hash_key(const string& key) {
    int hash = 0;
    for (char c : key) {
        hash = (hash * 31 + c) % 100000;
    }
    return hash;
}

string forward_request(const string& request, int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        closesocket(sock);
        return "ERROR: KV server unreachable\n";
    }

    send(sock, request.c_str(), request.size(), 0);

    char buffer[1024] = {0};
    recv(sock, buffer, 1024, 0);

    closesocket(sock);
    return string(buffer);
}

// thread function
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
    string command, key;
    ss >> command >> key;

    if (key.empty()) {
        string response = "ERROR: Invalid request\n";
        send(client_socket, response.c_str(), response.size(), 0);
        closesocket(client_socket);
        return 0;
    }

    int hash = hash_key(key);
    int server_index = hash % server_ports.size();
    int target_port = server_ports[server_index];

    cout << "[Router] Key: " << key << " → Routed to port: " << target_port << endl;

    string response = forward_request(input, target_port);

    send(client_socket, response.c_str(), response.size(), 0);

    closesocket(client_socket);
    return 0;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    int router_port = 5000;

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(router_port);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, SOMAXCONN);

    cout << "Router running on port " << router_port << endl;

    while (true) {
        SOCKET client_socket = accept(server_fd, NULL, NULL);

        // create thread
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