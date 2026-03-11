#include <open62541/plugin/log_stdout.h>
#include <open62541/pubsub_heartbeat.h>

#include "open62541/plugin/log.h"
#include "open62541/types.h"

#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>

UA_DateTime prevHbTime = 0;
int epoll_fd = -1;
// int heartbeatCount = 0; // Consider remove this
struct sockaddr_in server_addr;
char buffer[BUFFER_SIZE];
struct sockaddr_in sender;
socklen_t senderLen = sizeof(sender);

void *
initHeartBeat(void *arg) {
    HeartbeatConfig *config = (HeartbeatConfig *)arg;

    UA_StatusCode initStatus = setupHeartbeat(config->ipAddress, *config->port,
                                              config->sockfd, config->isPrimary);
    if(initStatus != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Failed to init heartbeat");
        return NULL;
    }

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Heartbeat initialized, isPrimary=%d", *config->isPrimary);

    runHeartbeat(config->ipAddress, *config->port, config->sockfd, config->isPrimary);

    return NULL;
}

UA_StatusCode
setupHeartbeat(const char *ipAddress, int port, int *sockfd,
               UA_Boolean const *isPrimary) {
    // Init
    if(*isPrimary) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Init primary");
        setupHeartbeatSender(ipAddress, port, sockfd);
    } else {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Init as backup");

        UA_StatusCode status = setupHeartbeatReceiver(port, sockfd);

        if(status != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                         "Failed to init heartbeat listener");
            return UA_STATUSCODE_BAD;
        }
    }

    sleep(INITSLEEP);  // wait for heartbeat to stabilize

    return UA_STATUSCODE_GOOD;
}

void
runHeartbeat(const char *ipAddress, int port, int *sockfd, UA_Boolean *isPrimary) {
    UA_DateTime prevHbTime = 0;

    // Runtime
    while(1) {
        if(*isPrimary) {
            sendHeartbeat(sockfd);
        } else {
            UA_StatusCode status = receiveHeartbeat(*sockfd, isPrimary, &prevHbTime);

            // if(status == UA_STATUSCODE_GOOD) {
            //     UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Primary alive.");
            // } else {
            if(status != UA_STATUSCODE_GOOD) {
                *isPrimary = UA_TRUE;
                UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                             "Primary failed. Taking over.");
                setupHeartbeatSender(ipAddress, port, sockfd);
            }
        }

        usleep((__useconds_t)HEARTBEATPERIOD);
    }
}

/* -----------------------------------------
   Initialize UDP listener
----------------------------------------- */
UA_StatusCode
setupHeartbeatReceiver(int port, int *sockfd) {
    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0)
        return UA_STATUSCODE_BAD;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Failed to bind socket\n");
        return UA_STATUSCODE_BAD;
    }

    epoll_fd = epoll_create1(0);
    if(epoll_fd == -1) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "epoll_create1 failed");
        close(sock);
        return UA_STATUSCODE_BAD;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sock;

    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev) == -1) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "epoll_ctl failed");
        close(epoll_fd);
        close(sock);
        return UA_STATUSCODE_BAD;
    }

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Heartbeat listener initialized on port %d", port);

    *sockfd = sock;
    return UA_STATUSCODE_GOOD;
}

/* -----------------------------------------
   Non-blocking heartbeat check
----------------------------------------- */
UA_StatusCode
receiveHeartbeat(int sockfd, UA_Boolean *isPrimary, UA_DateTime *prevHbTime) {
    struct epoll_event events[1];
    int nfds = epoll_wait(epoll_fd, events, 1, 0);  // 0 ms timeout for non-blocking

    if(nfds <= 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "No heartbeat received");
        return UA_STATUSCODE_BAD;
    }

    if(events[0].events & EPOLLIN) {
        while(1) {

            int bytes = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                                 (struct sockaddr *)&sender, &senderLen);

            if(bytes <= 0)
                break;

            UA_DateTime now = UA_DateTime_nowMonotonic();

            if(*prevHbTime != 0) {
                UA_DateTime diff = now - *prevHbTime;
                UA_Int64 diff_ms = diff / UA_DATETIME_MSEC;

                UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                            "Heartbeat received after %lld ms", diff_ms);
            }

            *prevHbTime = now;
        }
    }

    // Timeout detection
    UA_DateTime now = UA_DateTime_nowMonotonic();
    UA_DateTime time_ago = now - *prevHbTime;
    if(*prevHbTime != 0 && time_ago > HEARBEATIMEOUT) {

        UA_Int64 elapsed_ms = time_ago / UA_DATETIME_MSEC;

        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Heartbeat timeout (%lld ms)", elapsed_ms);

        return UA_STATUSCODE_BAD;
    }

    return UA_STATUSCODE_GOOD;  // primary alive
}

UA_StatusCode
setupHeartbeatSender(const char *ipAddress, int port, int *sockfd) {
    // Create UDP socket
    *sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(*sockfd < 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Socket creation failed");
        return UA_STATUSCODE_BAD;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if(inet_pton(AF_INET, ipAddress, &server_addr.sin_addr) <= 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Invalid address");
        close(*sockfd);
        return UA_STATUSCODE_BAD;
    }

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
sendHeartbeat(int const *sockfd) {
    ssize_t sent = sendto(*sockfd, HEARTBEAT_MSG, strlen(HEARTBEAT_MSG), 0,
                          (const struct sockaddr *)&server_addr, sizeof(server_addr));

    if(sent < 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Sendto failed");
        close(*sockfd);
        return UA_STATUSCODE_BAD;
    }

    // if(heartbeatCount >= HEARTBEATYEETCOUNT) {
    //     UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Yeeted %d heartbeats",
    //     heartbeatCount); heartbeatCount = 0;
    // }

    // heartbeatCount += 1;

    return UA_STATUSCODE_GOOD;
}
