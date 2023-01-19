#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define PORT        "9000"
#define MAX_CONN_Q  1
#define ADD_SPACE   1024 //bytes
#define BUF_MAX_LEN 1024
#define LOGFILE     "./socketdata_log"

int switchoff = 0;

void sig_handle(int signal)
{
    switchoff = 1;
    return;
}

void sig_handle_alrm(int signal)
{
    printf("alarm\n");
    return;
}

struct mem_resp {
    char * data;
    long int len;
    long int total;
};

typedef struct mem_resp resp;

long int store_to_resp(resp * resp_struct, char * buf, int buflen)
{
    if ( resp_struct->len + buflen < resp_struct->total )
    {
        memcpy( resp_struct->data + resp_struct->len , buf, buflen );
        resp_struct->len = resp_struct->len + buflen;
        
        return resp_struct->len;
    } 
    else {
        char * tmp = NULL;
        long int tmp_sz = 0;
        if ( resp_struct->len + buflen >= resp_struct->total + ADD_SPACE) /*hack to avoid future possible error when we-
                                                                            adjust the buflen max value.*/
        {
            tmp_sz = resp_struct->total + buflen + ADD_SPACE;
        }
        else {
            tmp_sz = resp_struct->total + ADD_SPACE;
        }
        tmp = realloc(resp_struct->data, resp_struct->len + ADD_SPACE);
        if (tmp == NULL)
        {
            printf("realloc fail: %s", strerror(errno));
            return 0;
        }
        resp_struct->data  = tmp;
        resp_struct->total = tmp_sz;
        memcpy( resp_struct->data + resp_struct->len , buf, buflen );
        resp_struct->len   = resp_struct->len + buflen;
        return resp_struct->len;
    }
}

int store_to_file(resp * resp_struct)
{
    if (resp_struct->data == NULL)
    {
        return 1;
    }
    FILE * fp;
    fp = fopen(LOGFILE, "a");
    if (fp == NULL)
    {
        printf("Error in file open: %s", strerror(errno));
        return -1;
    }
    int ret = fwrite(resp_struct->data, 1, resp_struct->len, fp);
    if (ret < resp_struct->len)
    {
        printf("Error in file write not fully written.\n");
        return -1;
    }
    fclose(fp);
    return 0;
}


void free_resp(resp * resp_struct)
{
    if (resp_struct->data != NULL)
    {
        free(resp_struct->data);
        resp_struct->data  = NULL;
    }
    resp_struct->total = 0;
    resp_struct->len   = 0;
}

int main()
{
    signal (SIGINT, sig_handle);
    signal (SIGTERM, sig_handle);
    //setting up the signals:
    struct sigaction act_int;
    act_int.sa_handler = sig_handle;
    if (sigaction( SIGINT, &act_int , NULL) == -1)
    {
        printf("Error in sigint handle prep: %s\n", strerror(errno));
        syslog(LOG_ERR, "Error in sigint handle prep: %s", strerror(errno));
        return -1;
    }

    if (sigaction( SIGTERM, &act_int , NULL) == -1)
    {
        printf("Error in sigterm handle prep: %s\n", strerror(errno));
        syslog(LOG_ERR, "Error in sigterm handle prep: %s", strerror(errno));
        return -2;
    }

    struct sigaction act_alrm;
    act_alrm.sa_handler = sig_handle_alrm;
    sigemptyset(&act_alrm.sa_mask);
    act_alrm.sa_flags = 0;
    if (sigaction( SIGALRM, &act_alrm , NULL) == -1) 
    {   
        printf("Error in sigalrm handle prep: %s\n", strerror(errno));
        syslog(LOG_ERR, "Error in sigalrm handle prep: %s", strerror(errno));
        return -3; 
    }


    int sockfd;
    int domain = PF_INET;
    int type = SOCK_STREAM;
    int protocol = 0;
    int ret = 0;
    openlog(NULL, LOG_CONS || LOG_NDELAY || LOG_PID || LOG_PERROR, LOG_USER);
    sockfd = socket(domain, type, protocol);
    if (sockfd == -1)
    {
        printf("socket creation error: %s\n", strerror(errno));
        syslog(LOG_ERR, "socket creation error: %s", strerror(errno));
        return 1;
    }

    struct addrinfo  hints;
    struct addrinfo *res;
    
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;

    ret = getaddrinfo(NULL, PORT, &hints, &res);
    if (ret != 0)
    {
        if (ret != EAI_SYSTEM)
        {
            printf("getaddrinfo() fail: %s\n", gai_strerror(ret));
            syslog(LOG_ERR, "getaddrinfo() fail: %s", gai_strerror(ret));
        }
        else
        {
            printf("getaddrinfo() fail: %s\n", strerror(errno));
            syslog(LOG_ERR, "getaddrinfo() fail: %s", strerror(errno));
        }
        return 1;
    }

    ret = bind (sockfd, res->ai_addr, sizeof(struct sockaddr));
    if(ret == -1)
    {
        printf("bind adress failed: %s\n", strerror(errno));
        syslog(LOG_ERR, "bind adress failed: %s", strerror(errno));
        return 2;
    }
    ret = listen(sockfd, MAX_CONN_Q);
    if (ret == -1)
    {
        printf("listen error: %s\n", strerror(errno));
        syslog(LOG_ERR, "listen error on PORT: %s: %s", PORT, strerror(errno));
        return 3;
    }

    struct sockaddr addr;
    struct sockaddr_in *add_disp;
    add_disp = (struct sockaddr_in *)&addr;
    socklen_t addrlen = sizeof (struct sockaddr);
    int conn;
    int recvret = 0;
    char buf[BUF_MAX_LEN];
    int buflen = 0;
    //steup resp structure:
    resp data;
    data.data = NULL;
    data.len = 0;
    data.total = 0;
    while (1)
    {
        if (switchoff == 1)
        {
            freeaddrinfo(res);
            free_resp(&data);
            close(conn);
            exit(1);
        }
        alarm(5);//block accept for 5sec then interupp using sigalrm.
        conn = accept(sockfd, &addr, &addrlen);
        printf("conn: %d\n", conn);
        if (conn == -1)
        {
            printf("accept error: %s\n", strerror(errno));
            syslog(LOG_ERR, "accept error: %s", strerror(errno));
            continue;
        } else {
            printf("Accepted connection from %d\n", ntohs(add_disp->sin_port));
            syslog(LOG_DEBUG, "Connection accepted from: %s", addr.sa_data);
        }

        while(1)
        {
            buflen = recv( conn, buf, BUF_MAX_LEN, MSG_DONTWAIT);
            if (buflen == -1)
            {
                if (strchr(buf, '\n') != NULL)
                {
                    ret = store_to_file(&data);
                    if (ret != 0)
                    { 
                        printf("Cannot write to file %s\n", LOGFILE);
                        syslog(LOG_ERR, "Cannot write to file: %s", LOGFILE);
                    }
                    //send back the data:
                    ret = send( conn, data.data, data.len, MSG_CONFIRM);
                    if (ret < data.len)
                    {
                        printf("Cannot send data: %s\n", strerror(errno));
                        syslog(LOG_ERR, "Cannot send data: %s", strerror(errno));
                    }
                }
                break;
             }
             printf("%s", buf);
             ret = store_to_resp( &data, buf, buflen);
             if (ret == 0)
             {
                 syslog(LOG_ERR, "Cannot load recieved chunk into memory");
                 break;
             }
        }

        free_resp(&data);
        close(conn);
        printf("Closed connection from %d\n",ntohs(add_disp->sin_port));
        syslog(LOG_DEBUG, "Closed connection from %s", addr.sa_data);
    }

    freeaddrinfo(res);
}
