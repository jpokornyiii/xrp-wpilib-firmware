#pragma once

#include <string>
#include <vector>

// This should get incremented everytime we make changes here
#define XRP_CONFIG_VERSION 2

enum NetworkMode { AP, STA, NOT_CONFIGURED };

class XRPNetConfig {
  public:
    NetworkMode mode { NetworkMode::NOT_CONFIGURED };
    std::string defaultAPName {""};
    std::string defaultAPPassword {""};
    std::vector< std::pair<std::string, std::string> > networkList;

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
};

class XRPConfiguration {
  public:
    static XRPConfiguration * getInstance();
    XRPNetConfig networkConfig;
    void generateDefaultConfig();
    std::string generateDefaultSSID();
    void loadConfiguration();
    std::string toJsonString();
    char chipID[20];
  private:
    XRPConfiguration();
    ~XRPConfiguration();
};

/*
class XRPConfiguration {
  pubilc:
    static XRPConfiguration * getInstance();
    std::string getDefaultAPName();
    std::string getDefaultAPPassword();
    NetworkMode getMode();
    const std::vector< std::pair<std::string, std::string> > & getNetworkList();

  private:
    XRPConfiguration();
     ~XRPConfiguration();
     void createChipID();
    void generateDefaultConfig();
    boolean loadJsonDocument(JsonDocument & doc);
    void loadConfiguration();
    void resetConfiguration();
    std::string toJsonString();
    void writeToDisk();

    NetworkMode mode { NetworkMode::NOT_CONFIGURED };
    char chipID[20];
    std::string defaultAPName {""};
    std::string defaultAPPassword {""};
    std::vector< std::pair<std::string, std::string> > networkList;
}

class XRPNetworkSetings {
  public:
    NetworkMode m_mode { NetworkMode::NOT_CONFIGURED };
    std::string m_ssid {""};
    std::string m_password {""};
}
*/