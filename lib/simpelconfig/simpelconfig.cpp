#include "simpelconfig.h"
#include <nvs.h>
#include <esp_log.h>
#include <cstdio>
#include <cstring>

static const char *TAG = "SIMPLECONFIG";

static void makeKey(int line, char *key_buf, size_t key_len)
{
    snprintf(key_buf, key_len, "line_%d", line);
}

SimpleConfig::SimpleConfig(const char *namespace_name)
    : _namespace(namespace_name) {}

bool SimpleConfig::setLine(int line, const char *text)
{
    nvs_handle_t handle;
    char key[16];
    makeKey(line, key, sizeof(key));

    esp_err_t err = nvs_open(_namespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed (%s)", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(handle, key, text);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "setLine %d failed (%s)", line, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool SimpleConfig::getLine(int line, char *out_buf, size_t buf_len)
{
    nvs_handle_t handle;
    char key[16];
    makeKey(line, key, sizeof(key));

    esp_err_t err = nvs_open(_namespace, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed (%s)", esp_err_to_name(err));
        return false;
    }

    err = nvs_get_str(handle, key, out_buf, &buf_len);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "line %d not set yet", line);
        return false;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "getLine %d failed (%s)", line, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool SimpleConfig::clearLine(int line)
{
    nvs_handle_t handle;
    char key[16];
    makeKey(line, key, sizeof(key));

    esp_err_t err = nvs_open(_namespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    err = nvs_erase_key(handle, key);
    if (err == ESP_OK) nvs_commit(handle);
    nvs_close(handle);
    return (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND);
}

bool SimpleConfig::clearAll()
{
    nvs_handle_t handle;

    esp_err_t err = nvs_open(_namespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    err = nvs_erase_all(handle);
    if (err == ESP_OK) nvs_commit(handle);
    nvs_close(handle);
    return (err == ESP_OK);
}