#include <string.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"

#define AP_SSID      "ESP32_SETUP"
#define AP_PASS      "configureme"
#define AP_CHANNEL   1
#define MAX_STA_CONN 1

static const char *TAG = "AP_PROV";
static const bool save_credentials = false; // Set false: always AP (testing), true: save WiFi (production)

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA starting, trying to connect...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
        ESP_LOGE(TAG, "Wi-Fi disconnect, reason: %d", event ? event->reason : -1);
        ESP_LOGE(TAG, "Failed to connect to main Wi-Fi. Re-attempting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected to main Wi-Fi. Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void connect_to_wifi(const char* ssid, const char* pass)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    ESP_LOGI(TAG, "Attempting Wi-Fi connection with SSID: %s", ssid);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char *resp_str =
        "<html><body><h2>Wi-Fi Setup</h2>"
        "<form method='POST' action='/save'>"
        "SSID: <input name='ssid'><br>"
        "Password: <input name='pass' type='password'><br>"
        "Claim Token: <input name='token'><br>"
        "<input type='submit' value='Save'>"
        "</form></body></html>";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[256], ssid[33] = {0}, pass[65] = {0}, token[65] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to receive data in /save: %d", ret);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    ESP_LOGI(TAG, "POST body: %s", buf);

    sscanf(buf, "ssid=%32[^&]&pass=%64[^&]&token=%64s", ssid, pass, token);
    ESP_LOGI(TAG, "Received SSID: %s, Password: %s, Claim Token: %s", ssid, pass, token);

    if (save_credentials) {
        nvs_handle_t nvs;
        esp_err_t err = nvs_open("wifi_creds", NVS_READWRITE, &nvs);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Saving credentials...");
            nvs_set_str(nvs, "ssid", ssid);
            nvs_set_str(nvs, "pass", pass);
            nvs_set_str(nvs, "token", token);
            nvs_commit(nvs);
            nvs_close(nvs);
            httpd_resp_sendstr(req, "Saved! Now connecting...");
        } else {
            ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
            httpd_resp_sendstr(req, "NVS Error!");
            return ESP_FAIL;
        }
    } else {
        ESP_LOGW(TAG, "save_credentials=false, not saving credentials. (TEST MODE)");
        httpd_resp_sendstr(req, "Testing mode: Credentials not saved. Connecting...");
    }
    if (strlen(ssid) > 0 && strlen(pass) > 0) {
        connect_to_wifi(ssid, pass);
    } else {
        ESP_LOGE(TAG, "Empty SSID or password, not attempting Wi-Fi connect");
    }
    return ESP_OK;
}

// Redirect GET /save to /
static esp_err_t save_get_handler(httpd_req_t *req)
{
    ESP_LOGW(TAG, "GET /save requested; redirecting to /");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

void start_softap_with_server(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = AP_CHANNEL,
            .password = AP_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    ESP_LOGI(TAG, "Starting SoftAP: %s", AP_SSID);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_LOGI(TAG, "Starting HTTP server...");
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &root_uri);
        httpd_uri_t save_post_uri = { .uri = "/save", .method = HTTP_POST, .handler = save_post_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &save_post_uri);
        httpd_uri_t save_get_uri = { .uri = "/save", .method = HTTP_GET, .handler = save_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &save_get_uri);
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_LOGI(TAG, "Booting device: Always starting SoftAP for testing Wi-Fi provisioning");
    start_softap_with_server();
}
