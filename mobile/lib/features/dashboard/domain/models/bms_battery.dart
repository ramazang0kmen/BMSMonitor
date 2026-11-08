import 'dart:math' as math;

import 'package:equatable/equatable.dart';

import '../../../../core/utils/json_value.dart';
import 'cell_voltage.dart';

class BmsBattery extends Equatable {
  const BmsBattery({
    required this.id,
    required this.order,
    required this.online,
    required this.connected,
    required this.lastSeen,
    required this.name,
    required this.rssi,
    required this.voltage,
    required this.current,
    required this.power,
    required this.soc,
    required this.remainingAh,
    required this.capacityAh,
    required this.temperatures,
    required this.minCellVoltage,
    required this.maxCellVoltage,
    required this.cellDeltaMv,
    required this.chargeMos,
    required this.dischargeMos,
    required this.balancing,
    required this.protectionFlags,
    required this.cells,
  });

  factory BmsBattery.fromJson(Object? value, {String? id, int? order}) {
    final json = asStringMap(value);
    final rawCells = asObjectList(
      json['cells'],
    ).map((cell) => asDouble(cell)).toList(growable: false);
    final suppliedMin = asDouble(json['minCellVoltage']);
    final suppliedMax = asDouble(json['maxCellVoltage']);
    final calculatedMin = rawCells.isEmpty
        ? suppliedMin
        : rawCells.reduce(math.min);
    final calculatedMax = rawCells.isEmpty
        ? suppliedMax
        : rawCells.reduce(math.max);
    const epsilon = 0.0005;
    final cells = [
      for (var index = 0; index < rawCells.length; index++)
        CellVoltage(
          index: index,
          voltage: rawCells[index],
          isMinimum: (rawCells[index] - calculatedMin).abs() <= epsilon,
          isMaximum: (rawCells[index] - calculatedMax).abs() <= epsilon,
        ),
    ];
    return BmsBattery(
      id: id ?? asText(json['id'], ''),
      order: order ?? asInt(json['order']),
      online: asBool(json['online']),
      connected: asBool(json['connected'], asBool(json['online'])),
      lastSeen: asInt(json['lastSeen']),
      name: asText(json['name'], 'BMS'),
      rssi: asInt(json['rssi']),
      voltage: asDouble(json['voltage']),
      current: asDouble(json['current']),
      power: asDouble(json['power']),
      soc: asDouble(json['soc']),
      remainingAh: asDouble(json['remainingAh']),
      capacityAh: asDouble(json['capacityAh']),
      temperatures: [
        asDouble(json['temperature1']),
        asDouble(json['temperature2']),
        asDouble(json['temperature3']),
      ],
      minCellVoltage: calculatedMin,
      maxCellVoltage: calculatedMax,
      cellDeltaMv: asInt(json['cellDeltaMv']),
      chargeMos: asBool(json['chargeMos']),
      dischargeMos: asBool(json['dischargeMos']),
      balancing: asBool(json['balancing']),
      protectionFlags: asInt(json['protectionFlags']),
      cells: cells,
    );
  }

  static const empty = BmsBattery(
    id: '',
    order: 0,
    online: false,
    connected: false,
    lastSeen: 0,
    name: 'BMS',
    rssi: 0,
    voltage: 0,
    current: 0,
    power: 0,
    soc: 0,
    remainingAh: 0,
    capacityAh: 0,
    temperatures: [0, 0, 0],
    minCellVoltage: 0,
    maxCellVoltage: 0,
    cellDeltaMv: 0,
    chargeMos: false,
    dischargeMos: false,
    balancing: false,
    protectionFlags: 0,
    cells: [],
  );

  final String id;
  final int order;
  final bool online;
  final bool connected;
  final int lastSeen;
  final String name;
  final int rssi;
  final double voltage;
  final double current;
  final double power;
  final double soc;
  final double remainingAh;
  final double capacityAh;
  final List<double> temperatures;
  final double minCellVoltage;
  final double maxCellVoltage;
  final int cellDeltaMv;
  final bool chargeMos;
  final bool dischargeMos;
  final bool balancing;
  final int protectionFlags;
  final List<CellVoltage> cells;

  @override
  List<Object?> get props => [
    id,
    order,
    online,
    connected,
    lastSeen,
    name,
    rssi,
    voltage,
    current,
    power,
    soc,
    remainingAh,
    capacityAh,
    temperatures,
    minCellVoltage,
    maxCellVoltage,
    cellDeltaMv,
    chargeMos,
    dischargeMos,
    balancing,
    protectionFlags,
    cells,
  ];
}
