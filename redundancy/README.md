# OPC UA PubSub Redundancy

This project implements redundancy mechanisms for OPC UA Publisher-Subscriber (PubSub) communication using open62541. It provides active/passive redundancy with automatic fail-over, heartbeat monitoring, and state synchronization.

## Overview

The redundancy system is designed to ensure high availability of OPC UA PubSub systems by:
- **Heartbeat monitoring**: Primary and backup publishers exchange heartbeats to detect failures
- **Automatic fail-over**: Backup publisher detects primary failure and takes over publishing
- **State synchronization**: Backup maintains synchronized state with primary for seamless handover
- **Subscriber continuity**: Subscribers continue receiving data with minimal interruption during fail-over

## Architecture

The system consists of two main roles:

### Publisher (Primary/Backup)
- **Primary Publisher**: Main publisher responsible for sending data to subscribers
- **Backup Publisher**: Monitors primary via heartbeat, maintains synchronized state, and takes over if primary fails
- Runs two parallel mechanisms:
  - **Heartbeat thread**: Monitors primary availability on a dedicated socket connection
  - **Sync thread**: Synchronizes application state between primary and backup

### Subscriber
- Receives data from publishers
- Detects publisher changes (deadline misses, sequence number rejects) during fail-over
- Implements deadline-based rejection and state monitoring (NetMessage and DataSetMessage validation)

## Key Components

### Core Modules

| Module | File | Description |
|--------|------|-------------|
| **Heartbeat** | `src/pubsub_heartbeat.c` | Periodic heartbeat mechanism for primary failure detection |
| **State Sync** | `src/pubsub_sync.c` | OPC UA PubSub-based state synchronization between publishers |
| **Redundancy** | `src/pubsub_redundancy.c` | Main redundancy orchestration and fail-over logic |
| **State Management** | `src/redundancy_state.c` | Application state representation and management |

### Configuration

Publisher configuration is defined in `pubsub_publisher.h`:
- `PUBLISHER_PUBLISHERID`: Unique identifier for the publisher (4333)
- `PUBLISHER_WRITERGROUDID`: Writer group identifier (433)
- `PUBLISHER_DATASETWRITERID`: Dataset writer identifier (43)
- `PUBLISHER_PUBLISHINGINTERVAL`: Data publishing interval (50 ms)
- `PUBLISHER_KEYFRAMECOUNT`: Keyframe frequency (every 10 messages)
- `CHECK_PRIMARY_STATUS_INTERVAL`: Heartbeat check interval (100 ms)

## Building

```bash
cd redundancy
cmake .
make
```

Executables are generated in `../build/bin/redundancy/`:
- `pubsub_publisher`: Publisher executable (primary/backup mode)
- `pubsub_subscriber`: Subscriber executable

## Running

### Publisher (Primary Mode)
```bash
./pubsub_publisher [--primary] <IP>
```

### Publisher (Backup Mode)
```bash
./pubsub_publisher --backup <IP>
```

### Subscriber
```bash
./pubsub_subscriber
```

## Redundancy Mechanism Details

### Heartbeat Protocol
- **Frequency**: 5 seconds interval with 15 ms timeout
- **Sender**: Primary publishes heartbeats via UDP
- **Receiver**: Backup listens for heartbeats; triggers fail-over after 1000 missed beats (~5 seconds)
- **Socket**: Dedicated connection independent of OPC UA PubSub

### State Synchronization
- **Method**: OPC UA PubSub messages on a separate channel
- **Frequency**: Every 20 ms
- **Content**: Application state variables (array of Int64 values)
- **Purpose**: Ensures backup has identical state to resume publishing seamlessly

### Fail-Over Sequence
1. Primary stops sending heartbeats
2. Backup detects missed heartbeats after timeout period
3. Backup transitions from secondary to primary role
4. Backup begins publishing to subscribers
5. Normal operation resumes with backup as new publisher

## Event Timestamps (Monitoring)

When testing redundancy with monitoring, the following events are tracked:

| Event | Role | Description |
|-------|------|-------------|
| P1 | Primary | Primary failure/goes down |
| B1 | Backup | Backup detects primary failure |
| B2 | Backup | Backup prepared to publish |
| B3 | Backup | Backup sent first message |
| S1 | Subscriber | Deadline miss detected |
| S2 | Subscriber | NetworkMessage sequence reject |
| S3 | Subscriber | DataSetMessage sequence reject |
| T1 | Overall | Total time from failure (P1) to first backup message (B3) |

## Implementation Details

### Threading Model
- **Main thread**: OPC UA server loop
- **Heartbeat thread**: Monitors primary status
- **Sync thread**: Handles state synchronization publishing
- **Subscriber threads**: Process incoming messages

### Socket Configuration
- Heartbeat uses UDP for low-overhead monitoring
- OPC UA PubSub uses UDP multicast for data distribution
- Separate socket for state synchronization to avoid network contention

### State Management
- Application maintains an array of Int64 values representing its state
- State is synchronized bidirectionally (primary → backup)
- Backup applies state updates to be ready for immediate fail-over


## Dependencies

- **open62541**: OPC UA library (header-only or compiled)
- **CMake**: Build system
- **POSIX threads**: For multi-threaded redundancy
- **Standard C library**: For networking and synchronization
