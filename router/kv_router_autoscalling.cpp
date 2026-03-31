#include <iostream>
#include <sstream>
#include <vector>
#include <queue>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// ================= CONFIG =================
const int MIN_SERVERS = 2;
const int MAX_SERVERS = 4;

const int SCALE_UP_THRESHOLD = 50;
const int SCALE_DOWN_THRESHOLD = 10;

const int WORKER_THREADS = 32;

// ==========================================

// Queue
queue<SOCKET> task_queue;
CRITICAL_SECTION queue_lock;

// Server struct
struct Server {
    int port;
    PROCESS_INFORMATION proc;
    bool healthy;
};

vector<Server> servers;

// ==========================================

// Hash
int hash_key(const string& key) {
    int hash = 0;
    for (char c : key)
        hash = (hash * 31 + c) % 100000;
    return hash;
}

// ==========================================

// Start KV Server (FIXED)
void start_kv_server(int port) {
    string cmd = "../server/server.exe " + to_string(port);

    vector<char> buffer(cmd.begin(), cmd.end());
    buffer.push_back('\0');

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(
        NULL,
        buffer.data(),
        NULL, NULL, FALSE,
        CREATE_NEW_CONSOLE,
        NULL, NULL,
        &si, &pi
    )) {
        Server s;
        s.port = port;
        s.proc = pi;
        s.healthy = true;

        servers.push_back(s);

        cout << "[AUTO SCALE] Started KV server on port " << port << endl;
    } else {
        cout << "Failed to start server on port " << port
             << " Error: " << GetLastError() << endl;
    }
}

// Stop KV Server
void stop_kv_server() {
    if (servers.size() <= MIN_SERVERS) return;

    Server s = servers.back();

    TerminateProcess(s.proc.hProcess, 0);
    CloseHandle(s.proc.hProcess);
    CloseHandle(s.proc.hThread);

    cout << "[AUTO SCALE] Stopped KV server on port " << s.port << endl;

    servers.pop_back();
}

// ==========================================

// Health Check
bool is_server_healthy(int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    bool ok = (connect(sock, (sockaddr*)&addr, sizeof(addr)) >= 0);

    closesocket(sock);
    return ok;
}

// Health Monitor
DWORD WINAPI health_monitor(LPVOID param) {
    while (true) {
        for (auto& s : servers) {
            bool h = is_server_healthy(s.port);

            if (!h && s.healthy) {
                cout << "[HEALTH] DOWN: " << s.port << endl;
                s.healthy = false;
            }
            else if (h && !s.healthy) {
                cout << "[HEALTH] RECOVERED: " << s.port << endl;
                s.healthy = true;
            }
        }
        Sleep(2000);
    }
    return 0;
}

// ==========================================

// Auto Scale
void auto_scale() {
    int size;

    EnterCriticalSection(&queue_lock);
    size = task_queue.size();
    LeaveCriticalSection(&queue_lock);

    // SCALE UP
    if (size > SCALE_UP_THRESHOLD && servers.size() < MAX_SERVERS) {
        int new_port = 5001 + servers.size();
        start_kv_server(new_port);
    }

    // SCALE DOWN
    if (size < SCALE_DOWN_THRESHOLD && servers.size() > MIN_SERVERS) {
        stop_kv_server();
    }
}

// ==========================================

// Get Active Servers
vector<int> get_active_ports() {
    vector<int> active;

    for (auto& s : servers) {
        if (s.healthy) {
            active.push_back(s.port);
        }
    }

    return active;
}

// ==========================================

// Forward Request (WITH DEBUG)
string forward_request(const string& request, int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "Connect failed to port " << port << endl;
        closesocket(sock);
        return "";
    }

    send(sock, request.c_str(), request.size(), 0);

    char buffer[1024] = {0};
    int bytes = recv(sock, buffer, 1024, 0);

    closesocket(sock);

    if (bytes <= 0) {
        cout << "No response from port " << port << endl;
        return "";
    }

    return string(buffer);
}

// ==========================================

//  Process Client (CRITICAL FIX)
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

    string final_response = "ERROR\n";

    if (!key.empty()) {
        vector<int> active = get_active_ports();

        cout << "Active servers: " << active.size() << endl;

        if (!active.empty()) {
            int port = active[hash_key(key) % active.size()];

            cout << "Routing to port: " << port << endl;

            string response = forward_request(input, port);

            if (!response.empty()) {
                final_response = response;
            }
        } else {
            final_response = "NO_SERVER\n";
        }
    }

    send(client_socket, final_response.c_str(), final_response.size(), 0);

    closesocket(client_socket);
}

// ==========================================

// Worker Thread
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

// ==========================================

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // Start initial servers
    for (int i = 0; i < MIN_SERVERS; i++) {
        start_kv_server(5001 + i);
    }

    InitializeCriticalSection(&queue_lock);

    // Start health monitor
    CreateThread(NULL, 0, health_monitor, NULL, 0, NULL);

    // Start workers
    for (int i = 0; i < WORKER_THREADS; i++) {
        CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
    }

    // Router socket
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(5000);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, SOMAXCONN);

    cout << "Router running on port 5000\n";

    while (true) {
        SOCKET client_socket = accept(server_fd, NULL, NULL);

        EnterCriticalSection(&queue_lock);
        task_queue.push(client_socket);
        LeaveCriticalSection(&queue_lock);

        auto_scale();
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}