#include <open62541/pubsub_heartbeat.h>
#include <open62541/plugin/log_stdout.h>
#include "open62541/types.h"
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

UA_DateTime prevHbTime = 0;
struct sockaddr_in server_addr;
char buffer[BUFFER_SIZE];
struct sockaddr_in sender;
socklen_t senderLen = sizeof(sender);

void*
initHeartBeat(void* arg) {
    HeartbeatConfig *config = (HeartbeatConfig*) arg;

    UA_StatusCode initStatus = setupHeartbeat(config->ipAddress, *config->port, config->sockfd, config->isPrimary);
    if(initStatus != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Failed to init heartbeat");
        return NULL;
    }

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Heartbeat initialized, isPrimary=%d", *config->isPrimary);

    runHeartbeat(config->ipAddress, *config->port, config->sockfd, config->isPrimary);

    return NULL;
}


UA_StatusCode
setupHeartbeat(const char *ipAddress, int port, int *sockfd, UA_Boolean const *isPrimary) {
    // Init
    if(*isPrimary) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Init primary");
        setupHeartbeatSender(ipAddress, port, sockfd);
    } else {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Init as backup");

        UA_StatusCode status = setupHeartbeatReceiver(port, sockfd);

        if(status != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Failed to init heartbeat listener");
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
            UA_StatusCode status = receiveHeartbeat(*sockfd, isPrimary, prevHbTime);

            if(status == UA_STATUSCODE_GOOD) {
                UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Primary alive.");
            } else {
                *isPrimary = UA_TRUE;
                UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                             "Primary failed. Taking over.");
                setupHeartbeatSender(ipAddress, port, sockfd);
            }
        }

        sleep(HEARTBEATPERIOD);
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
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,"Failed to bind socket\n");
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
receiveHeartbeat(int sockfd, UA_Boolean *isPrimary, UA_DateTime prevHbTime) {

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;  // non-blocking

    // Read from socket
    int isActive = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
    UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                 "Heartbeat check select returned %d on sock %d", isActive, sockfd);

    // Check if message
    if(isActive <= 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "No heartbeat received");
        return UA_STATUSCODE_BAD;
    }

    // Check if correct msg
    if(isActive > 0 && FD_ISSET(sockfd, &readfds)) {
        int bytes = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&sender,
                             &senderLen);

        if(bytes > 0) {
            char sender_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sender.sin_addr), sender_ip, INET_ADDRSTRLEN);
            prevHbTime = UA_DateTime_nowMonotonic();
        }
    }

    // Timeout detection
    long long now = UA_DateTime_nowMonotonic();
    if(prevHbTime != 0 && now - prevHbTime > HEARBEATIMEOUT) {
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
    // Send heartbeat message
    ssize_t sent = sendto(*sockfd, HEARTBEAT_MSG, strlen(HEARTBEAT_MSG), 0,
                          (const struct sockaddr *)&server_addr, sizeof(server_addr));

    if(sent < 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Sendto failed");
        close(*sockfd);
        return UA_STATUSCODE_BAD;
    }
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Yeeted");

    return UA_STATUSCODE_GOOD;
}
