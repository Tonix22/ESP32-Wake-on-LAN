#include <string.h>
#include <errno.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

#define WOL_PORT       9

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     10

static const char *TAG = "ESP32C3_WOL";

static EventGroupHandle_t wifi_event_group;
static int wifi_retry_count = 0;

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

static esp_err_t parse_mac_address(const char *mac_text, uint8_t mac[6])
{
    if (strlen(mac_text) != 17) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < 6; i++) {
        int high = hex_nibble(mac_text[i * 3]);
        int low = hex_nibble(mac_text[i * 3 + 1]);

        if (high < 0 || low < 0) {
            return ESP_ERR_INVALID_ARG;
        }

        mac[i] = (uint8_t)((high << 4) | low);

        if (i < 5 && mac_text[i * 3 + 2] != ':') {
            return ESP_ERR_INVALID_ARG;
        }
    }

    if (mac_text[17] != '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_count < WIFI_MAX_RETRY) {
            wifi_retry_count++;
            ESP_LOGW(TAG, "WiFi disconnected. Retry %d/%d",
                     wifi_retry_count, WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        wifi_retry_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            NULL
        )
    );

    wifi_config_t wifi_config = {0};

    strncpy((char *)wifi_config.sta.ssid,
            CONFIG_WOL_WIFI_SSID,
            sizeof(wifi_config.sta.ssid) - 1);

    strncpy((char *)wifi_config.sta.password,
            CONFIG_WOL_WIFI_PASSWORD,
            sizeof(wifi_config.sta.password) - 1);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", CONFIG_WOL_WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(20000)
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi");
        return ESP_OK;
    }

    if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        return ESP_FAIL;
    }

    ESP_LOGE(TAG, "WiFi connection timeout");
    return ESP_ERR_TIMEOUT;
}

static void build_magic_packet(uint8_t packet[102], const uint8_t mac[6])
{
    memset(packet, 0xFF, 6);

    for (int i = 0; i < 16; i++) {
        memcpy(&packet[6 + i * 6], mac, 6);
    }
}

static esp_err_t send_wol_packet(
    const char *broadcast_ip,
    uint16_t port,
    const uint8_t mac[6]
)
{
    uint8_t packet[102];
    build_magic_packet(packet, mac);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed. errno=%d", errno);
        return ESP_FAIL;
    }

    int broadcast_enable = 1;
    int err = setsockopt(
        sock,
        SOL_SOCKET,
        SO_BROADCAST,
        &broadcast_enable,
        sizeof(broadcast_enable)
    );

    if (err < 0) {
        ESP_LOGE(TAG, "setsockopt(SO_BROADCAST) failed. errno=%d", errno);
        close(sock);
        return ESP_FAIL;
    }

    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, broadcast_ip, &dest_addr.sin_addr) != 1) {
        ESP_LOGE(TAG, "Invalid broadcast IP: %s", broadcast_ip);
        close(sock);
        return ESP_FAIL;
    }

    for (int i = 0; i < 3; i++) {
        int sent = sendto(
            sock,
            packet,
            sizeof(packet),
            0,
            (struct sockaddr *)&dest_addr,
            sizeof(dest_addr)
        );

        if (sent < 0) {
            ESP_LOGE(TAG, "sendto() failed. errno=%d", errno);
            close(sock);
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "WOL packet sent %d/3, bytes=%d", i + 1, sent);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    close(sock);
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(wifi_init_sta());

    uint8_t target_mac[6];
    esp_err_t mac_ret = parse_mac_address(CONFIG_WOL_TARGET_MAC, target_mac);
    if (mac_ret != ESP_OK) {
        ESP_LOGE(TAG, "Invalid target MAC address: %s", CONFIG_WOL_TARGET_MAC);
        ESP_ERROR_CHECK(mac_ret);
    }

    ESP_LOGI(TAG, "Sending Wake-on-LAN packet...");
    ESP_ERROR_CHECK(send_wol_packet(CONFIG_WOL_BROADCAST_IP, WOL_PORT, target_mac));

    ESP_LOGI(TAG, "Done.");
}
