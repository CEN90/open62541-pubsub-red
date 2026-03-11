#include <open62541/plugin/log_stdout.h>
#include <open62541/pubsub_heartbeat.h>
#include <open62541/pubsub_redundancy.h>
#include <open62541/pubsub_sync.h>

#include "open62541/plugin/log.h"
#include "open62541/types.h"

#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>  // close()

#include <arpa/inet.h>  // inet_pton()
#include <sys/socket.h>

void
testPrimary(UA_Boolean const *isPrimary, State_s *state);

UA_StatusCode
syncState(State_s *state) {
    // Implement the logic to synchronize the state with the server
    // Example implementation:
    // send_heartbeat(state->server_ip, state->server_port);

    return UA_STATUSCODE_GOOD;
}

void
testPrimary(UA_Boolean const *isPrimary, State_s *state) {
    while(1) {
        if(*isPrimary)
            state->state += 1;

        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Sequence number: %d",
                    state->state);

        sleep(1);
    }
}

UA_StatusCode
init(UA_Boolean *isPrimary, State_s *state, connectionConfig_s *config) {
    int sockfd = 0;

    pthread_t heartbeatThread;
    pthread_t syncThread;

    HeartbeatConfig heartbeatConfig = {
        .ipAddress = config->ipAddress,
        .port = config->port,
        .sockfd = &sockfd,
        .isPrimary = isPrimary,
    };

    cbstruct_s stateStruct = {state, isPrimary};

    pthread_create(&heartbeatThread, NULL, initHeartBeat, &heartbeatConfig);
    pthread_create(&syncThread, NULL, initSync, &stateStruct);

    testPrimary(isPrimary, state);

    pthread_join(syncThread, NULL);
    pthread_join(heartbeatThread, NULL);

    close(sockfd);
    return UA_STATUSCODE_GOOD;
}

int
main(int argc, char *argv[]) {
    const int port = 10001;
    UA_Boolean isPrimary = UA_FALSE;
    char redDcnIp[IPADDRLEN] = "127.0.0.1";
    State_s state = {.state = (UA_Int64)MAGICNUMBER};

    if(argc == 3) {
        strcpy(redDcnIp, argv[2]);
        
        if(strcmp(argv[1], "--primary") == 0)
            isPrimary = UA_TRUE;
        else if(strcmp(argv[1], "--backup") == 0)
            isPrimary = UA_FALSE;
        else {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Invalid argument: %s", argv[1]);
            return -1;
        }
    } else {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Usage: %s [--primary | --backup] <redundancy Controller IP>", argv[0]);
        return -1;
    }

    connectionConfig_s config = {
        .port = &port,
        .ipAddress = redDcnIp,
    };

    init(&isPrimary, &state, &config);

    return 0;
}
