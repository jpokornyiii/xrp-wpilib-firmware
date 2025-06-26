// This object is used to setup and create Wifi connections

#pragma once

class Network {

};

/*
class XRPNetwork {
  public:
    void setupNetwork();
    void writeStatusToDisk();

    NetworkMode m_mode { NetworkMode::NOT_CONFIGURED };
    std::string m_defaultName {""};
    std::string m_defaultPassword {""};
}
*/

void setupNetwork();