#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

int router_port = 5000;
int requests_per_client = 10;


LONG total_requests_sent = 0;
LONG total_success = 0;
LONG total_failed = 0;

LONG set_success = 0;
LONG get_success = 0;

// Send request and classify result
void send_request(const string& request, bool is_set) {
    InterlockedIncrement(&total_requests_sent);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        InterlockedIncrement(&total_failed);
        return;
    }

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(router_port);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // connection failed
    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        InterlockedIncrement(&total_failed);
        closesocket(sock);
        return;
    }

    send(sock, request.c_str(), request.size(), 0);

    char buffer[1024] = {0};
    int bytes = recv(sock, buffer, 1024, 0);

    if (bytes <= 0) {
        InterlockedIncrement(&total_failed);
    } else {
        string response(buffer);

        // success = any valid non-empty response
        if (!response.empty()) {
            InterlockedIncrement(&total_success);

            if (is_set) {
                InterlockedIncrement(&set_success);
            } else {
                InterlockedIncrement(&get_success);
            }
        } else {
            InterlockedIncrement(&total_failed);
        }
    }

    closesocket(sock);
}

// One client thread
DWORD WINAPI client_task(LPVOID param) {
    int client_id = (int)param;

    for (int i = 0; i < requests_per_client; i++) {
        string key = "user:" + to_string(client_id) + "_" + to_string(i);

        string set_req = "SET " + key + " value" + to_string(i);
        send_request(set_req, true);

        string get_req = "GET " + key;
        send_request(get_req, false);
    }

    return 0;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    int num_clients = 1000;  // increase gradually: 100 → 1000 → 5000

    HANDLE* threads = new HANDLE[num_clients];

    auto start = chrono::high_resolution_clock::now();

    // spawn clients
    for (int i = 0; i < num_clients; i++) {
        threads[i] = CreateThread(
            NULL,
            0,
            client_task,
            (LPVOID)i,
            0,
            NULL
        );
    }

    // wait in batches (max 64)
    int batch_size = 64;
    for (int i = 0; i < num_clients; i += batch_size) {
        int current_batch = min(batch_size, num_clients - i);

        WaitForMultipleObjects(
            current_batch,
            &threads[i],
            TRUE,
            INFINITE
        );
    }

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> duration = end - start;

    cout << "=============================\n";
    cout << "Total Clients: " << num_clients << endl;
    cout << "Requests per Client: " << requests_per_client << endl;

    cout << "Total Requests Sent: " << total_requests_sent << endl;

    cout << "✅ Total Success: " << total_success << endl;
    cout << "   SET Success: " << set_success << endl;
    cout << "   GET Success: " << get_success << endl;

    cout << "❌ Total Failed: " << total_failed << endl;

    cout << "Time Taken: " << duration.count() << " sec\n";

    cout << "Throughput: "
         << (total_success / duration.count())
         << " req/sec (SUCCESS ONLY)\n";

    cout << "Success Rate: "
         << (100.0 * total_success / total_requests_sent)
         << " %\n";

    cout << "=============================\n";

    delete[] threads;
    WSACleanup();
    return 0;
}