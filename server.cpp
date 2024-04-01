#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <stdarg.h>

#define SERVER_PORT 5208 // Puerto de escucha
#define BUF_SIZE 1024
#define MAX_CLNT 256 // Máximo número de clientes

void handle_clnt(int clnt_sock);
void send_msg(int clnt_sock, const std::string &msg); 
void change_status(int clnt_sock, const std::string &status);
void list_users(int clnt_sock);
void show_user_info(int clnt_sock, const std::string &username);
int output(const char *arg, ...);
int error_output(const char *arg, ...);
void error_handling(const std::string &message);

int clnt_cnt = 0;
std::mutex mtx;
std::unordered_map<std::string, int> clnt_socks;

int main(int argc, const char **argv, const char **envp)
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;

    serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serv_sock == -1)
    {
        error_handling("socket() failed!");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        error_handling("bind() failed!");
    }

    printf("the server is running on port %d\n", SERVER_PORT);

    if (listen(serv_sock, MAX_CLNT) == -1)
    {
        error_handling("listen() error!");
    }

    while (1)
    {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
        if (clnt_sock == -1)
        {
            error_handling("accept() failed!");
        }

        mtx.lock();
        clnt_cnt++;
        mtx.unlock();

        std::thread th(handle_clnt, clnt_sock);
        th.detach();

        output("Connected client IP: %s \n", inet_ntoa(clnt_addr.sin_addr));
    }

    close(serv_sock);
    return 0;
}

void handle_clnt(int clnt_sock)
{
    char msg[BUF_SIZE];
    int flag = 0;

    char tell_name[13] = "#new client:";
    try
    {
        while (recv(clnt_sock, msg, sizeof(msg), 0) != 0)
        {
            if (std::strlen(msg) > std::strlen(tell_name))
            {
                char pre_name[13];
                std::strncpy(pre_name, msg, 12);
                pre_name[12] = '\0';
                if (std::strcmp(pre_name, tell_name) == 0)
                {
                    char name[20];
                    std::strcpy(name, msg + 12);
                    if (clnt_socks.find(name) == clnt_socks.end())
                    {
                        output("the name of socket %d: %s\n", clnt_sock, name);
                        clnt_socks[name] = clnt_sock;
                    }
                    else
                    {
                        std::string error_msg = std::string(name) + " exists already. Please quit and enter with another name!";
                        send(clnt_sock, error_msg.c_str(), error_msg.length() + 1, 0);
                        mtx.lock();
                        clnt_cnt--;
                        mtx.unlock();
                        flag = 1;
                    }
                }
            }

            if (flag == 0)
                send_msg(clnt_sock, std::string(msg));
        }
    }
    catch (const std::exception &e)
    {
        error_output("Exception occurred in handle_clnt: %s\n", e.what());
    }

    std::string name;
    mtx.lock();
    for (auto it = clnt_socks.begin(); it != clnt_socks.end(); ++it)
    {
        if (it->second == clnt_sock)
        {
            name = it->first;
            clnt_socks.erase(it->first);
            break;
        }
    }
    clnt_cnt--;
    mtx.unlock();

    if (flag == 0)
    {
        std::string leave_msg = "Client " + name + " left the chat room";
        send_msg(clnt_sock, leave_msg);
        output("Client %s left the chat room\n", name.c_str());
    }

    close(clnt_sock);
}

void send_msg(int clnt_sock, const std::string &msg) 
{
    mtx.lock();
    int first_space = msg.find_first_of(" ");
    if (first_space != std::string::npos)
    {
        std::string command = msg.substr(0, first_space);
        if (command == "#status")
        {
            std::string status = msg.substr(first_space + 1);
            change_status(clnt_sock, status);
        }
        else if (command == "#list")
        {
            list_users(clnt_sock);
        }
        else if (command == "#info")
        {
            std::string username = msg.substr(first_space + 1);
            show_user_info(clnt_sock, username);
        }
        else
        {
            // Broadcast message
            for (auto it = clnt_socks.begin(); it != clnt_socks.end(); it++)
            {
                send(it->second, msg.c_str(), msg.length() + 1, 0);
            }
        }
    }
    mtx.unlock();
}

void change_status(int clnt_sock, const std::string &status)
{
    mtx.lock();
    for (auto it = clnt_socks.begin(); it != clnt_socks.end(); ++it)
    {
        if (it->second == clnt_sock)
        {
            std::string username = it->first;
            output("Changing status of user '%s' to '%s'\n", username.c_str(), status.c_str());
            
            break;
        }
    }
    mtx.unlock();
}

void list_users(int clnt_sock)
{
    mtx.lock();
    std::string user_list = "Usuarios conectados:\n";
    for (const auto &pair : clnt_socks)
    {
        user_list += pair.first + "\n";
    }
    send(clnt_sock, user_list.c_str(), user_list.length() + 1, 0);
    mtx.unlock();
}

void show_user_info(int clnt_sock, const std::string &username)
{
    mtx.lock();
    std::string info_msg;
    if (clnt_socks.find(username) != clnt_socks.end())
    {
        info_msg = "Información de usuario '" + username + "':\n";
    
    }
    else
    {
        info_msg = "El usuario '" + username + "' no está conectado.\n";
    }
    send(clnt_sock, info_msg.c_str(), info_msg.length() + 1, 0);
    mtx.unlock();
}

int output(const char *arg, ...)
{
    int res;
    va_list ap;
    va_start(ap, arg);
    res = vfprintf(stdout, arg, ap);
    va_end(ap);
    return res;
}

int error_output(const char *arg, ...)
{
    int res;
    va_list ap;
    va_start(ap, arg);
    res = vfprintf(stderr, arg, ap);
    va_end(ap);
    return res;
}

void error_handling(const std::string &message)
{
    std::cerr << message << std::endl;
    exit(1);
}
