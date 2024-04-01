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

#define SERVER_PORT 5208 // Puerto de escucha del servidor
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

    serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Crea el socket del servidor
    if (serv_sock == -1)
    {
        error_handling("socket() failed!"); // Maneja el error si falla la creación del socket
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        error_handling("bind() failed!"); // Maneja el error si falla la asignación de dirección y puerto al socket
    }

    printf("the server is running on port %d\n", SERVER_PORT); // Imprime un mensaje indicando que el servidor está en ejecución

    if (listen(serv_sock, MAX_CLNT) == -1)
    {
        error_handling("listen() error!"); // Maneja el error si falla la configuración del socket para aceptar conexiones entrantes
    }

    while (1)
    {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
        if (clnt_sock == -1)
        {
            error_handling("accept() failed!"); // Maneja el error si falla la aceptación de la conexión entrante
        }

        mtx.lock();
        clnt_cnt++; // Incrementa el contador de clientes
        mtx.unlock();

        std::thread th(handle_clnt, clnt_sock); // Crea un hilo para manejar al cliente
        th.detach();

        output("Connected client IP: %s \n", inet_ntoa(clnt_addr.sin_addr)); // Imprime un mensaje indicando la IP del cliente conectado
    }

    close(serv_sock); // Cierra el socket del servidor
    return 0;
}

// Función para manejar al cliente
void handle_clnt(int clnt_sock)
{
    char msg[BUF_SIZE];
    int flag = 0;

    char tell_name[13] = "#new client:"; // Prefijo para la transmisión del nombre del cliente por primera vez
    try
    {
        while (recv(clnt_sock, msg, sizeof(msg), 0) != 0)
        {
            if (std::strlen(msg) > std::strlen(tell_name))
            {
                char pre_name[13];
                std::strncpy(pre_name, msg, 12);
                pre_name[12] = '\0';
                if (std::strcmp(pre_name, tell_name) == 0) // Verifica si el mensaje contiene el nombre del cliente
                {
                    char name[20];
                    std::strcpy(name, msg + 12);
                    if (clnt_socks.find(name) == clnt_socks.end()) // Verifica si el nombre del cliente ya existe
                    {
                        output("the name of socket %d: %s\n", clnt_sock, name); // Imprime el nombre del cliente y el socket asociado
                        clnt_socks[name] = clnt_sock;                           // Almacena el nombre y el socket del cliente en un mapa
                    }
                    else
                    {
                        std::string error_msg = std::string(name) + " exists already. Please quit and enter with another name!";
                        send(clnt_sock, error_msg.c_str(), error_msg.length() + 1, 0); // Envía un mensaje de error al cliente si el nombre ya está en uso
                        mtx.lock();
                        clnt_cnt--;
                        mtx.unlock();
                        flag = 1;
                    }
                }
            }

            if (flag == 0)
                send_msg(clnt_sock, std::string(msg)); // Envía el mensaje recibido a todos los clientes conectados
        }
    }
    catch (const std::exception &e)
    {
        error_output("Exception occurred in handle_clnt: %s\n", e.what()); // Maneja una excepción si ocurre un error durante el manejo del cliente
    }

    std::string name;
    mtx.lock();
    for (auto it = clnt_socks.begin(); it != clnt_socks.end(); ++it)
    {
        if (it->second == clnt_sock)
        {
            name = it->first;
            clnt_socks.erase(it->first); // Elimina al cliente del mapa de clientes
            break;
        }
    }
    clnt_cnt--;
    mtx.unlock();

    if (flag == 0)
    {
        std::string leave_msg = "Client " + name + " left the chat room";
        send_msg(clnt_sock, leave_msg);                         // Envía un mensaje de salida a todos los clientes cuando un cliente se desconecta
        output("Client %s left the chat room\n", name.c_str()); // Imprime un mensaje indicando que el cliente se desconectó
    }

    close(clnt_sock); // Cierra el socket del cliente
}

// Función para enviar mensajes a los clientes
void send_msg(int clnt_sock, const std::string &msg)
{
    mtx.lock();
    int first_space = msg.find_first_of(" ");
    if (first_space != std::string::npos)
    {
        std::string command = msg.substr(0, first_space);
        std::string pre = "@";
        if (command == "#status")
        {
            std::string status = msg.substr(first_space + 1);
            change_status(clnt_sock, status); // Cambia el estado de un cliente
        }
        else if (command == "#list")
        {
            list_users(clnt_sock); // Lista los usuarios conectados
        }
        else if (command == "#info")
        {
            std::string username = msg.substr(first_space + 1);
            show_user_info(clnt_sock, username); // Muestra la información de un usuario en particular
        }
        else if (msg.compare(first_space + 1, 1, pre) == 0)
        {
            // Envía un mensaje privado
            int space = msg.find_first_of(" ", first_space + 1);
            std::string receive_name = msg.substr(first_space + 2, space - first_space - 2);
            std::string send_name = msg.substr(1, first_space - 2);
            if (clnt_socks.find(receive_name) == clnt_socks.end())
            {
                // Si el usuario al que se quiere enviar el mensaje privado no existe
                std::string error_msg = "[error] there is no client named " + receive_name;
                send(clnt_sock, error_msg.c_str(), error_msg.length() + 1, 0);
            }
            else
            {
                send(clnt_socks[receive_name], msg.c_str(), msg.length() + 1, 0); // Envía el mensaje al destinatario
                send(clnt_socks[send_name], msg.c_str(), msg.length() + 1, 0);    // Envía una copia del mensaje al remitente
            }
        }
        else
        {
            // Envía el mensaje a todos los clientes
            for (auto it = clnt_socks.begin(); it != clnt_socks.end(); it++)
            {
                send(it->second, msg.c_str(), msg.length() + 1, 0);
            }
        }
    }
    mtx.unlock();
}

// Función para cambiar el estado de un cliente
void change_status(int clnt_sock, const std::string &status)
{
    mtx.lock();
    for (auto it = clnt_socks.begin(); it != clnt_socks.end(); ++it)
    {
        if (it->second == clnt_sock)
        {
            std::string username = it->first;
            output("Changing status of user '%s' to '%s'\n", username.c_str(), status.c_str()); // Imprime un mensaje indicando el cambio de estado de un cliente

            break;
        }
    }
    mtx.unlock();
}

// Función para listar los usuarios conectados
void list_users(int clnt_sock)
{
    mtx.lock();
    std::string user_list = "Usuarios conectados:\n";
    for (const auto &pair : clnt_socks)
    {
        user_list += pair.first + "\n";
    }
    send(clnt_sock, user_list.c_str(), user_list.length() + 1, 0); // Envía la lista de usuarios conectados al cliente
    mtx.unlock();
}

// Función para mostrar la información de un usuario en particular
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
    send(clnt_sock, info_msg.c_str(), info_msg.length() + 1, 0); // Envía la información del usuario al cliente
    mtx.unlock();
}

// Función para imprimir en la salida estándar
int output(const char *arg, ...)
{
    int res;
    va_list ap;
    va_start(ap, arg);
    res = vfprintf(stdout, arg, ap);
    va_end(ap);
    return res;
}

// Función para imprimir mensajes de error
int error_output(const char *arg, ...)
{
    int res;
    va_list ap;
    va_start(ap, arg);
    res = vfprintf(stderr, arg, ap);
    va_end(ap);
    return res;
}

// Función para manejar errores
void error_handling(const std::string &message)
{
    std::cerr << message << std::endl;
    exit(1);
}
