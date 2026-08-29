import 'package:equatable/equatable.dart';

import '../../../../core/utils/json_value.dart';

class SystemBattery extends Equatable {
  const SystemBattery({
    required this.voltage,
    required this.current,
    required this.power,
    required this.soc,
    required this.remainingAh,
    required this.totalCapacityAh,
    required this.socMismatch,
    required this.socDelta,
  });

  factory SystemBattery.fromJson(Object? value) {
    final json = asStringMap(value);
    return SystemBattery(
      voltage: asDouble(json['voltage']),
      current: asDouble(json['current']),
      power: asDouble(json['power']),
      soc: asDouble(json['soc']),
      remainingAh: asDouble(json['remainingAh']),
      totalCapacityAh: asDouble(json['totalCapacityAh']),
      socMismatch: asBool(json['socMismatch']),
      socDelta: asDouble(json['socDelta']),
    );
  }

  static const empty = SystemBattery(
    voltage: 0,
    current: 0,
    power: 0,
    soc: 0,
    remainingAh: 0,
    totalCapacityAh: 0,
    socMismatch: false,
    socDelta: 0,
  );

  final double voltage;
  final double current;
  final double power;
  final double soc;
  final double remainingAh;
  final double totalCapacityAh;
  final bool socMismatch;
  final double socDelta;

  @override
  List<Object?> get props => [
    voltage,
    current,
    power,
    soc,
    remainingAh,
    totalCapacityAh,
    socMismatch,
    socDelta,
  ];
}
