#include "../include/pubsub_redundancy.h"

#include <open62541/plugin/log_stdout.h>

#include "open62541/plugin/log.h"
#include "open62541/server_pubsub.h"
#include "open62541/types.h"

#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>  // close()

#include "../include/pubsub_sync.h"
#include <arpa/inet.h>  // inet_pton()
#include <sys/socket.h>

void
testPrimary(UA_Boolean const *isPrimary, RedundancyState_s *state);

UA_Boolean lastState = -1;
/* lastState not equal to UA_TRUE or UA_FALSE allows
entering the loop to enable/disable writergroup starting as either primary
or backup */

UA_StatusCode
syncState(RedundancyState_s *state, UA_Boolean const *isPrimary, UA_Server *server,
          UA_NodeId writerGroupIdent, UA_NodeId dataSetWriterId) {

    if(*isPrimary != lastState) {
        if(*isPrimary) {
            UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                         "syncState: PRIMARY -> start publishing %d \n", *isPrimary);

            UA_StatusCode rv = UA_Server_setWriterGroupSequenceNumber(
                server, writerGroupIdent, state->nmSequenceNr);
            if(rv == UA_STATUSCODE_GOOD) {
                UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                             "syncState: WriterGroup sequence number set to %u",
                             state->nmSequenceNr);
            } else {
                UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                               "syncState: Failed to set WriterGroup sequence number, "
                               "StatusCode: 0x%08x",
                               rv);
            }
            rv = UA_Server_setDataSetWriterSequenceNumber(server, dataSetWriterId,
                                                          state->dswSequenceNr);
            if(rv != UA_STATUSCODE_GOOD) {
                UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                               "syncState: Failed to set DataSetWriter sequence number, "
                               "StatusCode: 0x%08x",
                               rv);
            }

            UA_Server_setWriterGroupOperational(server, writerGroupIdent);
        } else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: BACKUP -> Not publishing");

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
                        "syncState: Current sequence number: %u", seq);
        } else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: Failed to get sequence number, StatusCode: 0x%08x",
                           rv);
        }

        rv = UA_Server_getDataSetWriterSequenceNumber(server, dataSetWriterId, &seq);
        if(rv == UA_STATUSCODE_GOOD) {
            state->dswSequenceNr = seq;
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                        "syncState: Current DataSetWriter sequence number: %u", seq);
        } else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: Failed to get DataSetWriter sequence number, "
                           "StatusCode: 0x%08x",
                           rv);
        }
    }

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
init(UA_Boolean *isPrimary, RedundancyState_s *state, HeartbeatConfig *config) {
    int sockfd = 0;

    pthread_t heartbeatThread;
    pthread_t syncThread;

    cbstruct_s stateStruct = {
        .data = state,
        .isPrimary = isPrimary,
    };

    if(config->sockfd == NULL) {
        config->sockfd = &sockfd;
    }

    pthread_create(&heartbeatThread, NULL, initHeartBeat, config);
    pthread_create(&syncThread, NULL, initSync, &stateStruct);

    pthread_join(syncThread, NULL);
    pthread_join(heartbeatThread, NULL);

    close(sockfd);
    return UA_STATUSCODE_GOOD;
}

