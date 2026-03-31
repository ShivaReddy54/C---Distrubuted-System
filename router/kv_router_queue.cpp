#include <iostream>
#include <sstream>
#include <vector>
#include <winsock2.h>
#include <windows.h>
#include <queue>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

vector<int> server_ports = {5001, 5002};

// Queue
queue<SOCKET> task_queue;
CRITICAL_SECTION queue_lock;

// 🔥 Hash
int hash_key(const string& key) {
    int hash = 0;
    for (char c : key)
        hash = (hash * 31 + c) % 100000;
    return hash;
}

// Forward request
string forward_request(const string& request, int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        closesocket(sock);
        return "";
    }

    send(sock, request.c_str(), request.size(), 0);

    char buffer[1024] = {0};
    int bytes = recv(sock, buffer, 1024, 0);

    closesocket(sock);

    if (bytes <= 0) return "";
    return string(buffer);
}

// Process client
void process_client(SOCKET client_socket) {
    char buffer[1024] = {0};
    int bytes = recv(client_socket, buffer, 1024, 0);

    if (bytes <= 0) {
        closesocket(client_socket);
        return;
    }

    string input(buffer);

    stringstream ss(input);
    string command, key;
    ss >> command >> key;

    if (key.empty()) {
        closesocket(client_socket);
        return;
    }

    int port = server_ports[hash_key(key) % server_ports.size()];

    string response = forward_request(input, port);

    if (!response.empty()) {
        send(client_socket, response.c_str(), response.size(), 0);
    }

    closesocket(client_socket);
}

// Worker thread
DWORD WINAPI worker_thread(LPVOID param) {
    while (true) {
        SOCKET client_socket = INVALID_SOCKET;

        EnterCriticalSection(&queue_lock);

        if (!task_queue.empty()) {
            client_socket = task_queue.front();
            task_queue.pop();
        }

        LeaveCriticalSection(&queue_lock);

        if (client_socket != INVALID_SOCKET) {
            process_client(client_socket);
        } else {
            Sleep(0);
        }
    }
    return 0;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(5000);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, SOMAXCONN);

    cout << "Router (Thread Pool FIXED) on port 5000\n";

    InitializeCriticalSection(&queue_lock);

    int num_workers = 8;
    for (int i = 0; i < num_workers; i++) {
        CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
    }

    while (true) {
        SOCKET client_socket = accept(server_fd, NULL, NULL);

        EnterCriticalSection(&queue_lock);
        task_queue.push(client_socket);
        LeaveCriticalSection(&queue_lock);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}