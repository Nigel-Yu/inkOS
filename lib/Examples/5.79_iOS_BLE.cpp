#include <Arduino.h>
#include <NimBLEDevice.h>

// Apple Media Service (AMS) Specifications
const char* AMS_SERVICE_UUID           = "89D3502B-0F36-433A-8EF4-C502AD55F8DC";
const char* AMS_ENTITY_UPDATE_UUID    = "2F7C0CE7-1D95-442F-B053-AEA1E4B83A2A";
const char* AMS_ENTITY_ATTRIBUTE_UUID = "C6B2F38C-1067-4384-90F6-FF991448A90B";
const char* AMS_REMOTE_COMMAND_UUID   = "9B3C81D8-57B1-4A8A-B8DF-E742C686A9D3";

// Thread-safe UI Transfer Buffers (Global state)
struct MediaMetadata {
    char title[64]  = "Unknown Title";
    char artist[64] = "Unknown Artist";
    char album[64]  = "Unknown Album";
    bool isPlaying  = false;
    bool volatile hasUpdates = false; 
} currentTrack;

// State management variables
uint16_t volatile activeConnHandle = BLE_HS_CONN_HANDLE_NONE;
bool volatile processDiscovery     = false;

// Forward Declarations
bool initializeAMSClient(NimBLEClient* pClient);
void amsNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

// Global State Updates
NimBLEAddress peerAddr; // Stores the connected iPhone's MAC address
uint16_t volatile activeConnHandle = BLE_HS_CONN_HANDLE_NONE;
bool volatile processDiscovery     = false;

class TargetServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        Serial.printf("iPhone connected (Handle: %d). Bond state starting.\n", desc->conn_handle);
        activeConnHandle = desc->conn_handle;
        
        // Capture the physical MAC address of the connected phone
        peerAddr = NimBLEAddress(desc->peer_ota_addr); 
        processDiscovery = true; 
    }

    void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        Serial.println("iPhone disconnected. Resetting state...");
        activeConnHandle = BLE_HS_CONN_HANDLE_NONE;
        processDiscovery = false;
        NimBLEDevice::startAdvertising();
    }
};

void setup() {
    Serial.begin(115200);

    // 1. Initialize NimBLE Stack
    NimBLEDevice::init("inkOS Display");

    // 2. Configure Security & Mandatory iOS Bonding Parameters
    // iOS requires active bonding, Man-In-The-Middle protection, and Secure Connections for AMS access
    NimBLEDevice::setSecurityAuth(/*bonding=*/true, /*mitm=*/true, /*sc=*/true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    // 3. Spawning Server to catch connection
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new TargetServerCallbacks());

    // 4. Configure Advertising Data
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    // Advertising the AMS service UUID targets background connection capabilities on iOS
    pAdvertising->addServiceUUID(AMS_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("BLE Stack initialized. Advertising inkOS Display...");
}

void loop() {
    if (processDiscovery) {
        processDiscovery = false;
        
        // Delay slightly to give the native iOS bonding process room to stabilize keys
        delay(1500); 

        NimBLEClient* pClient = nullptr;

        // 1. Check if a client instance already exists for this peer
        if (NimBLEDevice::getClientListSize() > 0) {
            pClient = NimBLEDevice::getClientByPeerAddress(peerAddr);
        }

        // 2. If not, create a new local client entity
        if (!pClient) {
            pClient = NimBLEDevice::createClient();
        }

        if (pClient) {
            // 3. Bind the client context to the active connection
            if (!pClient->isConnected()) {
                // Pass false to 'deleteServices' to prevent wiping out services during re-entry
                if (!pClient->connect(peerAddr, false)) {
                    Serial.println("Failed to bind local client to peer address.");
                    return;
                }
            }

            Serial.println("Exchanging MTU and maps...");
            if (!initializeAMSClient(pClient)) {
                Serial.println("AMS configuration execution failed. Retrying cycle deferred.");
            }
        }
    }

    // Main task execution block for UI rendering
    if (currentTrack.hasUpdates) {
        currentTrack.hasUpdates = false;

        Serial.println("--- UI Refresh Event Triggered ---");
        Serial.printf("Target Title : %s\n", currentTrack.title);
        Serial.printf("Target Artist: %s\n", currentTrack.artist);
        Serial.printf("Target Album : %s\n", currentTrack.album);
        Serial.printf("State        : %s\n", currentTrack.isPlaying ? "PLAYING" : "PAUSED");
    }
}

bool initializeAMSClient(NimBLEClient* pClient) {
    NimBLERemoteService* pRemoteService = pClient->getService(AMS_SERVICE_UUID);
    if (!pRemoteService) {
        Serial.println("Critical Error: Core Apple Media Service not found on peer.");
        return false;
    }

    NimBLERemoteCharacteristic* pEntityUpdateChar = pRemoteService->getCharacteristic(AMS_ENTITY_UPDATE_UUID);
    if (pEntityUpdateChar && pEntityUpdateChar->canNotify()) {
        // Subscribe to the notification stream
        if (!pEntityUpdateChar->subscribe(true, amsNotificationCallback)) {
            Serial.println("Failed descriptor mapping subscription on Entity Update.");
            return false;
        }

        // AMS Handshake Contract: Inform iOS which entity attribute updates to push down the stream
        // Command syntax structure: [EntityID][AttributeID_0][AttributeID_1]...
        
        // Track Entity (ID 2) -> Request Title (0), Album (1), Artist (2)
        uint8_t registerTrackAttributes[] = {2, 0, 1, 2};
        pEntityUpdateChar->writeValue(registerTrackAttributes, sizeof(registerTrackAttributes), true);

        // Player Entity (ID 0) -> Request Playback Info (1) (Playing/Paused state string)
        uint8_t registerPlayerAttributes[] = {0, 1};
        pEntityUpdateChar->writeValue(registerPlayerAttributes, sizeof(registerPlayerAttributes), true);

        Serial.println("AMS Registration Handshake Successful.");
        return true;
    }

    Serial.println("Error: Entity Update characteristic notification flag structural failure.");
    return false;
}

// Low-level asynchronous BLE execution interrupt callback
void amsNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (length < 3) return; // Prevent out-of-bounds scanning on empty chunks

    uint8_t entityId    = pData[0];
    uint8_t attributeId = pData[1];
    // uint8_t flags    = pData[2]; // Bit 0 verified if string data is truncated (requires manual evaluation if true)

    size_t stringLength = length - 3;
    std::string value((char*)(pData + 3), stringLength);

    if (entityId == 2) { // Track Entity Event
        switch (attributeId) {
            case 0: // Title
                snprintf(currentTrack.title, sizeof(currentTrack.title), "%s", value.c_str());
                break;
            case 1: // Album
                snprintf(currentTrack.album, sizeof(currentTrack.album), "%s", value.c_str());
                break;
            case 2: // Artist
                snprintf(currentTrack.artist, sizeof(currentTrack.artist), "%s", value.c_str());
                break;
        }
        currentTrack.hasUpdates = true;
    } 
    else if (entityId == 0) { // Player Entity Event
        if (attributeId == 1) { // Playback Info Attribute
            // Payload string structure parsed format standard: "PlaybackState,PlaybackRate,ElapsedTime"
            // State index values: 0=Paused, 1=Playing, 2=Rewinding, 3=FastForwarding
            if (stringLength > 0) {
                currentTrack.isPlaying = (pData[3] == '1');
                currentTrack.hasUpdates = true;
            }
        }
    }
}