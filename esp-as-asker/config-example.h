const char* endpoint = "https://example.com/manage";

struct Credentials {
    char ssid[20];
    char password[15];
};

const int size = 1;
Credentials credentials[size] = {
  {"your-ssid-here", "password-for-ssid"},
};