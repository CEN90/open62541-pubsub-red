#include <open62541/plugin/log_stdout.h>
#include <open62541/pubsub_heartbeat.h>
#include <open62541/pubsub_publisher.h>
#include <open62541/pubsub_redundancy.h>
#include <open62541/pubsub_sync.h>

#include "open62541/plugin/log.h"
#include "open62541/types.h"

#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>  // close()

#include <arpa/inet.h>  // inet_pton()
#include <sys/socket.h>

void
testPrimary(UA_Boolean const *isPrimary, RedundancyState_s *state);

UA_Boolean lastState = -1;
/* lastState not equal to UA_TRUE or UA_FALSE allows
entering the loop to enable/disable writergroup starting as either primary
or backup */

UA_StatusCode
syncState(RedundancyState_s *state, UA_Boolean *isPrimary, UA_Server *server, UA_NodeId writerGroupIdent) {
    // Implement the logic to synchronize the state with the server
    // Example implementation:
    // send_heartbeat(state->server_ip, state->server_port);
    // 
    if(*isPrimary != lastState) {

        if(*isPrimary) {
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                         "Becoming PRIMARY -> start publishing %d \n", *isPrimary);

            UA_StatusCode rv = UA_Server_setWriterGroupSequenceNumber(
                server, writerGroupIdent, state->nmSequenceNr);
            if(rv == UA_STATUSCODE_GOOD) {
                UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                            "WriterGroup sequence number set to %u", state->nmSequenceNr);
            } else {
                UA_LOG_WARNING(
                    UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                    "Failed to set WriterGroup sequence number, StatusCode: 0x%08x",
                    rv);
            }
            UA_Server_setWriterGroupOperational(server, writerGroupIdent);
        } else {
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                         "Becoming BACKUP -> stop publishing");

            UA_Server_setWriterGroupDisabled(server, writerGroupIdent);
        }

        lastState = *isPrimary;
    }

    if(*isPrimary) {
        UA_UInt16 seq = 0;
        UA_StatusCode rv =
            UA_Server_getWriterGroupSequenceNumber(server, writerGroupIdent, &seq);
        if(rv == UA_STATUSCODE_GOOD) {
            state->nmSequenceNr = seq;
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                        "Current sequence number: %u", seq);
        } else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "Failed to get sequence number, StatusCode: 0x%08x", rv);
        }
    }

    return UA_STATUSCODE_GOOD;
}

void
testPrimary(UA_Boolean const *isPrimary, RedundancyState_s *state) {
    while(1) {
        if(*isPrimary)
            state->states.state += 1;

        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "isPrimary: %d, Sequence number: %d",
                    *isPrimary, state->states.state);

        sleep(1);
    }
}

UA_StatusCode
init(UA_Boolean *isPrimary, RedundancyState_s *state, heartbeatConfig_s *config) {
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

    // testPrimary(stateStruct->isPrimary, stateStruct->data);

    pthread_join(publisherThread, NULL);
    pthread_join(syncThread, NULL);
    pthread_join(heartbeatThread, NULL);

    close(sockfd);
    return UA_STATUSCODE_GOOD;
}


