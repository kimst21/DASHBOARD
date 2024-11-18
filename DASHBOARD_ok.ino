/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-iot-shield-pcb-dashboard/
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// 필수 라이브러리 가져오기
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

// 네트워크 자격 증명으로 바꾸기
String ssid =    "  ";
String password = "   ";


// 웹 서버 HTTP 인증 자격 증명
const char* http_username = "admin";
const char* http_password = "admin";

Adafruit_BME280 bme;         // BME280 connect to ESP32 I2C 
const int buttonPin = 47;    // Pushbutton
const int ledPin = 45;       // Status LED
const int output = 42;       // Output socket
const int ldr = 6;           // LDR (Light Dependent Resistor)
const int motionSensor = 47; // PIR Motion Sensor

int ledState = LOW;           // 출력 핀의 현재 상태
int buttonState;              // 입력 핀의 전류 판독값
int lastButtonState = LOW;    // 입력 핀의 이전 판독값
bool motionDetected = false;  // 플래그 변수를 사용하여 동작 알림 메시지를 전송합니다.
bool clearMotionAlert = true; // 웹 페이지에서 마지막 동작 경고 메시지 지우기

unsigned long lastDebounceTime = 0;  // 출력 핀이 마지막으로 토글된 시간
unsigned long debounceDelay = 50;    // 디바운스 시간; 출력이 깜빡이면 증가합니다.

// 포트 80에 AsyncWebServer 개체 생성
AsyncWebServer server(80);
AsyncEventSource events("/events");

const char* PARAM_INPUT_1 = "state";

// 동작이 감지되었는지 확인
void IRAM_ATTR detectsMovement() {
  motionDetected = true;
  clearMotionAlert = false;
}

// 루트 URL의 기본 HTML 웹 페이지
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP IOT DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h3 {font-size: 1.8rem; color: white;}
    h4 { font-size: 1.2rem;}
    p { font-size: 1.4rem;}
    body {  margin: 0;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px; margin-bottom: 20px;}
    .switch input {display: none;}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 68px;   opacity: 0.8;   cursor: pointer;}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 68px}
    input:checked+.slider {background-color: #1b78e2}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
    .topnav { overflow: hidden; background-color: #1b78e2;}
    .content { padding: 20px;}
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);}
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));}
    .slider2 { -webkit-appearance: none; margin: 14px;  height: 20px; background: #ccc; outline: none; opacity: 0.8; -webkit-transition: .2s; transition: opacity .2s; margin-bottom: 40px; }
    .slider:hover, .slider2:hover { opacity: 1; }
    .slider2::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 40px; height: 40px; background: #008B74; cursor: pointer; }
    .slider2::-moz-range-thumb { width: 40px; height: 40px; background: #008B74; cursor: pointer;}
    .reading { font-size: 2.6rem;}
    .card-switch {color: #50a2ff; }
    .card-light{ color: #008B74;}
    .card-bme{ color: #572dfb;}
    .card-motion{ color: #3b3b3b; cursor: pointer;}
    .icon-pointer{ cursor: pointer;}
  </style>
</head>
<body>
  <div class="topnav">
    <h3>ESP IOT DASHBOARD <span style="text-align:right;">&nbsp;&nbsp; <i class="fas fa-user-slash icon-pointer" onclick="logoutButton()"></i></span></h3>
  </div>
  <div class="content">
    <div class="cards">
      %BUTTONPLACEHOLDER%
      <div class="card card-bme">
        <h4><i class="fas fa-chart-bar"></i> TEMPERATURE</h4><div><p class="reading"><span id="temp"></span>&deg;C</p></div>
      </div>
      <div class="card card-bme">
        <h4><i class="fas fa-chart-bar"></i> HUMIDITY</h4><div><p class="reading"><span id="humi"></span>&percnt;</p></div>
      </div>
      <div class="card card-light">
        <h4><i class="fas fa-sun"></i> LIGHT</h4><div><p class="reading"><span id="light"></span></p></div>
      </div>
      <div class="card card-motion" onClick="clearMotionAlert()">
        <h4><i class="fas fa-running"></i> MOTION SENSOR</h4><div><p class="reading"><span id="motion">%MOTIONMESSAGE%</span></p></div>
      </div>
  </div>
<script>
function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}
function controlOutput(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/output?state=1", true); }
  else { xhr.open("GET", "/output?state=0", true); }
  xhr.send();
}
function toggleLed(element) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/toggle", true);
  xhr.send();
}
function clearMotionAlert() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/clear-motion", true);
  xhr.send();
  setTimeout(function(){
    document.getElementById("motion").innerHTML = "No motion";
    document.getElementById("motion").style.color = "#3b3b3b";
  }, 1000);
}
if (!!window.EventSource) {
 var source = new EventSource('/events');
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 source.addEventListener('led_state', function(e) {
  console.log("led_state", e.data);
  var inputChecked;
  if( e.data == 1){ inputChecked = true; }
  else { inputChecked = false; }
  document.getElementById("led").checked = inputChecked;
 }, false);
 source.addEventListener('motion', function(e) {
  console.log("motion", e.data);
  document.getElementById("motion").innerHTML = e.data;
  document.getElementById("motion").style.color = "#b30000";
 }, false); 
 source.addEventListener('temperature', function(e) {
  console.log("temperature", e.data);
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 source.addEventListener('humidity', function(e) {
  console.log("humidity", e.data);
  document.getElementById("humi").innerHTML = e.data;
 }, false);
 source.addEventListener('light', function(e) {
  console.log("light", e.data);
  document.getElementById("light").innerHTML = e.data;
 }, false);
}</script>
</body>
</html>)rawliteral";

String outputState(int gpio){
  if(digitalRead(gpio)){
    return "checked";
  }
  else {
    return "";
  }
}

String processor(const String& var){
  //Serial.println(var);
  if(var == "BUTTONPLACEHOLDER"){
    String buttons;
    String outputStateValue = outputState(32);
    buttons+="<div class=\"card card-switch\"><h4><i class=\"fas fa-lightbulb\"></i> OUTPUT</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"controlOutput(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"></span></label></div>";
    outputStateValue = outputState(19);
    buttons+="<div class=\"card card-switch\"><h4><i class=\"fas fa-lightbulb\"></i> STATUS LED</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleLed(this)\" id=\"led\" " + outputStateValue + "><span class=\"slider\"></span></label></div>";
    return buttons;
  }
  else if(var == "MOTIONMESSAGE"){
    if(!clearMotionAlert) {
      return String("<span style=\"color:#b30000;\">MOTION DETECTED!</span>");
    }
    else {
      return String("No motion");
    }
  }
  return String();
}

//로그아웃한 웹 페이지
const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <p>Logged out or <a href="/">return to homepage</a>.</p>
  <p><strong>Note:</strong> close all web browser tabs to complete the logout process.</p>
</body>
</html>
)rawliteral";

void setup(){
  // 디버깅 목적의 직렬 포트
  Serial.begin(115200);
    
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  
  // 푸시 버튼 핀을 입력으로 초기화합니다.
  pinMode(buttonPin, INPUT);
  // LED 핀을 출력으로 초기화합니다.
  pinMode(ledPin, OUTPUT);
  // LED 핀을 출력으로 초기화
  pinMode(output, OUTPUT);
  // PIR 모션 센서 모드 INPUT_PULLUP
  pinMode(motionSensor, INPUT_PULLUP);
  // 모션센서 핀을 인터럽트로 설정하고, 인터럽트 기능을 할당하고, 상승 모드를 설정합니다.
  attachInterrupt(digitalPinToInterrupt(motionSensor), detectsMovement, RISING);
  
  // Wi-Fi에 연결
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  //ESP32 로컬 IP 주소 인쇄
  Serial.println(WiFi.localIP());

  // 루트/웹 페이지 경로
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
   if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", logout_html, processor);
  });
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });
  // 출력 소켓 <ESP_IP>/출력?상태=<입력메시지> 제어를 위한 GET 요청을 보냅니다.
  server.on("/output", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    String inputMessage;
    // GET gpio 및 상태 값
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      digitalWrite(output, inputMessage.toInt());
      request->send(200, "text/plain", "OK");
    }
    request->send(200, "text/plain", "Failed");
  });
  // 온보드 상태 LED <ESP_IP>/토글 제어를 위한 GET 요청을 보냅니다.
  server.on("/toggle", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
    request->send(200, "text/plain", "OK");
  });
  // "모션 감지됨" 메시지 <ESP_IP>/clear-motion을 지우려면 GET 요청을 보냅니다.
  server.on("/clear-motion", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    clearMotionAlert = true;
    request->send(200, "text/plain", "OK");
  });
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // "안녕하세요!" 메시지와 함께 이벤트를 보내고, 아이디는 현재 밀리초로 설정하고 재접속 지연을 1초로 설정합니다
    client->send("hello!",NULL,millis(),1000);
  });
  server.addHandler(&events);
  
  // 서버 시작
  server.begin();
}
 
void loop(){
  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 10000;
  // 스위치의 상태를 로컬 변수로 읽습니다.
  int reading = digitalRead(buttonPin);

  // 스위치가 변경된 경우
  if (reading != lastButtonState) {
    // 디바운스 타이머 재설정
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // 버튼 상태가 변경된 경우:
    if (reading != buttonState) {
      buttonState = reading;
      // 새 버튼 상태가 높음인 경우에만 LED를 토글합니다.
      if (buttonState == HIGH) {
        ledState = !ledState;
        digitalWrite(ledPin, ledState);
        events.send(String(digitalRead(ledPin)).c_str(),"led_state",millis());
      }
    }
  }

  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
    events.send("ping",NULL,millis());
    events.send(String(bme.readTemperature()).c_str(),"temperature",millis());
    events.send(String(bme.readHumidity()).c_str(),"humidity",millis());
    events.send(String(analogRead(ldr)).c_str(),"light",millis());
    lastEventTime = millis();
  }
  
  if(motionDetected & !clearMotionAlert){
    events.send(String("MOTION DETECTED!").c_str(),"motion",millis());
    motionDetected = false;
  }
  
  // 판독값을 저장합니다. 다음 번 루프를 통과할 때는 마지막 버튼 상태가 됩니다:
  lastButtonState = reading;
}
