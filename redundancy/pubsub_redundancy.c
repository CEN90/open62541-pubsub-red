#include "open62541/pubsub_redundancy.h"
#include "open62541/pubsub_heartbeat.h"

#include <open62541/plugin/log_stdout.h>

#include "open62541/plugin/log.h"
#include "open62541/types.h"

#include <stdbool.h>
#include <unistd.h>  // close()

#include <arpa/inet.h>  // inet_pton()
#include <sys/socket.h>

#include <pthread.h>


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
    *isPrimary = UA_FALSE;
    return UA_STATUSCODE_GOOD;
}

int
main(int argc, char *argv[]) {
    UA_Boolean isPrimary = UA_FALSE;

    int sockfd = 0;
    const int port = 10001;
    const char controllerIP[] = "172.17.0.1"; //"10.56.127.36";

    pthread_t heartbeatThread;
    
    HeartbeatConfig heartbeatConfig = {
        .ipAddress = controllerIP,
        .port = &port,
        .sockfd = &sockfd,
        .isPrimary = &isPrimary,
    };
    
    pthread_create(&heartbeatThread, NULL, initHeartBeat, &heartbeatConfig);
    
    pthread_join(heartbeatThread, NULL);

    close(sockfd);

    return 0;
}
