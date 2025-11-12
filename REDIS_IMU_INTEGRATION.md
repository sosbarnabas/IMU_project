# Redis IMU Controller Integration Guide

## Overview

The `RedisSingleIMUController` provides Redis-based command and telemetry interface for ICM-20948 IMU devices. It follows the same architectural pattern as the motor controllers, enabling seamless integration into a unified sensor/actuator infrastructure.

**Key Features:**
- Async independent loop (~120 Hz) with no blocking on external devices
- Command-based control via Redis queue
- Separate Redis stream per IMU for telemetry
- Thread-safe design (mutex-protected Redis and MCP2221 I2C operations)
- Graceful shutdown via `exit` flag
- Full error logging to Redis

## Architecture

```
┌─────────────────────────────────────────┐
│ Main Application Thread                 │
│ ┌─────────────────────────────────────┐ │
│ │ 1. Initialize MCP2221 (USB-I2C)     │ │
│ │ 2. Initialize ICM20948 IMU          │ │
│ │ 3. Start IMU producer thread        │ │
│ │ 4. Create RedisSingleIMUController  │ │
│ │ 5. Launch controller loop in jthread│ │
│ └─────────────────────────────────────┘ │
└──────────┬──────────────────────────────┘
           │
           ├─────────────┬──────────────┐
           │             │              │
       ┌───▼────┐   ┌────▼────┐   ┌───▼────┐
       │ Redis  │   │ IMU Loop│   │ I2C Bus│
       │ Queue  │   │Thread   │   │(MCP)   │
       │        │   │(jthread)│   │        │
       └────────┘   └────┬────┘   └────────┘
           ▲              │
    Commands │             ├─► FIFO Read
           │              ├─► Sample XADD
           │              └─► Response LPUSH
```

## Data Flow

### Command Processing (Async)

```
Redis: LPUSH command:imu:1 "t|start|-1|1"
         │
         ├─► RedisSingleIMUController.loop() receives keyspace notification
         │
         ├─► RPOP command:imu:1 
         │
         ├─► processCommand() parses "t|start|-1|1"
         │    - type = "t"
         │    - command = "start"
         │    - idx = -1 (self)
         │    - value = "1"
         │
         ├─► Execute: imu.start() or similar
         │
         └─► LPUSH commandres:start:imu:1:t "OK:started"
```

### Telemetry Streaming (Async ~120 Hz)

```
sync:loop:next SET event (external trigger)
         │
         ├─► RedisSingleIMUController.loop() callback
         │
         ├─► No command? Measure sample
         │
         ├─► measureAndStore() reads FIFO, applies zeroing
         │
         ├─► XADD xdata:imu:1 "*" {t_ns, seq, accel_x, ...}
         │
         └─► signal_data_ready(redis_, 1) increments sync counter
```

## Redis Key Schema

### Commands (Input)

| Key Pattern | Type | Purpose | Example |
|-------------|------|---------|---------|
| `command:imu:1` | List (FIFO) | Command queue | `LPUSH command:imu:1 "t\|start\|-1\|1"` |

**Command Format:** `type|command|idx|value`
- `type`: Transaction identifier (usually "t")
- `command`: One of: `start`, `stop`, `zero`, `read`, `connect`, `disconnect`
- `idx`: IMU index, -1 for self
- `value`: Command parameter

### Telemetry (Output)

| Key Pattern | Type | Purpose | Fields |
|-------------|------|---------|--------|
| `xdata:imu:1` | Stream | IMU samples | See "Stream Fields" below |

**Stream Fields:**
- `t_ns`: Timestamp (nanoseconds, steady_clock)
- `seq`: Per-IMU sample sequence number
- `imu_id`: IMU identifier (1, 2, etc.)
- `accel_x`, `accel_y`, `accel_z`: Acceleration (m/s²)
- `gyro_x`, `gyro_y`, `gyro_z`: Angular velocity (rad/s)
- `mag_x`, `mag_y`, `mag_z`: Magnetic field (if available)
- `euler_x`, `euler_y`, `euler_z`: Euler angles (degrees, if zeroing applied)
- `flags`: Status flags (fifo_overflow, fifo_underflow, mag_ok, reserved bits)

### Responses (Output)

| Key Pattern | Type | Purpose |
|-------------|------|---------|
| `commandres:<cmd>:<device_key>:<type>` | List | Command responses |

**Response Format:**
- Success: `OK:<response_data>`
- Error: `ER:<error_message>`

Examples:
- `commandres:start:imu:1:t` → `["OK:started"]`
- `commandres:zero:imu:1:t` → `["OK:zero:enabled"]`
- `commandres:connect:imu:1:t` → `["OK"]` or `["ER:IMU initialization failed"]`

### Logging

| Key Pattern | Type | Purpose |
|-------------|------|---------|
| `log` | Stream | System log (shared with motors) |

**Log Format (Stream Fields):**
- `t`: Timestamp
- `src`: Source (e.g., "IMU:imu:1")
- `level`: DEBUG, INFO, WARNING, ERROR
- `msg`: Log message

## Usage Examples

### Example 1: Basic Initialization (C++)

```cpp
#include "exoskeleton/core/RedisSingleIMUController.h"

// Hardware setup
MCP2221 mcp;
mcp.open();

ICM20948 imu(mcp, 0x69, 1, def_imu_cfg);
imu.Initialize();
imu.FIFOConfig();
imu.CalibrateAccelGyro(1000);

// Start producer thread
TSQueue<ImuSample> queue;
imu.start(queue);

// Create controller
exoskeleton::core::RedisSingleIMUController ctrl(imu, "imu:1", 1);

// Launch in thread
auto ctrl_thread = std::jthread([&](auto st) { ctrl.loop(); });

// Now send commands via Redis...
// (ctrl_thread runs independently at ~120 Hz)
```

### Example 2: Send Commands (Redis CLI)

```bash
# Start IMU producer loop
redis-cli LPUSH command:imu:1 "t|start|-1|1"

# Check response
redis-cli LRANGE commandres:start:imu:1:t 0 -1

# Read latest samples
redis-cli XRANGE xdata:imu:1 - + COUNT 5

# Toggle zeroing
redis-cli LPUSH command:imu:1 "t|zero|-1|1"

# Stop and shutdown
redis-cli LPUSH command:imu:1 "t|stop|-1|1"
redis-cli SET exit 1
```

### Example 3: Python Consumer

```python
import redis
import json

r = redis.Redis(host='127.0.0.1', port=6379, decode_responses=True)

# Read latest 10 samples
samples = r.xrange('xdata:imu:1', count=10)
for entry_id, data in samples:
    print(f"ID: {entry_id}")
    print(f"  Accel: {data['accel_x']}, {data['accel_y']}, {data['accel_z']}")
    print(f"  Gyro: {data['gyro_x']}, {data['gyro_y']}, {data['gyro_z']}")
    print(f"  Euler: {data['euler_x']}, {data['euler_y']}, {data['euler_z']}")
```

## Thread Safety

### Design Principles

1. **Independent Loops**: Each IMU controller runs its own async loop (~120 Hz), independent from motor loops. No cross-device synchronization blocking.

2. **Mutex Protection**:
   - Redis operations wrapped in `std::lock_guard<std::mutex>` (redis_mutex_)
   - MCP2221 I2C operations protected by its internal mutex
   - No lock held during blocking `subscriber.consume()` calls

3. **Safe Sharing**:
   - Multiple IMU controllers can share single MCP2221 instance (MCP2221's mutex serializes I2C access)
   - Each controller has its own Redis client (sw::redis::Redis is thread-safe within controller)
   - ICM20948 producer thread (started via `imu.start()`) uses TSQueue for thread-safe sample passing

### Guarantees

- **No race conditions**: Redis operations atomic, I2C serialized
- **No deadlocks**: Consistent lock ordering (redis_mutex_ only, no nested locks)
- **Graceful shutdown**: stop_token pattern in producer thread + exit flag check

## Comparison with Motor Controllers

| Aspect | Motor | IMU |
|--------|-------|-----|
| Device | SinglePortExoMotor | ICM20948 |
| Controller | RedisSinglePortController | RedisSingleIMUController |
| Loop Rate | ~60 Hz | ~120-125 Hz |
| Latency | Consistent | 30-200 ms variable |
| Sync Strategy | All devices in `sync:data:cnt` | Independent (n=1) |
| Commands | start, stop, zero, offset, function* | start, stop, zero, read, connect, disconnect |
| Stream | `xdata:<addr>` (motor pos/torque) | `xdata:imu:<id>` (accel/gyro/mag/euler) |

*Note: IMU commands bundled in state parameter; function management not applicable.

## Troubleshooting

### IMU Not Responding

**Symptom**: No data appearing in `xdata:imu:1` stream

**Check**:
1. Redis running: `redis-cli ping`
2. Command sent: `LLEN command:imu:1` (should grow)
3. Errors in log: `XRANGE log - + | grep IMU:imu:1`
4. Controller thread alive: Check main application console

### Commands Being Queued But Not Processed

**Likely Cause**: sync:loop:next not being triggered

**Solution**:
```bash
# Trigger manually in another terminal
redis-cli SET sync:loop:next "trigger"
```

### Graceful Shutdown Not Working

**Cause**: exit flag not properly set or loop not checking it

**Manual Shutdown**:
```bash
redis-cli SET exit 1
# Wait 1 second for controller to notice
# Kill application process if needed
```

## Performance Notes

- **Throughput**: ~120-125 samples/sec (limited by ICM-20948 FIFO sampling rate)
- **Latency**: 30-200 ms (sensor hardware constraint)
- **Redis Overhead**: <1 ms per XADD (negligible)
- **CPU**: Low (async event-driven, no busy polling)

## Future Extensions

1. **Multi-IMU Support**: Instantiate multiple RedisSingleIMUController objects with different device_keys/imu_ids
2. **Burst Configuration**: Add command to adjust FIFO packet size / burst threshold
3. **Streaming Subscription**: Add consumer pattern for real-time sample streaming (XREAD BLOCK)
4. **Function Presets**: Similar to motor function slots (zero/offset profiles)
5. **Timing Synchronization**: Optional sync with motor loop for correlated sensor fusion if needed

## References

- **ICM20948 Driver**: `ICM/icm20948.h/cpp`
- **MCP2221 Driver**: `MCP/mcp2221.h/cpp`
- **RedisTools**: `exoskeleton/core/RedisTools.h/cpp`
- **Sample Data Structure**: `include/sample.h`
- **Example Main**: `main_redis.cpp`
- **Test Program**: `exoskeleton/tests/imu_redis_test.cpp`
