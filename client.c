#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <pthread.h>

#define BUF_SIZE 1024

void *send_msg(void *sock);
void *recv_msg(void *sock);
void send_command(int sock, const char *command);
int output(const char *arg, ...);
int error_output(const char *arg, ...);
void error_handling(const char *message);

char name[BUF_SIZE] = "DEFAULT"; // Nombre predeterminado del cliente
char msg[BUF_SIZE];              // Mensaje para enviar/recibir

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        error_output("Usage: %s <Name> <Server_IP> <Server_Port>\n", argv[0]);
        exit(1);
    }

    strcpy(name, "[");
    strcat(name, argv[1]);
    strcat(name, "]");

    const char *server_ip = argv[2]; // Dirección IP del servidor
    int server_port = atoi(argv[3]); // Puerto del servidor
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
    char my_name[BUF_SIZE];
    strcpy(my_name, "#new client:");
    strcat(my_name, argv[1]);
    send(sock, my_name, strlen(my_name) + 1, 0);

    // Espera a que los hilos de envío y recepción terminen
    pthread_t snd_thread, rcv_thread;
    pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);
    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);

    pthread_join(snd_thread, NULL);
    pthread_join(rcv_thread, NULL);

    close(sock);

    return 0;
}

// Función para enviar mensajes al servidor
void *send_msg(void *sock)
{
    int client_sock = *((int *)sock);
    while (1)
    {
        fgets(msg, BUF_SIZE, stdin);
        msg[strcspn(msg, "\n")] = '\0';                           // Eliminar el salto de línea
        if (strcmp(msg, "Quit") == 0 || strcmp(msg, "quit") == 0) // Si se escribe "Quit" o "quit", sale del programa
        {
            close(client_sock);
            exit(0);
        }
        else if (strcmp(msg, "status") == 0) // Solicita al servidor el estado
        {
            send_command(client_sock, "#status");
        }
        else if (strcmp(msg, "list") == 0) // Solicita al servidor la lista de usuarios conectados
        {
            send_command(client_sock, "#list");
        }
        else if (strcmp(msg, "help") == 0 || strcmp(msg, "Help") == 0) // Muestra el menú de ayuda
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
            char name_msg[BUF_SIZE];
            strcpy(name_msg, name);
            strcat(name_msg, " ");
            strcat(name_msg, msg);
            send(client_sock, name_msg, strlen(name_msg) + 1, 0); // Envía el mensaje al servidor
        }
    }
    return NULL;
}

// Función para recibir mensajes del servidor
void *recv_msg(void *sock)
{
    int client_sock = *((int *)sock);
    char name_msg[BUF_SIZE];
    while (1)
    {
        int str_len = recv(client_sock, name_msg, BUF_SIZE, 0); // Recibe un mensaje del servidor
        if (str_len == -1)
        {
            error_handling("recv() failed!");
        }
        else if (str_len == 0) // Si el cliente se desconecta
        {
            error_output("El servidor ha cerrado la conexión.\n");
            close(client_sock);
            exit(0);
        }
        else
        {
            printf("%s\n", name_msg); // Muestra el mensaje recibido
        }
    }
    return NULL;
}

// Función para enviar comandos al servidor
void send_command(int sock, const char *command)
{
    send(sock, command, strlen(command) + 1, 0); // Envía el comando al servidor
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
void error_handling(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(1);
}
