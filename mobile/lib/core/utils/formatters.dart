import '../constants/app_constants.dart';

double displayCurrent(double value) =>
    AppConstants.invertCurrentSign ? -value : value;

double displayPower(double value) =>
    AppConstants.invertCurrentSign ? -value : value;

String powerDirection(double current) {
  final adjusted = displayCurrent(current);
  if (adjusted > 0.2) return 'Şarj oluyor';
  if (adjusted < -0.2) return 'Tüketim';
  return 'Beklemede';
}

String relativeTime(int epochMs, DateTime now) {
  if (epochMs <= 0) return 'Henüz yok';
  final seconds = ((now.millisecondsSinceEpoch - epochMs) / 1000).floor().clamp(
    0,
    1 << 30,
  );
  if (seconds < 2) return 'Az önce';
  if (seconds < 60) return '$seconds sn önce';
  final minutes = seconds ~/ 60;
  if (minutes < 60) return '$minutes dk önce';
  final hours = minutes ~/ 60;
  if (hours < 24) return '$hours sa önce';
  return '${hours ~/ 24} gün önce';
}

String formatUptime(int seconds) {
  final days = seconds ~/ 86400;
  final hours = (seconds % 86400) ~/ 3600;
  final minutes = (seconds % 3600) ~/ 60;
  if (days > 0) return '$days gün $hours saat';
  if (hours > 0) return '$hours saat $minutes dk';
  return '$minutes dk ${seconds % 60} sn';
}

String rssiQuality(int rssi) {
  if (rssi > -50) return 'Çok iyi';
  if (rssi >= -65) return 'İyi';
  if (rssi >= -75) return 'Orta';
  return 'Zayıf';
}

String cellDeltaLabel(int deltaMv) {
  if (deltaMv <= 10) return 'İyi';
  if (deltaMv <= 30) return 'Normal';
  if (deltaMv <= 80) return 'Dikkat';
  return 'Yüksek fark';
}

String protectionLabel(int flags) => flags == 0
    ? 'Koruma hatası yok'
    : 'Protection: 0x${flags.toRadixString(16).padLeft(4, '0').toUpperCase()}';
