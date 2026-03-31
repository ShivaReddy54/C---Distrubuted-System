#include <iostream>
#include <sstream>
#include <vector>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

vector<int> server_ports = {5001, 5002};

int hash_key(const string& key){
    int hash = 0;
    for(char c: key) hash = (hash * 31 + c) % 100000;
    return hash;
}

string forward_request(string request, int port){
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr));

    send(sock, request.c_str(), request.size(), 0);

    char buffer[1024] = {0};
    recv(sock, buffer, 1024, 0);

    closesocket(sock);
    return string(buffer);
}

int main(){
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    cout << "Router Started... \n";
    
    while(1){
        string input;
        getline(cin, input);

        if(input.empty()) continue;

        stringstream ss(input);
        string command, key;
        ss >> command >> key;

        int hash = hash_key(key);
        int server_index = hash % server_ports.size();

        int target_port = server_ports[server_index];

        cout << "Routing to port: " << target_port << endl;

        string response = forward_request(input, target_port);
        cout << "Response: " << response << "\n";
    }

    WSACleanup();

    return 0;
}