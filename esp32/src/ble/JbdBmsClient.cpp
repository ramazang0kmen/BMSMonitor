#include "ble/JbdBmsClient.h"

#include <algorithm>
#include <cmath>

#include "config/BmsConfig.h"

JbdBmsClient::~JbdBmsClient() {
  disconnect();
  if (client_) NimBLEDevice::deleteClient(client_);
}

void JbdBmsClient::begin(BmsDeviceState& device) {
  device_ = &device;
  if (client_) return;
  client_ = NimBLEDevice::createClient();
  if (!client_) {
    Serial.printf("[ERROR] %s BLE client allocation failed\n", device_->name);
    failureEvent_ = true;
    return;
  }
  client_->setClientCallbacks(this, false);
  client_->setConnectionParams(12, 24, 0, 150);
  client_->setConnectTimeout(BmsConfig::CONNECT_TIMEOUT_MS);
}

bool JbdBmsClient::startConnect(uint32_t now) {
  if (!client_ || !device_ || stage_ != Stage::Disconnected) return false;
  notifyCharacteristic_ = nullptr;
  writeCharacteristic_ = nullptr;
  receiveBuffer_.clear();
  basicReceived_ = false;
  cellsReceived_ = false;
  stage_ = Stage::Connecting;
  stageStartedMs_ = now;
  Serial.printf("[INFO] %s connecting to %s\n", device_->name, device_->mac);
  const NimBLEAddress address(device_->mac, BLE_ADDR_PUBLIC);
  // Async connect leaves loop() available for the second BMS and display.
  if (!client_->connect(address, true, true, true)) {
    stage_ = Stage::Disconnected;
    failureEvent_ = true;
    ++device_->errorCount;
    return false;
  }
  return true;
}

void JbdBmsClient::update(uint32_t now) {
  if (!client_ || !device_) return;
  if (stage_ == Stage::Connecting && client_->isConnected()) stage_ = Stage::Discovering;
  if (stage_ == Stage::Connecting && now - stageStartedMs_ > BmsConfig::CONNECT_TIMEOUT_MS + 1000U) {
    markCommunicationFailure("connect timeout");
    disconnect();
    return;
  }
  if (stage_ == Stage::Discovering) {
    if (!discoverJbdGatt(now)) {
      markCommunicationFailure("FF00/FF01/FF02 discovery failed");
      disconnect();
    }
    return;
  }
  if (stage_ == Stage::Settling && now - stageStartedMs_ >= BmsConfig::SUBSCRIBE_SETTLE_MS) {
    stage_ = Stage::Ready;
    nextPollMs_ = now;
  }
  if (stage_ == Stage::Ready && static_cast<int32_t>(now - nextPollMs_) >= 0) {
    basicReceived_ = false;
    cellsReceived_ = false;
    if (!sendJbdReadCommand(0x03)) {
      markCommunicationFailure("basic-info write failed");
      disconnect();
      return;
    }
    stage_ = Stage::AwaitBasic;
    stageStartedMs_ = now;
  }
  if (stage_ == Stage::AwaitBasic && basicReceived_) {
    basicReceived_ = false;
    if (!sendJbdReadCommand(0x04)) {
      markCommunicationFailure("cell-info write failed");
      disconnect();
      return;
    }
    stage_ = Stage::AwaitCells;
    stageStartedMs_ = now;
  } else if (stage_ == Stage::AwaitBasic && now - stageStartedMs_ > BmsConfig::RESPONSE_TIMEOUT_MS) {
    markCommunicationFailure("basic-info response timeout");
  }
  if (stage_ == Stage::AwaitCells && cellsReceived_) {
    cellsReceived_ = false;
    if (device_->lastUpdateMs != 0) device_->updateIntervalMs = now - device_->lastUpdateMs;
    device_->lastUpdateMs = now;
    device_->rssi = client_->getRssi();
    consecutiveResponseFailures_ = 0;
    stage_ = Stage::Ready;
    nextPollMs_ = now + BmsConfig::POLL_INTERVAL_MS;
  } else if (stage_ == Stage::AwaitCells && now - stageStartedMs_ > BmsConfig::RESPONSE_TIMEOUT_MS) {
    markCommunicationFailure("cell-info response timeout");
  }
}

bool JbdBmsClient::discoverJbdGatt(uint32_t now) {
  NimBLERemoteService* service = client_->getService(NimBLEUUID("ff00"));
  if (!service) return false;
  notifyCharacteristic_ = service->getCharacteristic(NimBLEUUID("ff01"));
  writeCharacteristic_ = service->getCharacteristic(NimBLEUUID("ff02"));
  if (!notifyCharacteristic_ || !writeCharacteristic_) return false;
  if (!notifyCharacteristic_->canNotify() ||
      (!writeCharacteristic_->canWrite() && !writeCharacteristic_->canWriteNoResponse())) return false;
  const bool subscribed = notifyCharacteristic_->subscribe(
      true,
      [this](NimBLERemoteCharacteristic*, uint8_t* bytes, size_t length, bool) {
        handleIncoming(bytes, length);
      }, true);
  if (!subscribed) return false;
  stage_ = Stage::Settling;
  stageStartedMs_ = now;
  Serial.printf("[INFO] %s connected | RSSI %d dBm\n", device_->name, client_->getRssi());
  return true;
}

bool JbdBmsClient::sendJbdReadCommand(uint8_t registerAddress) {
  if (!client_ || !client_->isConnected() || !writeCharacteristic_) return false;
  const uint16_t checksum = static_cast<uint16_t>(0U - registerAddress);
  const uint8_t command[] = {0xDD, 0xA5, registerAddress, 0x00,
                             static_cast<uint8_t>(checksum >> 8),
                             static_cast<uint8_t>(checksum), 0x77};
  if (BmsConfig::DEBUG_JBD_RAW) {
    Serial.printf("[%s] TX: ", device_->name);
    printHex(command, sizeof(command));
  }
  return writeCharacteristic_->writeValue(command, sizeof(command), writeCharacteristic_->canWrite());
}

void JbdBmsClient::handleIncoming(const uint8_t* data, size_t length) {
  if (BmsConfig::DEBUG_JBD_RAW) {
    Serial.printf("[%s] RX RAW: ", device_->name);
    printHex(data, length);
  }
  if (!data || length == 0) return;
  if (receiveBuffer_.size() + length > 512) {
    receiveBuffer_.clear();
    ++device_->errorCount;
  }
  receiveBuffer_.insert(receiveBuffer_.end(), data, data + length);
  processReceiveBuffer();
}

void JbdBmsClient::processReceiveBuffer() {
  while (true) {
    const auto start = std::find(receiveBuffer_.begin(), receiveBuffer_.end(), 0xDD);
    if (start == receiveBuffer_.end()) { receiveBuffer_.clear(); return; }
    receiveBuffer_.erase(receiveBuffer_.begin(), start);
    if (receiveBuffer_.size() < 4) return;
    const size_t frameLength = static_cast<size_t>(receiveBuffer_[3]) + 7U;
    if (frameLength > 263U) {
      receiveBuffer_.erase(receiveBuffer_.begin());
      ++device_->errorCount;
      continue;
    }
    if (receiveBuffer_.size() < frameLength) return;
    std::vector<uint8_t> frame(receiveBuffer_.begin(), receiveBuffer_.begin() + frameLength);
    receiveBuffer_.erase(receiveBuffer_.begin(), receiveBuffer_.begin() + frameLength);
    if (validateFrame(frame)) parseFrame(frame); else ++device_->errorCount;
  }
}

bool JbdBmsClient::validateFrame(const std::vector<uint8_t>& frame) const {
  if (frame.size() < 7 || frame.front() != 0xDD || frame.back() != 0x77) return false;
  if (frame.size() != static_cast<size_t>(frame[3]) + 7U || frame[2] != 0x00) return false;
  uint16_t sum = 0;
  for (size_t i = 2; i < frame.size() - 3; ++i) sum = static_cast<uint16_t>(sum + frame[i]);
  return static_cast<uint16_t>(0U - sum) == readBe16(&frame[frame.size() - 3]);
}

void JbdBmsClient::parseFrame(const std::vector<uint8_t>& frame) {
  if (frame[1] == 0x03) basicReceived_ = parseBasicInfo(&frame[4], frame[3]);
  else if (frame[1] == 0x04) cellsReceived_ = parseCellInfo(&frame[4], frame[3]);
}

bool JbdBmsClient::parseBasicInfo(const uint8_t* payload, size_t length) {
  if (!device_ || length < 23) return false;
  BatteryData& data = device_->data;
  data.voltage = readBe16(payload) * 0.01f;
  data.current = static_cast<int16_t>(readBe16(payload + 2)) * 0.01f;
  data.power = data.voltage * data.current;
  data.remainingCapacityAh = readBe16(payload + 4) * 0.01f;
  data.nominalCapacityAh = readBe16(payload + 6) * 0.01f;
  data.balancing = (static_cast<uint32_t>(readBe16(payload + 12)) |
                    (static_cast<uint32_t>(readBe16(payload + 14)) << 16)) != 0;
  data.protectionFlags = readBe16(payload + 16);
  data.soc = payload[19];
  data.chargeMos = (payload[20] & 0x01U) != 0;
  data.dischargeMos = (payload[20] & 0x02U) != 0;
  const size_t available = (length - 23U) / 2U;
  data.temperatureCount = std::min<uint8_t>(payload[22], static_cast<uint8_t>(std::min<size_t>(available, 8U)));
  for (uint8_t i = 0; i < data.temperatureCount; ++i)
    data.temperatures[i] = readBe16(payload + 23U + i * 2U) * 0.1f - 273.15f;
  return true;
}

bool JbdBmsClient::parseCellInfo(const uint8_t* payload, size_t length) {
  if (!device_ || length < 2 || (length % 2U) != 0) return false;
  BatteryData& data = device_->data;
  data.cellCount = std::min<uint8_t>(length / 2U, 32U);
  data.minCellVoltage = INFINITY;
  data.maxCellVoltage = -INFINITY;
  for (uint8_t i = 0; i < data.cellCount; ++i) {
    data.cellVoltages[i] = readBe16(payload + i * 2U) * 0.001f;
    data.minCellVoltage = std::min(data.minCellVoltage, data.cellVoltages[i]);
    data.maxCellVoltage = std::max(data.maxCellVoltage, data.cellVoltages[i]);
  }
  data.cellDelta = data.maxCellVoltage - data.minCellVoltage;
  return true;
}

void JbdBmsClient::markCommunicationFailure(const char* reason) {
  ++device_->errorCount;
  ++consecutiveResponseFailures_;
  Serial.printf("[WARN] %s %s (errors=%lu)\n", device_->name, reason,
                static_cast<unsigned long>(device_->errorCount));
  if (consecutiveResponseFailures_ >= 3) disconnect();
  else { stage_ = Stage::Ready; nextPollMs_ = millis() + 500U; }
}

void JbdBmsClient::disconnect() {
  if (client_ && client_->isConnected()) client_->disconnect();
  if (device_) device_->connected = false;
  stage_ = Stage::Disconnected;
}

void JbdBmsClient::onConnect(NimBLEClient*) {
  if (device_) device_->connected = true;
  stage_ = Stage::Discovering;
}

void JbdBmsClient::onConnectFail(NimBLEClient*, int reason) {
  if (device_) {
    device_->connected = false;
    ++device_->errorCount;
    Serial.printf("[WARN] %s connect failed (reason %d)\n", device_->name, reason);
  }
  stage_ = Stage::Disconnected;
  failureEvent_ = true;
}

void JbdBmsClient::onDisconnect(NimBLEClient*, int reason) {
  if (device_) {
    device_->connected = false;
    Serial.printf("[WARN] %s disconnected (reason %d)\n", device_->name, reason);
  }
  stage_ = Stage::Disconnected;
  failureEvent_ = true;
}

bool JbdBmsClient::consumeFailureEvent() { const bool value = failureEvent_; failureEvent_ = false; return value; }
bool JbdBmsClient::isDisconnected() const { return stage_ == Stage::Disconnected; }
uint16_t JbdBmsClient::readBe16(const uint8_t* data) { return (static_cast<uint16_t>(data[0]) << 8) | data[1]; }
void JbdBmsClient::printHex(const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; ++i) Serial.printf("%02X%s", data[i], i + 1 == length ? "\n" : " ");
}
