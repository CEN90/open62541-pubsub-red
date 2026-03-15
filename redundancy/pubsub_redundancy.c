#include <open62541/pubsub_redundancy.h>
#include <open62541/pubsub_heartbeat.h>
#include <open62541/pubsub_sync.h>
#include <open62541/pubsub_publisher.h>


#include <open62541/plugin/log_stdout.h>

#include "open62541/plugin/log.h"
#include "open62541/types.h"

#include <stdbool.h>
#include <unistd.h>  // close()

#include <arpa/inet.h>  // inet_pton()
#include <sys/socket.h>

#include <pthread.h>


void testPrimary(UA_Boolean const *isPrimary, State_s *state);


UA_StatusCode
syncState(State_s *state) {
    // Implement the logic to synchronize the state with the server
    // Example implementation:
    // send_heartbeat(state->server_ip, state->server_port);

    return UA_STATUSCODE_GOOD;
}


void
testPrimary(UA_Boolean const *isPrimary, State_s *state) {
    while (1) {
        if (*isPrimary)
            state->state += 1;

        UA_LOG_INFO(
                    UA_Log_Stdout,
                    UA_LOGCATEGORY_USERLAND,
                    "STATE: %d", state->state
                );

        sleep(1);
    }
}


UA_StatusCode
init(UA_Boolean *isPrimary, State_s *state, connectionConfig_s *config) {
    int sockfd = 0;

    pthread_t heartbeatThread;
    pthread_t syncThread;
    pthread_t publisherThread;

    HeartbeatConfig heartbeatConfig = {
        .ipAddress = config->ipAddress,
        .port = config->port,
        .sockfd = &sockfd,
        .isPrimary = isPrimary,
    };

    cbstruct_s *stateStruct = malloc(sizeof(cbstruct_s));

    stateStruct->data = state;
    stateStruct->isPrimary = isPrimary;


    pthread_create(&heartbeatThread, NULL, initHeartBeat, &heartbeatConfig);
    pthread_create(&syncThread, NULL, initSync, stateStruct);
    pthread_create(&publisherThread, NULL, runPublisher, stateStruct);

    //testPrimary(stateStruct->isPrimary, stateStruct->data);

    pthread_join(publisherThread, NULL);
    pthread_join(syncThread, NULL);
    pthread_join(heartbeatThread, NULL);

    close(sockfd);
    return UA_STATUSCODE_GOOD;
}

int
main(int argc, char *argv[]) {
    UA_Boolean isPrimary = (argc > 1 && strcmp(argv[1], "primary") == 0);
    State_s state = { .state = (UA_Int64) MAGICNUMBER };

    const int port = 10001;
    const char *controllerIP;

    if(isPrimary) {
        controllerIP = "172.17.0.1";
    } else {
        controllerIP = "172.17.0.2";
    }

    connectionConfig_s config = {
        .port = &port,
        .ipAddress = controllerIP,
    };

    init(&isPrimary, &state, &config);

    return 0;
}
