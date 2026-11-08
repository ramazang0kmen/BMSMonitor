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
    required this.discoveredBmsCount,
    required this.connectedBmsCount,
    required this.onlineBmsCount,
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
      discoveredBmsCount: asInt(
        json['discoveredBmsCount'],
        asBool(json['bms2Online']) ? 2 : (asBool(json['bms1Online']) ? 1 : 0),
      ),
      connectedBmsCount: asInt(
        json['connectedBmsCount'],
        (asBool(json['bms1Online']) ? 1 : 0) +
            (asBool(json['bms2Online']) ? 1 : 0),
      ),
      onlineBmsCount: asInt(
        json['onlineBmsCount'],
        (asBool(json['bms1Online']) ? 1 : 0) +
            (asBool(json['bms2Online']) ? 1 : 0),
      ),
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
    discoveredBmsCount: 0,
    connectedBmsCount: 0,
    onlineBmsCount: 0,
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
  final int discoveredBmsCount;
  final int connectedBmsCount;
  final int onlineBmsCount;

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
    discoveredBmsCount,
    connectedBmsCount,
    onlineBmsCount,
  ];
}
