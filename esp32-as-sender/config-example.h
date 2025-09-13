const char* mqttServer = "255.255.255.0";
const int mqttPort = 1883;
const char* secretKey = "";
const char* iam = "sender";

struct Credentials {
    char ssid[20];
    char password[15];
};

const int size = 1;
Credentials credentials[size] = {
  {"your-ssid-here", "password-for-ssid"},
};