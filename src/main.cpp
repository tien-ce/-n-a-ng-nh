#include "RTOS_Task.h"
#include "Wire.h"
// #include "Pindefine.h"
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // Không cần chân reset
#define LED_PIN  27 
#define PUMP_PIN 23
#define VALUE_LED_PIN 47
#define SDA_PIN GPIO_NUM_11
#define SCL_PIN GPIO_NUM_12
#define PWM_RESOLUTION 8 // Độ phân giải (8-bit: giá trị từ 0-255)
#define DHT_Pin 21

// constexpr char WIFI_SSID[] = "Cloud Coffee";
// constexpr char WIFI_PASSWORD[] = "23456789";
// constexpr char WIFI_SSID[] = "Trung Ha";
// constexpr char WIFI_PASSWORD[] = "trung2004";
// constexpr char WIFI_SSID[] = "OXY 24H 5G";
// constexpr char WIFI_PASSWORD[] = "oxy24hcoffee";
constexpr char CLIENT_ID[] = "SensorT2"; // Client ID từ ThingsBoard
// constexpr char USERNAME     "mqtt_user"     // Username từ ThingsBoard
constexpr char PASSWORD[] = "sensorT2";          // Password từ ThingsBoard
constexpr char TOKEN[] = "llzsme7rtcrpvm1bkt9i"; // SensorT1
// constexpr char WIFI_SSID[] = "Thuy Nguyen";
// constexpr char WIFI_PASSWORD[] = "Hoangduong0701";
// constexpr char WIFI_SSID[] = "ACLAB";
// constexpr char WIFI_PASSWORD[] = "ACLAB2023";
// constexpr char WIFI_SSID[] = "HYPERION";
// constexpr char WIFI_PASSWORD[] = "trung2004";
constexpr char WIFI_SSID[] = "vantien";
constexpr char WIFI_PASSWORD[] = "12341234";
// constexpr char TOKEN[] = "oB2UtY3K9tEz6uBXg0rP";   // SensorT3
// constexpr char TOKEN[] = "zsx5bm8e26lcesjhqi0g";   // SensorT2
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io"; // Địa chỉ của server ThingsBoard
constexpr uint16_t THINGSBOARD_PORT = 1883U;            // Cổng kết nối MQTT (1883 là cổng mặc định cho MQTT)

// constexpr uint32_t SERIAL_DEBUG_BAUD = 9600U;   // Tốc độ baud rate cho giao tiếp Serial (dùng để debug)
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;               // Tốc độ baud rate cho giao tiếp Serial (dùng để debug)
constexpr char BLINKING_INTERVAL_ATTR[] = "blinkingInterval"; // Thuộc tính điều chỉnh tần suất nhấp nháy của LED
constexpr char LED_MODE_ATTR[] = "ledMode";                   // Thuộc tính điều chỉnh chế độ LED (ON, OFF, Blink)
constexpr char LED_STATE_ATTR[] = "ledState";                 // Thuộc tính hiển thị trạng thái hiện tại của LED (bật/tắt)
constexpr char PUMP_ATTR[] = "fanControl";
volatile bool attributesChanged = false; // Biến đánh dấu nếu thuộc tính bị thay đổi từ ThingsBoard
volatile int ledMode = 0;                // Chế độ LED: 0 (OFF), 1 (Blink), 2 (ON)
volatile bool ledState = false;          // Trạng thái hiện tại của LED (true: bật, false: tắt)
volatile bool pump_state = false;
constexpr uint16_t BLINKING_INTERVAL_MS_MIN = 10U;    // Khoảng thời gian nhấp nháy LED tối thiểu (10ms)
constexpr uint16_t BLINKING_INTERVAL_MS_MAX = 60000U; // Khoảng thời gian nhấp nháy LED tối đa (60 giây)
volatile uint16_t blinkingInterval = 1000U;           // Mặc định LED nhấp nháy mỗi 1000ms (1 giây)

uint32_t previousStateChange; // Thời gian lưu trạng thái lần cuối để điều khiển nhấp nháy LED

uint32_t previousDataSend; // Thời gian gửi dữ liệu cảm biến lần cuối

constexpr std::array<const char *, 2U> SHARED_ATTRIBUTES_LIST = {
    // Cấu hình các thuộc tính chia sẻ với ThingsBoard
    LED_STATE_ATTR,
    BLINKING_INTERVAL_ATTR};
/*------------------------Declaration-------------------------------------*/
// DHT_VAL re_val = {DHT11}; // For Non-Basic Task
DHT_VAL re_val = {Dht20};// For Non-Basic Task

// Global biến nằm trong RAM an toàn
__attribute__((section(".iram0.bss"))) size_t currentChunk ;
__attribute__((section(".iram0.bss"))) size_t totalChunk ;
LIGHT_VAL light_val;
SOIL_VAL soil_val;
OLED_VAL oled_val = {&re_val,&soil_val};
LCD_VAL lcd_val = {&re_val,&currentChunk,&totalChunk};
// Uart_VAL uart_val = {&re_val,NULL};

ThingsBoard_VAL things_val = {&re_val, &light_val, &soil_val, WIFI_SSID, WIFI_PASSWORD, THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT};
/*********************************** Các biến cho RPC **************/
void updatedCallback(const bool &success)
{
  if (success)
  {
    Serial.println("Done, Reboot now");
    esp_restart();
    return;
  }
  Serial.println("Downloading firmware failed");
}

void progressCallback(const size_t &currentChunk, const size_t &totalChuncks)
{
  *(lcd_val.currentChunk) = currentChunk;
  *(lcd_val.totalChuncks) = totalChuncks;
}
/*--------------------------------------------------------------------------------*/
RPC_Response getLedState(const RPC_Data &data)
{
  bool currentState = digitalRead(LED_PIN);
  Serial.println("Return for things board");
  Serial.println(currentState);
  return RPC_Response("getLedState", currentState); // Gửi trạng thái hiện tại về ThingsBoard
}
RPC_Response setLedSwitchState(const RPC_Data &data)
{                                          // Hàm xử lý lệnh RPC từ ThingsBoard (Bật/Tắt LED từ xa)
  Serial.println("Received Switch state"); // Hiển thị thông tin nhận được lệnh điều khiển
  bool newState = data;                    // Đọc trạng thái mới từ dữ liệu nhận được (true: bật, false: tắt)
  Serial.print("Switch state change: ");
  Serial.println(newState);
  digitalWrite(LED_PIN, newState);              // Thay đổi trạng thái của LED
  attributesChanged = true;                     // Đánh dấu đã có thay đổi để cập nhật lại ThingsBoard
  return RPC_Response("setStateLED", newState); // Trả về kết quả để cập nhật trạng thái trên Dashboard
}
RPC_Response setValueLed(const RPC_Data &data)
{                                       // Hàm xử lý lệnh RPC từ ThingsBoard (Bật/Tắt LED từ xa)
  Serial.println("Received Value LED"); // Hiển thị thông tin nhận được lệnh điều khiển
  uint32_t value = data;                // Đọc trạng thái mới từ dữ liệu nhận được (true: bật, false: tắt)
  Serial.print("Value Led Change: ");
  Serial.println(value);
  int pwmValue = map(value, 0, 100, 0, 225);
  pinMode(VALUE_LED_PIN, OUTPUT);
  analogWrite(VALUE_LED_PIN, pwmValue); // Thay đổi trạng thái của LED

  attributesChanged = true;                  // Đánh dấu đã có thay đổi để cập nhật lại ThingsBoard
  return RPC_Response("setValueLed", value); // Trả về kết quả để cập nhật trạng thái trên Dashboard
}

// const std::array<RPC_Callback, 2U> callbacks = { // Đăng ký hàm điều khiển từ xa (RPC) với ThingsBoard
//   RPC_Callback{ "setLedSwitchValue", setLedSwitchState }, //setLedSwitchValue là tên lệnh được gửi từ ThingsBoard.
//   RPC_Callback{ "setValueLED", setValueLed},
//   //Khi lệnh này được kích hoạt trên Dashboard, ESP32 sẽ gọi hàm setLedSwitchState để xử lý
// };

bool processSharedAttributes(const Shared_Attribute_Data &data)
{ // Xử lý các thuộc tính từ ThingsBoard
  bool is_user_key = false;
  for (auto it = data.begin(); it != data.end(); ++it)
  { // Duyệt qua tất cả các thuộc tính được gửi từ ThingsBoard
    if (strcmp(it->key().c_str(), BLINKING_INTERVAL_ATTR) == 0)
    {                                                           // Kiểm tra nếu thuộc tính là "blinkingInterval"
      const uint16_t new_interval = it->value().as<uint16_t>(); // Đọc giá trị khoảng thời gian nhấp nháy mới
      if (new_interval >= BLINKING_INTERVAL_MS_MIN && new_interval <= BLINKING_INTERVAL_MS_MAX)
      {
        blinkingInterval = new_interval; // Cập nhật khoảng nhấp nháy LED nếu nằm trong giới hạn cho phép
        Serial.print("Blinking interval is set to: ");
        Serial.println(new_interval); // In ra khoảng thời gian mới để kiểm tra
      }
      is_user_key = true;
    }
    else if (strcmp(it->key().c_str(), LED_STATE_ATTR) == 0)
    {                                    // Kiểm tra nếu thuộc tính là "ledState"
      ledState = it->value().as<bool>(); // Đọc trạng thái mới của LED (bật/tắt)
      digitalWrite(LED_PIN, ledState);   // Thay đổi trạng thái LED theo lệnh từ ThingsBoard
      Serial.print("LED state is set to: ");
      Serial.println(ledState); // In trạng thái LED để kiểm tra
      is_user_key = true;
    }
    else if (strcmp(it->key().c_str(), PUMP_ATTR) == 0)
    {
      pump_state = it->value().as<bool>();
      digitalWrite(PUMP_PIN, pump_state);
      Serial.print("Pump state is set to: ");
      Serial.println(pump_state);
      is_user_key = true;
    }
  }
  attributesChanged = true; // Đánh dấu rằng thuộc tính đã được thay đổi
  if(!is_user_key){
    return false;
  }
  return true;
}

// Đăng ký Callback để nhận thuộc tính từ ThingsBoard
// const Shared_Attribute_Callback attributes_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());
// const Attribute_Request_Callback attribute_shared_request_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());
void setup()
{
  currentChunk = 0;
  totalChunk = 0;
  // Wire.begin(21, 22); // Khởi tạo giao tiếp I2C với cảm biến DHT20
  pinMode(PUMP_PIN, OUTPUT);
  Serial.begin(SERIAL_DEBUG_BAUD); // Khởi động giao tiếp Serial với tốc độ 115200 để debug
  pinMode(LED_PIN,OUTPUT);
  callback.Add_RPC("setValueLED", setValueLed);
  callback.Add_RPC("setStateLED", setLedSwitchState);
  callback.Add_Shared_Attribute(BLINKING_INTERVAL_ATTR);
  callback.Add_Shared_Attribute(LED_STATE_ATTR);
  callback.Add_Shared_Attribute(PUMP_ATTR);
  callback.Shared_Attribute_Begin(processSharedAttributes);
  callback.Print_List();
  callback.subcribe_OTA_Update(&progressCallback,&updatedCallback);
  delay(1000);  // Chờ 1 giây trước khi thực hiện các tác vụ tiếp theo
  #ifdef TASK_BLINKY
    task.addTask(TaskBlinky, "Blinky", 1024, 19, 1000, NULL);
  #endif

  #ifdef TASK_DHT
    task.addTask(TaskDht, "dht", 2048, DHT_Pin, 1000, &re_val);
  #endif
  #ifdef TASK_LCD
  task.addTask(TaskLCD,"LCD",2048,225,500,&lcd_val);
  #endif
  #ifdef TASK_SOIL
    task.addTask(Tasksoil, "Soil", 2048, 32, 1000, &soil_val);
  #endif
  #ifdef TASK_LIGHT
  task.addTask(TaskLight, "Light", 2048, 33, 1000, &light_val);  
  #endif
  #ifdef TASK_OLED
    task.addTask(TaskOled,"OLED",2048,225,1000,&oled_val);
  #endif
  task.addTask(TaskPublishDataToThingsboard, "Thingsboard", 16384, 225, 1000, &things_val);
  task.beginTask();

}

void loop()
{
}
