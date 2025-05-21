#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SingleFileDrive.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <Wire.h>

#include <vector>

#include "byteutils.h"
#include "config.h"
#include "Network.h"
#include "imu.h"
#include "robot.h"
#include "wpilibudp.h" 
#include "encoder.h"

// Resource strings
extern "C" {
const unsigned char* GetResource_index_html(size_t* len);
const unsigned char* GetResource_normalize_css(size_t* len);
const unsigned char* GetResource_skeleton_css(size_t* len);
const unsigned char* GetResource_xrp_js(size_t* len);
}

// Beta board uses Wire where Production board uses Wire1
#ifdef ARDUINO_SPARKFUN_XRP_CONTROLLER_BETA
  #define MYWIRE Wire
#else
  #define MYWIRE Wire1
#endif

// HTTP server
WebServer webServer(5000);

// UDP
WiFiUDP udp;
char udpPacketBuf[UDP_TX_PACKET_MAX_SIZE + 1];
IPAddress udpRemoteAddr;
uint16_t udpRemotePort;

// std::vector<std::string> outboundMessages;

// TEMP: Status
unsigned long _wsMessageCount = 0;
unsigned long _lastMessageStatusPrint = 0;
int _baselineUsedHeap = 0;

unsigned long _avgLoopTimeUs = 0;
unsigned long _loopTimeMeasurementCount = 0;

uint16_t seq = 0;

// ==================================================
// UDP Management Functions
// ==================================================

// Update the remote UDP socket information (used to send data upstream)
void updateRemoteInfo() {
  // Update the remote address if needed
  if (!udpRemoteAddr.isSet()) {
    Serial.printf("[NET] Received first UDP connect from %s:%d\n", udp.remoteIP().toString().c_str(), udp.remotePort());
    udpRemoteAddr = udp.remoteIP();
    udpRemotePort = udp.remotePort();
  }
  else {
    bool shouldUpdate = false;
    if (udpRemoteAddr != udp.remoteIP()) {
      shouldUpdate = true;
    }
    if (udpRemotePort != udp.remotePort()) {
      shouldUpdate = true;
    }

    if (shouldUpdate) {
      udpRemoteAddr = udp.remoteIP();
      udpRemotePort = udp.remotePort();
    }
  }
}

void sendData() {
  int size = 0;
  char buffer[512];
  int ptr = 0;

  uint16ToNetwork(seq, buffer);
  buffer[2] = 0; // Unset the control byte
  ptr = 3;

  // Encoders
  for (int i = 0; i < 4; i++) {
    int encoderValue = xrp::readEncoderRaw(i);
    uint encoderPeriod = xrp::readEncoderPeriod(i);

    // We want to flip the encoder 0 value (left motor encoder) so that this returns
    // positive values when moving forward.
    if (i == 0) {
      encoderValue = -encoderValue;
      encoderPeriod ^= 1; //Last bit is direction bit; Flip it.
    }

    static constexpr uint divisor = xrp::Encoder::getDivisor();

    ptr += wpilibudp::writeEncoderData(i, encoderValue, encoderPeriod, divisor, buffer, ptr);
  } // 4x 15 bytes

  // DIO (currently just the button)
  ptr += wpilibudp::writeDIOData(0, xrp::isUserButtonPressed(), buffer, ptr);
  // 1x 4 bytes

  // Gyro and accel data
  float gyroRates[3] = {
    xrp::imuGetGyroRateX(),
    xrp::imuGetGyroRateY(),
    xrp::imuGetGyroRateZ()
  };

  float gyroAngles[3] = {
    xrp::imuGetRoll(),
    xrp::imuGetPitch(),
    xrp::imuGetYaw()
  };

  float accels[3] = {
    xrp::imuGetAccelX(),
    xrp::imuGetAccelY(),
    xrp::imuGetAccelZ()
  };

  ptr += wpilibudp::writeGyroData(gyroRates, gyroAngles, buffer, ptr);
  // 1x 26 bytes
  ptr += wpilibudp::writeAccelData(accels, buffer, ptr);
  // 1x 14 bytes

  if (xrp::reflectanceInitialized()) {
    ptr += wpilibudp::writeAnalogData(0, xrp::getReflectanceLeft5V(), buffer, ptr);
    ptr += wpilibudp::writeAnalogData(1, xrp::getReflectanceRight5V(), buffer, ptr);
  }

  if (xrp::rangefinderInitialized()) {
    ptr += wpilibudp::writeAnalogData(2, xrp::getRangefinderDistance5V(), buffer, ptr);
  }

  // ptr should now point to 1 past the last byte
  size = ptr;

  // Send
  if (udpRemoteAddr.isSet()) {
    udp.beginPacket(udpRemoteAddr.toString().c_str(), udpRemotePort);
    udp.write(buffer, size);
    udp.endPacket();
    seq++;
  }
}

// ==================================================
// Web Server Management Functions
// ==================================================
void setupWebServerRoutes() {
  webServer.on("/", []() {
    size_t len;
    webServer.send(200, "text/html", GetResource_index_html(&len), len);
  });

  webServer.on("/normalize.css", []() {
    size_t len;
    webServer.send(200, "text/css", GetResource_normalize_css(&len), len);
  });

  webServer.on("/skeleton.css", []() {
    size_t len;
    webServer.send(200, "text/css", GetResource_skeleton_css(&len), len);
  });

  webServer.on("/xrp.js", []() {
    size_t len;
    webServer.send(200, "text/javascript", GetResource_xrp_js(&len), len);
  });

  webServer.on("/getconfig", []() {
    File f = LittleFS.open("/config.json", "r");
    if (webServer.streamFile(f, "text/json") != f.size()) {
      Serial.println("[WEB] Sent less data than expected for /getconfig");
    }
    f.close();
  });

  webServer.on("/resetconfig", []() {
    if (webServer.method() != HTTP_POST) {
      webServer.send(405, "text/plain", "Method Not Allowed");
      return;
    }
    File f = LittleFS.open("/config.json", "w");
    XRPConfiguration  * config = XRPConfiguration::getInstance();
    config->generateDefaultConfig();
    f.print(config->toJsonString().c_str());
    f.close();
    webServer.send(200, "text/plain", "OK");
  });

  webServer.on("/saveconfig", []() {
    if (webServer.method() != HTTP_POST) {
      webServer.send(405, "text/plain", "Method Not Allowed");
      return;
    }
    auto postBody = webServer.arg("plain");
    File f = LittleFS.open("/config.json", "w");
    f.print(postBody);
    f.close();
    Serial.println("[CONFIG] Configuration Updated Remotely");

    webServer.send(200, "text/plain", "OK");
  });
}

void checkPrintStatus() {
  if (millis() - _lastMessageStatusPrint > 5000) {

    int usedHeap = rp2040.getUsedHeap();
    Serial.printf("t(ms):%u h:%d msg:%u lt(us):%u\n", millis(), usedHeap, _wsMessageCount, _avgLoopTimeUs);
    _lastMessageStatusPrint = millis();
  }
}

void updateLoopTime(unsigned long loopStart) {
  unsigned long loopTime = micros() - loopStart;
  unsigned long totalTime = _avgLoopTimeUs * _loopTimeMeasurementCount;
  _loopTimeMeasurementCount++;

  _avgLoopTimeUs = (totalTime + loopTime) / _loopTimeMeasurementCount;
}

void setupWebServer() {
  // Set up HTTP server routes
  Serial.println("[NET] Setting up Config webserver");
  setupWebServerRoutes();

  webServer.begin();
  Serial.println("[NET] Config webserver listening on *:5000");

  // Set up UDP
  udp.begin(3540);
  Serial.println("[NET] UDP socket listening on *:3540");

  Serial.println("[NET] Network Ready");
  Serial.printf("[NET] SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("[NET] IP: %s\n", WiFi.localIP().toString().c_str());
}

void setup() {
  // Start Serial port for logging
  Serial.begin(115200);

  // Delay to allow to connect to Serial port
  delay(4000);
  Serial.println("[SETUP] Starting robot ...\n");

  // Start LittleFS for read/write from disk
  LittleFS.begin();
  
  // Set up the I2C pins
  MYWIRE.setSCL(I2C_SCL_1);
  MYWIRE.setSDA(I2C_SDA_1);
  MYWIRE.begin();

  XRPConfiguration * config = XRPConfiguration::getInstance();

  // Read Config
  config->loadConfiguration();

  // MUST BE BEFORE imuCalibrate (has digitalWrites) and configureNetwork
  xrp::robotInit();

  // Initialize IMU
  Serial.println("[IMU] Initializing IMU");
  xrp::imuInit(IMU_I2C_ADDR, &MYWIRE);

  Serial.println("[IMU] Beginning IMU calibration");
  xrp::imuCalibrate(5000);

  // Setup Network
  setupNetwork();

  // Setup webserver
  setupWebServer();

  // NOTE: For now, we'll force init the reflectance sensor
  // TODO Enable this via configuration
  xrp::reflectanceInit();

  // NOTE: For now we'll force init the rangefinder
  // TODO enable this via configuration
  xrp::rangefinderInit();

  _lastMessageStatusPrint = millis();
  _baselineUsedHeap = rp2040.getUsedHeap();

  // Emulates a FAT-formatted USB stick 
  // to allow txt file to be read if USB connected
  singleFileDrive.begin("status.txt", "XRP-Status.txt");
}

void loop() {
  unsigned long loopStartTime = micros();

  // Check for (configuration) requests from webServer
  webServer.handleClient();

  // Check for data via udp (from client code)
  int packetSize = udp.parsePacket();
  if (packetSize) {
    updateRemoteInfo();

    // Read the packet
    int n = udp.read(udpPacketBuf, UDP_TX_PACKET_MAX_SIZE);
    wpilibudp::processPacket(udpPacketBuf, n);
  }

  xrp::imuPeriodic();
  xrp::rangefinderPollForData();

  // Disable the robot when the UDP watchdog timesout
  // Also reset the max sequence number so we can handle reconnects
  if (!wpilibudp::dsWatchdogActive()) {
    wpilibudp::resetState();
    xrp::robotSetEnabled(false);
    xrp::imuSetEnabled(false);
  }

  if (xrp::robotPeriodic()) {
    // Package up and send all the data to client udp
    sendData();
  }

  updateLoopTime(loopStartTime);
  checkPrintStatus();
}

void loop1() {
  if (xrp::rangefinderInitialized()) {
    xrp::rangefinderPeriodic();
  }

  delay(50);
}
