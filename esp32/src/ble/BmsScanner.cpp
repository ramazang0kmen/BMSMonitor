#include "ble/BmsScanner.h"

#include <algorithm>
#include <cctype>

void BmsScanner::begin() {
  NimBLEScan* scanner = NimBLEDevice::getScan();
  scanner->setScanCallbacks(this, false);
  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(80);
}

void BmsScanner::scan(uint32_t durationMs) {
  devices_.clear();
  NimBLEScan* scanner = NimBLEDevice::getScan();
  scanner->clearResults();
  Serial.printf("[BLE] Scanning for %lu seconds...\n", durationMs / 1000UL);
  // getResults(duration) blocks until the scan-end event. NimBLE 2.x start() is
  // asynchronous, which would let candidate processing race the callbacks.
  scanner->getResults(durationMs, false);
  scanner->clearResults();
  Serial.printf("[BLE] Scan complete. %u unique device(s).\n",
                static_cast<unsigned>(devices_.size()));
}

void BmsScanner::onResult(const NimBLEAdvertisedDevice* device) {
  const std::string mac = device->getAddress().toString();
  const auto duplicate = std::find_if(
      devices_.begin(), devices_.end(),
      [&mac](const DiscoveredBleDevice& item) { return item.bms.mac == mac; });
  if (duplicate != devices_.end()) {
    if (device->getRSSI() > duplicate->bms.rssi) duplicate->bms.rssi = device->getRSSI();
    return;
  }

  DiscoveredBleDevice found;
  found.bms.name = device->haveName() ? device->getName() : "";
  found.bms.mac = mac;
  found.bms.addressType = device->getAddressType();
  found.bms.rssi = device->getRSSI();
  found.possibleBms = looksLikeJbd(*device);

  Serial.println("\n=== BLE DEVICE ===");
  Serial.printf("Name: %s\n", found.bms.name.empty() ? "<unknown>" : found.bms.name.c_str());
  Serial.printf("MAC: %s\n", found.bms.mac.c_str());
  Serial.printf("RSSI: %d dBm\n", found.bms.rssi);
  Serial.printf("Address type: %s (%u)\n", addressTypeName(found.bms.addressType),
                found.bms.addressType);
  Serial.println("Services:");
  for (size_t i = 0; i < device->getServiceUUIDCount(); ++i) {
    const std::string uuid = device->getServiceUUID(i).toString();
    found.services.push_back(uuid);
    Serial.printf("  - %s\n", uuid.c_str());
  }
  if (found.services.empty()) Serial.println("  <none advertised>");

  if (device->haveManufacturerData()) {
    const auto data = device->getManufacturerData();
    Serial.print("Manufacturer Data: ");
    printHex(reinterpret_cast<const uint8_t*>(data.data()), data.size());
  } else {
    Serial.println("Manufacturer Data: <none>");
  }

  if (device->getServiceDataCount() > 0) {
    Serial.println("Service Data:");
    for (size_t i = 0; i < device->getServiceDataCount(); ++i) {
      const auto data = device->getServiceData(i);
      Serial.printf("  %s: ", device->getServiceDataUUID(i).toString().c_str());
      printHex(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    }
  } else {
    Serial.println("Service Data: <none>");
  }
  Serial.printf("Possible JBD BMS: %s\n", found.possibleBms ? "YES" : "NO");
  Serial.println("==================");
  devices_.push_back(found);
}

bool BmsScanner::looksLikeJbd(const NimBLEAdvertisedDevice& device) {
  for (size_t i = 0; i < device.getServiceUUIDCount(); ++i) {
    if (device.getServiceUUID(i) == NimBLEUUID("ff00")) return true;
  }

  std::string name = device.haveName() ? device.getName() : "";
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  const char* traces[] = {"JBD", "BMS", "XIAOXIANG", "JIABAIDA", "SP", "BT05", "LH-"};
  for (const char* trace : traces) {
    if (name.find(trace) != std::string::npos) return true;
  }
  return false;
}

void BmsScanner::printCandidateSummary() const {
  Serial.println("\n=== POSSIBLE BMS DEVICES ===");
  size_t number = 0;
  for (const auto& device : devices_) {
    if (!device.possibleBms) continue;
    Serial.printf("\nBMS Candidate #%u\n", static_cast<unsigned>(++number));
    Serial.printf("Name: %s\n", device.bms.name.empty() ? "<unknown>" : device.bms.name.c_str());
    Serial.printf("MAC: %s\n", device.bms.mac.c_str());
    Serial.printf("RSSI: %d dBm\n", device.bms.rssi);
    Serial.println("Services:");
    for (const auto& uuid : device.services) Serial.printf("  - %s\n", uuid.c_str());
    if (device.services.empty()) Serial.println("  <none advertised>");
  }
  if (number == 0) Serial.println("No likely JBD BMS was identified in this scan.");
  Serial.println("================================");
}

void BmsScanner::printHex(const uint8_t* data, size_t length) {
  if (!data || length == 0) {
    Serial.println("<empty>");
    return;
  }
  for (size_t i = 0; i < length; ++i) Serial.printf("%02X%s", data[i], i + 1 == length ? "" : " ");
  Serial.println();
}

const char* BmsScanner::addressTypeName(uint8_t type) {
  switch (type) {
    case 0: return "PUBLIC";
    case 1: return "RANDOM";
    case 2: return "PUBLIC_ID";
    case 3: return "RANDOM_ID";
    default: return "UNKNOWN";
  }
}
