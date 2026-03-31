#include <iostream>
#include <unordered_map>
#include <sstream>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

unordered_map<string, string> store; // key - value store

void handle_client(SOCKET client_socket, int port){
    char buffer[1024] = {0};
    recv(client_socket, buffer, 1024, 0);

    string input(buffer);
    if(input.empty()){
        closesocket(client_socket);
        return;
    }

    cout << "Handled request on port: " << port << "\n";
    stringstream ss(input);

    string command, key, value;
    ss >> command >> key;

    if(command == "SET"){
        ss >> value;
        store[key] = value;
        string response = "OK\n";
        send(client_socket, response.c_str(), response.size(), 0);
    }
    else if(command == "GET"){
        if(store.find(key) != store.end()){
            string response = store[key] + "\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }
        else{
            string response = "NOT_FOUND\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }
    }

    closesocket(client_socket);
}

int main(int argc, char* argv[]){
    int port = stoi(argv[1]);
    if(argc < 2){
        cout << "Usage: kv_server " << port << "\n";
        return 1;
    }


    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    bind(server_fd, (sockaddr*)& address, sizeof(address));
    listen(server_fd, 5);

    cout << "KV Server running on port " << port << endl;

    while(1){
        SOCKET client_socket = accept(server_fd, NULL, NULL);
        handle_client(client_socket, port);
    }

    closesocket(server_fd);
    WSACleanup();

    return 0;
}