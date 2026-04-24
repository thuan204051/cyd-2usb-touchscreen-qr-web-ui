#include "wifi_manager.h"
#include "ui.h"
#include <nvs_flash.h>
#include <nvs.h>

lv_obj_t *wifi_list = NULL;

/**
 Save WiFi SSID and password to NVS (Non-Volatile Storage).
 Data persists after reset/power loss.
 @return always true (NVS errors are not handled separately)
*/
bool save_wifi_credentials(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    if (nvs_open("wifi", NVS_READWRITE, &handle) != ESP_OK) return false;
    nvs_set_str(handle, "ssid", ssid);
    nvs_set_str(handle, "password", password);
    nvs_commit(handle);
    nvs_close(handle);
    return true;
}
/**
 Read WiFi SSID and password from NVS.
 @param ssid      Buffer to receive SSID (minimum 64 bytes)
 @param password  Buffer to receive password (minimum 64 bytes)
 @return true if read successfully and SSID is not empty
*/
bool load_wifi_credentials(char *ssid, char *password)
{
    nvs_handle_t handle;
    if (nvs_open("wifi", NVS_READONLY, &handle) != ESP_OK) return false;

    size_t ssid_len = 64, pwd_len = 64;
    bool ok = (nvs_get_str(handle, "ssid", ssid, &ssid_len) == ESP_OK) &&
              (nvs_get_str(handle, "password", password, &pwd_len) == ESP_OK);
    nvs_close(handle);
    return ok && strlen(ssid) > 0;
}
/**
 Update the WiFi list on the UI after the scan is complete.
 Called via lv_async_call() from wifi_scan_task → thread-safe with LVGL.
 @param scan_result  Number of networks found (cast from int to intptr_t)
*/
void update_wifi_list_ui(void *scan_result)
{
    int n = (int)(intptr_t)scan_result;
    lv_label_set_text(ui_Label6, LV_SYMBOL_OK " Completed");
    lv_obj_clean(ui_WifiList);

    if (wifi_list) { lv_obj_delete(wifi_list); wifi_list = NULL; }

    wifi_list = lv_list_create(ui_WifiList);
    lv_obj_set_size(wifi_list, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(wifi_list, 0, 0);

    if (n <= 0) {
        lv_list_add_text(wifi_list, "No WiFi found");
        return;
    }

    int display_count = (n > 15) ? 15 : n;

    lv_list_add_text(wifi_list, "Select WiFi Network:");
    for (int i = 0; i < display_count; i++) {
        lv_obj_t *btn = lv_list_add_button(wifi_list, LV_SYMBOL_WIFI, WiFi.SSID(i).c_str());
        lv_obj_add_event_cb(btn, wifi_item_event_handler, LV_EVENT_CLICKED, NULL);
    }
    WiFi.scanDelete();
}

/**
 Scan WiFi and update the UI.
 Runs in the background to avoid blocking loop()/LVGL.
 Results are returned via lv_async_call to ensure thread-safety with LVGL.
*/
void wifi_scan_task(void *pvParameters)
{   
    
    if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    WiFi.scanNetworks(true); // async
    // Polling loop to wait for scan result (max 10s)
    int n = WIFI_SCAN_RUNNING;
    int waited = 0;
    while (n == WIFI_SCAN_RUNNING && waited < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        n = WiFi.scanComplete();
        waited++;
    }

    if (n < 0) n = 0; // WIFI_SCAN_FAILED

    lv_async_call(update_wifi_list_ui, (void *)(intptr_t)n);
    vTaskDelete(NULL);
}

void start_wifi_scan(void)
{
    xTaskCreate(wifi_scan_task, "wifi_scan", 8192, NULL, 1, NULL);
}

/**
 Connect to WiFi with the provided SSID/password, running in a background task.
 Also saves credentials to NVS for future auto-connection.
*/
void start_wifi_connect(const char *ssid, const char *password)
{
    static char _ssid[64], _pwd[64];
    strncpy(_ssid, ssid, sizeof(_ssid) - 1); _ssid[63] = '\0';
    strncpy(_pwd, password, sizeof(_pwd) - 1); _pwd[63] = '\0';

    xTaskCreate([](void *param) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        vTaskDelay(pdMS_TO_TICKS(100));
        WiFi.begin(_ssid, _pwd);

        /**Poll status, maximum 10 seconds (20 times x 500ms)*/
        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 20) {
            vTaskDelay(pdMS_TO_TICKS(500));
            retry++;
        }
        bool connected = (WiFi.status() == WL_CONNECTED);
        lv_async_call([](void*){ 
        if (qr_obj) {
            // Update QR với IP thật, không tạo mới
            String data = "http://" + WiFi.localIP().toString();
            lv_qrcode_update(qr_obj, data.c_str(), data.length());
        }
    }, NULL);

        /** Update UI within LVGL context (thread-safe) */
        lv_async_call([](void *arg) {
            bool ok = (bool)(intptr_t)arg;
            lv_label_set_text(ui_Label6, ok ? LV_SYMBOL_OK " WiFi Connected"
                                            : LV_SYMBOL_CLOSE " Wrong Password");
            lv_label_set_text(ui_wifiState, ok ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING);
        }, (void *)(intptr_t)connected);

        vTaskDelete(NULL);
    }, "wifi_connect", 8192, NULL, 1, NULL);

    save_wifi_credentials(ssid, password);
    init_server();
}