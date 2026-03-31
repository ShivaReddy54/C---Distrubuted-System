#include <iostream>
#include <sstream>
#include <vector>
#include <queue>
#include <winsock2.h>
#include <windows.h>
#include <chrono>
#include <atomic>   

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace std::chrono;

// ================= CONFIG =================
const int MIN_SERVERS = 2;
const int MAX_SERVERS = 4;

const int SCALE_UP_THRESHOLD = 50;
const int SCALE_DOWN_THRESHOLD = 10;

const int WORKER_THREADS = 32;

// ================= STATS =================
atomic<long long> total_requests(0);
atomic<long long> rate_limited(0);
atomic<long long> success_requests(0);
atomic<long long> failed_requests(0);

// RATE LIMIT
class TokenBucket {
private:
    int capacity;
    double tokens;
    double refill_rate;
    steady_clock::time_point last_refill;
    CRITICAL_SECTION lock;

public:
    TokenBucket(int cap, double rate)
        : capacity(cap), tokens(cap), refill_rate(rate) {
        last_refill = steady_clock::now();
        InitializeCriticalSection(&lock);
    }

    bool allow_request() {
        EnterCriticalSection(&lock);

        auto now = steady_clock::now();
        double elapsed = duration<double>(now - last_refill).count();

        tokens = min((double)capacity, tokens + elapsed * refill_rate);
        last_refill = now;

        if (tokens >= 1.0) {
            tokens -= 1.0;
            LeaveCriticalSection(&lock);
            return true;
        }

        LeaveCriticalSection(&lock);
        return false;
    }
};

TokenBucket rate_limiter(1000, 500);

// ==========================================

queue<SOCKET> task_queue;
CRITICAL_SECTION queue_lock;

struct Server {
    int port;
    PROCESS_INFORMATION proc;
    bool healthy;
};

vector<Server> servers;

// ==========================================

int hash_key(const string& key) {
    int hash = 0;
    for (char c : key)
        hash = (hash * 31 + c) % 100000;
    return hash;
}

// ==========================================

void start_kv_server(int port) {
    string cmd = "../server/server.exe " + to_string(port);

    vector<char> buffer(cmd.begin(), cmd.end());
    buffer.push_back('\0');

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessA(NULL, buffer.data(), NULL, NULL, FALSE,
        CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {

        servers.push_back({port, pi, true});
        cout << "[AUTO SCALE] Started server " << port << endl;
    } else {
        cout << "Failed start " << port << " err=" << GetLastError() << endl;
    }
}

// ==========================================

vector<int> get_active_ports() {
    vector<int> active;
    for (auto& s : servers)
        if (s.healthy) active.push_back(s.port);
    return active;
}

// ==========================================

string forward_request(const string& req, int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        closesocket(sock);
        return "";
    }

    send(sock, req.c_str(), req.size(), 0);

    char buffer[1024] = {0};
    int bytes = recv(sock, buffer, 1024, 0);

    closesocket(sock);

    if (bytes <= 0) return "";
    return string(buffer);
}

// ==========================================

void process_client(SOCKET client_socket) {
    char buffer[1024] = {0};
    int bytes = recv(client_socket, buffer, 1024, 0);

    if (bytes <= 0) {
        closesocket(client_socket);
        failed_requests++;   
        return;
    }

    string input(buffer);
    stringstream ss(input);
    string cmd, key;
    ss >> cmd >> key;

    string response = "ERROR\n";

    auto active = get_active_ports();

    if (!active.empty() && !key.empty()) {
        int port = active[hash_key(key) % active.size()];
        string res = forward_request(input, port);

        if (!res.empty()) {
            response = res;
            success_requests++;
        } else {
            failed_requests++;
        }
    } else {
        response = "NO_SERVER\n";
        failed_requests++;
    }

    send(client_socket, response.c_str(), response.size(), 0);
    closesocket(client_socket);
}

// ==========================================

DWORD WINAPI worker(LPVOID) {
    while (true) {
        SOCKET s = INVALID_SOCKET;

        EnterCriticalSection(&queue_lock);
        if (!task_queue.empty()) {
            s = task_queue.front();
            task_queue.pop();
        }
        LeaveCriticalSection(&queue_lock);

        if (s != INVALID_SOCKET)
            process_client(s);
        else
            Sleep(0);
    }
}

// ==========================================
// STATS THREAD (NEW)
DWORD WINAPI stats_printer(LPVOID) {
    while (true) {
        Sleep(3000);

        long long total = total_requests.load();
        long long rl = rate_limited.load();
        long long success = success_requests.load();
        long long failed = failed_requests.load();

        EnterCriticalSection(&queue_lock);
        int qsize = task_queue.size();
        LeaveCriticalSection(&queue_lock);

        cout << "\n========= ROUTER STATS =========\n";
        cout << "Total Requests   : " << total << endl;
        cout << "Success          : " << success << endl;
        cout << "Failed           : " << failed << endl;
        cout << "Rate Limited     : " << rl << endl;
        cout << "Queue Size       : " << qsize << endl;

        if (total > 0) {
            cout << "Success Rate     : "
                 << (100.0 * success / total) << " %\n";
        }

        cout << "================================\n";
    }
}

// ==========================================

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    InitializeCriticalSection(&queue_lock);

    // start initial servers
    for (int i = 0; i < MIN_SERVERS; i++)
        start_kv_server(5001 + i);

    // worker threads
    for (int i = 0; i < WORKER_THREADS; i++)
        CreateThread(NULL, 0, worker, NULL, 0, NULL);

    // START STATS THREAD (NEW)
    CreateThread(NULL, 0, stats_printer, NULL, 0, NULL);

    // router socket
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(5000);

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, SOMAXCONN);

    cout << "Router running on 5000\n";

    while (true) {
        SOCKET client_socket = accept(server_fd, NULL, NULL);

        total_requests++;

        // RATE LIMIT HERE
        if (!rate_limiter.allow_request()) {
            rate_limited++;

            string msg = "RATE_LIMITED\n";
            send(client_socket, msg.c_str(), msg.size(), 0);
            closesocket(client_socket);
            continue;
        }

        EnterCriticalSection(&queue_lock);
        task_queue.push(client_socket);
        LeaveCriticalSection(&queue_lock);
    }
}