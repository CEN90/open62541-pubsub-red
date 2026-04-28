#include "../include/pubsub_redundancy.h"

#include <open62541/plugin/log_stdout.h>

#include "open62541/plugin/log.h"
#include "open62541/server_pubsub.h"
#include "open62541/types.h"

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

#include "../include/pubsub_sync.h"
#include <arpa/inet.h>  // inet_pton()
#include <sys/socket.h>

UA_Boolean lastState = NULL;
RedundancyState_s state;

UA_StatusCode
onFirstSyncState() {
    if(*isPrimary) {
        UA_StatusCode retval = UA_Server_setWriterGroupSequenceNumber(
            server, writerGroupIdent, state.nmSequenceNr + 1);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "onFirstSyncState: Failed to set WriterGroup sequence number, "
                           "StatusCode: 0x%08x",
                           retval);
        }
        
        retval = UA_Server_setDataSetWriterSequenceNumber(server, dataSetWriterId,
                                                          state.dswSequenceNr + 1);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_WARNING(
                UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                "onFirstSyncState: Failed to set DataSetWriter sequence number, "
                "StatusCode: 0x%08x",
                retval);
        }
    } else {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                       "onFirstSyncState: BACKUP -> Not publishing");
    }

    lastState = *isPrimary;

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
syncState(UA_UInt32 appStateSize, UA_Int64 **applicationStates) {
    if(*isPrimary != lastState) {
        onFirstSyncState();
    }

    if(*isPrimary) {
        UA_UInt16 wgSeq = 0;
        UA_UInt16 nmSeq = 0;

        UA_StatusCode retval =
            UA_Server_getWriterGroupSequenceNumber(server, writerGroupIdent, &wgSeq);

        if(retval == UA_STATUSCODE_GOOD) {
            state.nmSequenceNr = wgSeq;
        } else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: Failed to get sequence number, StatusCode: 0x%08x",
                           retval);
        }

        retval =
            UA_Server_getDataSetWriterSequenceNumber(server, dataSetWriterId, &nmSeq);

        if(retval == UA_STATUSCODE_GOOD) {
            state.dswSequenceNr = nmSeq;
        } else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: Failed to get DataSetWriter seq nr, "
                           "StatusCode: 0x%08x",
                           retval);
        }

        UA_StatusCode retv =
            UA_Array_copy(*applicationStates, appStateSize,
                          (void *)&(state.applicationStates), &UA_TYPES[UA_TYPES_INT64]);
        if(retv != UA_STATUSCODE_GOOD) {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: Failed to copy applicationStates, "
                           "StatusCode: 0x%08x",
                           retv);
        }
    } else {
        if(state.applicationStates == NULL) {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: applicationStates not initialized yet on backup");
            return UA_STATUSCODE_UNCERTAIN;
        }

        UA_StatusCode retv =
            UA_Array_copy((void *)(state.applicationStates), appStateSize,
                          (void **)applicationStates, &UA_TYPES[UA_TYPES_INT64]);
        if(retv != UA_STATUSCODE_GOOD) {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: Failed to copy applicationStates, "
                           "StatusCode: 0x%08x",
                           retv);
        }        
    }

    return UA_STATUSCODE_GOOD;
}

static void *
initSyncThreads(void *arg) {
    syncThreadArgs_s args = *(syncThreadArgs_s *)arg;

    server = args.server;
    isPrimary = args.isPrimary;
    dataSetWriterId = args.dataSetWriterId;
    writerGroupIdent = args.writerGroupIdent;
    int sockfd = 0;

    pthread_t heartbeatThread;
    pthread_t syncThread;

    state.nmSequenceNr = 0;
    state.dswSequenceNr = 0;
    state.applicationStatesSize = args.appStateSize;

    state.applicationStates = UA_Array_new(args.appStateSize, &UA_TYPES[UA_TYPES_INT64]);
    UA_StatusCode retv =
        UA_Array_copy(args.applicationStates, args.appStateSize,
                      (void *)&(state.applicationStates), &UA_TYPES[UA_TYPES_INT64]);

    cbstruct_s stateStruct = {.data = &state,
                              .isPrimary = args.isPrimary,
                              .server = server,
                              .writerGroupId = args.writerGroupIdent,
                              .DatasetReaderGroupId = args.dataSetWriterId};

    if(args.config->sockfd == NULL) {
        args.config->sockfd = &sockfd;
    }

    pthread_create(&heartbeatThread, NULL, initHeartBeat, args.config);
    pthread_create(&syncThread, NULL, initSync, &stateStruct);

    pthread_join(syncThread, NULL);
    pthread_join(heartbeatThread, NULL);

    close(sockfd);

    return NULL;
}

UA_StatusCode
initStateSync(UA_Boolean *isPrimary, UA_Server *_server, HeartbeatConfig *config,
              UA_NodeId writerGroupIdent, UA_NodeId dataSetWriterId,
              UA_UInt32 appStateSize, UA_Int64 *applicationStates) {
    syncThreadArgs_s args = {
        .isPrimary = isPrimary,
        .server = _server,
        .config = config,
        .writerGroupIdent = writerGroupIdent,
        .dataSetWriterId = dataSetWriterId,
        .appStateSize = appStateSize,
        .applicationStates = applicationStates,
    };

    pthread_t syncThread;

    pthread_create(&syncThread, NULL, initSyncThreads, &args);

    usleep((PUBLISHINGINTERVAL * PUBLISHINGINTERVAL));

    return UA_STATUSCODE_GOOD;
}
