#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdarg.h>

#define BUF_SIZE 1024
#define SERVER_PORT 5208 // Puerto de escucha del servidor
#define IP "10.0.2.15"

void send_msg(int sock);
void recv_msg(int sock);
void send_command(int sock, const std::string &command);
int output(const char *arg, ...);
int error_output(const char *arg, ...);
void error_handling(const std::string &message);

std::string name = "DEFAULT";
std::string msg;

int main(int argc, const char **argv, const char **envp)
{
    int sock;
    struct sockaddr_in serv_addr;

    if (argc != 2)
    {
        error_output("Usage : %s <Name> \n", argv[0]);
        exit(1);
    }

    name = "[" + std::string(argv[1]) + "]";

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1)
    {
        error_handling("socket() failed!");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(IP);
    serv_addr.sin_port = htons(SERVER_PORT);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        error_handling("connect() failed!");
    }

    // Envía el nombre del cliente al servidor para validación
    std::string my_name = "#new client:" + std::string(argv[1]);
    send(sock, my_name.c_str(), my_name.length() + 1, 0);

    // Espera a que el hilo de envío y recepción terminen
    std::thread snd(send_msg, sock);
    std::thread rcv(recv_msg, sock);

    snd.join();
    rcv.join();

    close(sock);

    return 0;
}

void send_msg(int sock)
{
    while (1)
    {
        getline(std::cin, msg);
        if (msg == "Quit" || msg == "quit")
        {
            close(sock);
            exit(0);
        }
        else if (msg == "status")
        {
            send_command(sock, "#status");
        }
        else if (msg == "list")
        {
            send_command(sock, "#list");
        }
        else if (msg == "help")
        {
            output("1. Chatear con todos los usuarios (broadcasting).\n"
                   "2. Enviar y recibir mensajes directos, privados, aparte del chat general.\n"
                   "3. Cambiar de status.\n"
                   "4. Listar los usuarios conectados al sistema de chat.\n"
                   "5. Desplegar información de un usuario en particular.\n"
                   "6. Ayuda.\n"
                   "7. Salir.\n");
        }
        else
        {
            std::string name_msg = name + " " + msg;
            send(sock, name_msg.c_str(), name_msg.length() + 1, 0);
        }
    }
}

void recv_msg(int sock)
{
    char name_msg[BUF_SIZE + name.length() + 1];
    while (1)
    {
        int str_len = recv(sock, name_msg, BUF_SIZE + name.length() + 1, 0);
        if (str_len == -1)
        {
            error_handling("recv() failed!");
        }
        else if (str_len == 0) // Si el cliente se desconecta
        {
            error_output("El servidor ha cerrado la conexión.\n");
            close(sock);
            exit(0);
        }
        else
        {
            std::cout << std::string(name_msg) << std::endl;
        }
    }
}

void send_command(int sock, const std::string &command)
{
    send(sock, command.c_str(), command.length() + 1, 0);
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
