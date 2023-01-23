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
//#define LOGFILE     "/var/tmp/aesdsocketdata"
#define LOGFILE     "./socketdata_log"

int switchoff = 0;

void sig_handle(int signal)
{
    switchoff = 1;
    return;
}

struct mem_resp {
    char * data;
    long int len;
    long int total;
    long int saved;//total len saved already.
};

typedef struct mem_resp resp;

//realloc and save any amount of data to data buffer:
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
        tmp = realloc(resp_struct->data, tmp_sz);
        if (tmp == NULL)
        {
            syslog(LOG_ERR, "realloc fail: %s", strerror(errno));
            return 0;
        }
        resp_struct->data  = tmp;
        resp_struct->total = tmp_sz;
        memset(resp_struct->data + resp_struct->len, '\0', resp_struct->total - resp_struct->len);
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
        syslog(LOG_ERR, "Error in opening file %s : %s", LOGFILE, strerror(errno));
        return -1;
    }
    int ret = fwrite(resp_struct->data + resp_struct->saved, 1, resp_struct->len - resp_struct->saved, fp);
    if (ret < (resp_struct->len - resp_struct->saved))
    {
        syslog(LOG_ERR, "Error in writing file %s: %s", LOGFILE, strerror(errno));
        return -1;
    }
    resp_struct->saved = resp_struct->len;
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

int main(int argc, char *argv[])
{
    signal (SIGINT, sig_handle);
    signal (SIGTERM, sig_handle);
    //setting up the signals:
    struct sigaction act_int;
    act_int.sa_handler = sig_handle;
    sigemptyset(&act_int.sa_mask);
    act_int.sa_flags = 0;
    if (sigaction( SIGINT, &act_int , NULL) == -1)
    {
        syslog(LOG_ERR, "Error in sigint handle prep: %s", strerror(errno));
        return -1;
    }
    struct sigaction act_term;
    act_term.sa_handler = sig_handle;
    sigemptyset(&act_term.sa_mask);
    act_term.sa_flags = 0;
    if (sigaction( SIGTERM, &act_term , NULL) == -1)
    {
        syslog(LOG_ERR, "Error in sigterm handle prep: %s", strerror(errno));
        return -2;
    }

    int sockfd;
    int domain = PF_INET;
    int type = SOCK_STREAM;
    int protocol = 0;
    int ret = 0;
    openlog(NULL, LOG_CONS | LOG_NDELAY | LOG_PID | LOG_PERROR, LOG_USER);
    sockfd = socket(domain, type, protocol);
    if (sockfd == -1)
    {
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
            syslog(LOG_ERR, "getaddrinfo() fail: %s", gai_strerror(ret));
        }
        else
        {
            syslog(LOG_ERR, "getaddrinfo() fail: %s", strerror(errno));
        }
        freeaddrinfo(res);
        return 1;
    }

    //reuse address and avoid time wait when trying to close socket:
    int optval = 1; 
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR , &optval, sizeof(int));
    if (ret == -1)
    {
        syslog(LOG_ERR, "setopt fail: %s", strerror(errno));
    }

    ret = bind (sockfd, res->ai_addr, sizeof(struct sockaddr));
    if(ret == -1)
    {
        syslog(LOG_ERR, "bind adress failed: %s", strerror(errno));
        freeaddrinfo(res);
        return 2;
    }

    //check to seee if -d argument present (overdone):
    if (argc > 1)
    {
        for (int i ; i<argc; i++)
        {
            if (strcmp("-d", argv[i]) == 0)
            {
                switch ((ret = fork()))
                {
                    case -1:
                        syslog(LOG_ERR, "Error on fork: %s", strerror(errno));
                        break;
                    case 0:
                        syslog(LOG_DEBUG, "Started as a deamon");
                        break;
                    default:
                        exit(EXIT_SUCCESS);
                        break;
                }
            }
        }
    }

    ret = listen(sockfd, MAX_CONN_Q);
    if (ret == -1)
    {
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
    data.saved = 0;
    
    //main loop:
    while (1)
    {
        //if signal is caught free everything:
        if (switchoff == 1)
        {
            freeaddrinfo(res);
            free_resp(&data);
            close(conn);
            close(sockfd);
            remove(LOGFILE);
            syslog(LOG_DEBUG, "Caught signal, exiting");
            exit(1);
        }
        memset (buf, '\0', BUF_MAX_LEN);
        conn = accept(sockfd, &addr, &addrlen);
        if (conn == -1)
        {
            syslog(LOG_ERR, "accept error: %s", strerror(errno));
            continue;
        } else {
            syslog(LOG_DEBUG, "Connection accepted from: %d", ntohs(add_disp->sin_port));
        }
        //read till there is nothing to read:
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
                        syslog(LOG_ERR, "Cannot write to file: %s", LOGFILE);
                    }
                    ret = send( conn, data.data, data.len, 0);
                    if (ret < data.len)
                    {
                        syslog(LOG_ERR, "Cannot send data: %s", strerror(errno));
                    }
                }
                break;
             }
             ret = store_to_resp( &data, buf, buflen);
             if (ret == 0)
             {
                 syslog(LOG_ERR, "Cannot load recieved chunk into memory");
                 break;
             }
        }

        close(conn);
        syslog(LOG_DEBUG, "Closed connection from %d", ntohs(add_disp->sin_port));
    }

    freeaddrinfo(res);
    return 0;
}
