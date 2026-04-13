#include "../include/pubsub_redundancy.h"

#include <open62541/plugin/log_stdout.h>

#include "open62541/plugin/log.h"
#include "open62541/server_pubsub.h"
#include "open62541/types.h"

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>  // close()

#include "../include/pubsub_sync.h"
#include <arpa/inet.h>  // inet_pton()
#include <sys/socket.h>


UA_Boolean lastState = NULL;
RedundancyState_s state;


UA_StatusCode
onFirstSyncState() {
    if(*isPrimary) {
        UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                     "onFirstSyncState: PRIMARY -> start publishing %d \n", *isPrimary);

        UA_StatusCode retval = UA_Server_setWriterGroupSequenceNumber(
            server, writerGroupIdent, state.nmSequenceNr);
        if(retval == UA_STATUSCODE_GOOD) {
            UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                         "onFirstSyncState: WriterGroup sequence number set to %u",
                         state.nmSequenceNr);
        } else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "onFirstSyncState: Failed to set WriterGroup sequence number, "
                           "StatusCode: 0x%08x",
                           retval);
        }
        retval = UA_Server_setDataSetWriterSequenceNumber(server, dataSetWriterId,
                                                      state.dswSequenceNr);
        if(retval != UA_STATUSCODE_GOOD) {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "onFirstSyncState: Failed to set DataSetWriter sequence number, "
                           "StatusCode: 0x%08x",
                           retval);
        }

        UA_Server_setWriterGroupOperational(server, writerGroupIdent);
    } else {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                       "onFirstSyncState: BACKUP -> Not publishing");

        UA_Server_setWriterGroupDisabled(server, writerGroupIdent);
    }

    lastState = *isPrimary;
    
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
syncState(UA_UInt32 appStateSize, UA_KeyValuePair *applicationStates) {
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
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                        "syncState: writerGroupSequenceNr: %u", wgSeq);
        } else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: Failed to get sequence number, StatusCode: 0x%08x",
                           retval);
        }

        retval = UA_Server_getDataSetWriterSequenceNumber(server, dataSetWriterId, &nmSeq);

        if(retval == UA_STATUSCODE_GOOD) {
            state.dswSequenceNr = nmSeq;
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                        "syncState: dataSetWriterSequenceNr: %u", nmSeq);
        } else {
            UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_APPLICATION,
                           "syncState: Failed to get DataSetWriter seq nr, "
                           "StatusCode: 0x%08x",
                           retval);
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
    
    state.applicationStates = UA_Array_new(args.appStateSize, &UA_TYPES[UA_TYPES_KEYVALUEPAIR]);
    UA_StatusCode retv = UA_Array_copy(args.applicationStates, args.appStateSize, (void *)&(state.applicationStates), &UA_TYPES[UA_TYPES_KEYVALUEPAIR]);
    // state.applicationStates = args.applicationStates;

    cbstruct_s stateStruct = {
        .data = &state,
        .isPrimary = args.isPrimary,
        .server = server,
        .writerGroupId = args.writerGroupIdent,
        .DatasetReaderGroupId = args.dataSetWriterId
    };

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
initStateSync(UA_Boolean *isPrimary, UA_Server *_server, 
            HeartbeatConfig *config, UA_NodeId writerGroupIdent, 
            UA_NodeId dataSetWriterId, UA_UInt32 appStateSize, 
            UA_KeyValuePair *applicationStates) {
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
