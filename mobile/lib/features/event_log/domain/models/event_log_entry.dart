import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:equatable/equatable.dart';

import '../../../../core/utils/json_value.dart';

enum EventSeverity { info, warning, error }

/// What an entry is about, used by the filter chips on the log page.
enum EventCategory { battery, load, bms, system }

class EventSystemSnapshot extends Equatable {
  const EventSystemSnapshot({
    required this.voltage,
    required this.current,
    required this.power,
    required this.soc,
    required this.remainingAh,
    required this.socDelta,
  });

  factory EventSystemSnapshot.fromJson(Object? value) {
    final json = asStringMap(value);
    return EventSystemSnapshot(
      voltage: asDouble(json['voltage']),
      current: asDouble(json['current']),
      power: asDouble(json['power']),
      soc: asDouble(json['soc']),
      remainingAh: asDouble(json['remainingAh']),
      socDelta: asDouble(json['socDelta']),
    );
  }

  final double voltage;
  final double current;
  final double power;
  final double soc;
  final double remainingAh;
  final double socDelta;

  @override
  List<Object?> get props => [
    voltage,
    current,
    power,
    soc,
    remainingAh,
    socDelta,
  ];
}

class EventBmsSnapshot extends Equatable {
  const EventBmsSnapshot({
    required this.online,
    required this.soc,
    required this.voltage,
    required this.current,
    required this.power,
    required this.remainingAh,
    required this.minCellVoltage,
    required this.maxCellVoltage,
    required this.temperature,
    required this.chargeMos,
    required this.dischargeMos,
    required this.protectionFlags,
  });

  factory EventBmsSnapshot.fromJson(Object? value) {
    final json = asStringMap(value);
    return EventBmsSnapshot(
      online: asBool(json['online']),
      soc: asInt(json['soc']),
      voltage: asDouble(json['voltage']),
      current: asDouble(json['current']),
      power: asDouble(json['power']),
      remainingAh: asDouble(json['remainingAh']),
      minCellVoltage: asDouble(json['minCellVoltage']),
      maxCellVoltage: asDouble(json['maxCellVoltage']),
      temperature: asDouble(json['temperature']),
      chargeMos: asBool(json['chargeMos']),
      dischargeMos: asBool(json['dischargeMos']),
      protectionFlags: asInt(json['protectionFlags']),
    );
  }

  final bool online;
  final int soc;
  final double voltage;
  final double current;
  final double power;
  final double remainingAh;
  final double minCellVoltage;
  final double maxCellVoltage;
  final double temperature;
  final bool chargeMos;
  final bool dischargeMos;
  final int protectionFlags;

  int get cellDeltaMv => ((maxCellVoltage - minCellVoltage) * 1000).round();

  @override
  List<Object?> get props => [
    online,
    soc,
    voltage,
    current,
    power,
    remainingAh,
    minCellVoltage,
    maxCellVoltage,
    temperature,
    chargeMos,
    dischargeMos,
    protectionFlags,
  ];
}

/// The last reading taken while the load was still on, attached to
/// `discharge_stopped` entries: the voltage under load right before the cut.
class EventBeforeSnapshot extends Equatable {
  const EventBeforeSnapshot({
    required this.voltage,
    required this.current,
    required this.power,
    required this.soc,
    required this.bms1Voltage,
    required this.bms2Voltage,
  });

  static EventBeforeSnapshot? fromJson(Object? value) {
    if (value is! Map) return null;
    final json = asStringMap(value);
    return EventBeforeSnapshot(
      voltage: asDouble(json['voltage']),
      current: asDouble(json['current']),
      power: asDouble(json['power']),
      soc: asDouble(json['soc']),
      bms1Voltage: asDouble(json['bms1Voltage']),
      bms2Voltage: asDouble(json['bms2Voltage']),
    );
  }

  final double voltage;
  final double current;
  final double power;
  final double soc;
  final double bms1Voltage;
  final double bms2Voltage;

  @override
  List<Object?> get props => [
    voltage,
    current,
    power,
    soc,
    bms1Voltage,
    bms2Voltage,
  ];
}

class EventLogEntry extends Equatable {
  const EventLogEntry({
    required this.id,
    required this.type,
    required this.severity,
    required this.createdAt,
    required this.bmsIndex,
    required this.detail,
    required this.firmwareVersion,
    required this.uptimeSeconds,
    required this.system,
    required this.bms1,
    required this.bms2,
    this.before,
  });

  factory EventLogEntry.fromJson(String id, Object? value) {
    final json = asStringMap(value);
    return EventLogEntry(
      id: id,
      type: asText(json['type'], 'unknown'),
      severity: switch (json['severity']) {
        'error' => EventSeverity.error,
        'warning' => EventSeverity.warning,
        _ => EventSeverity.info,
      },
      createdAt: _asDateTime(json['createdAt']),
      bmsIndex: asInt(json['bmsIndex']),
      detail: asInt(json['detail']),
      firmwareVersion: asText(json['firmwareVersion'], '-'),
      uptimeSeconds: asInt(json['uptimeSeconds']),
      system: EventSystemSnapshot.fromJson(json['system']),
      bms1: EventBmsSnapshot.fromJson(json['bms1']),
      bms2: EventBmsSnapshot.fromJson(json['bms2']),
      before: EventBeforeSnapshot.fromJson(json['before']),
    );
  }

  final String id;
  final String type;
  final EventSeverity severity;
  final DateTime? createdAt;

  /// 0 for system-wide events, 1 or 2 for the BMS the event is about.
  final int bmsIndex;

  /// Type-specific: ESP32 reset reason for `boot`, protection flags for
  /// `bms_protection`.
  final int detail;
  final String firmwareVersion;
  final int uptimeSeconds;
  final EventSystemSnapshot system;
  final EventBmsSnapshot bms1;
  final EventBmsSnapshot bms2;
  final EventBeforeSnapshot? before;

  EventCategory get category => switch (type) {
    'avg_soc_low' ||
    'avg_soc_empty' ||
    'avg_soc_full' ||
    'bms_soc_low' ||
    'bms_soc_empty' ||
    'soc_mismatch' => EventCategory.battery,
    'discharge_stopped' || 'discharge_started' => EventCategory.load,
    'boot' => EventCategory.system,
    _ => EventCategory.bms,
  };

  @override
  List<Object?> get props => [
    id,
    type,
    severity,
    createdAt,
    bmsIndex,
    detail,
    firmwareVersion,
    uptimeSeconds,
    system,
    bms1,
    bms2,
    before,
  ];
}

DateTime? _asDateTime(Object? value) => switch (value) {
  Timestamp() => value.toDate(),
  DateTime() => value,
  num() => DateTime.fromMillisecondsSinceEpoch(value.toInt()),
  String() => DateTime.tryParse(value),
  _ => null,
};
