#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiMulti.h>
#include <WiFi.h>

#include "config.h"

std::string XRPConfiguration::generateDefaultSSID() {
  char default_ssid[32];
  memset(default_ssid, 0, sizeof(default_ssid));
  sprintf(default_ssid, "XRP-%s", chipID);
  return std::string(default_ssid);
}

void XRPConfiguration::generateDefaultConfig() {

  std::string defaultAPName = generateDefaultSSID();  
  XRPNetConfig a_networkConfig;
  // Set up the default AP
  a_networkConfig.defaultAPName = defaultAPName;
  a_networkConfig.defaultAPPassword = "xrp-wpilib";
  a_networkConfig.mode = NetworkMode::AP;

  // Load in a test AP
  networkConfig.networkList.push_back(std::make_pair("Test Network", "Test Password"));

  networkConfig = a_networkConfig;
}

std::string XRPConfiguration::toJsonString() {
  JsonDocument config;

  config["configVersion"] = XRP_CONFIG_VERSION;

  // Network
  JsonObject network = config["network"].to<JsonObject>();
  JsonObject defaultAP = network["defaultAP"].to<JsonObject>();

  defaultAP["ssid"] = networkConfig.defaultAPName;
  defaultAP["password"] = networkConfig.defaultAPPassword;

  JsonArray prefNetworks = network["networkList"].to<JsonArray>();
  network["mode"] = networkConfig.mode == NetworkMode::AP ? "AP" : "STA";

  for (auto netInfo : networkConfig.networkList) {
    JsonDocument networkObj;
    networkObj["ssid"] = netInfo.first;
    networkObj["password"] = netInfo.second;
    prefNetworks.add(networkObj);
  }

  std::string ret;
  serializeJsonPretty(config, ret);
  return ret;
}

void writeConfigToDisk(XRPConfiguration config) {
  File f = LittleFS.open("/config.json", "w");
  f.print(config.toJsonString().c_str());
  f.close();
}

void XRPConfiguration::loadConfiguration() {
  
  File f = LittleFS.open("/config.json", "r");
  if (!f) {
    Serial.println("[CONFIG] No config file found. Creating default");
    
    generateDefaultConfig();
    writeConfigToDisk(*this);
    return;
  }

  // Load and verify
  JsonDocument configJson;
  auto jsonErr = deserializeJson(configJson, f);
  f.close();

  if (jsonErr) {
    Serial.print("[CONFIG] Deserialization failed: ");
    Serial.println(jsonErr.f_str());
    Serial.println("[CONFIG] Using default");

    generateDefaultConfig();
    writeConfigToDisk(*this);
    return;
  }

  // If new config version, wipe out old file due to potential backwards incompatability
  if (configJson["configVersion"] != XRP_CONFIG_VERSION) {
    Serial.print("[CONFIG] Configuration version mismatch. Using default");

    generateDefaultConfig();
    writeConfigToDisk(*this);
  }

  /************ Network Section ****************/

  // If no network info, generate default file
  if (!configJson["network"].is<JsonVariant>()) {
    Serial.print("[CONFIG] No network information specified. Using defaults");

    generateDefaultConfig();
    writeConfigToDisk(*this);
  }

  auto networkInfo = configJson["network"];

  // Generate a temporary defaults object
  XRPConfiguration tempDefault;
  tempDefault.generateDefaultConfig();
  bool shouldWrite = false;

  // Check if there's a default AP provided
  if (networkInfo["defaultAP"].is<JsonVariant>()) {
    auto defaultAPInfo = networkInfo["defaultAP"];
    if (defaultAPInfo["ssid"].is<JsonString>() && defaultAPInfo["ssid"].as<std::string>().length() != 0) {
      networkConfig.defaultAPName = defaultAPInfo["ssid"].as<std::string>();
    }
    else {
      Serial.println("[CONFIG] Default AP SSID missing. Using default");
      networkConfig.defaultAPName = tempDefault.networkConfig.defaultAPName;
      shouldWrite = true;
    }

    if (defaultAPInfo["password"].is<JsonString>()) {
      networkConfig.defaultAPPassword = defaultAPInfo["password"].as<std::string>();
    }
    else {
      Serial.println("[CONFIG] Default AP Password missing. Using default");
      networkConfig.defaultAPPassword = tempDefault.networkConfig.defaultAPPassword;
      shouldWrite = true;
    }
  }

  // Load in the preferred network list
  if (networkInfo["networkList"].is<JsonVariant>()) {
    auto networkList = networkInfo["networkList"];
    JsonArray networks = networkList.as<JsonArray>();
    for (auto v : networks) {
      if (v["ssid"].is<JsonString>() && v["password"].is<JsonString>()) {
        networkConfig.networkList.push_back(std::make_pair<std::string, std::string>(v["ssid"], v["password"]));
      }
    }
  }

  // Check if we're in STA mode. If so, we'll need at least 1 network in the list
  if (networkInfo["mode"].is<JsonString>()) {
    if (networkInfo["mode"] == "STA") {
      if (networkConfig.networkList.size() > 0) {
        networkConfig.mode = NetworkMode::STA;
      }
      else {
        Serial.println("[CONFIG] Network mode set to STA but no provided networks. Resettign to AP");
        networkConfig.mode = NetworkMode::AP;
        shouldWrite = true;
      }
    }
    else {
      networkConfig.mode = NetworkMode::AP;
    }
  }
  else {
    Serial.println("[CONFIG] Network Mode missing. Defaulting to AP");
    networkConfig.mode = NetworkMode::AP;
    shouldWrite = true;
  }

  if (shouldWrite) {
    writeConfigToDisk(*this);
  }
}

XRPConfiguration::XRPConfiguration() {
  // Get and store chip id
  memset(chipID, 0, sizeof(chipID));
  pico_unique_board_id_t id_out;
  pico_get_unique_board_id(&id_out);
  sprintf(chipID, "%02x%02x-%02x%02x", id_out.id[4], id_out.id[5], id_out.id[6], id_out.id[7]);
}

XRPConfiguration::~XRPConfiguration() {}

XRPConfiguration * XRPConfiguration::getInstance() {
  static XRPConfiguration * m_instance = nullptr;
  // Arduino is single threaded
  if(m_instance == nullptr) {
    m_instance = new XRPConfiguration();
  }
  return m_instance;
}

/*
class XRPConfiguration {
  pubilc:
      static XRPConfiguration * getInstance();

      XRPNetConfig & operator=(const XRPNetConfig & other) {
        if(this != &other) {
          defaultAPName = other.defaultAPName;
          defaultAPPassword = other.defaultAPPassword;
          mode = other.mode;
          networkList.clear();
          for(const auto & pair : other.networkList) {
            networkList.push_back(std::make_pair(pair.first, pair.second));
          }
        }
        return *this;
      }

  private:
      void generateDefaultConfig();
      void loadConfiguration();
      std::string toJsonString();
      void writeToDisk();

      NetworkMode mode { NetworkMode::NOT_CONFIGURED };
      char chipID[20];
      std::string defaultAPName {""};
      std::string defaultAPPassword {""};
      std::vector< std::pair<std::string, std::string> > networkList;
}

XRPConfiguration::XRPConfiguration() {
  createChipID();
  generateDefaultConfig();
}

boolean loadJsonDocument( JsonDocument & doc ) {

  File f = LittleFS.open("/config.json", "r");

  if (!f) {
    Serial.println("[CONFIG] No config file found. Creating default");
    return false;
  }

  // Load and verify
  auto jsonErr = deserializeJson(doc, f);
  f.close();

  if (jsonErr) {
    Serial.print("[CONFIG] Deserialization failed: ");
    Serial.println(jsonErr.f_str());
    Serial.println("[CONFIG] Using default");
    return false;
  }

  // If new config version, wipe out old file due to potential backwards incompatability
  if (doc["configVersion"] != XRP_CONFIG_VERSION) {
    Serial.print("[CONFIG] Configuration version mismatch. Using default");
    return false;
  }

  // If no network info, generate default file
  if (!doc["network"].is<JsonVariant>()) {
    Serial.print("[CONFIG] No network information specified. Using defaults");
    return false;
  }

  return true;

}

void XRPConfiguration::loadConfiguration() {

  JsonDocument configJson;
  if( !loadJsonDocument(configJson) ) {  
    generateDefaultConfig();
    writeConfigToDisk();
    return faluse;
  }

  //************ Network Section ****************

  auto networkInfo = configJson["network"];

  // Check if there's default AP info provided
  if (networkInfo["defaultAP"].is<JsonVariant>()) {
    auto defaultAPInfo = networkInfo["defaultAP"];
    if (defaultAPInfo["ssid"].is<JsonString>() && defaultAPInfo["ssid"].as<std::string>().length() != 0) {
      m_defaultAPName = defaultAPInfo["ssid"].as<std::string>();
    }
    else {
      Serial.println("[CONFIG] Default AP SSID missing. Using default");
    }

    if (defaultAPInfo["password"].is<JsonString>()) {
      m_defaultAPPassword = defaultAPInfo["password"].as<std::string>();
    }
    else {
      Serial.println("[CONFIG] Default AP Password missing. Using default");
    }
  }

  // Load in the network list
  if (networkInfo["networkList"].is<JsonVariant>()) {
    auto networkList = networkInfo["networkList"];
    JsonArray networks = networkList.as<JsonArray>();
    for (auto v : networks) {
      if (v["ssid"].is<JsonString>() && v["password"].is<JsonString>()) {
        m_networkList.push_back(std::make_pair<std::string, std::string>(v["ssid"], v["password"]));
      }
    }
  }

  // Check if we're in STA mode. If so, we'll need at least 1 network in the list
  if (networkInfo["mode"].is<JsonString>()) {
    if (networkInfo["mode"] == "STA") {
      if (networkConfig.networkList.size() > 0) {
        m_mode = NetworkMode::STA;
      }
      else {
        Serial.println("[CONFIG] Network mode set to STA but no provided networks. Resetting to AP");
        m_mode = NetworkMode::AP;
      }
    }
    else {
      m_mode = NetworkMode::AP;
    }
  }
  else {
    Serial.println("[CONFIG] Network Mode missing. Defaulting to AP");
    m_mode = NetworkMode::AP;
  }
}

void XRPConfigurations::createChipID() {
  // Get and store chip id
  memset(m_chipID, 0, sizeof(chipID));
  pico_unique_board_id_t id_out;
  pico_get_unique_board_id(&id_out);
  sprintf(m_chipID, "%02x%02x-%02x%02x", id_out.id[4], id_out.id[5], id_out.id[6], id_out.id[7]);
}

void XRPConfiguration::generateDefaultConfig() {

  resetConfig();

  // Create default AP name, password, and mode
  char default_ssid[32];
  memset(default_ssid, 0, sizeof(default_ssid));
  sprintf(default_ssid, "XRP-%s", chipID);
  m_defaultAPName = std::string(default_ssid);
  m_defaultAPPassword = "xrp-wpilib";
  m_mode = NetworkMode::AP;

  // Create example network
  m_networkList.push_back(std::make_pair("Test Network", "Test Password"));
}

void XRPConfiguration::resetConfig() {
  m_defaultAPName.clear();
  m_defaultAPPassword.clear();
  m_networkList.clear();
  m_mode = NetworkMode::NOT_CONFIGURED;
}

void XRPConfiguration::writeConfigToDisk() {
  File f = LittleFS.open("/config.json", "w");
  f.print(toJsonString().c_str());
  f.close();
}

std::string XRPConfiguration::getDefaultAPName() {
  return m_defaultAPName;
}

std::string XRPConfiguration::getDefaultAPPassword() {
  return m_defaultAPPassword;
}

NetworkMode XRPConfiguration::getMode() {
  return m_mode;
}

const std::vector< std::pair<std::string, std::string> > & getNetworkList() {
  return networkList;
}

*/