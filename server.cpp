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
#define MAX_CLNT 256 // Número máximo de conexiones

void handle_clnt(int clnt_sock);
void send_msg(const std::string &msg);
int output(const char *arg, ...);
int error_output(const char *arg, ...);
void error_handling(const std::string &message);

int clnt_cnt = 0;
std::mutex mtx;
// Utilizar unordered_map para almacenar el nombre y el socket de cada cliente
std::unordered_map<std::string, int> clnt_socks;

int main(int argc, const char **argv, const char **envp)
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;

    // Crear un socket, con los siguientes parámetros:
    //   AF_INET: Utilizar IPv4
    //   SOCK_STREAM: Comunicación orientada a la conexión
    //   IPPROTO_TCP: Utilizar TCP
    serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serv_sock == -1)
    {
        error_handling("¡socket() falló!");
    }
    // Enlazar el socket a una dirección IP y puerto específicos.
    // Rellenar serv_addr con ceros (es una estructura sockaddr_in).
    memset(&serv_addr, 0, sizeof(serv_addr));
    // Configurar IPv4
    // Configurar la dirección IP
    // Configurar el puerto
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERVER_PORT);

    // Realizar el enlace
    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        error_handling("¡bind() falló!");
    }
    printf("El servidor se está ejecutando en el puerto %d\n", SERVER_PORT);
    // Poner el socket en estado de escucha, esperando las solicitudes de los clientes.
    if (listen(serv_sock, MAX_CLNT) == -1)
    {
        error_handling("¡error en listen()!");
    }

    while (1)
    { // Escuchar continuamente las solicitudes de los clientes
        clnt_addr_size = sizeof(clnt_addr);
        // Cuando no hay clientes conectados, accept() bloqueará la ejecución del programa hasta que un cliente se conecte.
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
        if (clnt_sock == -1)
        {
            error_handling("¡accept() falló!");
        }

        // Incrementar el contador de clientes
        mtx.lock();
        clnt_cnt++;
        mtx.unlock();

        // Crear un hilo para manejar al cliente
        std::thread th(handle_clnt, clnt_sock);
        th.detach();

        output("Cliente conectado IP: %s \n", inet_ntoa(clnt_addr.sin_addr));
    }
    close(serv_sock);
    return 0;
}

void handle_clnt(int clnt_sock)
{
    char msg[BUF_SIZE];
    int flag = 0;

    // Prefijo para la transmisión del nombre del cliente por primera vez
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
                        output("nombre del socket %d: %s\n", clnt_sock, name);
                        clnt_socks[name] = clnt_sock;
                    }
                    else
                    {
                        std::string error_msg = std::string(name) + " ya existe. ¡Por favor, salga e ingrese con otro nombre!";
                        send(clnt_sock, error_msg.c_str(), error_msg.length() + 1, 0);
                        mtx.lock();
                        clnt_cnt--;
                        mtx.unlock();
                        flag = 1;
                    }
                }
            }

            if (flag == 0)
                send_msg(std::string(msg));
        }
    }
    catch (const std::exception &e)
    {
        error_output("Se produjo una excepción en handle_clnt: %s\n", e.what());
    }

    // Cliente desconectado, eliminarlo del mapa
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
        std::string leave_msg = "El cliente " + name + " abandonó la sala de chat";
        send_msg(leave_msg);
        output("El cliente %s abandonó la sala de chat\n", name.c_str());
    }

    close(clnt_sock);
}

void send_msg(const std::string &msg)
{
    mtx.lock();
    // Formato para mensajes privados: [send_clnt] @recv_clnt message
    // Comprobar si hay @ después de [send_clnt]. Si hay, entonces es un mensaje privado.
    std::string pre = "@";
    int first_space = msg.find_first_of(" ");
    if (msg.compare(first_space + 1, 1, pre) == 0)
    {
        // Unicast
        // space es el espacio entre recv_clnt y el mensaje
        int space = msg.find_first_of(" ", first_space + 1);
        std::string receive_name = msg.substr(first_space + 2, space - first_space - 2);
        std::string send_name = msg.substr(1, first_space - 2);
        if (clnt_socks.find(receive_name) == clnt_socks.end())
        {
            // Si el usuario al que se envía el mensaje privado no existe
            std::string error_msg = "[error] no hay cliente con el nombre " + receive_name;
            send(clnt_socks[send_name], error_msg.c_str(), error_msg.length() + 1, 0);
        }
        else
        {
            send(clnt_socks[receive_name], msg.c_str(), msg.length() + 1, 0);
            send(clnt_socks[send_name], msg.c_str(), msg.length() + 1, 0);
        }
    }
    else
    {
        // Broadcast
        for (auto it = clnt_socks.begin(); it != clnt_socks.end(); it++)
        {
            send(it->second, msg.c_str(), msg.length() + 1, 0);
        }
    }
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
