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

int
main(int argc, char *argv[]) {
    const int port = 10001;
    UA_Boolean isPrimary = UA_FALSE;
    char redDcnIp[IPADDRLEN] = "127.0.0.1";
    RedundancyState_s state;

    if(argc == 3) {
        strcpy(redDcnIp, argv[2]);

        if(strcmp(argv[1], "--primary") == 0)
            isPrimary = UA_TRUE;
        else if(strcmp(argv[1], "--backup") == 0)
            isPrimary = UA_FALSE;
        else {
            UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Invalid argument: %s",
                         argv[1]);
            return -1;
        }
    } else {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Usage: %s [--primary | --backup] <redundancy Controller IP>",
                     argv[0]);
        return -1;
    }

    heartbeatConfig_s config = {
        .port = &port,
        .ipAddress = redDcnIp,
    };

    init(&isPrimary, &state, &config);

    return 0;
}
