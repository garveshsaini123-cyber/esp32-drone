/**
 * ============================================================================
 *  SkyPilot Drone - WiFi Controller Implementation
 * ============================================================================
 *  Sets up the ESP32 as a WiFi Access Point and runs:
 *    1. A UDP server (port 4210) to receive joystick commands from phone
 *    2. An HTTP web server (port 80) to serve the phone controller page
 *
 *  Packet Format (8 bytes):
 *    [0xAA] [Throttle_H] [Throttle_L] [Roll] [Pitch] [Yaw] [Flags] [Checksum]
 *
 *  The phone connects to "SkyPilot-Drone" WiFi and either:
 *    - Opens the built-in web controller at http://192.168.4.1
 *    - Uses a dedicated UDP app sending to 192.168.4.1:4210
 * ============================================================================
 */

#include "wifi_controller.h"
#include "config.h"

#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/err.h"

/* ── Logging tag ──────────────────────────────────────────────────────────── */
static const char *TAG = "WIFI";

/* ── Shared control input (protected by mutex) ────────────────────────────── */
static control_input_t  s_control_input;
static SemaphoreHandle_t s_input_mutex = NULL;
static uint32_t         s_last_packet_time_ms = 0;
static bool             s_phone_connected = false;

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void udp_server_task(void *pvParameters);
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);

/* ═════════════════════════════════════════════════════════════════════════════
 *  Embedded HTML - Phone Controller Page
 *  This gets served from the ESP32 itself at http://192.168.4.1
 * ═════════════════════════════════════════════════════════════════════════════ */
static const char PHONE_CONTROLLER_HTML[] =
"<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0,user-scalable=no'>"
"<title>SkyPilot Drone</title><style>"
"*{margin:0;padding:0;box-sizing:border-box;touch-action:none;-webkit-touch-callout:none;-webkit-user-select:none;user-select:none}"
"body{background:#0a0a0f;font-family:'Segoe UI',system-ui,sans-serif;color:#e0e0e0;overflow:hidden;height:100vh;width:100vw}"
".hud{position:fixed;top:0;left:0;right:0;display:flex;justify-content:space-between;align-items:center;padding:8px 16px;"
"background:linear-gradient(180deg,rgba(0,0,0,0.8),transparent);z-index:10}"
".logo{font-size:14px;font-weight:700;letter-spacing:2px;color:#00ff88}"
".status{display:flex;gap:12px;align-items:center;font-size:11px}"
".badge{padding:3px 8px;border-radius:4px;font-weight:600;font-size:10px;text-transform:uppercase;letter-spacing:1px}"
".badge.armed{background:#ff4444;color:#fff;animation:pulse 1s infinite}"
".badge.disarmed{background:#333;color:#888}"
".badge.conn{background:#00ff8833;color:#00ff88;border:1px solid #00ff8855}"
".badge.disconn{background:#ff444433;color:#ff4444;border:1px solid #ff444455}"
"@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}"
".telemetry{position:fixed;top:40px;left:50%;transform:translateX(-50%);display:flex;gap:20px;font-size:11px;"
"background:rgba(0,0,0,0.5);padding:6px 16px;border-radius:8px;border:1px solid #222;z-index:10}"
".telem-val{color:#00ff88;font-weight:700;font-family:monospace;font-size:13px}"
".controls{position:fixed;bottom:0;left:0;right:0;height:55vh;display:flex;justify-content:space-between;padding:0 20px 20px}"
".stick-zone{width:45%;height:100%;position:relative;display:flex;align-items:center;justify-content:center}"
".stick-base{width:180px;height:180px;border-radius:50%;border:2px solid #1a3a2a;background:radial-gradient(circle,#0a1a10,#050d08);"
"position:relative;display:flex;align-items:center;justify-content:center}"
".stick-knob{width:60px;height:60px;border-radius:50%;background:radial-gradient(circle at 30% 30%,#00ff88,#009955);"
"position:absolute;cursor:grab;box-shadow:0 0 20px #00ff8844,0 0 40px #00ff8822;transition:box-shadow 0.2s}"
".stick-knob.active{box-shadow:0 0 30px #00ff8888,0 0 60px #00ff8844;transform:scale(1.05)}"
".stick-label{position:absolute;bottom:-24px;font-size:10px;letter-spacing:2px;color:#555;text-transform:uppercase}"
".crosshair{position:absolute;width:100%;height:100%}"
".crosshair::before,.crosshair::after{content:'';position:absolute;background:#1a3a2a}"
".crosshair::before{width:1px;height:100%;left:50%}"
".crosshair::after{width:100%;height:1px;top:50%}"
".btn-row{position:fixed;bottom:12px;left:50%;transform:translateX(-50%);display:flex;gap:10px;z-index:10}"
".btn{padding:8px 18px;border-radius:6px;border:1px solid #333;background:#111;color:#aaa;font-size:11px;"
"font-weight:600;letter-spacing:1px;text-transform:uppercase;cursor:pointer;transition:all 0.2s}"
".btn:active{transform:scale(0.95)}"
".btn.arm{border-color:#ff4444;color:#ff4444}.btn.arm:active{background:#ff4444;color:#fff}"
".btn.land{border-color:#ffaa00;color:#ffaa00}.btn.land:active{background:#ffaa00;color:#000}"
".btn.cal{border-color:#4488ff;color:#4488ff}.btn.cal:active{background:#4488ff;color:#fff}"
".values{position:fixed;bottom:55vh;left:50%;transform:translateX(-50%);font-family:monospace;font-size:11px;"
"color:#444;display:flex;gap:16px;z-index:10}"
"</style></head><body>"
"<div class='hud'><div class='logo'>▲ SKYPILOT</div>"
"<div class='status'><span class='badge disconn' id='connBadge'>NO LINK</span>"
"<span class='badge disarmed' id='armBadge'>DISARMED</span></div></div>"
"<div class='telemetry'>"
"<div>THR <span class='telem-val' id='tThr'>0</span></div>"
"<div>ROLL <span class='telem-val' id='tRoll'>0</span></div>"
"<div>PITCH <span class='telem-val' id='tPitch'>0</span></div>"
"<div>YAW <span class='telem-val' id='tYaw'>0</span></div></div>"
"<div class='values'><span id='rawL'>L: 0,0</span><span id='rawR'>R: 0,0</span></div>"
"<div class='controls'>"
"<div class='stick-zone' id='leftZone'><div class='stick-base'><div class='crosshair'></div>"
"<div class='stick-knob' id='leftKnob'></div></div><div class='stick-label'>Throttle / Yaw</div></div>"
"<div class='stick-zone' id='rightZone'><div class='stick-base'><div class='crosshair'></div>"
"<div class='stick-knob' id='rightKnob'></div></div><div class='stick-label'>Pitch / Roll</div></div></div>"
"<div class='btn-row'>"
"<button class='btn arm' id='btnArm' onclick='toggleArm()'>ARM</button>"
"<button class='btn land' id='btnLand' onclick='sendLand()'>LAND</button>"
"<button class='btn cal' onclick='sendCal()'>CALIBRATE</button></div>"
"<script>"
"const UDP_PORT=4210,SEND_RATE=50;"
"let armed=false,landReq=false,calReq=false;"
"let lx=0,ly=0,rx=0,ry=0;"
"let ws=null,sendTimer=null;"

"function initWebSocket(){"
"  ws=new WebSocket('ws://'+location.hostname+':81');"
"  ws.binaryType='arraybuffer';"
"  ws.onopen=()=>{document.getElementById('connBadge').className='badge conn';"
"    document.getElementById('connBadge').textContent='LINKED';startSending()};"
"  ws.onclose=()=>{document.getElementById('connBadge').className='badge disconn';"
"    document.getElementById('connBadge').textContent='NO LINK';setTimeout(initWebSocket,2000)};"
"  ws.onerror=()=>{ws.close()};}"

"function startSending(){if(sendTimer)return;"
"  sendTimer=setInterval(()=>{if(!ws||ws.readyState!==1)return;"
"  let thr=Math.round(mapVal(-ly,-1,1,0,2000));"
"  let roll=Math.round(mapVal(rx,-1,1,0,255));"
"  let pitch=Math.round(mapVal(-ry,-1,1,0,255));"
"  let yaw=Math.round(mapVal(lx,-1,1,0,255));"
"  let flags=(armed?1:0)|(landReq?2:0)|(calReq?4:0);"
"  calReq=false;landReq=false;"
"  let buf=new Uint8Array(8);"
"  buf[0]=0xAA;buf[1]=(thr>>8)&0xFF;buf[2]=thr&0xFF;"
"  buf[3]=roll;buf[4]=pitch;buf[5]=yaw;buf[6]=flags;"
"  let chk=0;for(let i=0;i<7;i++)chk^=buf[i];buf[7]=chk;"
"  ws.send(buf);"
"  document.getElementById('tThr').textContent=thr;"
"  document.getElementById('tRoll').textContent=roll-128;"
"  document.getElementById('tPitch').textContent=pitch-128;"
"  document.getElementById('tYaw').textContent=yaw-128;"
"  },1000/SEND_RATE)}"

"function mapVal(v,inMin,inMax,outMin,outMax){"
"  return Math.max(outMin,Math.min(outMax,(v-inMin)*(outMax-outMin)/(inMax-inMin)+outMin))}"

"function toggleArm(){armed=!armed;"
"  let b=document.getElementById('armBadge');"
"  let a=document.getElementById('btnArm');"
"  if(armed){b.className='badge armed';b.textContent='ARMED';a.textContent='DISARM'}"
"  else{b.className='badge disarmed';b.textContent='DISARMED';a.textContent='ARM'}}"
"function sendLand(){landReq=true}"
"function sendCal(){calReq=true}"

/* Joystick touch handling */
"function setupStick(zoneId,knobId,isLeft){"
"  const zone=document.getElementById(zoneId),knob=document.getElementById(knobId);"
"  const base=knob.parentElement;let touching=false,tid=null;"
"  function getPos(e){"
"    let t=e.touches?[...e.touches].find(x=>x.target===zone||zone.contains(x.target)||x.identifier===tid):e;"
"    if(!t&&e.touches)t=e.touches[0];if(!t)return null;"
"    let r=base.getBoundingClientRect(),cx=r.left+r.width/2,cy=r.top+r.height/2;"
"    let dx=(t.clientX-cx)/(r.width/2),dy=(t.clientY-cy)/(r.height/2);"
"    let d=Math.sqrt(dx*dx+dy*dy);if(d>1){dx/=d;dy/=d}return{x:dx,y:dy}}"
"  zone.addEventListener('touchstart',(e)=>{e.preventDefault();touching=true;"
"    tid=e.changedTouches[0].identifier;knob.classList.add('active');update(e)});"
"  zone.addEventListener('touchmove',(e)=>{e.preventDefault();if(touching)update(e)});"
"  zone.addEventListener('touchend',(e)=>{e.preventDefault();touching=false;tid=null;"
"    knob.classList.remove('active');"
"    if(isLeft){lx=0;knob.style.transform='translate(0px,0px)';document.getElementById('rawL').textContent='L: 0, 0'}"
"    else{rx=0;ry=0;knob.style.transform='translate(0px,0px)';document.getElementById('rawR').textContent='R: 0, 0'}});"
"  function update(e){let p=getPos(e);if(!p)return;"
"    let px=p.x*(base.offsetWidth/2-30),py=p.y*(base.offsetHeight/2-30);"
"    if(isLeft){lx=p.x;ly=p.y;knob.style.transform='translate('+px+'px,'+py+'px)';"
"      document.getElementById('rawL').textContent='L: '+p.x.toFixed(2)+', '+p.y.toFixed(2)}"
"    else{rx=p.x;ry=p.y;knob.style.transform='translate('+px+'px,'+py+'px)';"
"      document.getElementById('rawR').textContent='R: '+p.x.toFixed(2)+', '+p.y.toFixed(2)}}}"
"setupStick('leftZone','leftKnob',true);"
"setupStick('rightZone','rightKnob',false);"
"initWebSocket();"
"</script></body></html>";

/* ═════════════════════════════════════════════════════════════════════════════
 *  WiFi Event Handler
 * ═════════════════════════════════════════════════════════════════════════════ */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "📱 Phone connected! MAC=" MACSTR " AID=%d",
                 MAC2STR(event->mac), event->aid);
        s_phone_connected = true;
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGW(TAG, "📱 Phone disconnected. MAC=" MACSTR " AID=%d",
                 MAC2STR(event->mac), event->aid);
        s_phone_connected = false;
    }
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  HTTP Web Server Handler - Serves the phone controller HTML page
 * ═════════════════════════════════════════════════════════════════════════════ */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, PHONE_CONTROLLER_HTML, strlen(PHONE_CONTROLLER_HTML));
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  WebSocket Handler - Receives binary control packets from phone
 * ═════════════════════════════════════════════════════════════════════════════ */
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake from phone controller");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t buf[UDP_BUFFER_SIZE];
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = buf;
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, UDP_BUFFER_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WS receive failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Process binary packet (same format as UDP) */
    if (ws_pkt.len >= UDP_PACKET_SIZE && buf[0] == UDP_PACKET_HEADER) {
        /* Verify checksum */
        uint8_t checksum = 0;
        for (int i = 0; i < 7; i++) {
            checksum ^= buf[i];
        }

        if (checksum == buf[7]) {
            if (xSemaphoreTake(s_input_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                s_control_input.throttle = (uint16_t)((buf[1] << 8) | buf[2]);
                s_control_input.roll     = (int16_t)buf[3] - 128;
                s_control_input.pitch    = (int16_t)buf[4] - 128;
                s_control_input.yaw      = (int16_t)buf[5] - 128;
                s_control_input.armed        = (buf[6] & 0x01) ? 1 : 0;
                s_control_input.land_request = (buf[6] & 0x02) ? 1 : 0;
                s_control_input.calibrate    = (buf[6] & 0x04) ? 1 : 0;

                s_last_packet_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                xSemaphoreGive(s_input_mutex);
            }
        }
    }

    return ESP_OK;
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  UDP Server Task - Fallback for native UDP apps
 * ═════════════════════════════════════════════════════════════════════════════ */
static void udp_server_task(void *pvParameters)
{
    ESP_LOGI(TAG, "UDP server starting on port %d...", UDP_PORT);

    struct sockaddr_in server_addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(UDP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    /* Set socket timeout to prevent blocking forever */
    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind UDP socket: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "✓ UDP server listening on port %d", UDP_PORT);

    uint8_t rx_buffer[UDP_BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        int len = recvfrom(sock, rx_buffer, UDP_BUFFER_SIZE, 0,
                           (struct sockaddr *)&client_addr, &addr_len);

        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Timeout, no data — normal */
                continue;
            }
            ESP_LOGE(TAG, "recvfrom error: errno %d", errno);
            continue;
        }

        /* Validate packet */
        if (len >= UDP_PACKET_SIZE && rx_buffer[0] == UDP_PACKET_HEADER) {
            /* Verify checksum (XOR of bytes 0–6) */
            uint8_t checksum = 0;
            for (int i = 0; i < 7; i++) {
                checksum ^= rx_buffer[i];
            }

            if (checksum != rx_buffer[7]) {
                ESP_LOGW(TAG, "Bad checksum: got 0x%02X, expected 0x%02X",
                         rx_buffer[7], checksum);
                continue;
            }

            /* Parse and store control input (thread-safe) */
            if (xSemaphoreTake(s_input_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                s_control_input.throttle = (uint16_t)((rx_buffer[1] << 8) | rx_buffer[2]);
                s_control_input.roll     = (int16_t)rx_buffer[3] - 128;
                s_control_input.pitch    = (int16_t)rx_buffer[4] - 128;
                s_control_input.yaw      = (int16_t)rx_buffer[5] - 128;

                s_control_input.armed        = (rx_buffer[6] & 0x01) ? 1 : 0;
                s_control_input.land_request = (rx_buffer[6] & 0x02) ? 1 : 0;
                s_control_input.calibrate    = (rx_buffer[6] & 0x04) ? 1 : 0;

                s_last_packet_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                xSemaphoreGive(s_input_mutex);
            }
        }
    }

    close(sock);
    vTaskDelete(NULL);
}

/* ═════════════════════════════════════════════════════════════════════════════
 *  Public API
 * ═════════════════════════════════════════════════════════════════════════════ */

esp_err_t wifi_controller_init(void)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════╗");
    ESP_LOGI(TAG, "║   Initializing WiFi AP Mode      ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════╝");

    /* Create mutex for thread-safe control input access */
    s_input_mutex = xSemaphoreCreateMutex();
    if (s_input_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create input mutex");
        return ESP_FAIL;
    }

    /* Initialize default control state */
    memset(&s_control_input, 0, sizeof(control_input_t));
    s_control_input.roll  = 0;
    s_control_input.pitch = 0;
    s_control_input.yaw   = 0;

    /* ── Initialize TCP/IP stack ──────────────────────────────────────────── */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    /* ── WiFi init with default config ────────────────────────────────────── */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    /* ── Configure AP ─────────────────────────────────────────────────────── */
    wifi_config_t wifi_config = {
        .ap = {
            .ssid           = WIFI_AP_SSID,
            .ssid_len       = strlen(WIFI_AP_SSID),
            .password       = WIFI_AP_PASSWORD,
            .channel        = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode       = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "✓ WiFi AP started: SSID=\"%s\" Password=\"%s\" Channel=%d",
             WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL);
    ESP_LOGI(TAG, "  Phone connects to WiFi → opens http://192.168.4.1");

    /* ── Start UDP server task ────────────────────────────────────────────── */
    xTaskCreatePinnedToCore(
        udp_server_task,
        "udp_server",
        WIFI_TASK_STACK,
        NULL,
        WIFI_TASK_PRIORITY,
        NULL,
        0   /* Run on core 0 (networking core) */
    );

    return ESP_OK;
}

esp_err_t wifi_start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = UDP_WEB_SERVER_PORT;
    config.stack_size = WEB_TASK_STACK;
    config.max_uri_handlers = 4;
    config.max_open_sockets = 3;

    httpd_handle_t server = NULL;
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register root URI handler (serves the phone controller page) */
    httpd_uri_t root_uri = {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = root_get_handler,
    };
    httpd_register_uri_handler(server, &root_uri);

    /* Register WebSocket handler for real-time control */
    httpd_uri_t ws_uri = {
        .uri          = "/ws",
        .method       = HTTP_GET,
        .handler      = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(server, &ws_uri);

    ESP_LOGI(TAG, "✓ Web server started on port %d", UDP_WEB_SERVER_PORT);
    ESP_LOGI(TAG, "  Controller page: http://192.168.4.1");
    ESP_LOGI(TAG, "  WebSocket endpoint: ws://192.168.4.1/ws");

    return ESP_OK;
}

esp_err_t wifi_get_control_input(control_input_t *input)
{
    if (input == NULL) return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(s_input_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        memcpy(input, &s_control_input, sizeof(control_input_t));
        xSemaphoreGive(s_input_mutex);

        /* Check if data is stale */
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if ((now - s_last_packet_time_ms) > UDP_TIMEOUT_MS) {
            return ESP_ERR_TIMEOUT;
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

bool wifi_is_connected(void)
{
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return s_phone_connected &&
           ((now - s_last_packet_time_ms) < UDP_TIMEOUT_MS);
}

uint32_t wifi_get_last_packet_age_ms(void)
{
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return now - s_last_packet_time_ms;
}
