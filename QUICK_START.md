# Quick Start: Redis IMU Integration

## 5-Minute Setup

### Prerequisites
- Redis running on `localhost:6379`
- C++23 compiler (MSVC on Windows)
- CMake 3.20+
- MCP2221 USB driver installed
- ICM-20948 IMU connected via MCP2221

### Step 1: Build

```bash
cd IMU_project
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Step 2: Start IMU Controller

**Option A: Use main_redis.cpp**
```bash
# Modify main.cpp or create main_redis.cpp build target
./IMU_project  # or IMU_project_redis
```

Expected output:
```
[MAIN] Starting Redis IMU Controller
[MAIN] Initializing MCP2221 (USB-I2C bridge)...
[MAIN] MCP2221 opened successfully
[MAIN] Initializing ICM20948 IMU at 0x69
[MAIN] IMU initialized successfully in XXX ms
[MAIN] FIFO configured
[MAIN] Calibration complete
[MAIN] Starting IMU producer thread with TSQueue...
[MAIN] Producer thread started
[MAIN] Creating RedisSingleIMUController...
[MAIN] RedisSingleIMUController created
[MAIN] Launching IMU controller loop in separate thread...
[IMU_THREAD] Starting event loop
[MAIN] IMU system running. Send commands via Redis.
```

### Step 3: Control via Redis

**In another terminal:**

```bash
# Start IMU measurements
redis-cli LPUSH command:imu:1 "t|start|-1|1"

# Check response
redis-cli LRANGE commandres:start:imu:1:t 0 -1
# Output: ["OK:started"]

# Wait a moment for samples to arrive
sleep 1

# Read latest 3 samples
redis-cli XRANGE xdata:imu:1 - + COUNT 3

# Toggle zeroing (set Euler reference)
redis-cli LPUSH command:imu:1 "t|zero|-1|1"
redis-cli LRANGE commandres:zero:imu:1:t 0 -1

# Stop measurements
redis-cli LPUSH command:imu:1 "t|stop|-1|1"

# Graceful shutdown
redis-cli SET exit 1
```

## Command Reference

### Start IMU Producer

```bash
redis-cli LPUSH command:imu:1 "t|start|-1|1"
```

### Stop IMU Producer

```bash
redis-cli LPUSH command:imu:1 "t|stop|-1|1"
```

### Toggle Zeroing (Euler Offset)

```bash
redis-cli LPUSH command:imu:1 "t|zero|-1|1"
```

### Immediate Measurement

```bash
redis-cli LPUSH command:imu:1 "t|read|-1|1"
```

### Reconnect IMU (Reinitialize)

```bash
redis-cli LPUSH command:imu:1 "t|connect|-1|1"
```

### Disconnect IMU

```bash
redis-cli LPUSH command:imu:1 "t|disconnect|-1|1"
```

## Reading Telemetry

### View Latest 5 Samples

```bash
redis-cli XRANGE xdata:imu:1 - + COUNT 5
```

### Monitor in Real-Time (Bash)

```bash
watch -n 0.1 "redis-cli XRANGE xdata:imu:1 - + COUNT 1"
```

### Python Consumer

```python
import redis
import time

r = redis.Redis(host='localhost', port=6379, decode_responses=True)
last_id = '0'

while True:
    # Read new samples since last_id
    messages = r.xread({'xdata:imu:1': last_id}, count=10, block=100)
    
    if messages:
        stream_data = messages[0][1]  # Get list of entries
        for entry_id, data in stream_data:
            print(f"[{entry_id}] Accel: ({data['accel_x']}, {data['accel_y']}, {data['accel_z']})")
            last_id = entry_id
```

### View System Logs

```bash
redis-cli XRANGE log - + | grep "IMU:imu:1"
```

## Troubleshooting

### No Data Appearing?

1. Check Redis is running:
   ```bash
   redis-cli ping
   # Output: PONG
   ```

2. Check commands reached queue:
   ```bash
   redis-cli LRANGE command:imu:1 0 -1
   ```

3. Check for errors in log:
   ```bash
   redis-cli XRANGE log - + | tail -20
   ```

4. Verify IMU hardware:
   - Check MCP2221 driver installed
   - Check ICM-20948 I2C address (0x68 or 0x69)
   - Try running original `main.cpp` to verify hardware works

### Commands Not Processing?

**Cause**: `sync:loop:next` not being triggered

**Solution**: Manually trigger events in Redis:
```bash
# In one terminal
redis-cli SUBSCRIBE "__keyspace@0__:sync:loop:next"

# In another terminal, repeatedly SET the key
watch -n 0.1 "redis-cli SET sync:loop:next '$(date)'"
```

### IMU Controller Thread Exiting Unexpectedly?

Check console output for exceptions and check Redis log stream:
```bash
redis-cli XRANGE log - + | grep ERROR | tail -10
```

## Next Steps

- **Read Full Documentation**: See `REDIS_IMU_INTEGRATION.md`
- **Multi-IMU Setup**: Instantiate multiple controllers with different `imu_id` and `device_key`
- **Motor Integration**: Create `RedisSingleMotorController` instances alongside IMU controller
- **Consumer Application**: Build application reading from `xdata:imu:*` streams for sensor fusion

## Architecture at a Glance

```
┌─────────────────┐
│   Your App      │
│ (main_redis.cpp)│
└────────┬────────┘
         │ Creates
         ▼
┌─────────────────────────────┐
│ RedisSingleIMUController    │ ◄─── Runs in std::jthread
│  - Subscribes sync:loop:next│
│  - RPOP command:imu:1       │
│  - XADD xdata:imu:1         │
│  - LPUSH commandres:*       │
└────────┬────────────────────┘
         │ Controls
         ▼
    ┌─────────────┐
    │ ICM20948    │ ◄─── Hardware
    │   (IMU)     │
    └─────────────┘
         ▲
         │ I2C via
    ┌────┴─────┐
    │ MCP2221  │ ◄─── USB/I2C Bridge
    └──────────┘
         ▲
         │ USB to
      ┌──┴───────────────────┐
      │ Your Computer        │
      └──────────────────────┘
         
Redis Database (localhost:6379)
├── command:imu:1           (LPUSH commands here)
├── xdata:imu:1             (sample stream)
├── commandres:*:imu:1:t    (command responses)
└── log                     (system logs)
```

## File Structure

```
IMU_project/
├── main_redis.cpp                        # Integration example (run this)
├── main.cpp                              # Original simple version
├── CMakeLists.txt                        # Updated with controller sources
├── REDIS_IMU_INTEGRATION.md              # Full documentation
├── QUICK_START.md                        # This file
├── exoskeleton/
│   └── core/
│       ├── RedisSingleIMUController.h    # New: IMU controller interface
│       ├── RedisSingleIMUController.cpp  # New: IMU controller implementation
│       ├── RedisTools.h                  # Updated: added xadd_imu_data()
│       └── RedisTools.cpp                # Updated: added xadd_imu_data()
├── ICM/
│   ├── icm20948.h                        # Existing: IMU driver
│   └── icm20948.cpp                      # Existing: IMU driver
├── MCP/
│   ├── mcp2221.h                         # Existing: USB-I2C bridge (thread-safe)
│   └── mcp2221.cpp                       # Existing: USB-I2C bridge
└── include/
    └── sample.h                          # Existing: ImuSample struct
```

## Key Differences from Original Architecture

| Aspect | Before | After |
|--------|--------|-------|
| Control | Direct C++ function calls | Redis command queue |
| Sampling | Polling in main thread | Async loop in separate thread |
| Frequency | Blocked by calibration | ~120-125 Hz independent |
| Multi-Device | One IMU at a time | Multiple IMUs via separate controllers |
| Coordination | None | Async independent (no blocking) |
| Telemetry | Console stdout | Redis stream (`xdata:imu:*`) |

## Hardware Verification

Before running controller, verify hardware with original code:

```bash
# Restore original main.cpp and run
cmake --build . --config Release
./IMU_project  # Should output: "IMU at address: 0x69 initialized in XXX ms"
```

If hardware test fails, check:
1. MCP2221 driver and USB connection
2. ICM-20948 I2C address (should be 0x69 for IMU1, 0x68 for IMU2)
3. I2C pullup resistors (4.7kΩ recommended)
4. Sensor orientation and soldering

## Support

For issues, check:
- **Full documentation**: `REDIS_IMU_INTEGRATION.md`
- **Source code comments**: `RedisSingleIMUController.h/cpp`
- **Redis logs**: `redis-cli XRANGE log - + | grep IMU`
- **Test program**: `exoskeleton/tests/imu_redis_test.cpp`
