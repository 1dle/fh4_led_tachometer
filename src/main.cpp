#include <Arduino.h>
#include <ShiftRegister74HC595.h> //https://github.com/Simsso/ShiftRegister74HC595
#include <WiFi.h>
#include <AsyncUDP.h> //https://github.com/espressif/arduino-esp32
#include <cmath> //for std::round

const char* ssid = "WiFi hotspot name";
const char* password = "passwd";

const int udp_port = 1101; //port to listen to

// parameters: <number of shift registers> (data pin, clock pin, latch pin)
ShiftRegister74HC595<1> sr(16, 17, 18);
AsyncUDP udp;

float idle_rpm = 0.0;
float current_rpm = 0.0;
float max_rpm = 0.0;


unsigned long last_packet_in = 0;

void connectWifi(){
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) { 
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());

}

void setTachometer(){
  if(current_rpm == 0.0 && max_rpm == 0.0 && idle_rpm == 0.0){
    sr.setAllLow();
  }else{
    float cr = current_rpm - idle_rpm;
    float mr = max_rpm - idle_rpm;

    float cr_percentage = cr / mr; //between 0 and 1

    int no_leds = round(8 * cr_percentage);

    for (int i = 0; i < 8; i++) {
      if(i < no_leds){
        sr.set(i, HIGH);
      }else
      {
        sr.set(i, LOW);
      }
    }
  }
  sr.updateRegisters();


}

void listenUDP(int port){
  if(udp.listen(port)) {
        Serial.print("UDP Listening on IP: ");
        Serial.println(WiFi.localIP());
        udp.onPacket([](AsyncUDPPacket packet) {
          if (packet.length() == 324 ) {
            char rpm[4]; // four bytes in a float 32

            //CURRENT_ENGINE_RPM
            memcpy(rpm, (packet.data()+16) , 4);
            current_rpm = *( (float*) rpm);

            // IDLE_ENGINE_RPM
            memcpy(rpm, (packet.data()+12) , 4);
            if( idle_rpm != *( (float*) rpm)){
              idle_rpm = *( (float*) rpm);
            }

            //MAX_ENGINE_RPM
            memcpy(rpm, (packet.data()+8) , 4);
            if( max_rpm != *( (float*) rpm)){
              max_rpm = *( (float*) rpm);
            }

            if(current_rpm > 0.1){
              setTachometer();
              last_packet_in = millis();
            }
            
          }
        });
    }
}

void setup() {
  Serial.begin(115200);
  connectWifi();
  listenUDP(udp_port);
  sr.setAllLow();
  sr.updateRegisters();
}

void loop() {
  unsigned long current_millis = millis();

  //if didn't get packets more than 1 sec, turn off all led
  if(current_millis - last_packet_in >= 1000 && current_rpm <= 0.1){
    current_rpm = 0.0;
    max_rpm = 0.0;
    idle_rpm = 0.0;
    setTachometer();
  }
}