#include <iostream>
#include <unordered_map>
#include <sstream>
#include <winsock2.h>
#include <windows.h>
#include <queue>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Shared KV store
unordered_map<string, string> store;
CRITICAL_SECTION store_lock;

// Task queue
queue<SOCKET> task_queue;
CRITICAL_SECTION queue_lock;

// Process request
void process_client(SOCKET client_socket) {
    char buffer[1024] = {0};
    int bytes = recv(client_socket, buffer, 1024, 0);

    if (bytes <= 0) {
        closesocket(client_socket);
        return;
    }

    string input(buffer);
    stringstream ss(input);

    string command, key, value;
    ss >> command >> key;

    string response;

    if (command == "SET") {
        ss >> value;

        EnterCriticalSection(&store_lock);
        store[key] = value;
        LeaveCriticalSection(&store_lock);

        response = "OK\n";
    }
    else if (command == "GET") {
        EnterCriticalSection(&store_lock);

        if (store.find(key) != store.end()) {
            response = store[key] + "\n";
        } else {
            response = "NOT_FOUND\n";
        }

        LeaveCriticalSection(&store_lock);
    }
    else {
        response = "INVALID\n";
    }

    send(client_socket, response.c_str(), response.size(), 0);
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
            Sleep(0); // prevent CPU spinning
        }
    }
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

    cout << "KV Server (Thread Pool FIXED) on port " << port << endl;

    // Init locks
    InitializeCriticalSection(&queue_lock);
    InitializeCriticalSection(&store_lock);

    // Create worker threads
    int num_workers = 8;
    for (int i = 0; i < num_workers; i++) {
        CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
    }

    // Accept loop
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