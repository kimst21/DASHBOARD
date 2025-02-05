// 필수 라이브러리 가져오기
#include "WiFi.h"                // ESP32의 Wi-Fi 기능을 사용하기 위한 라이브러리
#include "ESPAsyncWebServer.h"    // 비동기 웹 서버를 실행하기 위한 라이브러리
#include <Adafruit_BME280.h>      // BME280 센서를 사용하기 위한 라이브러리
#include <Adafruit_Sensor.h>      // Adafruit 센서 라이브러리 (BME280 데이터 처리)

// 네트워크 자격 증명 설정 (Wi-Fi 연결 정보)
String ssid = "  ";       // Wi-Fi SSID (네트워크 이름, 사용자가 입력해야 함)
String password = "   ";   // Wi-Fi 비밀번호 (사용자가 입력해야 함)

// 웹 서버 HTTP 기본 인증 정보 설정
const char* http_username = "admin";  // 웹 대시보드 접근 시 인증할 사용자 이름
const char* http_password = "admin";  // 웹 대시보드 접근 시 인증할 비밀번호

// BME280 센서 객체 생성 (I2C 통신을 사용하여 ESP32와 연결)
Adafruit_BME280 bme;         

// 하드웨어 핀 정의
const int buttonPin = 47;    // 푸시 버튼 핀
const int ledPin = 45;       // 상태 LED 핀
const int output = 42;       // 출력 제어 소켓
const int ldr = 6;           // 조도 센서 (LDR)
const int motionSensor = 47; // PIR 모션 센서 핀

// 변수 설정
int ledState = LOW;           // LED의 현재 상태 저장
int buttonState;              // 현재 버튼 상태
int lastButtonState = LOW;    // 이전 버튼 상태 저장
bool motionDetected = false;  // 동작 감지 여부 플래그
bool clearMotionAlert = true; // 웹 페이지에서 마지막 동작 경고 메시지를 지울지 여부

// 디바운스 설정
unsigned long lastDebounceTime = 0;  // 버튼이 마지막으로 눌린 시간
unsigned long debounceDelay = 50;    // 디바운스 시간 설정

// 포트 80에서 웹 서버 실행
AsyncWebServer server(80);
// SSE (Server-Sent Events) 이벤트 처리
AsyncEventSource events("/events");

// HTTP GET 요청에서 전달받을 매개변수
const char* PARAM_INPUT_1 = "state";

// PIR 센서에서 동작이 감지될 경우 실행될 인터럽트 함수
void IRAM_ATTR detectsMovement() {
  motionDetected = true;
  clearMotionAlert = false;
}

// 웹 페이지의 HTML 코드 저장 (ESP32의 플래시 메모리에 저장)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP IOT DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h3 {font-size: 1.8rem; color: white;}
    h4 {font-size: 1.2rem;}
    p {font-size: 1.4rem;}
    body {margin: 0;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px;}
    .switch input {display: none;}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 68px;}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; transition: .4s; border-radius: 68px}
    input:checked+.slider {background-color: #1b78e2}
    input:checked+.slider:before {transform: translateX(52px)}
    .topnav {overflow: hidden; background-color: #1b78e2;}
    .content {padding: 20px;}
    .card {background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);}
    .cards {max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));}
    .reading {font-size: 2.6rem;}
  </style>
</head>
<body>
  <div class="topnav">
    <h3>ESP IOT DASHBOARD <i class="fas fa-user-slash icon-pointer" onclick="logoutButton()"></i></h3>
  </div>
  <div class="content">
    <div class="cards">
      %BUTTONPLACEHOLDER%
      <div class="card"><h4><i class="fas fa-chart-bar"></i> TEMPERATURE</h4><p class="reading"><span id="temp"></span>&deg;C</p></div>
      <div class="card"><h4><i class="fas fa-chart-bar"></i> HUMIDITY</h4><p class="reading"><span id="humi"></span>&percnt;</p></div>
      <div class="card"><h4><i class="fas fa-sun"></i> LIGHT</h4><p class="reading"><span id="light"></span></p></div>
      <div class="card" onClick="clearMotionAlert()"><h4><i class="fas fa-running"></i> MOTION SENSOR</h4><p class="reading"><span id="motion">%MOTIONMESSAGE%</span></p></div>
  </div>
<script>
function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}
if (!!window.EventSource) {
 var source = new EventSource('/events');
 source.addEventListener('temperature', function(e) {
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 source.addEventListener('humidity', function(e) {
  document.getElementById("humi").innerHTML = e.data;
 }, false);
 source.addEventListener('light', function(e) {
  document.getElementById("light").innerHTML = e.data;
 }, false);
 source.addEventListener('motion', function(e) {
  document.getElementById("motion").innerHTML = e.data;
 }, false);
}
</script>
</body>
</html>)rawliteral";

void setup() {
  Serial.begin(115200);  // 시리얼 통신 시작

  if (!bme.begin(0x76)) {  // BME280 센서 초기화
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

    pinMode(buttonPin, INPUT);  // 푸시 버튼 핀을 입력 모드로 설정
  pinMode(ledPin, OUTPUT);  // 상태 LED 핀을 출력 모드로 설정
  pinMode(output, OUTPUT);  // 출력 소켓 핀을 출력 모드로 설정
  pinMode(motionSensor, INPUT_PULLUP);  // PIR 모션 센서를 내부 풀업 저항과 함께 입력 모드로 설정

  attachInterrupt(digitalPinToInterrupt(motionSensor), detectsMovement, RISING);  
  // PIR 센서에서 신호가 상승(RISING)할 때 인터럽트를 발생시키고, detectsMovement() 함수 실행

  WiFi.begin(ssid, password);  // 설정된 SSID 및 비밀번호로 Wi-Fi 네트워크 연결 시도
  while (WiFi.status() != WL_CONNECTED) {  
    // Wi-Fi가 연결될 때까지 대기
    delay(1000);
    Serial.println("Connecting to WiFi..");  // 연결 시도 중 출력
  }
  Serial.println(WiFi.localIP());  // 연결이 완료되면 ESP32의 로컬 IP 주소를 출력

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){  
    // 클라이언트가 루트 URL('/')로 GET 요청을 보낼 때 실행
    if(!request->authenticate(http_username, http_password))  
      return request->requestAuthentication();  
      // 사용자 인증을 요구 (설정된 username과 password 확인)
    request->send_P(200, "text/html", index_html);  
    // 인증이 성공하면 HTML 페이지(index_html)를 클라이언트에게 전송
  });

  server.on("/toggle", HTTP_GET, [] (AsyncWebServerRequest *request) {  
    // 클라이언트가 "/toggle" URL로 GET 요청을 보낼 때 실행
    if(!request->authenticate(http_username, http_password))  
      return request->requestAuthentication();  
      // 사용자 인증을 요구
    ledState = !ledState;  // LED 상태를 반전 (ON → OFF, OFF → ON)
    digitalWrite(ledPin, ledState);  // 변경된 LED 상태를 적용
    request->send(200, "text/plain", "OK");  
    // 클라이언트에게 "OK" 응답을 전송
  });

  events.onConnect([](AsyncEventSourceClient *client){  
    // SSE(Server-Sent Events) 클라이언트가 연결될 때 실행
    client->send("hello!", NULL, millis(), 1000);  
    // 클라이언트에게 "hello!" 메시지를 보내고, 1초 후 다시 전송
  });

  server.addHandler(&events);  // 웹 서버에 이벤트 스트리밍 기능 추가
  server.begin();  // 웹 서버 시작

void loop(){
  if (motionDetected && !clearMotionAlert){
    // 모션 센서가 감지되었고, 경고 메시지를 지우지 않았다면 실행
    events.send("MOTION DETECTED!", "motion", millis());  
    // "MOTION DETECTED!" 메시지를 이벤트를 통해 클라이언트로 전송
    motionDetected = false;  
    // 모션 감지 플래그를 false로 설정하여 중복 감지 방지
  }

  events.send(String(bme.readTemperature()).c_str(), "temperature", millis());  
  // BME280 센서에서 측정한 온도 데이터를 문자열로 변환하여 클라이언트로 전송
  events.send(String(bme.readHumidity()).c_str(), "humidity", millis());  
  // BME280 센서에서 측정한 습도 데이터를 문자열로 변환하여 클라이언트로 전송
  events.send(String(analogRead(ldr)).c_str(), "light", millis());  
  // 조도 센서(LDR)에서 측정한 조도 값을 클라이언트로 전송

  delay(10000);  
  // 10초마다 센서 데이터를 전송하여 실시간 업데이트 구현
}
