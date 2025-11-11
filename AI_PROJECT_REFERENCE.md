# ExoGUI - AI Assistant Referencia Dokumentum

## Projekt Alapadatok

**Név:** ExoGUI  
**Típus:** Qt6 C++ Desktop Alkalmazás  
**Cél:** Exoskeleton rendszer valós idejű vezérlése és monitorozása  
**Build:** CMake 3.19+, C++20  
**Platform:** Windows (CLion), Linux (Dev Container)  
**Verziókezelés:** Git, branch: `integrate-redis-classes`

## Kritikus Szabályok (MINDIG OLVASD EL!)

### 1. Kódolási Konvenciók
```cpp
// Osztálynevek: PascalCase
class MotorController {};

// Member változók: camelCase_
QString motorName_;
QTimer* updateTimer_;

// Függvények: camelCase
void updateMotorStatus();
void setEnabled(bool);

// Konstansok: UPPER_SNAKE_CASE
constexpr auto DEFAULT_TIMEOUT = 1000;

// Qt specifikus:
- Q_OBJECT macro signal/slot osztályokban
- explicit egyparaméteres konstruktoroknál
- emit kulcsszó signal kibocsátásnál
- nullptr használata (soha NULL)
```

### 2. Fájlkezelési Szabályok
- Header: `#pragma once` (soha ne `#ifndef`)
- Minden új fájlt hozzáadni kell a `CMakeLists.txt`-hez
- Include directory-kat is frissíteni kell CMake-ben
- Header extension: `.h`, implementation: `.cpp`

### 3. Git Workflow
- Branch: `integrate-redis-classes`
- Commit előtt MINDIG: `git add -A && git commit -m "..."`
- Push után user-nek kell Windows-on pull-olnia és rebuild-elnie
- Verbose commit üzenetek (mi, miért, hatás)

### 4. Qt Memory Management
- Parent-child ownership: Qt automatikusan törli a child widget-eket
- SOHA ne használj `delete` parent-el rendelkező widget-re
- `std::unique_ptr` használata nem-Qt objektumokhoz
- `QObject::deleteLater()` Qt objektumok biztonságos törléséhez

### 5. Backend Folder
**SOHA NE MÓDOSÍTSD!** A `BackEnd/` mappa a Python backend kódbázis, ami független.

## Architektúra Áttekintés

### Főbb Komponensek

```
ExoGUI (Qt Application)
│
├── Application/                 # Entry point
│   ├── main.cpp                # App indítás
│   └── MainWindow.*            # Fő ablak, page management
│
├── Core/Redis/                 # Redis integráció (KÖZPONTI!)
│   ├── Client/                 # Redis client réteg
│   │   ├── CommandClient.*         # Parancs küldés motorokhoz
│   │   ├── RedisTelemetryClient.*  # Telemetry stream olvasás
│   │   └── TelemetryController.*   # UI-Redis koordináció
│   ├── Config/                 # Konfiguráció és beállítások
│   │   ├── RedisKeys.h             # Redis kulcs konstansok
│   │   ├── Settings.*              # conf:env struktúra
│   │   ├── UserParams.*            # conf:user struktúra
│   │   └── EnvLoader.*             # .env fájl betöltés
│   ├── Data/                   # Adatkezelés
│   │   ├── RedisFacade.*           # Redis hozzáférés facade
│   │   └── SlotFunctionManager.*   # Spring function kezelés
│   ├── Infrastructure/         # Docker és utility
│   │   ├── RedisDockerManager.*    # Docker konténer kezelés
│   │   └── RedisTools.*            # Utility függvények
│   └── Testing/                # Tesztelés
│       └── TelemetrySimulator.cpp  # Test szimulátor
│
├── Pages/                      # Alkalmazás oldalak
│   ├── HomePage/               # Motor státusz, real-time plot
│   ├── SettingsPage/           # Motor konfiguráció
│   ├── ArmSettingsPage/        # Fizikai mérések (user params)
│   └── SpringPage/             # Spring function editor
│
├── Widgets/                    # Újrafelhasználható komponensek
│   ├── SlotWidget/             # Motor kártya (position, torque, enabled)
│   ├── LivePlotWidget/         # Real-time QCustomPlot
│   ├── FunctionEditorWidget/   # Spring function szerkesztő
│   ├── ActionButtons/          # Parancs gombok
│   ├── TopBar/                 # Navigációs bar
│   ├── ToggleSwitch/           # Custom toggle widget
│   └── Settings/               # Settings UI komponensek
│
└── ThirdParty/
    └── QCustomPlot/            # Plotting library (NE MÓDOSÍTSD!)
```

### Redis Kommunikáció

#### 1. Parancs Küldés (Command Flow)
```
UI (ActionButtons)
  → CommandClient::sendCommand(cmd, addr, params)
  → Redis: RPUSH command:{addr} "timestamp|cmd|addr|params"
  → Backend: LPOP command:{addr}
  → Backend feldolgozza
  → Backend: SET commandres:{cmd}:{addr}:{uuid} "OK:value" vagy "ER:error"
  → CommandClient::checkCommandResponse()
  → UI feedback (500ms timer)
```

**Példa:**
```cpp
// Parancs küldés egyedi ID-val
auto result = commandClient_->sendCommand("enable", 1, "");
// result.uniqueId tartalmazza az UUID-t

// Response ellenőrzés (500ms-enként)
auto response = commandClient_->checkCommandResponse("enable", 1, uniqueId);
if (response.found) {
    if (response.success) {
        // OK: value
    } else {
        // ER: error
    }
}
```

#### 2. Telemetry Olvasás (Data Flow)
```
Backend
  → Redis: XADD xdata:{addr} * enabled 1 position 123 torque 45 ...
  → RedisTelemetryClient (worker thread)
  → Redis: XREAD BLOCK 250 STREAMS xdata:0 xdata:1 ... {lastId}
  → emit sampleReceived(address, sample)
  → TelemetryController::handleSampleReceived()
  → SlotWidget::setValues(position, torque)
  → LivePlotWidget::addDataPoint(motor, timestamp, position)
```

#### 3. Konfigurációs Adatok

**conf:env (HASH)** - Rendszer konfiguráció
```redis
HGETALL conf:env
motor_e_flex     "CSTNY004"
motor_e_ext      "CSTNY005"
multiport_motors "1"
mock_motors      "0"
restapi_port     "8002"
```

**conf:user (HASH)** - User specifikus paraméterek
```redis
HGETALL conf:user
user_id          "0"
bodyweight       "75"
upper_arm        "350"
motorforce_min   "0"
motorforce_max   "127"
motorforce       "50"
assist           "50"
```

**run:addrs (HASH)** - Motor address mapping
```redis
HGETALL run:addrs
e_flex    "0|CSTNY004"
e_ext     "1|CSTNY005"
s_flex    "2|CSTNY006"
```
Format: `address|serial_number`

**slot:function:{addr}:{slot} (STRING)** - Spring function JSON
```json
{
  "name": "test1",
  "values": [0, 10, 20, ..., 100]
}
```
 
## Redis Docker Manager (Core/Redis/Infrastructure)

Ez a projektfrissítés külön fejezetet kap a Docker-alapú Redis indítás és kezelése miatt. A `Core/Redis` mappa átstrukturálása után a Docker indítást a `RedisDockerManager` osztály végzi, amely a következőket valósítja meg és figyelembe veszi:

- Keresési/hatókör:
    - A `Core/Redis` most logikailag felosztva: `Client`, `Config`, `Data`, `Infrastructure`, `Testing` mappák.
    - A Docker-specifikus kód a `Infrastructure/RedisDockerManager.*` fájlokban található.

- Főbb implementációs döntések (miért és hogyan):
    - QProcess::execute() használata: szinkron futtatás, megbízhatóbb viselkedés Qt GUI alkalmazásokban, különösen Windows-on (system() máshogy viselkedik GUI folyamatokban és rossz exit code-okat adhat).
    - Docker útvonal detektálás: a Qt alkalmazás nem mindig örökli a konzol PATH-ját Windows-on; a manager megpróbálja megtalálni a `docker.exe`-t (először `docker` a PATH-ból, majd tipikus telepítési helyek: `C:/Program Files/Docker/Docker/resources/bin/docker.exe`, stb.). A megtalált útvonalot az `m_dockerCommand` változó tárolja.
    - runDockerCommand(): központosított wrapper, amely minden docker hívást a felismert `m_dockerCommand` használatával hajt végre.
    - popen()/pclose() hívások frissítése: a fájlútvonal szóközök miatt a `popen()`-hoz idézőjelezve kerül a teljes `docker` parancs (pl. `"C:/Program Files/.../docker.exe" ps ...`).
    - Alapértelmezett image: a kód most `redis:latest`-et használ alapértelmezett helyett (korábban `redis:7-alpine`).

- Ismert Windows-specifikus probléma és megoldások:
    - Előfordulhat a `docker-credential-desktop` hiba: `docker: error getting credentials - err: exec: "docker-credential-desktop": executable file not found in %PATH%`. Ez nem a manager hibája, hanem a Docker credential helper hiánya/konfigurációja a Windows rendszerben.
        - Gyors megoldás: töltsd le a szükséges image-et manuálisan Docker Desktop-ból vagy terminálból (`docker pull redis:latest`).
        - Hosszabb távú: ellenőrizd vagy szerkeszd a `C:\Users\<user>\.docker\config.json` fájlt és távolítsd el vagy módosítsd a credential helper bejegyzést, ha szükséges.

- Tesztelési lépések (helyi Windows / Dev container):
    1. Győződj meg róla, hogy Docker Desktop fut és a CLI elérhető; a `RedisDockerManager` logja megmutatja a megtalált `docker.exe` útvonalat.
 2. Ha nem szeretnéd, hogy a futtató alkalmazás képes legyen letölteni képeket, töltsd le előre: `docker pull redis:latest` (PowerShell vagy CMD esetén használd a teljes útvonalat, ha szükséges: `"C:\Program Files\Docker\Docker\resources\bin\docker.exe" pull redis:latest`).
 3. Rebuild: miután a kódot frissítetted, építsd újra a projektet (`cmake --build build`) és indítsd el az alkalmazást.

Az új implementáció célja, hogy a Qt GUI környezetben megbízhatóan indítsa és kezelje a Redis konténert Windows rendszereken is, minimalizálva a PATH/credential helper okozta futás közbeni hibákat.

## Redis Docker Manager (Core/Redis/Infrastructure)

### Áttekintés
A `RedisDockerManager` osztály felelős a Redis Docker konténer automatikus indításáért és életciklus-kezeléséért az alkalmazás indításakor. A `Core/Redis/Infrastructure/` mappában található.

### Fő Funkcionalitás

**Konténer Indítás:**
```cpp
bool startRedis(const QString& containerName = "exogui-redis",
                int port = 6379,
                const QString& imageName = "redis:latest");
```

**Működés:**
1. Docker daemon elérhetőség ellenőrzése
2. Meglévő konténer keresése (név alapján)
3. Ha létezik és fut: redis-cli ping teszt
4. Ha létezik de nem fut, vagy nem responsive: konténer törlése
5. Új konténer indítása: `docker run -d -p 6379:6379 --name exogui-redis redis:latest`
6. Readiness check (redis-cli ping) 500ms-enként, max 10 másodpercig
7. Signal: `redisReady()` ha sikeres

### Windows-Specifikus Implementáció

**Probléma:** Qt GUI alkalmazás Windows-on nem örökli a teljes PATH környezeti változót a konzolból.

**Megoldás: Docker Path Detection**
```cpp
QString m_dockerCommand;  // Stores discovered docker.exe path

bool isDockerRunning() const {
    // Try multiple paths on Windows:
    QStringList possiblePaths = {
        "docker",  // Try PATH first
        "C:/Program Files/Docker/Docker/resources/bin/docker.exe",
        "C:/Program Files/Docker/Docker/resources/docker.exe",
        "C:/Program Files (x86)/Docker/Docker/resources/bin/docker.exe"
    };
    
    // Test each path with 'docker --version'
    // Save working path to m_dockerCommand
}
```

**Központosított Végrehajtás:**
```cpp
int runDockerCommand(const QStringList& args) const {
    return QProcess::execute(m_dockerCommand, args);
}

// Minden docker parancs ezt használja:
runDockerCommand(QStringList() << "ps" << "-q");
runDockerCommand(QStringList() << "run" << "-d" << ...);
```

### QProcess vs system()

**Miért QProcess::execute()?**
- `std::system()` rossz exit code-okat ad vissza Qt GUI környezetben Windows-on
- `QProcess::execute()` szinkron, megbízható exit code kezelés
- Natív Qt megoldás, cross-platform támogatás

**Exit Code Jelentések:**
- `0`: Sikeres végrehajtás
- `1-255`: Docker parancs hibakódja
- `-1`: Process crash
- `-2`: Process nem indult el (executable not found)

### popen() Hívások Javítása

**Probléma:** `checkContainerExists()` és `checkContainerRunning()` hardcoded `"docker"` stringet használtak.

**Megoldás:**
```cpp
// Előtte:
std::string cmd = "docker ps -a --filter name=^" + containerName.toStdString() + "$";

// Utána (m_dockerCommand használata, idézőjelek a szóközök miatt):
std::string cmd = "\"" + m_dockerCommand.toStdString() + "\" ps -a --filter name=^" 
                  + containerName.toStdString() + "$";
```

### Alapértelmezett Image

**Változás:** `redis:7-alpine` → `redis:latest`

**Indok:** User már letöltötte a `redis:latest` image-et lokálisan.

### Ismert Probléma: Docker Credential Helper

**Hibaüzenet:**
```
docker: error getting credentials - err: exec: "docker-credential-desktop": 
executable file not found in %PATH%
```

**Mit jelent:** Docker Desktop credential helper nincs konfigurálva/elérhető.

**Mikor jelentkezik:** Image pull (letöltés) során, NEM konténer indításkor.

**Megoldás:**
1. **Gyors:** Töltsd le előre az image-et manuálisan
   ```powershell
   docker pull redis:latest
   ```
   
2. **Hosszú távú:** Módosítsd `C:\Users\<user>\.docker\config.json`:
   ```json
   {
     "auths": {},
     "credsStore": ""  // Töröld ezt a sort vagy hagyd üresen
   }
   ```

### Debug Log Üzenetek

```
[RedisDockerManager] Checking if Docker is running...
Docker version 28.4.0, build d8eb465
[RedisDockerManager] Found docker at: "C:/Program Files/Docker/Docker/resources/bin/docker.exe"
[RedisDockerManager] Docker check (using: "...docker.exe") exit code: 0
[RedisDockerManager] Docker is responsive
[RedisDockerManager] Starting new Redis container...
[RedisDockerManager] ✓ Redis is ready and responsive
```

### Tesztelési Checklist

1. **Docker Desktop fut?** Ellenőrizd a system tray-ben
2. **Image letöltve?** `docker images | grep redis`
3. **Port szabad?** `netstat -ano | findstr :6379` (Windows)
4. **Rebuild után:** CMake build + futtatás
5. **Log ellenőrzés:** Nézd meg melyik docker.exe útvonalat találta meg

### Főbb Osztály Metódusok

```cpp
// Lifecycle
bool startRedis(containerName, port, imageName);
bool stopRedis();

// Docker checks
bool isDockerRunning() const;
bool ensureDockerRunning();

// Container checks
bool checkContainerExists(const QString& name) const;
bool checkContainerRunning(const QString& name) const;
bool checkRedisReadiness();

// Container operations
bool stopContainer(const QString& name);
bool removeContainer(const QString& name);
bool pullDockerImage(const QString& imageName);

// Internal helper
int runDockerCommand(const QStringList& args) const;
```

### Signals
```cpp
void redisReady();                      // Redis konténer elérhető
void redisStatusChanged(bool running);  // Státusz változás
void errorOccurred(const QString& msg); // Hiba történt
```

## Fontosabb Osztályok Részletesen

### TelemetryController
**Felelősség:** Redis telemetry és UI közötti koordináció

**Kulcs funkciók:**
- `startTelemetry()` - RedisTelemetryClient indítás
- `handleSampleReceived()` - Telemetry feldolgozás, UI update
- `updateHomeMotors()` - SlotWidget-ek létrehozása/frissítése
- `loadRedisSettings()` - conf:env és conf:user betöltés
- `checkUserParamsChanges()` - conf:user:set flag figyelés (1s timer)
- `processBatchedUpdates()` - Batch UI update (50ms timer, 20 Hz)

**Optimalizációk:**
- Batch processing: 50ms timer, max 100 pending update
- Value caching: csak változás esetén UI update
- Early return: üres/invalid sample kihagyása

**Signals:**
```cpp
void settingsLoaded(const exoskeleton::settings::Settings& settings);
void userParamsLoaded(const exoskeleton::settings::UserParams& params);
void redisUriChanged(const QString& uri);
```

### RedisTelemetryClient
**Felelősség:** Redis stream olvasás worker thread-ben

**Worker Thread Pattern:**
```cpp
// Main thread
client_->start();  // QThread::start() → moveToThread

// Worker thread
void telemetryLoop() {
    while (!stopRequested_) {
        xread_streams();  // BLOCKING, 250ms timeout
        parse_samples();
        emit sampleReceived();  // Thread-safe signal
    }
}
```

**Stream State Tracking:**
```cpp
struct StreamState {
    int address;
    std::string lastId;  // "1234567890-0"
    QString motorName;
};
std::unordered_map<std::string, StreamState> streamStates_;
```

### RedisFacade
**Felelősség:** Központi Redis hozzáférés, Settings/UserParams kezelés

**Főbb metódusok:**
```cpp
// Environment
Settings load_env(bool required = true);

// User params
UserParams load_user_params();
void set_user_params(const UserParams& params);
void set_user_param(const string& key, const string& value);

// Flags
bool user_params_changed(bool clear = false);  // conf:user:set
bool db_params_changed(bool clear = false);    // dbchanged

// Initialization
bool initialize_env_from_file(const path& env_path, bool overwrite);
void initialize_run_addrs_from_env();
```

**Settings Pattern (Python kompatibilitás):**
```cpp
// Load
auto settings = redisFacade_->load_env(false);
qDebug() << settings.motor_e_flex;

// User params load-modify-save
auto params = redisFacade_->load_user_params();
params.bodyweight = 80;
params.motorforce = 60;
redisFacade_->set_user_params(params);
```

### SlotWidget
**Felelősség:** Egy motor állapotának megjelenítése

**UI Elemek:**
- **Toggle Switch:** Plot line láthatóság (NEM motor enable/disable!)
- **ENABLED/DISABLED pill:** Motor aktív státusz (zöld/piros)
- **Color indicator:** Motor színkódja (plot line színe)
- **Position/Torque:** Numerikus értékek
- **Slot ID:** Aktív slot index megjelenítése
- **QCustomPlot:** Spring function görbe (ha van)
- **3 gomb:** Add Function, Create Function, Select Slot

**Fontos:**
```cpp
// Motor aktív státusz (backend enabled mező)
void setMotorActive(bool active);  // → ENABLED/DISABLED pill

// Toggle switch CSAK a plot line-t vezérli!
connect(toggle, &ToggleSwitch::toggled, this, &SlotWidget::plotVisibilityChanged);

// Telemetry update
void setValues(int position, int torque);  // Cached, csak változás esetén update

// Spring function
void setFunctionEmpty(bool isEmpty);  // Ha nincs function, QCustomPlot hidden
void setGraph(const QVector<double>& values);  // 101 érték (0-100)
```

### LivePlotWidget
**Felelősség:** 7 motor real-time position plot

**Optimalizációk:**
```cpp
// 20 FPS rendering
scheduleReplot();  // 50ms QTimer
performScheduledReplot();  // Batch replot

// Motor management
void addMotor(const QString& name, const QColor& color);
void removeMotor(const QString& name);
void setMotorVisible(const QString& name, bool visible);

// Data
void addDataPoint(const QString& motor, double time, double position);
void setTimeWindow(double seconds);  // 10s default
```

**QCustomPlot konfiguráció:**
```cpp
// 7 graph, mindegyik más színű (MotorColorScheme)
// X tengely: time (relative, 0-10s)
// Y tengely: position (-180 - 180 fok)
// Antialiasing: OFF (performance)
// Batch rendering: dataChanged flag
```

### ActionButtons
**Felelősség:** Motor parancs UI, command response tracking

**Parancsok:**
```cpp
// Alapparancsok
void handleConnect();     // → "connect"
void handleDisconnect();  // → "disconnect"
void handleEnable();      // → "enable"
void handleDisable();     // → "disable"
void handleZero();        // → "zero"
void handleOffset();      // → "offset" + érték
void handleRead();        // → "read"

// Function parancsok (fn_get, fn_select, fn_upload)
void handleFunctionsMenu();
```

**Command Response Tracking:**
```cpp
struct PendingCommand {
    QString command;
    int address;
    QString uniqueId;  // UUID
    QDateTime sentTime;
};

// 500ms timer
void checkPendingResponses() {
    for (pending in pendingCommands) {
        if (timeout > 5s) { remove; continue; }
        
        auto response = checkCommandResponse(cmd, addr, uuid);
        if (response.found) {
            if (response.success) {
                showStatus("✓ enable (addr 1): OK");
            } else {
                showStatus("✗ enable (addr 1): error");
            }
            remove;
        }
    }
}
```

**Apply to All:**
- Checkbox: applyToAllCheckbox_
- Ha checked: parancs megy minden motor címre (addresses_ hash)
- Status: "Command enable sent to all 7 motors successfully"

### SlotFunctionManager
**Felelősség:** Spring function CRUD Redis-ben

**Redis kulcsok:** `slot:function:{address}:{slot}`

**Műveletek:**
```cpp
// Upload
uploadSlotFunction(addr, slot, name, values);
// → SET slot:function:{addr}:{slot} '{"name":"test","values":[...]}'

// Fetch
fetchSlotFunction(addr, slot);
// → GET slot:function:{addr}:{slot}
// → parse JSON
// → emit functionFetched(addr, slot, name, values)

// List
listSlotFunctions();
// → KEYS slot:function:*
// → emit functionsListed(functions)

// Delete
deleteSlotFunction(addr, slot);
// → DEL slot:function:{addr}:{slot}
```

**JSON formátum:**
```json
{
  "name": "function_name",
  "values": [0, 5, 10, 15, ..., 100]  // 101 érték (0-100%)
}
```

### FunctionEditorWidget
**Felelősség:** Spring function szerkesztés (SpringPage-en)

**Komponensek:**
- QCustomPlot: Interactive görbe szerkesztés
- Mouse drag: Értékek módosítása
- Smooth curve: Catmull-Rom spline interpoláció
- Save button: Upload to Redis
- Name input: Function név

**Interpoláció:**
```cpp
// 101 control point (0-100)
// Catmull-Rom spline → smooth curve
// Y tengely: 0-100% (torque százalék)
```

## .env Automatikus Betöltés

### Indítási Folyamat
```
1. main.cpp: QApplication app
2. MainWindow konstruktor
3. TelemetryController konstruktor
4. RedisFacade::initialize_env_from_file(".env", overwrite=false)
5. EnvLoader::load_env_file()
   - Parse .env (exo_ prefix eltávolítás)
   - Strip quotes
6. EnvLoader::upload_to_redis()
   - Check: EXISTS conf:env
   - If not exists: HSET conf:env ...
7. RedisFacade::initialize_run_addrs_from_env()
   - Load conf:env
   - Build run:addrs: motor_name → "address|serial"
   - HSET run:addrs ...
8. TelemetryController::loadRedisSettings()
9. RedisTelemetryClient::start()
   - refreshEnv() → HGETALL conf:env
   - refreshAddresses() → HGETALL run:addrs
   - emit envLoaded(), addressesUpdated()
10. TelemetryController::updateHomeMotors()
11. HomePage::setMotors() → SlotWidget-ek létrehozása
```

### .env Fájl Keresési Utak (Prioritás sorrendben)
```cpp
1. "." (current working directory)
2. QCoreApplication::applicationDirPath() + "/.env"  // .exe mellett
3. QDir::currentPath() + "/.env"                      // Qt current path
4. QDir::homePath() + "/.env"                         // Home directory
```

**Debug output:**
```
[TelemetryController] Trying .env path: "C:/path/to/exe/.env"
[TelemetryController] Found .env at: "C:/path/to/exe/.env"
[EnvLoader] Loaded 11 entries from .env
[EnvLoader] conf:env exists in Redis: NO
[EnvLoader] Uploading 11 keys to conf:env...
[RedisFacade] Adding motor: e_flex = 0|CSTNY004
[RedisFacade] Initialized run:addrs with 7 motors
```

## Hibakeresés és Debug

### Console Output Prefixek
```
[TelemetryController] - Telemetry koordináció
[RedisTelemetryClient] - Stream olvasás
[CommandClient] - Parancs küldés
[RedisFacade] - Redis facade műveletek
[EnvLoader] - .env betöltés
[SlotFunctionManager] - Function kezelés
[ActionButtons] - UI parancs gombok
[SpringPage] - Spring page műveletek
```

### Gyakori Hibák és Megoldások

#### 1. SlotWidget-ek nem jelennek meg HomePage-en
**OK:** Nincs `run:addrs` a Redis-ben  
**Megoldás:** `RedisFacade::initialize_run_addrs_from_env()` meghívása startup-kor

#### 2. conf:env üres vagy hiányzó
**OK:** .env fájl nem található vagy nem olvasható  
**Megoldás:** 
- Ellenőrizd .env elérési utat (debug output)
- Másold .env-t az .exe mellé
- Vagy állítsd be working directory-t CMake/CLion-ban

#### 3. Motor "DISABLED" induláskor (HomePage)
**OK:** `setMotorActive(true)` hiányzik motor létrehozáskor  
**Megoldás:** HomePage::setMotors() -> `slot->setMotorActive(true);`

#### 4. Command nem kerül végrehajtásra
**OK:** Backend nem fut vagy Redis connection hiba  
**Ellenőrzés:**
```bash
# Redis működik?
redis-cli PING  # → PONG

# Command queue létezik?
redis-cli LLEN command:0  # → 0 vagy N

# Backend olvassa?
redis-cli LLEN command:0  # Csökken?
```

#### 5. Telemetry nem érkezik
**OK:** Backend nem küld xdata streamet vagy Redis connection hiba  
**Ellenőrzés:**
```bash
# Stream létezik?
redis-cli XLEN xdata:0  # → 0 vagy N

# Stream növekszik?
redis-cli XLEN xdata:0  # Nő az idő múlásával?

# RedisTelemetryClient fut?
# Console: "RedisTelemetryClient started" vagy error?
```

#### 6. Windows build error: "set_user_params is not a member"
**OK:** Fájlok nem szinkronizáltak Git-tel (dev container vs Windows)  
**Megoldás:**
```bash
# Dev container-ben
git add -A && git commit && git push

# Windows-on
git pull
# Clean + Rebuild in CLion
```

## Build és Deploy

### CMake Build
```bash
# Configure
cmake -B build -DQt6_DIR=/path/to/Qt6/lib/cmake/Qt6

# Build
cmake --build build --target ExoGUI

# Build simulator
cmake --build build --target TelemetrySimulator
```

### Új Fájl Hozzáadása
```cmake
# CMakeLists.txt
add_executable(ExoGUI
    ...
    NewFolder/NewFile.cpp
    NewFolder/NewFile.h
)

target_include_directories(ExoGUI PRIVATE
    ...
    ${CMAKE_SOURCE_DIR}/NewFolder
)
```

### Windows Deploy (Optional)
```cmake
# Uncomment in CMakeLists.txt
add_custom_command(TARGET ExoGUI POST_BUILD
  COMMAND "${Qt6_DIR}/../../../bin/windeployqt.exe" "$<TARGET_FILE:ExoGUI>"
  VERBATIM)
```

## Hasznos Kódrészletek

### Redis Connection Check
```cpp
try {
    auto redis = sw::redis::Redis("tcp://127.0.0.1:6379");
    redis.ping();  // Throws if connection fails
} catch (const std::exception& e) {
    qWarning() << "Redis error:" << e.what();
}
```

### Settings Load/Save Pattern
```cpp
// Load
auto settings = redisFacade_->load_env(false);
if (!settings.motor_e_flex.empty()) {
    qDebug() << "E_FLEX:" << QString::fromStdString(settings.motor_e_flex);
}

// User params (load existing, modify, save back)
try {
    auto params = redisFacade_->load_user_params();
    params.bodyweight = 75;
    redisFacade_->set_user_params(params);
} catch (const std::exception& e) {
    // conf:user doesn't exist yet
    UserParams params;
    params.bodyweight = 75;
    params.motorforce_min = 0;
    params.motorforce_max = 127;
    // ... set all required fields
    redisFacade_->set_user_params(params);
}
```

### Signal/Slot Connection
```cpp
// Lambda with capture
connect(button, &QPushButton::clicked, this, [this, motorName]() {
    sendCommand("enable", motorName);
});

// Member function
connect(timer, &QTimer::timeout, this, &MyClass::onTimeout);

// Qt::QueuedConnection (thread-safe)
connect(worker, &Worker::dataReady, this, &MyClass::onData, Qt::QueuedConnection);
```

### QCustomPlot Setup
```cpp
auto* plot = new QCustomPlot(parent);
plot->setInteraction(QCP::iRangeDrag, true);
plot->setInteraction(QCP::iRangeZoom, true);
plot->xAxis->setLabel("Time (s)");
plot->yAxis->setLabel("Position (deg)");
plot->xAxis->setRange(0, 10);
plot->yAxis->setRange(-180, 180);

// Add graph
auto* graph = plot->addGraph();
graph->setPen(QPen(Qt::red, 2));
graph->setData(xData, yData);
plot->replot();
```

### Batch UI Update Pattern
```cpp
// Member variables
QTimer* batchTimer_;
QList<UpdateData> pendingUpdates_;
bool dataChanged_{false};

// Setup
batchTimer_ = new QTimer(this);
batchTimer_->setInterval(50);  // 20 Hz
connect(batchTimer_, &QTimer::timeout, this, &MyClass::processBatch);
batchTimer_->start();

// Add to batch
void addUpdate(const UpdateData& data) {
    pendingUpdates_.append(data);
    dataChanged_ = true;
}

// Process batch
void processBatch() {
    if (!dataChanged_ || pendingUpdates_.isEmpty()) {
        return;
    }
    
    for (const auto& update : pendingUpdates_) {
        // Apply update to UI
    }
    
    pendingUpdates_.clear();
    dataChanged_ = false;
}
```

## Python Backend Kompatibilitás

### Fontos Fájlok (Referencia)
```
python_code_for_reference/
├── _redis_tools.py    # RedisFacade Python megfelelője
├── _settings.py       # Settings, UserParams Python verziók
└── .env.sample        # Példa .env konfiguráció
```

### Kulcs Különbségek

| Szempont | Python | C++ (ExoGUI) |
|----------|--------|--------------|
| Settings betöltés | pydantic BaseSettings | from_map() static method |
| User params | dataclass | struct with from_map() |
| Redis lib | redis-py | redis++/hiredis |
| .env prefix | exo_ | exo_ (ugyanaz) |
| conf:env access | settings() singleton | RedisFacade::load_env() |
| conf:user access | load_user_params() | RedisFacade::load_user_params() |

### Kompatibilitási Checklist
- ✅ Redis key names: Ugyanazok (RedisKeys.h)
- ✅ conf:env fields: Motor names, boolok, portok
- ✅ conf:user fields: 12 field (user_id, bodyweight, arm lengths, motor params)
- ✅ Stream format: xdata:{addr}, XADD/XREAD
- ✅ Command format: command:{addr}, timestamp\|cmd\|addr\|params
- ✅ Function format: slot:function:{addr}:{slot}, JSON
- ✅ .env prefix: exo_ (eltávolítva Redis-ben)

## Teljesítmény Optimalizációk

### Alkalmazott Technikák
1. **Batch Rendering (20 Hz):**
   - LivePlotWidget: 50ms QTimer
   - Több addDataPoint() → 1 replot()

2. **Batch UI Updates (20 Hz):**
   - TelemetryController: 50ms batch processing
   - Több telemetry sample → 1 UI update batch

3. **Value Caching:**
   - SlotWidget: lastPosition_, lastTorque_
   - Csak változás esetén setText()

4. **Early Return:**
   - Invalid/empty sample kihagyása
   - Unchanged value kihagyása

5. **Resize Throttling:**
   - HomePage: 100ms QTimer debounce
   - Sok resize event → 1 layout recompute

6. **String Caching:**
   - SlotWidget: cachedEnabledStyle_
   - QStringLiteral használata

7. **Worker Thread:**
   - RedisTelemetryClient: QThread
   - Blocking XREAD nem blokkolja UI-t

### Mérési Eredmények
- GUI: 60 FPS (16.6ms frame time)
- Telemetry update: 50ms batch (20 Hz)
- Plot render: 50ms (20 FPS)
- Command response check: 500ms

## Dokumentáció Hivatkozások

### Meglévő Docs
- `PROJECT_STRUCTURE.md` - Folder structure
- `ARCHITECTURE_DIAGRAM.md` - System architecture
- `CODE_DOCUMENTATION.md` - API documentation
- `ENV_AUTOLOAD_FEATURE.md` - .env loading details
- `REDIS_IMPLEMENTATION_ANALYSIS.md` - Redis correctness verification
- `SLOT_FUNCTION_MANAGEMENT.md` - Spring function details
- `REFACTORING_SUMMARY.md` - Historical refactoring notes

### External Documentation
- Qt6 Documentation: https://doc.qt.io/qt-6/
- redis++: https://github.com/sewenew/redis-plus-plus
- QCustomPlot: https://www.qcustomplot.com/

## Gyors Referencia - Redis Kulcsok

```redis
# Configuration
conf:env              # HASH: System configuration
conf:user             # HASH: User parameters
conf:user:set         # STRING: "1" if conf:user changed
dbchanged             # STRING: "1" if database params changed
currentuserid         # STRING: Active user ID

# Motor mapping
run:addrs             # HASH: motor_name → "address|serial"

# Commands
command:{addr}        # LIST: Commands to execute (FIFO queue)
commandres:{cmd}:{addr}:{uuid}  # STRING: "OK:value" or "ER:error"

# Telemetry
xdata:{addr}          # STREAM: Motor telemetry data

# Functions
slot:function:{addr}:{slot}  # STRING: JSON spring function
```

## Gyors Referencia - Motor Nevek

```
Elbow:
- e_flex (E_FLEX)     Address: 0
- e_ext  (E_EXT)      Address: 1

Shoulder:
- s_flex     (S_FLEX)       Address: 2
- s_ext      (S_EXT)        Address: 3
- s_add_pron (S_ADD_PRON)   Address: 4
- s_abd      (S_ABD)        Address: 5
- s_add_sup  (S_ADD_SUP)    Address: 6
```

## Changelog (Recent)

### 2025-10-27 - integrate-redis-classes branch

**Command Response Tracking:**
- UUID generálás minden parancshoz
- commandres:{cmd}:{addr}:{uuid} ellenőrzés
- OK:/ER: parsing
- ActionButtons: 500ms timer, 5s timeout
- UI feedback: ✓/✗ jelzések

**run:addrs Initialization:**
- Automatikus létrehozás conf:env-ből
- Format: motor_name → "address|serial"
- TelemetryController startup-ban
- Fix: SlotWidget-ek megjelenése HomePage-en

**.env Auto-loading:**
- EnvLoader class (parse + upload)
- Multi-path keresés (.exe mellett, working dir, home)
- exo_ prefix eltávolítás
- Automatikus conf:env inicializálás

**Motor Active State:**
- setMotorActive(true) default HomePage-en
- Telemetry felülírja enabled mezővel
- ENABLED/DISABLED pill zöld/piros

**ArmSettingsPage Simplification:**
- Csak fizikai mérések (7 mező)
- Motor paraméterek megőrzése save-nél
- Load existing → update only physical → save back

**Redis Integration:**
- RedisFacade bővítés (set_user_params, initialize_run_addrs)
- Settings, UserParams Python kompatibilitás
- SlotFunctionManager JSON kezelés
- TelemetryController batch processing (50ms, 20 Hz)

**Performance:**
- LivePlotWidget 20 FPS (50ms timer)
- Value caching, early return
- Resize throttling (100ms)
- QStringLiteral string optimization

---

**Dokumentum verzió:** 1.0  
**Utolsó frissítés:** 2025-10-27  
**Branch:** integrate-redis-classes  
**Cél:** AI Assistant gyors és precíz munkája
