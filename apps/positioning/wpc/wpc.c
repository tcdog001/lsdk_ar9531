/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <sys/select.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/netlink.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <ieee80211_wpc.h>
#include <wpc_mgr.h>
#include <arpa/inet.h>
#include <unistd.h>
#define PRIN_LOG(format, args...) printf(format "\n", ## args)

#ifndef MAX_PAYLOAD
#define MAX_PAYLOAD 8192 //1024
#endif


//TBD: Reduce these globals

int wpc_server_sock=0;
fd_set read_fds, error_set;
int fdcount, wpc_driver_sock;
struct iovec iov;
struct nlmsghdr *nlh = NULL;
struct msghdr msg;
/* Added to remove Warnings */
extern void wpc_mgr_init();
extern int create_wpc_sock();
extern int wpc_create_nl_sock();
extern int wpc_mgr_handletimeout();
extern int wpc_accept(int sockfd,struct sockaddr *addr, int *addrlen );
extern void wpc_nlsock_initialise(struct sockaddr_nl *dst_addr, struct nlmsghdr *nlh, struct iovec *iov, struct msghdr *msg);
int wpc_close(int sockfd);
int wpc_start()
{
    int wpc_listen_sock, newfd, addrlen;   // listen on sock_fd, new connection on new_fd
    struct sockaddr_in remoteaddr;         // connector's address information
    struct sockaddr_nl dst_addr;
    int fdmax;
    struct timeval tv;
    int tcp_nodelay = 1;

    wpc_listen_sock = create_wpc_sock();
    if (wpc_listen_sock < 0) {
        printf("NBP socket errno=%d\n", wpc_listen_sock);
        return wpc_listen_sock;
    }
    wpc_driver_sock = wpc_create_nl_sock();
    if (wpc_driver_sock < 0) {
        printf("Netlink socket errno= %d\n", wpc_driver_sock);
        return wpc_driver_sock;
    }

    nlh = (struct nlmsghdr *)calloc(NLMSG_SPACE(MAX_PAYLOAD),1);
    wpc_nlsock_initialise(&dst_addr, nlh, &iov, &msg); 

    FD_ZERO(&read_fds);
    FD_ZERO(&error_set);
    if(wpc_driver_sock > 0)
        FD_SET(wpc_driver_sock, &read_fds);
    if(wpc_driver_sock > 0)
        FD_SET(wpc_driver_sock, &error_set);
    if(wpc_listen_sock> 0)
        FD_SET(wpc_listen_sock,&read_fds);

    fdmax = ((wpc_listen_sock > wpc_driver_sock) ? wpc_listen_sock : wpc_driver_sock);

    for( ; ; ) {

        tv.tv_sec =0;
        tv.tv_usec = WPC_TIMERRESOLUTION; 
        if (select(fdmax+1, &read_fds, NULL, &error_set, &tv) == -1) {
            perror("select");
            if(errno == EINTR)
                continue;
        }
        if (FD_ISSET(wpc_driver_sock, &error_set))
            printf("Error: Netlink socket error\n");

        for(fdcount = 0; fdcount <= fdmax+1; fdcount++) {
            if (FD_ISSET(fdcount, &read_fds)) {

                //New connection
                if (fdcount == wpc_listen_sock) {
                    addrlen = sizeof(remoteaddr);
                    newfd = wpc_accept(wpc_listen_sock, (struct sockaddr *)&remoteaddr,&addrlen);
                    if (newfd == -1) {  
                        printf("Error: WPC socket error \n");
                    }
                    else {
                        if (newfd > fdmax)    
                            fdmax = newfd;
                        wpc_server_sock = newfd;
                        if( setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, &tcp_nodelay, sizeof(tcp_nodelay)) < 0 )
                            printf("Unable to set TCP_NODELAY option at the socket\n");
                        PRIN_LOG("New connection from %s on socket %d\n", inet_ntoa(remoteaddr.sin_addr), newfd);
                    }
                } else if ( fdcount == wpc_driver_sock ) {
                    //Message from driver
                    wpc_mgr_procdrivermsg();
                } else if ( fdcount == wpc_server_sock ) {
                    //Message from server
                    wpc_mgr_procservermsg();
                }
            }
        }
        //Timeout
        //Check if a timeout has occured (irrespective of whether select has timed out or 
        //if there has been activity on the fd) 
        wpc_mgr_handletimeout();

        FD_ZERO(&read_fds);

        if(wpc_server_sock > 0)
            FD_SET(wpc_server_sock, &read_fds); 

        if(wpc_listen_sock > 0)
            FD_SET(wpc_listen_sock, &read_fds); 

        if(wpc_driver_sock > 0)
            FD_SET(wpc_driver_sock, &read_fds); 
    }

    wpc_close(wpc_listen_sock);
    wpc_close(wpc_server_sock);
    free(nlh);
    return 0;
}

int main(int argc, char *argv[])
{
    int is_daemon = 1;
    if(argc != 2) {
        printf("usage:wpc -d or -n\n");
        return -1 ;
    }
    if(strcmp(argv[1],"-n") == 0){
        is_daemon = 0;
    } else if(strcmp(argv[1], "-d") != 0) {
        printf("usage:wpc -d or -n\n");
        return -1;
    }
    else
        daemon(0,0);

    wpc_mgr_init();
    wpc_start();
    return 0;
}
