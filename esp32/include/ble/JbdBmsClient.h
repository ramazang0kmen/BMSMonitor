#pragma once

#include <NimBLEDevice.h>
#include <vector>

#include "models/BatteryData.h"

class JbdBmsClient : public NimBLEClientCallbacks {
 public:
  JbdBmsClient() = default;
  ~JbdBmsClient();
  bool attach(BmsDeviceState& device);
  void release();
  bool startConnect(uint32_t now);
  void update(uint32_t now);
  void disconnect();
  bool consumeFailureEvent();
  bool isDisconnected() const;
  bool isConnecting() const;
  bool hasDevice() const { return device_ != nullptr; }

 private:
  enum class Stage : uint8_t { Disconnected, Connecting, Discovering, Settling, Ready, AwaitBasic, AwaitCells };
  void onConnect(NimBLEClient* client) override;
  void onConnectFail(NimBLEClient* client, int reason) override;
  void onDisconnect(NimBLEClient* client, int reason) override;
  bool discoverJbdGatt(uint32_t now);
  bool sendJbdReadCommand(uint8_t registerAddress);
  void handleIncoming(const uint8_t* data, size_t length);
  void processReceiveBuffer();
  bool validateFrame(const std::vector<uint8_t>& frame) const;
  void parseFrame(const std::vector<uint8_t>& frame);
  bool parseBasicInfo(const uint8_t* payload, size_t length);
  bool parseCellInfo(const uint8_t* payload, size_t length);
  void markCommunicationFailure(const char* reason);
  static uint16_t readBe16(const uint8_t* data);
  static void printHex(const uint8_t* data, size_t length);

  NimBLEClient* client_ = nullptr;
  NimBLERemoteCharacteristic* notifyCharacteristic_ = nullptr;
  NimBLERemoteCharacteristic* writeCharacteristic_ = nullptr;
  BmsDeviceState* device_ = nullptr;
  std::vector<uint8_t> receiveBuffer_;
  Stage stage_ = Stage::Disconnected;
  uint32_t stageStartedMs_ = 0;
  uint32_t nextPollMs_ = 0;
  bool basicReceived_ = false;
  bool cellsReceived_ = false;
  bool failureEvent_ = false;
  bool intentionalDisconnect_ = false;
  uint8_t consecutiveResponseFailures_ = 0;
};
