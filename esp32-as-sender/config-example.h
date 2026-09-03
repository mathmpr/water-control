const char* mqttServer = "255.255.255.0";
const int mqttPort = 1883;
const char* secretKey = "";
const char* iam = "sender";

constexpr const char* CURRENT_VERSION = "1.0.0";
constexpr const char* FAMILY_ID = "family-id";
constexpr const char* MQTT_CLIENT_ID = "mqtt-client-id";
constexpr const char* MANIFEST_URL = "firmware-url";
constexpr const char* PENDING_OTA_VERSION_KEY = "pending_ota_version";
constexpr unsigned long UPDATE_CHECK_INTERVAL_MS = 30000;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;

struct Credentials {
    char ssid[20];
    char password[15];
};

const int size = 1;
Credentials credentials[size] = {
  {"your-ssid-here", "password-for-ssid"},
};
