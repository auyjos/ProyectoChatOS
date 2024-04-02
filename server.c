#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <arpa/inet.h>

#define SERVER_PORT 5208
#define BUF_SIZE 1024
#define MAX_CLNT 256

void *handle_clnt(void *arg);
void send_msg(int clnt_sock, const char *msg);
void change_status(int clnt_sock, const char *status);
void list_users(int clnt_sock);
void show_user_info(int clnt_sock, const char *username);
int output(const char *arg, ...);
int error_output(const char *arg, ...);
void error_handling(const char *message);
char *trim(char *str)
{
    char *end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

int clnt_cnt = 0;
pthread_mutex_t mtx;
struct clnt_sock_map
{
    char name[20];
    char status[BUF_SIZE];
    int sock;
};
struct clnt_sock_map clnt_socks[MAX_CLNT];
char clnt_ips[MAX_CLNT][INET_ADDRSTRLEN]; // Add this line to store the IP addresses

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
        pthread_mutex_lock(&mtx);
        clnt_cnt++;
        strcpy(clnt_ips[clnt_cnt - 1], inet_ntoa(clnt_addr.sin_addr)); // Store the IP address
        pthread_mutex_unlock(&mtx);
        pthread_t t_id;
        pthread_create(&t_id, NULL, handle_clnt, (void *)&clnt_sock);
        pthread_detach(t_id);
        output("Connected client IP: %s \n", inet_ntoa(clnt_addr.sin_addr));
    }
    close(serv_sock);
    return 0;
}

void *handle_clnt(void *arg)
{
    int clnt_sock = *((int *)arg);
    char msg[BUF_SIZE];
    int flag = 0;
    char tell_name[13] = "#new client:";
    while (recv(clnt_sock, msg, sizeof(msg), 0) != 0)
    {
        char *trim_msg = trim(msg);
        if (strlen(msg) > strlen(tell_name))
        {
            char pre_name[13];
            strncpy(pre_name, msg, 12);
            pre_name[12] = '\0';
            if (strcmp(pre_name, tell_name) == 0)
            {
                char name[20];
                strcpy(name, msg + 12);
                bool name_exists = false;
                for (int i = 0; i < clnt_cnt; i++)
                {
                    if (strcmp(clnt_socks[i].name, name) == 0)
                    {
                        name_exists = true;
                        break;
                    }
                }
                if (!name_exists)
                {
                    output("the name of socket %d: %s\n", clnt_sock, name);
                    strcpy(clnt_socks[clnt_cnt - 1].name, name);
                    clnt_socks[clnt_cnt - 1].sock = clnt_sock;
                    strcpy(clnt_socks[clnt_cnt - 1].status, "ACTIVO");
                }
                else
                {
                    char error_msg[BUF_SIZE];
                    sprintf(error_msg, "%s exists already. Please quit and enter with another name!", name);
                    send(clnt_sock, error_msg, strlen(error_msg) + 1, 0);
                    pthread_mutex_lock(&mtx);
                    clnt_cnt--;
                    pthread_mutex_unlock(&mtx);
                    flag = 1;
                }
            }
        }
        else if (strcmp(msg, "#list") == 0)
        {
            printf("Se inicio la lista correctamente.\n");
            char list_msg[BUF_SIZE] = "Usuarios conectados:\n";
            pthread_mutex_lock(&mtx);
            for (int i = 0; i < clnt_cnt; i++)
            {
                strcat(list_msg, clnt_socks[i].name);
                strcat(list_msg, " (");
                strcat(list_msg, clnt_socks[i].status);
                strcat(list_msg, ")\n");
            }
            pthread_mutex_unlock(&mtx);
            send(clnt_sock, list_msg, strlen(list_msg) + 1, 0);
        
        }   
        else if (strstr(msg, "#tus:") == msg)
        {
            char status[BUF_SIZE];
            strcpy(status, msg + 5);

            pthread_mutex_lock(&mtx);
            for (int i = 0; i < clnt_cnt; i++)
            {
                if (clnt_socks[i].sock == clnt_sock)
                {
                    strcpy(clnt_socks[i].status, status); // Update the status

                    // Prepare the confirmation message
                    char confirm_msg[BUF_SIZE+36];
                    snprintf(confirm_msg, sizeof(confirm_msg), "Your status has been changed to '%s'\n", clnt_socks[i].status);

                    // Send the confirmation message to the client
                    send(clnt_sock, confirm_msg, strlen(confirm_msg) + 1, 0);

                    break;
                }
            }
            pthread_mutex_unlock(&mtx);
        }
        else if (strncmp(msg, "#inf:", 5) == 0)
        {
            char username[BUF_SIZE];
            strcpy(username, msg + 5);

            pthread_mutex_lock(&mtx);
            for (int i = 0; i < clnt_cnt; i++)
            {
                if (strcmp(clnt_socks[i].name, username) == 0)
                {
                    char inf_msg[BUF_SIZE];
                    sprintf(inf_msg, "Name: %s, IP: %s, Socket: %d", clnt_socks[i].name, clnt_ips[i], clnt_socks[i].sock);
                    send(clnt_sock, inf_msg, strlen(inf_msg) + 1, 0);
                    break;
                }
            }
            pthread_mutex_unlock(&mtx);
        }
        if (flag == 0)
            send_msg(clnt_sock, msg);
    }
    char name[20];
    pthread_mutex_lock(&mtx);
    for (int i = 0; i < clnt_cnt; i++)
    {
        if (clnt_socks[i].sock == clnt_sock)
        {
            strcpy(name, clnt_socks[i].name);
            for (int j = i; j < clnt_cnt - 1; j++)
            {
                clnt_socks[j] = clnt_socks[j + 1];
            }
            break;
        }
    }
    clnt_cnt--;
    pthread_mutex_unlock(&mtx);
    if (flag == 0)
    {
        char leave_msg[BUF_SIZE];
        sprintf(leave_msg, "Client %s left the chat room", name);
        send_msg(clnt_sock, leave_msg);
        output("Client %s left the chat room\n", name);
    }
    close(clnt_sock);
    return NULL;
}

void send_msg(int clnt_sock, const char *msg)
{
    pthread_mutex_lock(&mtx);
    int first_space = strcspn(msg, " ");
    if (first_space != strlen(msg))
    {
        char command[BUF_SIZE];
        strncpy(command, msg, first_space);
        command[first_space] = '\0';
        char pre[2] = "@";
        if (strncmp(msg, "#status:", 8) == 0)
        {
            char status[BUF_SIZE];
            strcpy(status, msg + 8);
            change_status(clnt_sock, status);
        }
        else if (strcmp(command, "#list") == 0)
        {
            list_users(clnt_sock);
        }
        else if (strcmp(command, "#info") == 0)
        {
            char username[BUF_SIZE];
            strcpy(username, msg + first_space + 1);
            show_user_info(clnt_sock, username);
        }
        else if (strncmp(msg + first_space + 1, pre, 1) == 0)
        {
            int space = strcspn(msg + first_space + 2, " ");
            char receive_name[BUF_SIZE];
            strncpy(receive_name, msg + first_space + 2, space);
            receive_name[space] = '\0';
            char send_name[BUF_SIZE];
            strncpy(send_name, msg + 1, first_space - 2);
            send_name[first_space - 2] = '\0';
            bool name_exists = false;
            int receive_sock;
            for (int i = 0; i < clnt_cnt; i++)
            {
                if (strcmp(clnt_socks[i].name, receive_name) == 0)
                {
                    name_exists = true;
                    receive_sock = clnt_socks[i].sock;
                    break;
                }
            }
            if (!name_exists)
            {
                char error_msg[BUF_SIZE];
                sprintf(error_msg, "[error] there is no client named %.990s", receive_name);
                send(clnt_sock, error_msg, strlen(error_msg) + 1, 0);
            }
            else
            {
                send(receive_sock, msg, strlen(msg) + 1, 0);
                send(clnt_sock, msg, strlen(msg) + 1, 0);
            }
        }
        else
        {
            for (int i = 0; i < clnt_cnt; i++)
            {
                send(clnt_socks[i].sock, msg, strlen(msg) + 1, 0);
            }
        }
    }
    pthread_mutex_unlock(&mtx);
}

void change_status(int clnt_sock, const char *status)
{
    pthread_mutex_lock(&mtx);
    for (int i = 0; i < clnt_cnt; i++)
    {
        if (clnt_socks[i].sock == clnt_sock)
        {
            strcpy(clnt_socks[i].status, status); // Update the status

            // Prepare the confirmation message
            char confirm_msg[BUF_SIZE];
            snprintf(confirm_msg, sizeof(confirm_msg), "Your status has been changed to '%s'\n", clnt_socks[i].status);

            // Send the confirmation message to the client
            send(clnt_sock, confirm_msg, strlen(confirm_msg) + 1, 0);

            break;
        }
    }
    pthread_mutex_unlock(&mtx);
}

void list_users(int clnt_sock)
{
    pthread_mutex_lock(&mtx);
    char user_list[BUF_SIZE] = "Users conectados:\n";
    for (int i = 0; i < clnt_cnt; i++)
    {
        strcat(user_list, clnt_socks[i].name);
        strcat(user_list, " (");
        strcat(user_list, clnt_socks[i].status);
        strcat(user_list, ")\n");
    }
    send(clnt_sock, user_list, strlen(user_list) + 1, 0);
    pthread_mutex_unlock(&mtx);
}

void show_user_info(int clnt_sock, const char *username)
{
    pthread_mutex_lock(&mtx);
    char info_msg[BUF_SIZE];
    bool name_exists = false;
    for (int i = 0; i < clnt_cnt; i++)
    {
        if (strcmp(clnt_socks[i].name, username) == 0)
        {
            name_exists = true;
            break;
        }
    }
    if (name_exists)
    {
        sprintf(info_msg, "Información de usuario '%s':\n", username);
    }
    else
    {
        sprintf(info_msg, "El usuario '%s' no está conectado.\n", username);
    }
    send(clnt_sock, info_msg, strlen(info_msg) + 1, 0);
    pthread_mutex_unlock(&mtx);
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

void error_handling(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(1);
}
