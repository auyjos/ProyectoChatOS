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

void send_msg(int sock);
void recv_msg(int sock);
void send_command(int sock, const std::string &command);
int output(const char *arg, ...);
int error_output(const char *arg, ...);
void error_handling(const std::string &message);

std::string name = "DEFAULT"; // Nombre predeterminado del cliente
std::string msg;              // Mensaje para enviar/recibir

int main(int argc, const char **argv, const char **envp)
{
    if (argc != 4)
    {
        error_output("Usage: %s <Name> <Server_IP> <Server_Port>\n", argv[0]);
        exit(1);
    }

    name = "[" + std::string(argv[1]) + "]"; // Asigna el nombre del cliente
    const char *server_ip = argv[2];         // Dirección IP del servidor
    int server_port = std::stoi(argv[3]);    // Puerto del servidor

    int sock;
    struct sockaddr_in serv_addr;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1)
    {
        error_handling("socket() failed!");
    }

    // Configuración de la dirección del servidor
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    serv_addr.sin_port = htons(server_port);

    // Conexión al servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        error_handling("connect() failed!");
    }

    // Envía el nombre del cliente al servidor para validación
    std::string my_name = "#new client:" + std::string(argv[1]);
    send(sock, my_name.c_str(), my_name.length() + 1, 0);

    // Espera a que los hilos de envío y recepción terminen
    std::thread snd(send_msg, sock);
    std::thread rcv(recv_msg, sock);

    snd.join();
    rcv.join();

    close(sock);

    return 0;
}

// Función para enviar mensajes al servidor
void send_msg(int sock)
{
    while (1)
    {
        getline(std::cin, msg);
        if (msg == "Quit" || msg == "quit") // Si se escribe "Quit" o "quit", sale del programa
        {
            close(sock);
            exit(0);
        }
        else if (msg == "status") // Solicita al servidor el estado
        {
            send_command(sock, "#status");
        }
        else if (msg == "list") // Solicita al servidor la lista de usuarios conectados
        {
            send_command(sock, "#list");
        }
        else if (msg == "help" || msg == "Help") // Muestra el menú de ayuda
        {
            output("1. Este es el chat general. Puedes mandar y recibir mensajes de todos los usuarios.\n"
                   "2. Para enviar mensajes privados, debes poner el nombre del usuarios con un arroba.\n"
                   "3. Cambiar de status.\n"
                   "4. Listar los usuarios conectados al sistema de chat.\n"
                   "5. Desplegar información de un usuario en particular.\n"
                   "6. Para ver el menu debes escribir help en el chat.\n"
                   "7. Para salir del chat, escribe quit.\n");
        }
        else
        {
            std::string name_msg = name + " " + msg;                // Construye el mensaje con el nombre del cliente
            send(sock, name_msg.c_str(), name_msg.length() + 1, 0); // Envía el mensaje al servidor
        }
    }
}

// Función para recibir mensajes del servidor
void recv_msg(int sock)
{
    char name_msg[BUF_SIZE + name.length() + 1];
    while (1)
    {
        int str_len = recv(sock, name_msg, BUF_SIZE + name.length() + 1, 0); // Recibe un mensaje del servidor
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
            std::cout << std::string(name_msg) << std::endl; // Muestra el mensaje recibido
        }
    }
}

// Función para enviar comandos al servidor
void send_command(int sock, const std::string &command)
{
    send(sock, command.c_str(), command.length() + 1, 0); // Envía el comando al servidor
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
