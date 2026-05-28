// wifi_manager.c

#include "wifi_manager.h" 

#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include <string.h>
#include <stdbool.h>
#include <ctype.h> 

#include "oled_ui.h"

static const char *TAG = "WIFI_MANAGER";

/* ===== Configuración por menuconfig (STA) ===== */
#define DEFAULT_WIFI_SSID          CONFIG_ESP_WIFI_SSID
#define DEFAULT_WIFI_PASS          CONFIG_ESP_WIFI_PASSWORD
#define DEFAULT_MAXIMUM_RETRY      CONFIG_ESP_MAXIMUM_RETRY

/* ===== Eventos WiFi y BITS ===== */
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0; 
static httpd_handle_t s_httpd_server = NULL;

/* ===== NVS keys ===== */
#define NVS_NS_WIFI   "wifi"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PASS  "pass"

/* ===== Prototipos internos ===== */
static void start_webserver(void);
static void wifi_init_softap(const char *ap_ssid, const char *ap_pass);
static void wifi_init_sta(const char *sta_ssid, const char *sta_pass, bool allow_fallback_to_ap);
static bool nvs_load_wifi(char *out_ssid, size_t ssid_len, char *out_pass, size_t pass_len);
static bool nvs_save_wifi(const char *ssid, const char *pass);
static bool is_valid_wpa2_pass(const char *pass);
static size_t url_decode(const char *src, char *dst, size_t dst_len);


/* ===== NVS helpers ===== */

static bool nvs_save_wifi(const char *ssid, const char *pass) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_WIFI, NVS_READWRITE, &h);
    if (err != ESP_OK) return false;

    err = nvs_set_str(h, NVS_KEY_SSID, ssid);
    if (err == ESP_OK) err = nvs_set_str(h, NVS_KEY_PASS, pass ? pass : "");
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "NVS: saved ssid='%s' pass_len=%d", ssid, (int)strlen(pass ? pass : ""));
    return err == ESP_OK;
}

static bool nvs_load_wifi(char *out_ssid, size_t ssid_len, char *out_pass, size_t pass_len) {
    if (!out_ssid || !out_pass) return false;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_WIFI, NVS_READONLY, &h);
    if (err != ESP_OK) return false;

    size_t ssz = ssid_len;
    size_t psz = pass_len;

    err = nvs_get_str(h, NVS_KEY_SSID, out_ssid, &ssz);
    if (err != ESP_OK || ssz == 0) { nvs_close(h); return false; }

    err = nvs_get_str(h, NVS_KEY_PASS, out_pass, &psz);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        out_pass[0] = '\0'; // sin contraseña
        nvs_close(h);
        return true; 
    }
    nvs_close(h);
    return err == ESP_OK;
}

/* ===== Helpers HTTP y URL ===== */

static bool is_valid_wpa2_pass(const char *pass) {
    return pass && strlen(pass) >= 8;
}

static size_t url_decode(const char *src, char *dst, size_t dst_len) {
    size_t si = 0, di = 0;
    while (src[si] && di + 1 < dst_len) {
        if (src[si] == '+') {
            dst[di++] = ' ';
            si++;
        } else if (src[si] == '%' && isxdigit((unsigned char)src[si+1]) && isxdigit((unsigned char)src[si+2])) {
            char hex[3] = { src[si+1], src[si+2], '\0' };
            dst[di++] = (char) strtol(hex, NULL, 16);
            si += 3;
        } else {
            dst[di++] = src[si++];
        }
    }
    dst[di] = '\0';
    return di;
}

/* ===== HTML de la página ===== */
static const char *INDEX_HTML =
    "<!DOCTYPE html>"
    "<html lang=\"es\">"
    "<head>"
    "  <meta charset=\"UTF-8\"/>"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
    "  <title>Configurar WiFi</title>"
    "  <style>body{font-family:system-ui, sans-serif;max-width:520px;margin:40px auto;padding:0 16px}label{display:block;margin:.6rem 0 .2rem}input{width:100%;padding:.5rem;font-size:1rem}button{margin-top:1rem;padding:.6rem 1rem;font-size:1rem}</style>"
    "</head>"
    "<body>"
    "  <h2>Conectar a una red WiFi</h2>"
    "  <p>Conecte GrassLamp a su red para activar las funciones IoT.</p>"
    "  <form method=\"POST\" action=\"/save\" accept-charset=\"UTF-8\">"
    "    <label for=\"ssid\">SSID</label>"
    "    <input id=\"ssid\" name=\"ssid\" type=\"text\" required maxlength=\"32\"/>"
    "    <label for=\"pass\">Contraseña</label>"
    "    <input id=\"pass\" name=\"pass\" type=\"password\" maxlength=\"64\"/>"
    "    <button type=\"submit\">Guardar y Reiniciar</button>"
    "  </form>"
    "  <p style=\"margin-top:8px;color:#666\">Si la contraseña tiene menos de 8 caracteres, el punto de acceso será abierto.</p>"
    "</body>"
    "</html>";

/* ===== Handlers HTTP ===== */

static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static void restart_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(800));
    ESP_LOGW(TAG, "Reiniciando el dispositivo...");
    esp_restart();
}

static esp_err_t save_post_handler(httpd_req_t *req) {
    char ctype[32] = {0};
    httpd_req_get_hdr_value_str(req, "Content-Type", ctype, sizeof(ctype));
    ESP_LOGI(TAG, "POST /save Content-Type: %s, len=%d", ctype[0] ? ctype : "(desconocido)", req->content_len);

    /* Leer body de formulario */
    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 512) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cuerpo invalido");
        return ESP_FAIL;
    }
    char *buf = (char*)malloc(total_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Sin memoria");
        return ESP_FAIL;
    }
    int received = 0;
    while (received < total_len) {
        int r = httpd_req_recv(req, buf + received, total_len - received);
        if (r <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Lectura fallida");
            return ESP_FAIL;
        }
        received += r;
    }
    buf[received] = '\0';

    /* Parse simple key=value&key=value */
    char ssid_raw[128] = {0};
    char pass_raw[128] = {0};

    char *p = buf;
    while (p && *p) {
        char *key = p;
        char *eq = strchr(key, '=');
        if (!eq) break;
        *eq = '\0';
        char *val = eq + 1;

        char *amp = strchr(val, '&');
        if (amp) *amp = '\0';

        if (strcmp(key, "ssid") == 0) {
            url_decode(val, ssid_raw, sizeof(ssid_raw));
        } 
        else if (strcmp(key, "pass") == 0) {
            url_decode(val, pass_raw, sizeof(pass_raw));
        }

        if (!amp) break;
        p = amp + 1;
    }
    free(buf);

    /* Sanitizar longitudes */
    char ssid[33] = {0};
    char pass[65]  = {0};
    strncpy(ssid, ssid_raw, sizeof(ssid) - 1);
    strncpy(pass, pass_raw, sizeof(pass) - 1);

    if (strlen(ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID vacío");
        return ESP_FAIL;
    }

    /* Guardar en NVS */
    if (!nvs_save_wifi(ssid, pass)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No se pudo guardar en NVS");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req,
        "<!DOCTYPE html><html><meta charset='utf-8'/>"
        "<body><h3>Credenciales guardadas</h3>"
        "<p>El dispositivo se reiniciará para aplicar la configuración...</p>"
        "</body></html>");
    xTaskCreate(restart_task, "restart_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}


static void start_webserver(void) {

    /* URIs */
    static const httpd_uri_t uri_root = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_get_handler,
    };

    static const httpd_uri_t uri_save = {
        .uri       = "/save",
        .method    = HTTP_POST,
        .handler   = save_post_handler,
    };

    if (s_httpd_server) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    
    if (httpd_start(&s_httpd_server, &config) == ESP_OK) {
        httpd_register_uri_handler(s_httpd_server, &uri_root);
        httpd_register_uri_handler(s_httpd_server, &uri_save);
        ESP_LOGI(TAG, "Servidor HTTP iniciado en puerto %d", config.server_port);
    } 
    else {
        ESP_LOGE(TAG, "No se pudo iniciar el servidor HTTP");
    }
}

/* ===== Event Handlers de WiFi ===== */
static void sta_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < DEFAULT_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Reintentando conexión a la AP... (%d/%d)", s_retry_num, DEFAULT_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGW(TAG, "Conexión fallida (STA)");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void ap_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* e = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Estación " MACSTR " conectada, AID=%d", MAC2STR(e->mac), e->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* e = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Estación " MACSTR " desconectada, AID=%d, reason=%d",
                 MAC2STR(e->mac), e->aid, e->reason);
    }
}

/* ===== Inicialización WiFi (Lógica principal) ===== */
static void wifi_init_softap(const char *ap_ssid, const char *ap_pass)
{
    ESP_LOGI(TAG, "Cambiando a modo AP + Configuración HTTP");

    esp_netif_t *netif_ap = esp_netif_create_default_wifi_ap();
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(netif_ap, &ip_info);

    char ip_str[16];
    snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));

    oled_ui_set_mode_ap(ip_str);

    // --- INICIO DE CAMBIOS ---
    
    // Definimos el nombre y contraseña fijos que quieres
    const char *FIXED_SSID = "IoTLamp";
    const char *FIXED_PASS = ""; // Dejar vacío para red abierta, o poner min 8 caracteres

    wifi_config_t wifi_config = { 0 };
    
    // Copiamos el nombre fijo "ESP32"
    strncpy((char*)wifi_config.ap.ssid, FIXED_SSID, sizeof(wifi_config.ap.ssid)-1);
    wifi_config.ap.ssid_len = strlen(FIXED_SSID);

    // Copiamos la contraseña fija
    strncpy((char*)wifi_config.ap.password, FIXED_PASS, sizeof(wifi_config.ap.password)-1);
    
    // --- FIN DE CAMBIOS ---

    // El resto sigue igual...
    wifi_config.ap.channel = 1; 
    wifi_config.ap.max_connection = 4; 
    
    // Verifica automáticamente si pusiste contraseña o no para elegir la seguridad
    wifi_config.ap.authmode = is_valid_wpa2_pass((char*)wifi_config.ap.password) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &ap_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_LOGI(TAG, "SoftAP iniciado. SSID:%s pass:%s canal:%d (auth:%s)",
             wifi_config.ap.ssid,
             wifi_config.ap.authmode == WIFI_AUTH_OPEN ? "(abierta)" : (char*)wifi_config.ap.password,
             wifi_config.ap.channel,
             wifi_config.ap.authmode == WIFI_AUTH_OPEN ? "OPEN" : "WPA2");

    start_webserver();
}

static void wifi_init_sta(const char *sta_ssid, const char *sta_pass, bool allow_fallback_to_ap)
{
    if (!sta_ssid || strlen(sta_ssid) == 0) {
        ESP_LOGE(TAG, "SSID vacío. No se puede continuar en STA.");
        if (allow_fallback_to_ap) {
            wifi_init_softap(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
        }
        return;
    }

    s_wifi_event_group = xEventGroupCreate();
    esp_netif_create_default_wifi_sta(); 

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &sta_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &sta_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = { 0 };
    strncpy((char*)wifi_config.sta.ssid, sta_ssid, sizeof(wifi_config.sta.ssid)-1);
    strncpy((char*)wifi_config.sta.password, sta_pass ? sta_pass : "", sizeof(wifi_config.sta.password)-1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK; 

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi STA iniciado, conectando a SSID:'%s' ...", sta_ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(20000)); // 20s timeout

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado a AP: %s", sta_ssid);
    } else {
        ESP_LOGW(TAG, "No se pudo conectar a SSID:%s (timeout o fallo).", sta_ssid);
        if (allow_fallback_to_ap) {
            /* Pasar a AP + portal */
            ESP_ERROR_CHECK(esp_wifi_stop());
            
            ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
            ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
            
            wifi_init_softap(sta_ssid, sta_pass);
        }
    }
}


esp_err_t wifi_manager_init(void)
{
    // NVS_FLASH_INIT debe ser llamado en app_main.c

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 1) Intentar con credenciales guardadas en NVS; si no hay, usar las de menuconfig */
    char ssid[33] = {0};
    char pass[65] = {0};
    bool have_nvs = nvs_load_wifi(ssid, sizeof(ssid), pass, sizeof(pass));

    if (!have_nvs) {
        strncpy(ssid, DEFAULT_WIFI_SSID, sizeof(ssid)-1);
        strncpy(pass, DEFAULT_WIFI_PASS, sizeof(pass)-1);
        ESP_LOGI(TAG, "Usando credenciales por defecto (menuconfig/AP).");
    } 
    else {
        ESP_LOGI(TAG, "Usando credenciales cargadas de NVS.");
    }
    oled_ui_set_mode_connecting(ssid);
    
    // Intenta conectar. Si falla, fallback se encarga de iniciar el AP
    wifi_init_sta(ssid, pass, /*allow_fallback_to_ap=*/true);

    if (s_httpd_server != NULL) {
        // Si el servidor HTTP no es NULL, significa que estamos en modo AP (Fallback)
        // En modo AP NO activamos ahorro de energía agresivo.
        return ESP_FAIL; 
    }
    
    // --- AGREGAR ESTO ---
    // WIFI_PS_MIN_MODEM: Ahorro de energía moderado, mejor latencia para recibir comandos.
    // WIFI_PS_MAX_MODEM: Máximo ahorro, pero puede aumentar el ping/latencia.
    // Para una lámpara que debe responder rápido, MIN_MODEM es lo ideal.
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    ESP_LOGI(TAG, "Modem Sleep (WIFI_PS_MIN_MODEM) habilitado");

    return ESP_OK; // El servidor HTTP NO está activo -> Modo STA (Conectado)
}

