#include "network.h"

#include <ctime>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_coexist.h"
#include "esp_http_client.h"

static const char *TAG = "wifi";
static int s_retry_num = 0;
static bool s_wifi_connected = false;

void network_send_node_data(uint8_t node_id, uint16_t total, const char* sound_name)
{
    if (!network_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected, skipping send");
        return;
    }

    time_t now;
    char time_str[32];
    time(&now);
    struct tm *t = localtime(&now);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

    char body[192];
    snprintf(body, sizeof(body),
        "{\"node_id\":%d,\"total\":%d,\"sound\":\"%s\",\"time\":\"%s\"}",
        node_id, total, sound_name, time_str);

    esp_http_client_config_t config = {};
    config.url = "http://localhost:3000/data";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Sent: %s", body);
    } else {
        ESP_LOGW(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
        } else {
            ESP_LOGW(TAG, "WiFi connection failed after %d retries", WIFI_MAX_RETRY);
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        s_retry_num = 0;
        s_wifi_connected = true;
        ESP_LOGI(TAG, "IP:  " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "GW:  " IPSTR, IP2STR(&event->ip_info.gw));
    }
}

static void wifi_init_sta(void)
{
    esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {};
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    memcpy(wifi_config.sta.ssid,     WIFI_SSID, strlen(WIFI_SSID));
    memcpy(wifi_config.sta.password, WIFI_PASS, strlen(WIFI_PASS));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());
}

void network_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
}

bool network_is_connected(void)
{
    return s_wifi_connected;
}