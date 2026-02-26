#include "open62541/pubsub_redundancy.h"
#include <open62541/plugin/log_stdout.h>
#include "open62541/plugin/log.h"
#include "open62541/types.h"
#include <string.h>
#include <unistd.h>         // close()
#include <arpa/inet.h>      // inet_pton()
#include <sys/socket.h>

#define HEARTBEAT_MSG "HEARTBEAT"
#define BUFFER_SIZE 1024

static int heartbeatSockfd;
static UA_DateTime prevHbTime;
struct sockaddr_in server_addr;



/* -----------------------------------------
   Initialize UDP listener
----------------------------------------- */
int 
initHeartbeatListener(int port)
{
    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}

/* -----------------------------------------
   Non-blocking heartbeat check
----------------------------------------- */
UA_StatusCode
checkHeartbeat(int sock)
{
    char buffer[BUFFER_SIZE];
    struct sockaddr_in sender;
    socklen_t senderLen = sizeof(sender);

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;   // non-blocking

    int isActive = select(sock + 1, &readfds, NULL, NULL, &timeout);

    if (isActive < 0)
        return UA_STATUSCODE_BAD;

    if (isActive > 0 && FD_ISSET(sock, &readfds))
    {
        int bytes = recvfrom(sock,
                             buffer,
                             BUFFER_SIZE - 1,
                             0,
                             (struct sockaddr*)&sender,
                             &senderLen);

        if (bytes > 0)
        {
            prevHbTime = UA_DateTime_nowMonotonic();
            *isPrimary = false;
        }
    }

    // Timeout detection
    if (!*isPrimary)
    {
        long long now = UA_DateTime_nowMonotonic();

        if (prevHbTime != 0 &&
            now - prevHbTime > HEARBEATIMEOUT)
        {
            *isPrimary = true;
            return UA_STATUSCODE_BAD;  // takeover
        }
    }

    return UA_STATUSCODE_GOOD;  // primary alive
}


UA_StatusCode
setupHeartbeat(const char *ipAddress, int port, int *heartbeatSockfd)
{
    // Create UDP socket
    *heartbeatSockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*heartbeatSockfd < 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Socket creation failed");
        return UA_STATUSCODE_BAD;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ipAddress, &server_addr.sin_addr) <= 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Invalid address");
        close(*heartbeatSockfd);
        return UA_STATUSCODE_BAD;
    }

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
sendHeartbeat(void) {
    // Send heartbeat message
    ssize_t sent = sendto(heartbeatSockfd,
                          HEARTBEAT_MSG,
                          strlen(HEARTBEAT_MSG),
                          0,
                          (const struct sockaddr *)&server_addr,
                          sizeof(server_addr));

    if (sent < 0) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Sendto failed");
        close(heartbeatSockfd);
        return UA_STATUSCODE_BAD;
    }

    return UA_STATUSCODE_GOOD;
}


UA_StatusCode
syncState(State_s *state) {
    // Implement the logic to synchronize the state with the server
    // Example implementation:
    // send_heartbeat(state->server_ip, state->server_port);

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
setupPubSub(void) {
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
init(UA_Boolean *isPrimary, State_s *state, connectionConfig_s *config) {
    *isPrimary = false;
    return UA_STATUSCODE_GOOD;
}

int main(int argc, char *argv[]){
    const int port = 10001;
    setupHeartbeat("10.56.127.36", port, &heartbeatSockfd);
    while(1){
        sendHeartbeat();
        sleep(1);
    }

    return 0;
}
