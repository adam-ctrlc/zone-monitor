// Runs the S3 firmware's C logic on a computer with fake hardware, under
// AddressSanitizer and UndefinedBehaviorSanitizer. See run.sh.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Included directly so its private functions can be stepped.
#include "../../zone_monitor/net_link.c"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_now.h"

#include "../../zone_monitor/buzzer.h"
#include "../../zone_monitor/lighting.h"
#include "../../zone_monitor/motor.h"
#include "../../zone_monitor/qr_clear.h"
#include "../../zone_monitor/app_log.h"

static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

// ---------------- clock ----------------
static int64_t fakeNowUs = 1000000000;
int64_t esp_timer_get_time(void) { return fakeNowUs; }
static void advance(uint32_t ms) { fakeNowUs += (int64_t)ms * 1000; }

// ---------------- gpio and pwm ----------------
static uint32_t pinLevel[64];
static uint32_t pendingDuty[2], hwDuty[2];
static int pwmWrites = 0;
esp_err_t gpio_reset_pin(gpio_num_t pin) { (void)pin; return ESP_OK; }
esp_err_t gpio_set_direction(gpio_num_t pin, int mode) { (void)pin; (void)mode; return ESP_OK; }
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) { pinLevel[pin] = level; return ESP_OK; }
esp_err_t ledc_timer_config(const ledc_timer_config_t* c) { (void)c; return ESP_OK; }
esp_err_t ledc_channel_config(const ledc_channel_config_t* c) { hwDuty[c->channel] = c->duty; return ESP_OK; }
esp_err_t ledc_set_duty(ledc_mode_t m, ledc_channel_t c, uint32_t duty) { (void)m; pendingDuty[c] = duty; return ESP_OK; }
esp_err_t ledc_update_duty(ledc_mode_t m, ledc_channel_t c) { (void)m; hwDuty[c] = pendingDuty[c]; pwmWrites++; return ESP_OK; }

// ---------------- esp-now ----------------
static esp_now_recv_cb_t receiveCallback = NULL;
static char servoSent[8][LINK_TEXT_SIZE];
static int servoCount = 0;
esp_err_t esp_now_init(void) { return ESP_OK; }
esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t cb) { receiveCallback = cb; return ESP_OK; }
esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer) { (void)peer; return ESP_OK; }
esp_err_t esp_now_send(const uint8_t* addr, const uint8_t* data, size_t len) {
  (void)addr;
  CHECK(len == LINK_TEXT_SIZE);
  CHECK(memchr(data, 0, len) != NULL);
  if (servoCount < 8) memcpy(servoSent[servoCount], data, LINK_TEXT_SIZE);
  servoCount++;
  return ESP_OK;
}

// ---------------- queue ----------------
struct fake_queue { uint32_t length, size, head, count; uint8_t* items; };
QueueHandle_t xQueueCreate(uint32_t length, uint32_t size) {
  QueueHandle_t q = calloc(1, sizeof(*q));
  q->length = length; q->size = size; q->items = calloc(length, size);
  return q;
}
BaseType_t xQueueSend(QueueHandle_t q, const void* item, TickType_t wait) {
  (void)wait;
  if (q->count == q->length) return pdFALSE;
  memcpy(q->items + ((q->head + q->count) % q->length) * q->size, item, q->size);
  q->count++;
  return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t q, void* item, TickType_t wait) {
  if (q->count == 0) { advance(wait); return pdFALSE; }
  memcpy(item, q->items + q->head * q->size, q->size);
  q->head = (q->head + 1) % q->length; q->count--;
  return pdTRUE;
}
BaseType_t xTaskCreatePinnedToCore(void (*t)(void*), const char* n, uint32_t s, void* a, int p, void* h, int c) {
  (void)t; (void)n; (void)s; (void)a; (void)p; (void)h; (void)c; return pdTRUE;
}

// ---------------- screen, lights, log, wifi ----------------
static int cardDraws = 0, bannerOn = 0, lightsRed = 0, serverDrawn = -1;
void drawCardLevel(int index) { CHECK(index >= 0 && index < ZONE_COUNT); cardDraws++; }
void drawServerStatus(bool online) { serverDrawn = online; }
void drawEstopBanner(void) { bannerOn = 1; }
void clearEstopBanner(void) { bannerOn = 0; }
void lightingSetEstop(bool on) { lightsRed = on; }
void appLog(const char* format, ...) { char line[96]; va_list a; va_start(a, format); vsnprintf(line, sizeof(line), format, a); va_end(a); }
static bool fakeWifi = true;
bool wifiIsConnected(void) { return fakeWifi; }

// ---------------- http ----------------
struct fake_http_client { char url[128]; esp_http_client_method_t method; http_event_handle_cb handler; const char* post; int postLen; int connected; };
static struct fake_http_client clients[4];
static int clientCount = 0;
#define MAX_POSTS 32
static char posted[MAX_POSTS][2][300];
static int postCount = 0, performCount = 0, closeCount = 0;
static const char* replyParts[4];
static int replyPartCount = 0;
static int replyStatus = 200, failNextPolls = 0;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* c) {
  esp_http_client_handle_t h = &clients[clientCount++];
  CHECK(clientCount <= 4);
  snprintf(h->url, sizeof(h->url), "%s", c->url); h->method = c->method; h->handler = c->event_handler;
  return h;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c, const char* k, const char* v) { (void)c; (void)k; (void)v; return ESP_OK; }
esp_err_t esp_http_client_set_url(esp_http_client_handle_t c, const char* u) { snprintf(c->url, sizeof(c->url), "%s", u); return ESP_OK; }
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t c, const char* d, int l) { c->post = d; c->postLen = l; return ESP_OK; }
esp_err_t esp_http_client_close(esp_http_client_handle_t c) { c->connected = 0; closeCount++; return ESP_OK; }
int esp_http_client_get_status_code(esp_http_client_handle_t c) { (void)c; return replyStatus; }
esp_err_t esp_http_client_perform(esp_http_client_handle_t c) {
  performCount++;
  if (c->method == HTTP_METHOD_POST) {
    CHECK(c->post != NULL && c->postLen > 0);
    if (postCount < MAX_POSTS) {
      snprintf(posted[postCount][0], 300, "%s", c->url);
      snprintf(posted[postCount][1], 300, "%.*s", c->postLen, c->post);
    }
    postCount++;
    return ESP_OK;
  }
  if (failNextPolls > 0) { failNextPolls--; return ESP_FAIL; }
  for (int i = 0; i < replyPartCount; i++) {
    esp_http_client_event_t e = { HTTP_EVENT_ON_DATA, (void*)replyParts[i], (int)strlen(replyParts[i]) };
    c->handler(&e);
  }
  return ESP_OK;
}
static void reply(const char* a) { replyParts[0] = a; replyPartCount = 1; replyStatus = 200; }

// ---------------- helpers ----------------
static const uint8_t WRONG_MAC[6] = {1, 2, 3, 4, 5, 6};
static void rawReceive(const uint8_t* mac, const void* data, int len) {
  uint8_t copy[6]; memcpy(copy, mac, 6);
  esp_now_recv_info_t info = { copy };
  receiveCallback(&info, data, len);
}
static void receive(const char* text) {
  char m[LINK_TEXT_SIZE]; memset(m, 0, sizeof(m)); snprintf(m, sizeof(m), "%s", text);
  rawReceive(CAM_MAC, m, sizeof(m));
}
static void tick(void) { estopUpdate(); serverStatusUpdate(); motorUpdate(); buzzerUpdate(); handleCameraMessages(); handleZoneResets(); motorUpdate(); }
static void runMs(uint32_t ms) { for (uint32_t i = 0; i < ms; i += 10) { advance(10); tick(); } }
static void pollNow(void) { lastPoll = clockMs() - NET_POLL_MS; netStep(); }

// ---------------- tests ----------------
static void testMotor(void) {
  motorSetup();
  buzzerSetup();
  motorRestartRamp();
  motorUpdate();
  CHECK(hwDuty[0] == 0 && hwDuty[1] == 0);
  CHECK(pinLevel[R_EN] == 1 && pinLevel[L_EN] == 1);
  advance(250); motorUpdate();
  CHECK(hwDuty[0] == 100);
  advance(1000); motorUpdate();
  CHECK(hwDuty[0] == SPEED);
  int writes = pwmWrites;
  for (int i = 0; i < 1000; i++) motorUpdate();
  CHECK(pwmWrites == writes); // no needless hardware writes
  motorHold(MOTOR_HOLD_QR);
  CHECK(hwDuty[0] == 0);      // stops before loop() runs
  motorHold(MOTOR_HOLD_ESTOP);
  motorRelease(MOTOR_HOLD_QR);
  advance(1000); motorUpdate();
  CHECK(hwDuty[0] == 0);      // a second hold still applies
  motorRelease(MOTOR_HOLD_ESTOP);
  motorUpdate();
  CHECK(hwDuty[0] == 0);      // soft start from zero
  advance(250); motorUpdate();
  CHECK(hwDuty[0] == 100);
  motorRelease(MOTOR_HOLD_ESTOP); // releasing again does not restart the ramp
  advance(50); motorUpdate();
  CHECK(hwDuty[0] == 120);
  advance(1000); motorUpdate();
  CHECK(hwDuty[1] == 0);
}

static void testMessages(void) {
  CHECK(espNowSetup());
  LinkMessage m;
  CHECK(!espNowReceive(&m));
  receive("Hello");
  rawReceive(WRONG_MAC, "Lirio st.", 32 > 10 ? 10 : 10); // short and wrong sender
  char big[64]; memset(big, 'A', sizeof(big));
  rawReceive(WRONG_MAC, big, sizeof(big));
  rawReceive(CAM_MAC, big, 5);                           // too short
  rawReceive(CAM_MAC, big, sizeof(big));                 // no terminator, too long
  CHECK(espNowReceive(&m) && strcmp(m.text, "Hello") == 0);
  CHECK(espNowReceive(&m) && strlen(m.text) == LINK_TEXT_SIZE - 1);
  CHECK(!espNowReceive(&m));
  for (int i = 0; i < 100; i++) receive("flood"); // queue full must only drop
  for (int i = 0; i < 16; i++) CHECK(espNowReceive(&m));
  CHECK(!espNowReceive(&m));
}

static void testQrPause(void) {
  runMs(3000); // leave the startup cooldown
  CHECK(hwDuty[0] == SPEED);
  receive("QR_SEEN");
  handleCameraMessages();
  CHECK(hwDuty[0] == 0); // belt stops as soon as the message is handled
  receive("QR_CLEARED");
  runMs(10);
  CHECK(pinLevel[BUZZER_PIN] == 1);
  runMs(600);
  CHECK(pinLevel[BUZZER_PIN] == 0);
  CHECK(hwDuty[0] == SPEED);

  receive("QR_SEEN");
  runMs(2900);
  CHECK(hwDuty[0] == 0);
  runMs(200);
  CHECK(hwDuty[0] > 0); // gave up after 3 s
  runMs(600);
  receive("QR_SEEN");
  runMs(10);
  CHECK(hwDuty[0] == SPEED); // cooldown: no second pause
  runMs(2000);
  receive("QR_SEEN");
  runMs(10);
  CHECK(hwDuty[0] == 0);
  zones[1].level = 0;
  postCount = 0;
  receive("LIRIO ST."); // a zone name also resumes, ignoring case
  runMs(600);
  CHECK(hwDuty[0] == SPEED);
  CHECK(zones[1].level == 1);
  netStep(); netStep();
  CHECK(postCount >= 2);
}

static void testZones(void) {
  for (int i = 0; i < ZONE_COUNT; i++) zones[i].level = 0;
  cardDraws = 0;
  zoneOnScan("Villarin st."); zoneOnScan("Villarin st."); zoneOnScan("Villarin st."); zoneOnScan("Villarin st.");
  CHECK(zones[5].level == MAX_LEVEL);
  CHECK(cardDraws == MAX_LEVEL);
  zoneOnScan("Nowhere");
  zoneOnScan("");
  zoneOnScan(NULL);
  updateZone(-1, 1); updateZone(6, 1); updateZone(1000000, 1);
  CHECK(cardDraws == MAX_LEVEL);
  networkSendLevel(-1, 1); networkSendLevel(6, 1);
  char tooLong[200]; memset(tooLong, 'x', sizeof(tooLong) - 1); tooLong[199] = 0;
  networkSendScan(tooLong); // cut to fit
  networkSendScan(NULL);
}

static void drainEvents(void) { while (eventQueue->count > 0) netStep(); }

static void testJsonEscape(void) {
  char out[16];
  CHECK(jsonEscape(out, sizeof(out), "ab") && strcmp(out, "ab") == 0);
  CHECK(jsonEscape(out, sizeof(out), "a\"b\\") && strcmp(out, "a\\\"b\\\\") == 0);
  CHECK(jsonEscape(out, sizeof(out), "\n") && strcmp(out, "\\u000a") == 0);
  CHECK(jsonEscape(out, sizeof(out), "\xc3\xa9") && strcmp(out, "\xc3\xa9") == 0);
  char tiny[3];
  CHECK(jsonEscape(tiny, sizeof(tiny), "ab"));
  CHECK(!jsonEscape(tiny, sizeof(tiny), "abc"));
  CHECK(!jsonEscape(tiny, sizeof(tiny), "\""  "\""));
  CHECK(!jsonEscape(tiny, 0, "a"));
  char worst[LINK_TEXT_SIZE * 6];
  char controls[LINK_TEXT_SIZE]; memset(controls, 1, sizeof(controls)); controls[LINK_TEXT_SIZE - 1] = 0;
  CHECK(jsonEscape(worst, sizeof(worst), controls));
}

static void testNetwork(void) {
  drainEvents();
  postCount = 0;
  networkSendScan("say \"hi\"\\");
  netStep();
  CHECK(postCount == 1);
  CHECK(strstr(posted[0][0], "/api/v1/device/scan") != NULL);
  CHECK(strcmp(posted[0][1], "{\"text\":\"say \\\"hi\\\"\\\\\"}") == 0);
  postCount = 0;
  networkSendLevel(2, 3);
  netStep();
  CHECK(strstr(posted[0][0], "/api/v1/device/level") != NULL);
  CHECK(strcmp(posted[0][1], "{\"zone\":\"MaxSuniel st.\",\"level\":3}") == 0);
  CHECK(pollClient == NULL || strstr(pollClient->url, "/api/v1/device/commands") != NULL);

  // First successful poll sends every level once.
  levelsSent = false;
  postCount = 0;
  reply("{\"commands\":[],\"estop\":false}");
  pollNow();
  CHECK(postCount == ZONE_COUNT);
  postCount = 0;
  pollNow();
  CHECK(postCount == 0);
  reply("{\"commands\":[],\"estop\":false,\"needLevels\":true}");
  pollNow();
  CHECK(postCount == ZONE_COUNT);
  serverStatusUpdate();
  CHECK(serverDrawn == 1);

  // Resets, split across pieces of the reply.
  replyParts[0] = "{\"commands\":[\"RES"; replyParts[1] = "ET:1\",\"RESET:4\"],\"es"; replyParts[2] = "top\":false}"; replyPartCount = 3;
  pollNow();
  CHECK(networkTakeResets() == ((1u << 1) | (1u << 4)));
  CHECK(networkTakeResets() == 0);
  reply("{\"commands\":[\"RESET_ALL\"],\"estop\":false}");
  pollNow();
  uint32_t all = networkTakeResets();
  CHECK(all == NET_RESET_ALL_MASK);
  atomic_fetch_or(&resetMask, all);
  servoCount = 0;
  zones[0].level = 2;
  handleZoneResets();
  CHECK(zones[0].level == 0);
  CHECK(servoCount == 1 && strcmp(servoSent[0], "RESET_ALL") == 0);
  atomic_fetch_or(&resetMask, 1u << 3);
  servoCount = 0;
  handleZoneResets();
  CHECK(servoCount == 1 && strcmp(servoSent[0], "RESET:3") == 0);

  // A reply too long to fit cannot be acted on, and cannot overflow.
  static char huge[COMMAND_BODY_SIZE * 2];
  memset(huge, ' ', sizeof(huge) - 1);
  memcpy(huge, "{\"estop\":true,", 14);
  huge[sizeof(huge) - 1] = 0;
  reply("{\"estop\":false}");
  pollNow();
  estopUpdate();
  CHECK(!estopActive());
  reply(huge);
  pollNow();
  estopUpdate();
  CHECK(!estopActive());
  CHECK(commandLength == COMMAND_BODY_SIZE - 1);
  CHECK(commandBody[COMMAND_BODY_SIZE - 1] == '\0');

  // Failures: one retry on a dropped connection, offline after two polls.
  reply("{\"estop\":false}");
  pollNow();
  CHECK(pollFails == 0);
  failNextPolls = 1;
  int performs = performCount;
  pollNow();
  CHECK(performCount == performs + 2); // retried once and worked
  CHECK(pollFails == 0);
  failNextPolls = 100;
  pollNow();
  serverStatusUpdate();
  CHECK(serverDrawn == 1);
  pollNow();
  serverStatusUpdate();
  CHECK(serverDrawn == 0);
  failNextPolls = 0;
  replyStatus = 500;
  performs = performCount;
  pollNow();
  CHECK(performCount == performs + 1); // no retry while already failing
  CHECK(pollFails > 2);
  reply("{\"estop\":false}");
  pollNow();
  serverStatusUpdate();
  CHECK(serverDrawn == 1);

  // No Wi-Fi: no requests.
  fakeWifi = false;
  performs = performCount;
  networkSendLevel(0, 1);
  netStep();
  pollNow();
  CHECK(performCount == performs);
  serverStatusUpdate();
  CHECK(serverDrawn == 0);
  fakeWifi = true;
  reply("{\"estop\":false}");
  pollNow();

  // netStep waits only until the next poll is due.
  lastPoll = clockMs();
  int64_t before = fakeNowUs;
  netStep();
  CHECK(fakeNowUs - before == (int64_t)NET_POLL_MS * 1000);
}

static void testEstop(void) {
  reply("{\"estop\":false}");
  pollNow();
  estopUpdate();
  runMs(1000);
  CHECK(hwDuty[0] == SPEED);

  reply("{\"estop\":true}");
  pollNow();
  CHECK(hwDuty[0] == 0);   // stopped by the website task, before loop() runs
  CHECK(!estopActive());
  servoCount = 0;
  runMs(10);
  CHECK(estopActive() && bannerOn && lightsRed);
  CHECK(servoCount == 1 && strcmp(servoSent[0], "ESTOP") == 0);

  int draws = cardDraws;
  zones[0].level = 0;
  receive("Vamenta st."); // ignored during a stop
  receive("QR_SEEN");
  receive("QR_CLEARED");
  runMs(1000);
  CHECK(zones[0].level == 0 && cardDraws == draws);
  CHECK(hwDuty[0] == 0);
  CHECK(pinLevel[BUZZER_PIN] == 0);

  servoCount = 0;
  reply("{\"estop\":false}");
  pollNow();
  runMs(10);
  CHECK(!estopActive() && !bannerOn && !lightsRed);
  CHECK(servoCount == 1 && strcmp(servoSent[0], "RESUME") == 0);
  runMs(1000);
  CHECK(hwDuty[0] == SPEED);

  // Stop and resume both requested before loop() runs: the hold must
  // not be left behind, and the stop must still reach everything.
  estopRequest(true);
  estopRequest(false);
  CHECK(hwDuty[0] == 0);
  servoCount = 0;
  estopUpdate();
  CHECK(estopActive());
  CHECK(servoCount == 1 && strcmp(servoSent[0], "ESTOP") == 0);
  estopUpdate();
  CHECK(!estopActive());
  runMs(1000);
  CHECK(hwDuty[0] == SPEED);

  // Paused for a QR, then a stop, then the pause times out: stays stopped.
  receive("QR_SEEN");
  runMs(10);
  estopRequest(true);
  runMs(4000);
  CHECK(hwDuty[0] == 0);
  estopRequest(false);
  runMs(1000);
  CHECK(hwDuty[0] == SPEED);
}

int main(void) {
  networkStart();
  testMotor();
  testMessages();
  testJsonEscape();
  testZones();
  testNetwork();
  testQrPause();
  testEstop();
  // The clock wraps at 49 days; timing must survive that.
  fakeNowUs = ((int64_t)UINT32_MAX - 1000) * 1000;
  motorRestartRamp();
  estopRequest(false);
  runMs(3000);
  CHECK(hwDuty[0] == SPEED);
  if (failures == 0) printf("All S3 logic tests passed\n");
  else printf("%d checks FAILED\n", failures);
  return failures != 0;
}
