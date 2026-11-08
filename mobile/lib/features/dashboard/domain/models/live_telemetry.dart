import 'package:equatable/equatable.dart';

import '../../../../core/utils/json_value.dart';
import 'bms_battery.dart';
import 'system_battery.dart';

class LiveTelemetry extends Equatable {
  const LiveTelemetry({
    required this.timestamp,
    required this.system,
    required this.batteries,
  });

  factory LiveTelemetry.fromJson(Object? value) {
    final json = asStringMap(value);
    final dynamicBms = asStringMap(json['bms']);
    final batteries = dynamicBms.entries
        .map((entry) => BmsBattery.fromJson(entry.value, id: entry.key))
        .toList();
    if (batteries.isEmpty) {
      if (json.containsKey('bms1')) {
        batteries.add(BmsBattery.fromJson(json['bms1'], id: 'bms_1', order: 0));
      }
      if (json.containsKey('bms2')) {
        batteries.add(BmsBattery.fromJson(json['bms2'], id: 'bms_2', order: 1));
      }
    }
    batteries.sort((a, b) {
      final byOrder = a.order.compareTo(b.order);
      return byOrder != 0 ? byOrder : a.name.compareTo(b.name);
    });
    return LiveTelemetry(
      timestamp: asInt(json['timestamp']),
      system: SystemBattery.fromJson(json['system']),
      batteries: batteries,
    );
  }

  final int timestamp;
  final SystemBattery system;
  final List<BmsBattery> batteries;

  BmsBattery get bms1 =>
      batteries.isNotEmpty ? batteries.first : BmsBattery.empty;
  BmsBattery get bms2 => batteries.length > 1 ? batteries[1] : BmsBattery.empty;

  @override
  List<Object?> get props => [timestamp, system, batteries];
}
