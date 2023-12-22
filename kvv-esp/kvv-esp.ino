#include "wifi_config.h"
// #define WIFI_SSID "your-ssid"
// #define WIFI_PASSWD "your-password"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define N_REQUESTED 4
#define MAX_JSON_SIZE (N_REQUESTED * 10000)

const char departure_dbg_print_fmt[] PROGMEM = "In %s (+%d) min.\t(Gleis %s)\t%s\tto %s\n";

struct KVVDeparture {
    const char* countdown;
    int16_t delay;
    char platform[5];
    char number[10];
    char direction[30];

    void parse(JsonObject dep) {
        countdown = dep["countdown"];
        strncpy(platform, dep["platform"], sizeof(platform) - 1);

        auto line = dep["servingLine"];
        strncpy(number, line["number"], sizeof(number) - 1);
        strncpy(direction, line["direction"], sizeof(direction) - 1);

        if(line.containsKey("delay"))
            delay = atoi(line["delay"]);
        else
            delay = 0;
    }

    void dbg_print() {
        Serial.printf(departure_dbg_print_fmt, countdown, delay, platform, number, direction);
    }
};

const char request_fmt[] PROGMEM = "https://projekte.kvv-efa.de/sl3-alone/XSLT_DM_REQUEST?outputFormat=JSON&coordOutputFormat=WGS84[dd.ddddd]&depType=stopEvents&locationServerActive=1&mode=direct&name_dm=%u&type_dm=stop&useOnlyStops=1&useRealtime=1&limit=%u";

char request_url[250];
char station_name[40];
KVVDeparture departure_list[N_REQUESTED] = {};

static void update_departures(void) {
    uint32_t station_id = 7001530;

    HTTPClient http;
    snprintf(request_url, sizeof(request_url) - 1, request_fmt, station_id, N_REQUESTED);
    http.begin(request_url);

    int response_code = http.GET();
    
    if(response_code <= 0) {
        Serial.print("Error code: ");
        Serial.println(response_code);
    }
    else {
        Serial.print("Response code: ");
        Serial.println(response_code);

        String json = http.getString();
        DynamicJsonDocument doc(MAX_JSON_SIZE);
        auto error = deserializeJson(doc, json);
        if(error) {
            Serial.print("Deserialization error: ");
            Serial.println(error.f_str());
        }

        auto departures = doc["departureList"];
        for(uint8_t i = 0; i < N_REQUESTED; i++) {
            departure_list[i].parse(departures[i]);
        }

        strncpy(station_name, doc["dm"]["points"]["point"]["name"], sizeof(station_name) - 1);
    }

    http.end();

    Serial.println("parsed.");
}

void setup() {
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    WiFi.mode(WIFI_MODE_STA);

    Serial.print("connecting [");

    while(WiFi.status() != WL_CONNECTED) {
        delay(10);
        Serial.print('.');
    }
    Serial.println(']');
    Serial.println(WiFi.localIP().toString());

    update_departures();

    Serial.printf("Departures from %s:\n", station_name);
    for(auto& dep : departure_list) {
        dep.dbg_print();
    }

    Serial.println("done.");
}

void loop() {
}
