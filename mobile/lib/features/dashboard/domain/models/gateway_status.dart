import 'package:equatable/equatable.dart';

import '../../../../core/utils/json_value.dart';

class GatewayStatus extends Equatable {
  const GatewayStatus({
    required this.online,
    required this.lastSeen,
    required this.firmwareVersion,
    required this.wifiRssi,
    required this.ip,
    required this.uptimeSeconds,
    required this.freeHeap,
    required this.minFreeHeap,
    required this.bms1Online,
    required this.bms2Online,
  });

  factory GatewayStatus.fromJson(Object? value) {
    final json = asStringMap(value);
    return GatewayStatus(
      online: asBool(json['online']),
      lastSeen: asInt(json['lastSeen']),
      firmwareVersion: asText(json['firmwareVersion'], '-'),
      wifiRssi: asInt(json['wifiRssi']),
      ip: asText(json['ip'], '-'),
      uptimeSeconds: asInt(json['uptimeSeconds']),
      freeHeap: asInt(json['freeHeap']),
      minFreeHeap: asInt(json['minFreeHeap']),
      bms1Online: asBool(json['bms1Online']),
      bms2Online: asBool(json['bms2Online']),
    );
  }

  static const empty = GatewayStatus(
    online: false,
    lastSeen: 0,
    firmwareVersion: '-',
    wifiRssi: 0,
    ip: '-',
    uptimeSeconds: 0,
    freeHeap: 0,
    minFreeHeap: 0,
    bms1Online: false,
    bms2Online: false,
  );

  final bool online;
  final int lastSeen;
  final String firmwareVersion;
  final int wifiRssi;
  final String ip;
  final int uptimeSeconds;
  final int freeHeap;
  final int minFreeHeap;
  final bool bms1Online;
  final bool bms2Online;

  @override
  List<Object?> get props => [
    online,
    lastSeen,
    firmwareVersion,
    wifiRssi,
    ip,
    uptimeSeconds,
    freeHeap,
    minFreeHeap,
    bms1Online,
    bms2Online,
  ];
}
