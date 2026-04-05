#include "qr_webserver.h"
#include "wifi_manager.h"
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

const char* mdnsName = "esp32";//esơ32.local
lv_obj_t*   qr_obj   = NULL;

WebServer server(80);

// ─── Internal functions ───────────────────────────────────────
static void handleRoot() {
    if (LittleFS.exists("/index.html")) {
        File file = LittleFS.open("/index.html", "r");
        server.streamFile(file, "text/html");
        file.close();
    } else {
        server.send(404, "text/plain", "index.html not found");
    }
}

static void handleSend() {
    if (server.hasArg("data")) {
        String data = server.arg("data");
        Serial.printf("[WebServer] Received: %s\n", data.c_str());

        // Update QR LVGL context (thread-safe)
        static char qr_buf[256];
        strncpy(qr_buf, data.c_str(), sizeof(qr_buf) - 1);

        lv_async_call([](void *arg) {
            const char *text = (const char *)arg;
            if (qr_obj) {
                lv_qrcode_update(qr_obj, text, strlen(text));
            }
        }, (void *)qr_buf);

        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing data");
    }
}

static void handleFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("[LittleFS] Mount failed!");
        return;
    }
    Serial.println("[LittleFS] Mounted OK");

    File root = LittleFS.open("/");
    File file = root.openNextFile();
    if (!file) {
        Serial.println("[LittleFS] Filesystem is EMPTY");
    } else {
        while (file) {
            Serial.printf("[LittleFS] Found: /%s (%d bytes)\n", file.name(), file.size());
            file = root.openNextFile();
        }
    }
}

// ─── QR ──────────────────────────────────────────────────────
static void create_qr_code(lv_obj_t *parent) {
    lv_color_t qr_bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
    lv_color_t qr_color    = lv_palette_darken(LV_PALETTE_BLUE, 4);

    qr_obj = lv_qrcode_create(parent);
    lv_qrcode_set_size(qr_obj, 200);
    lv_qrcode_set_dark_color(qr_obj, qr_color);
    lv_qrcode_set_light_color(qr_obj, qr_bg_color);
    lv_obj_set_style_border_color(qr_obj, qr_bg_color, 0);
    lv_obj_set_style_border_width(qr_obj, 5, 0);
}

void qr_task(void *arg) {
    create_qr_code(ui_QRarea);
    String data = "http://" + WiFi.localIP().toString();
    lv_qrcode_update(qr_obj, data.c_str(), data.length());
    lv_obj_center(qr_obj);
}

// ─── Server ──────────────────────────────────────────────────
void init_server() {
    handleFS();
    MDNS.begin(mdnsName);

    server.on("/",     HTTP_GET,  handleRoot);
    server.on("/send", HTTP_POST, handleSend);
    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });

    server.begin();
    Serial.printf("[WebServer] Started: http://%s\n", WiFi.localIP().toString().c_str());
}

void run_server() {
    server.handleClient();
}