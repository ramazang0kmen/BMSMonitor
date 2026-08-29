import 'package:equatable/equatable.dart';

import '../../../../core/utils/json_value.dart';
import 'bms_battery.dart';
import 'system_battery.dart';

class LiveTelemetry extends Equatable {
  const LiveTelemetry({
    required this.timestamp,
    required this.system,
    required this.bms1,
    required this.bms2,
  });

  factory LiveTelemetry.fromJson(Object? value) {
    final json = asStringMap(value);
    return LiveTelemetry(
      timestamp: asInt(json['timestamp']),
      system: SystemBattery.fromJson(json['system']),
      bms1: BmsBattery.fromJson(json['bms1']),
      bms2: BmsBattery.fromJson(json['bms2']),
    );
  }

  final int timestamp;
  final SystemBattery system;
  final BmsBattery bms1;
  final BmsBattery bms2;

  @override
  List<Object?> get props => [timestamp, system, bms1, bms2];
}
