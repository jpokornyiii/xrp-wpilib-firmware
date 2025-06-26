#include <LittleFS.h>
#include <WiFi.h>
#include <vector>

#include "Network.h"
#include "config.h"

extern "C" {
    const unsigned char* GetResource_VERSION(size_t* len);
}

// Generate the status text file
void writeStatusToDisk(NetworkMode netMode) {
  File f = LittleFS.open("/status.txt", "w");

  XRPConfiguration * config = XRPConfiguration::getInstance();
  size_t len;
  std::string versionString{reinterpret_cast<const char*>(GetResource_VERSION(&len)), len};
  f.printf("Version: %s\n", versionString.c_str());
  f.printf("Chip ID: %s\n", config->chipID);
  f.printf("WiFi Mode: %s\n", netMode == NetworkMode::AP ? "AP" : "STA");
  if (netMode == NetworkMode::AP) {
    f.printf("AP SSID: %s\n", config->networkConfig.defaultAPName.c_str());
    f.printf("AP PASS: %s\n", config->networkConfig.defaultAPPassword.c_str());
  }
  else {
    f.printf("Connected to %s\n", WiFi.SSID().c_str());
  }

  f.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  f.close();
}

NetworkMode configureNetwork(XRPConfiguration & config) {
  bool shouldUseAP = false;
  WiFiMulti multi;

  if (config.networkConfig.mode == NetworkMode::AP) {
    shouldUseAP = true;
  }
  else if (config.networkConfig.mode == NetworkMode::STA) {
    Serial.println("[NET] Attempting to start in STA Mode");
    Serial.println("[NET] Trying the following networks:");
    for (auto netInfo : config.networkConfig.networkList) {
      Serial.printf("* %s\n", netInfo.first.c_str());
      multi.addAP(netInfo.first.c_str(), netInfo.second.c_str());
    }

    // Attempt to connect
    if (multi.run() != WL_CONNECTED) {
      Serial.println("[NET] Failed to connect to any network on list. Falling back to AP");
      shouldUseAP = true;
    }
  }

  if (shouldUseAP) {
    Serial.println("[NET] Attempting to start in AP mode");
    bool result = WiFi.softAP(
          config.networkConfig.defaultAPName.c_str(),
          config.networkConfig.defaultAPPassword.c_str());
    
    if (result) {
      Serial.println("[NET] AP Ready");
    }
    else {
      Serial.println("[NET] AP Set up Failed");
    }
  }

  Serial.println("[NET] ### NETWORK CONFIGURED ###");
  Serial.printf("[NET] SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("[NET] Actual WiFi Mode: %s\n", shouldUseAP ? "AP" : "STA");

  return shouldUseAP ? NetworkMode::AP : NetworkMode::STA;
}

void setupNetwork() {
  XRPConfiguration * configuration = XRPConfiguration::getInstance();

  // Busy-loop if there's no WiFi hardware
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("[NET] No WiFi Module");
    while (true);
  }

  // Set up WiFi AP
  WiFi.setHostname(configuration->generateDefaultSSID().c_str());

  // Use configuration information
  NetworkMode netConfigResult = configureNetwork(*configuration);
  // Write current status file
  writeStatusToDisk(netConfigResult);
}

/*
// Generate the status text file
void XRPNetwork::writeStatusToDisk() {
  File f = LittleFS.open("/status.txt", "w");

  XRPConfiguration * config = XRPConfiguration::getInstance();
  size_t len;
  std::string versionString{reinterpret_cast<const char*>(GetResource_VERSION(&len)), len};
  f.printf("Version: %s\n", versionString.c_str());
  f.printf("Chip ID: %s\n", config->chipID);
  f.printf("WiFi Mode: %s\n", m_mode == NetworkMode::AP ? "AP" : "STA");
  
  if (m_mode == NetworkMode::AP) {
    f.printf("AP SSID: %s\n", m_defaultName.c_str());
    f.printf("AP PASS: %s\n", m_defaultPassword.c_str());
  }
  else {
    f.printf("Connected to %s\n", WiFi.SSID().c_str());
  }

  f.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  f.close();
}

void XRPNetwork::configureNetwork(XRPConfiguration & config) {
  
  WiFiMulti multi;

  m_mode = config.getMode();
  m_defaultName = config.getDefaultAPName();
  m_defaultPassword = config.getDefaultAPPassword();

  if (m_mode == NetworkMode::STA) {
    Serial.println("[NET] Attempting to start in STA Mode");
    Serial.println("[NET] Trying the following networks:");
    // Add networks to try in order specified in config file
    for (auto netInfo : config.getNetworkList()) {
      std::string ssid = netInfo.first;
      std::string password = netInfo.second;
      Serial.printf("* %s\n", ssid.c_str());
      multi.addAP(ssid.c_str(), password.c_str());
    }

    // Attempt to connect in order of network list in multi
    if (multi.run() != WL_CONNECTED) {
      Serial.println("[NET] Failed to connect to any network on list. Falling back to AP");
      m_mode = NetworkMode::AP;
    }
  } else {
    // Just in case it's set to not configured
    m_mode = NetworkMode::AP;
  }

  if (m_mode == NetworkMode::AP) {
    Serial.println("[NET] Attempting to start in AP mode");
    bool result = WiFi.softAP(
          m_defaultName.c_str(),
          m_defaultPassword.c_str());
    
    if (result) {
      Serial.println("[NET] AP Ready");
    }
    else {
      Serial.println("[NET] AP Set up Failed");
      return;
    }
  }

  Serial.println("[NET] ### NETWORK CONFIGURED ###");
  Serial.printf("[NET] SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("[NET] Actual WiFi Mode: %s\n", shouldUseAP ? "AP" : "STA");
}

void XRPNetwork::setupNetwork() {
  XRPConfiguration * configuration = XRPConfiguration::getInstance();

  // Busy-loop if there's no WiFi hardware
  // TODO: Does this do what is expected?
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("[NET] No WiFi Module");
    while (true);
  }

  // Set up WiFi AP
  WiFi.setHostname(configuration->getDefaultAPName().c_str());

  // Use configuration information
  configureNetwork(*configuration);
  
  // Write current status file
  writeStatusToDisk();
}
*/