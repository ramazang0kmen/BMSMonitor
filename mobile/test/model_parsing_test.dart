import 'dart:convert';
import 'dart:io';

import 'package:bmsmonitor/features/dashboard/domain/models/bms_battery.dart';
import 'package:bmsmonitor/features/dashboard/domain/models/live_telemetry.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('LiveTelemetry parsing', () {
    test('parses mixed int and double values from the real fixture', () {
      final json = jsonDecode(
        File('test/fixtures/live_telemetry.json').readAsStringSync(),
      );
      final telemetry = LiveTelemetry.fromJson(json);

      expect(telemetry.system.current, 0.0);
      expect(telemetry.system.totalCapacityAh, 200.0);
      expect(telemetry.bms1.soc, 17.0);
      expect(telemetry.bms1.temperatures[1], 25.0);
      expect(telemetry.bms1.cells, hasLength(8));
      expect(telemetry.bms1.minCellVoltage, 3.286);
      expect(telemetry.bms1.maxCellVoltage, 3.289);
      expect(
        telemetry.bms1.cells.where((cell) => cell.isMinimum),
        hasLength(2),
      );
      expect(telemetry.bms2.cells.last.isMaximum, isTrue);
      expect(telemetry.system.socMismatch, isTrue);
    });

    test('missing fields and empty cells are null-safe', () {
      final telemetry = LiveTelemetry.fromJson({
        'timestamp': 1,
        'bms1': {'cells': <Object?>[]},
      });

      expect(telemetry.system.voltage, 0.0);
      expect(telemetry.bms1.cells, isEmpty);
      expect(telemetry.bms1.online, isFalse);
      expect(telemetry.bms2, BmsBattery.fromJson(null));
      expect(telemetry.bms2.protectionFlags, 0);
    });

    test('offline data and non-zero protection flags are preserved', () {
      final battery = BmsBattery.fromJson({
        'online': false,
        'protectionFlags': 4660,
        'cells': [3, 3.1],
      });

      expect(battery.online, isFalse);
      expect(battery.protectionFlags, 0x1234);
      expect(battery.cells.first.voltage, 3.0);
      expect(battery.cells.last.voltage, 3.1);
    });

    test('sparse RTDB cell maps are accepted', () {
      final battery = BmsBattery.fromJson({
        'cells': {'0': 3.2, '2': 3.4, '1': 3.3},
      });

      expect(battery.cells.map((cell) => cell.voltage), [3.2, 3.3, 3.4]);
    });
  });
}
