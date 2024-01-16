#include "wifi_config.h"
// #define WIFI_SSID "your-ssid"
// #define WIFI_PASSWD "your-password"

#include <WiFi.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32
#define PANEL_CHAIN 2

#if(PANEL_CHAIN > 0)
    #include <NTPClient.h>
    #include <WiFiUdp.h>
#endif

#define PANEL_PIN_A 18
#define PANEL_PIN_R1 23
#define PANEL_PIN_G1 22
#define PANEL_PIN_B1 21
#define PANEL_PIN_R2 0
#define PANEL_PIN_G2 2
#define PANEL_PIN_CLK 14
#define PANEL_PIN_OE 25

#define N_REQUESTED 10

#define UTC_TIME_OFFSET 3600

#define SCROLL_INTERVAL 30

int32_t scroll_offset = 0;
int32_t last_scroll_time = 0;

#define UPDATE_INTERVAL 30000

int32_t last_update_time = 0;

#define PAGE_SWITCH_INTERVAL 7500

int32_t last_page_switch_time = 0;
int32_t current_page = N_REQUESTED - 1;

#define MAX_JSON_SIZE 20000

#define STATION_ID_ADDR 0

MatrixPanel_I2S_DMA display;
 
static void init_display(void) {
    HUB75_I2S_CFG mx_config(PANEL_WIDTH, PANEL_HEIGHT, PANEL_CHAIN);

    mx_config.gpio.a = PANEL_PIN_A;
    mx_config.gpio.r1 = PANEL_PIN_R1;
    mx_config.gpio.g1 = PANEL_PIN_G1;
    mx_config.gpio.b1 = PANEL_PIN_B1;
    mx_config.gpio.r2 = PANEL_PIN_R2;
    mx_config.gpio.g2 = PANEL_PIN_G2;
    mx_config.gpio.clk = PANEL_PIN_CLK;
    mx_config.gpio.oe = PANEL_PIN_OE;

    display = MatrixPanel_I2S_DMA(mx_config);
    display.begin();
    display.setBrightness(255);
    display.clearScreen();
    display.setLatBlanking(2);
}

const uint16_t black = display.color565(0, 0, 0);
const uint16_t white = display.color565(255, 255, 255);
const uint16_t green = display.color565(50, 200, 50);
const uint16_t red = display.color565(255, 50, 50);
const uint16_t yellow = display.color565(246, 211, 45);
const uint16_t grey = display.color565(150, 150, 150);
const uint16_t dark_grey = display.color565(50, 50, 50);
const uint16_t orange = display.color565(222, 123, 46);

const uint16_t sbahn_colors[] = {
    display.color565(0, 166, 110), // S1/S11
    display.color565(159, 99, 169), // S2
    display.color565(255, 220, 0), // S3
    display.color565(160, 21, 60), // S4
    display.color565(170, 89, 58), // S5
    display.color565(37, 32, 104), // S6
    display.color565(255, 241, 1), // S7/S71
    display.color565(111, 106, 42), // S8/S81
    display.color565(170, 89, 58) // S9
};

const uint16_t tram_colors[] = {
    display.color565(238, 29, 35), // 1
    display.color565(0, 112, 187), // 2
    display.color565(147, 113, 56), // 3
    display.color565(254, 202, 10), // 4
    display.color565(20, 192, 242), // 5
    display.color565(124, 193, 63), // 6
    display.color565(0, 0, 0), // ----
    display.color565(247, 147, 29), // 8
};

const char departure_dbg_print_fmt[] PROGMEM = "In %d (+%d) min.\t(Gleis %s)\t%s\tto %s\n";
const char info_separator[] PROGMEM = " /// ";

char info_text[1000] = "";

static void replace_umlauts(char* str, size_t length) {
    String s(str);
    s.replace("Ä", "Ae");
    s.replace("Ö", "Oe");
    s.replace("Ü", "Ue");
    s.replace("ä", "ae");
    s.replace("ö", "oe");
    s.replace("ü", "ue");
    s.replace("ß", "ss");
    s.replace("  ", " ");
    s.trim();
    if(!s.endsWith(".")) {
        s += '.';
    }
    strncpy(str, s.c_str(), length);
}

static void shorten_direction(char* str, size_t length) {
    String s(str);
    s.replace("Ä", "Ae");
    s.replace("Ö", "Oe");
    s.replace("Ü", "Ue");
    s.replace("ä", "ae");
    s.replace("ö", "oe");
    s.replace("ü", "ue");
    s.replace("ß", "ss");
    s.replace("Karlsruhe", "KA");
    s.replace("Hauptbahnhof", "Hbf");
    s.replace("Bahnhof", "Bf");
    s.replace("ueber", ">");
    s.trim();
    strncpy(str, s.c_str(), length);
}

struct KVVDeparture {
    int16_t countdown;
    int16_t delay;
    char platform[5];
    char number[10];
    char direction[30];

    void parse(JsonObject dep) {
        countdown = atoi(dep["countdown"]);
        strncpy(platform, dep["platform"], sizeof(platform) - 1);

        auto line = dep["servingLine"];
        strncpy(number, line["number"], sizeof(number) - 1);
        if(number[strlen(number) - 1] == ' ')
            number[strlen(number) - 1] = '\0';

        strncpy(direction, line["direction"], sizeof(direction) - 1);
        shorten_direction(direction, sizeof(direction) - 1);

        if(line.containsKey("delay"))
            delay = atoi(line["delay"]);
        else
            delay = 0;

        if(dep.containsKey("lineInfos") && strlen(info_text) == 0) {
            auto line_infos = dep["lineInfos"];
            if(line_infos.containsKey("lineInfo")) {
                const char* input = line_infos["lineInfo"]["infoText"]["subtitle"];
                const char* wml_input = line_infos["lineInfo"]["infoText"]["wmlText"];
                if(strlen(wml_input) > 0)
                    input = wml_input;
                strncpy(info_text, input, sizeof(info_text) - 1);
                replace_umlauts(info_text, sizeof(info_text) - 1);
                return;
            }

            size_t remaining = sizeof(info_text) - 1;
            char* info_text_ptr = info_text;
            auto line_info_arr = line_infos.as<JsonArray>();
            for(int i = 0; i < line_info_arr.size(); i++) {
                if(info_text_ptr != info_text && remaining >= sizeof(info_separator)) {
                    strcat(info_text_ptr, info_separator);
                    remaining -= sizeof(info_separator) - 1;
                    info_text_ptr += sizeof(info_separator) - 1;
                }

                const char* input = line_info_arr[i]["infoText"]["subtitle"];
                const char* wml_input = line_info_arr[i]["infoText"]["wmlText"];
                if(strlen(wml_input) > 0)
                    input = wml_input;
                strncpy(info_text_ptr, input, remaining);
                replace_umlauts(info_text_ptr, remaining);
                remaining -= strlen(info_text_ptr);
                info_text_ptr += strlen(info_text_ptr);                    
            }
        }
    }

    void dbg_print(void) {
        Serial.printf(departure_dbg_print_fmt, countdown, delay, platform, number, direction);
    }

    uint16_t get_train_color(void) {
        if(number[0] == 'S' && isdigit(number[1])) {
            return sbahn_colors[number[1] - '1'];
        }
        else if(isdigit(number[0]) && number[1] == '\0') {
            return tram_colors[number[0] - '1'];
        }

        return dark_grey;
    }

    void show_countdown(void) {
        display.setCursor(0, 0);
        display.setTextColor(white);

        if(countdown <= 0)
            display.print("sofort");
        else
            display.print(countdown);
        
        switch(delay) {
            case 0:
                display.setTextColor(green);
                display.print("(+0)");
                break;
            case -9999:
                display.setTextColor(red);
                display.print("(!!)");
                break;
            default:
                display.setTextColor(yellow);
                display.printf("(+%u)", delay);
                break;
        }

        display.setTextColor(white);
        if(countdown > 0)
            display.print(" min");
    }

    void show(uint32_t x, uint32_t y) {
        display.setCursor(x, y);
        size_t number_length = strlen(number);
        display.fillRect(x, y, 6 * number_length, 8, get_train_color());

        display.printf("%s ", number);

        display.setTextColor(grey);
        display.print(direction);
    }
};

const char request_fmt[] PROGMEM = "https://projekte.kvv-efa.de/sl3-alone/XSLT_DM_REQUEST?outputFormat=JSON&coordOutputFormat=WGS84[dd.ddddd]&depType=stopEvents&locationServerActive=1&mode=direct&name_dm=%u&type_dm=stop&useOnlyStops=1&useRealtime=1&limit=%u";

char request_url[250];
KVVDeparture departure_list[N_REQUESTED] = {};
bool redraw_departures = false;
bool update_successful = true;
uint32_t station_id = 7000801;

StaticJsonDocument<MAX_JSON_SIZE> doc;

#if(PANEL_CHAIN > 0)
    WiFiUDP udp_socket;
    NTPClient ntp_client(udp_socket, "pool.ntp.org");
#endif

Preferences prefs;

static bool update_departures(void) {
    Serial.printf("Requesting station %u...\n", station_id);

    memset(request_url, 0, sizeof(request_url));
    snprintf(request_url, sizeof(request_url) - 1, request_fmt, station_id, N_REQUESTED);
    
    HTTPClient http;
    http.begin(request_url);

    int response_code = http.GET();
    
    if(response_code <= 0) {
        Serial.print("Error code: ");
        Serial.println(response_code);
        return false;
    }

    Serial.print("Response code: ");
    Serial.println(response_code);

    Serial.printf("Downloading %u bytes...\n", http.getSize());

    auto& stream = http.getStream();
    if(!stream.find("\"departureList\":")) {
        Serial.println("ERROR: no departureList found!");
        return false;
    }

    stream.find("[");
    uint8_t i = 0;
    do {
        if(auto err = deserializeJson(doc, stream)) {
            Serial.print("Deserialization error: ");
            Serial.println(err.f_str());
            return false;
        }
        departure_list[i].parse(doc.as<JsonObject>());
    } while (i++ <= N_REQUESTED && stream.findUntil(",", "]"));

    Serial.printf("JSON parser memory: %zu bytes.\n", doc.memoryUsage());

    http.end();

    Serial.println("parsed.");

    redraw_departures = true;
    last_update_time = millis();
    return true;
}

static void display_page_indicator(void) {
    for(int i = 0; i < N_REQUESTED; i++) {
        int width = (PANEL_WIDTH * PANEL_CHAIN) / N_REQUESTED / 3;
        int pos = i * (PANEL_WIDTH * PANEL_CHAIN) / N_REQUESTED + width;

        display.writeFastHLine(pos, PANEL_HEIGHT - 1, width, i == current_page ? (update_successful ? white : red) : dark_grey);
    }
}

#if(PANEL_CHAIN > 0)

static void display_time(void) {
    int width = 6 * 5;
    int pos = PANEL_WIDTH * PANEL_CHAIN - width;
    display.setTextColor(orange);
    
#define FMT_STR "%2d:%02d"
    display.setCursor(pos, 0);
    display.printf(FMT_STR, ntp_client.getHours(), ntp_client.getMinutes());
    display.setCursor(pos + 1, 0);
    display.printf(FMT_STR, ntp_client.getHours(), ntp_client.getMinutes());
#undef FMT_STR
}

static void display_upcoming_departure(void) {
    display.setCursor(0, 16);
    display.setTextColor(white);
    auto& departure = departure_list[(current_page + 1) % N_REQUESTED];
    departure.show(0, 16);
}

#endif

TaskHandle_t update_task_handle;

static void update_task(void* _param) {
    for(;;) {
        if(millis() - last_page_switch_time > PAGE_SWITCH_INTERVAL) {
            last_page_switch_time = millis();
            current_page = (current_page + 1) % N_REQUESTED;
            redraw_departures = true;
        }
        if(redraw_departures) {
            redraw_departures = false;
            display.clearScreen();
            auto& departure = departure_list[current_page];
            departure.show_countdown();
            departure.show(0, 8);
            display_page_indicator();
#if(PANEL_CHAIN > 0)
            display_time();
            display_upcoming_departure();
#endif
            last_scroll_time = 0;
        }
        if(millis() - last_scroll_time > SCROLL_INTERVAL) {
            last_scroll_time = millis();
            int32_t text_len = strlen(info_text);
            if(text_len > 0) {
                display.setCursor(scroll_offset--, 24);
                display.fillRect(0, PANEL_HEIGHT - 8, (PANEL_WIDTH * PANEL_CHAIN), 8, black);

                display_page_indicator();

                display.setTextColor(orange);
                display.print(info_text);

                if(scroll_offset < text_len * -6)
                    scroll_offset = (PANEL_WIDTH * PANEL_CHAIN);
            }
        }
    }
}

static void read_new_station_id(bool* request_update) {
    if(Serial.available()) { 
        uint32_t new_station_id = atoi(Serial.readString().c_str());
        if(new_station_id)
            station_id = new_station_id;
        
        prefs.putULong("station_id", station_id);

        if(request_update)
            *request_update = true;
    }  
}

void setup() {
    Serial.begin(115200);

    init_display();

    display.setTextColor(white);
    display.clearScreen();
    display.setTextWrap(false);

    display.setCursor(0, 0);
    display.write("connecting...");

    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    WiFi.mode(WIFI_MODE_STA);

    Serial.print("connecting [");

    while(WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print('.');
    }
    Serial.println(']');
    Serial.println(WiFi.localIP().toString());

    prefs.begin("myPrefs", false);

    if(prefs.isKey("station_id")) {
        station_id = prefs.getULong("station_id");
        Serial.printf("Recovered station_id %u.\n", station_id);
    }

    display.clearScreen();
    display.setCursor(0, 0);
    display.write("updating...");

#if(PANEL_CHAIN > 0)
    ntp_client.begin();
    ntp_client.setTimeOffset(UTC_TIME_OFFSET);
    ntp_client.update();
#endif

    do {
        read_new_station_id(nullptr);
    } while(!update_departures());

    xTaskCreatePinnedToCore(update_task, "update_task", 10000, NULL, 1, &update_task_handle, 1);

    for(auto& dep : departure_list)
        dep.dbg_print();
    Serial.printf("Info: %s\n", info_text);
    
    Serial.println("done.");
}

void loop() {
    static bool request_update = false;
  
    if(millis() - last_update_time > UPDATE_INTERVAL || request_update) {
        last_update_time = millis();
        request_update = false;
        Serial.println("Updating...");
        do {
            read_new_station_id(nullptr);
        } while(!(update_successful = update_departures()));
    }

    read_new_station_id(&request_update);
}
