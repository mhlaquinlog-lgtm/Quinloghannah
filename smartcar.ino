#include <WiFi.h>
#include <WebSocketsServer.h>
#include <NewPing.h>

// ===== PID VARIABLES =====
float Kp = 8.0;
float Ki = 0.0;
float Kd = 3.0;

float error = 0;
float lastError = 0;
float integral = 0;

// ===== FILTER =====
float fDist = 0, lDist = 0, rDist = 0;

// ================= WIFI =================
const char* ssid = "ESP32_ROBOT";
const char* password = "12345678";

WebSocketsServer webSocket = WebSocketsServer(81);

// ================= MOTOR =================
#define ENA 25
#define IN1 26
#define IN2 27
#define ENB 14
#define IN3 12
#define IN4 13

int baseSpeed = 170;
int turnSpeed = 160;

// ================= PWM =================
#define PWM_FREQ 1000
#define PWM_RES 8
int speedVal = 180;

// ================= ULTRASONIC =================
#define TRIG_LEFT 17
#define ECHO_LEFT    5

#define TRIG_FRONT 4
#define ECHO_FRONT    16

#define TRIG_RIGHT 18
#define ECHO_RIGHT    19

#define MAX_DIST 200


//SENSORS 
NewPing sonarL(TRIG_LEFT, ECHO_LEFT, MAX_DIST);
NewPing sonarF(TRIG_FRONT, ECHO_FRONT, MAX_DIST);
NewPing sonarR(TRIG_RIGHT, ECHO_RIGHT, MAX_DIST);

// ================= TCRT500 SENSORS =================
#define LINE_FL 35
#define LINE_FR 34
#define LINE_BL 39
#define LINE_BR 36

String mode = "manual";

// ================= MOTOR =================
void setSpeed(int l, int r){
  ledcWrite(ENA, l);
  ledcWrite(ENB, r);
}

void forward(){
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);
  setSpeed(170, 170);
}

void backward(){
  digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH);
  digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH);
  setSpeed(speedVal, speedVal);
}

void curveLeft(){
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);
  setSpeed(100, speedVal);
}

void curveRight(){
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);
  setSpeed(speedVal, 100);
}

void stopMotor(){
  setSpeed(0,0);
}

// ================= ULTRASONIC =================
long getDistRaw(int t, int e){
  digitalWrite(t, LOW); delayMicroseconds(2);
  digitalWrite(t, HIGH); delayMicroseconds(10);
  digitalWrite(t, LOW);

  long d = pulseIn(e, HIGH, 30000);
  if(d == 0) return 200;

  return d * 0.034 / 2;
}

// FILTER
float smooth(float prev, float current){
  return (prev * 0.7) + (current * 0.3);
}

// ================= MAZE MODE =================
void mazeMode(){

  // ===== READ ULTRASONICS =====
  int front = sonarF.ping_cm();
  int left  = sonarL.ping_cm();
  int right = sonarR.ping_cm();

  // INVALID READING FIX
  if(front <= 0) front = 250;
  if(left  <= 0) left  = 250;
  if(right <= 0) right = 250;

  int threshold = 15;

  // ===== DEBUG =====
  Serial.print("F:");
  Serial.print(front);

  Serial.print(" L:");
  Serial.print(left);

  Serial.print(" R:");
  Serial.println(right);

  // ==================================================
  // FRONT CLEAR -> MOVE FORWARD
  // ==================================================
  if(front > threshold){

    Serial.println("FORWARD");

    forward();

    return;
  }

  // ==================================================
  // FRONT BLOCKED
  // ==================================================
  else{

    Serial.println("FRONT BLOCKED");

    // ------------------------------------------
    // RIGHT OPEN -> TURN RIGHT
    // ------------------------------------------
    if(right > threshold){

      Serial.println("TURN RIGHT");

      digitalWrite(IN1,HIGH);
      digitalWrite(IN2,LOW);

      digitalWrite(IN3,LOW);
      digitalWrite(IN4,HIGH);

      setSpeed(turnSpeed, turnSpeed);

      delay(300);

      return;
    }

    // ------------------------------------------
    // LEFT OPEN -> TURN LEFT
    // ------------------------------------------
    if(left > threshold){

      Serial.println("TURN LEFT");

      digitalWrite(IN1,LOW);
      digitalWrite(IN2,HIGH);

      digitalWrite(IN3,HIGH);
      digitalWrite(IN4,LOW);

      setSpeed(turnSpeed, turnSpeed);

      delay(300);

      return;
    }

    // ------------------------------------------
    // DEAD END
    // ------------------------------------------
    Serial.println("DEAD END");

    backward();
    delay(250);

    // TURN AROUND
    digitalWrite(IN1,HIGH);
    digitalWrite(IN2,LOW);

    digitalWrite(IN3,LOW);
    digitalWrite(IN4,HIGH);

    setSpeed(turnSpeed, turnSpeed);

    delay(600);
  }
}

// ================= SUMO MODE =================
void sumoMode(){

  // ===== FRONT ULTRASONIC =====
  int dist = sonarF.ping_cm();

  // INVALID FIX
  if(dist <= 0){
    dist = 250;
  }

  // DEBUG
  Serial.print("[SUMO] Enemy Distance: ");
  Serial.println(dist);

  // ==================================================
  // ENEMY DETECTED -> ATTACK
  // ==================================================
  if(dist < 40){

    Serial.println("ATTACK!");

    // FULL FORWARD
    digitalWrite(IN1,HIGH);
    digitalWrite(IN2,LOW);

    digitalWrite(IN3,HIGH);
    digitalWrite(IN4,LOW);

    setSpeed(255,255);

    return;
  }

  // ==================================================
  // SEARCH MODE
  // ==================================================
  Serial.println("SEARCHING");

  // ROTATE
  digitalWrite(IN1,HIGH);
  digitalWrite(IN2,LOW);

  digitalWrite(IN3,LOW);
  digitalWrite(IN4,HIGH);

  setSpeed(180,180);
}

// ================= COMMAND HANDLER =================
void handleCommand(String cmd){
  Serial.println("CMD: " + cmd);

  // ===== MOVEMENT =====
  if(cmd == "forward") forward();
  else if(cmd == "backward") backward();
  else if(cmd == "right") curveLeft();
  else if(cmd == "left") curveRight();
  else if(cmd == "stop") stopMotor();

  // ===== MODES =====
  else if(cmd == "maze_on") mode = "maze";
  else if(cmd == "maze_off") mode = "manual";

  else if(cmd == "sumo_on") mode = "sumo";
  else if(cmd == "sumo_off") mode = "manual";
}

// ================= WEBSOCKET =================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){

  if(type == WStype_CONNECTED){
    Serial.println("Client Connected");
  }

  else if(type == WStype_DISCONNECTED){
    Serial.println("Client Disconnected");
  }

  else if(type == WStype_TEXT){
    String cmd = String((char*)payload);
    handleCommand(cmd);
  }
}

// ================= SETUP =================
void setup(){
  Serial.begin(115200);

  // MOTOR PINS
  pinMode(IN1,OUTPUT);
  pinMode(IN2,OUTPUT);
  pinMode(IN3,OUTPUT);
  pinMode(IN4,OUTPUT);

  // ULTRASONIC
  pinMode(TRIG_FRONT,OUTPUT);
  pinMode(ECHO_FRONT,INPUT);
  pinMode(TRIG_LEFT,OUTPUT);
  pinMode(ECHO_LEFT,INPUT);
  pinMode(TRIG_RIGHT,OUTPUT);
  pinMode(ECHO_RIGHT,INPUT);

  // LINE
  pinMode(LINE_FL,INPUT);
  pinMode(LINE_FR,INPUT);
  pinMode(LINE_BL,INPUT);
  pinMode(LINE_BR,INPUT);

  // PWM SETUP
  ledcAttach(ENA, PWM_FREQ, PWM_RES);
  ledcAttach(ENB, PWM_FREQ, PWM_RES);

  // WIFI AP
  WiFi.softAP(ssid, password);
  Serial.println("WiFi Ready: 192.168.4.1");

  // WEBSOCKET
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("WebSocket Ready: ws://192.168.4.1:81");
}

// ================= LOOP =================
void loop(){

  webSocket.loop();  

  // AUTO MODES
  if(mode=="maze") mazeMode();
  else if(mode=="sumo") sumoMode();
}