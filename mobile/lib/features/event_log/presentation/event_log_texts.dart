import '../domain/models/event_log_entry.dart';

String eventTitle(EventLogEntry entry) {
  final bms = 'BMS ${entry.bmsIndex}';
  return switch (entry.type) {
    'boot' => 'Gateway yeniden başladı',
    'avg_soc_low' => 'Ortalama SOC %20\'ye düştü',
    'avg_soc_empty' => 'Ortalama SOC %0\'a indi',
    'avg_soc_full' => 'Aküler tam doldu',
    'bms_soc_low' => '$bms %20\'ye düştü',
    'bms_soc_empty' => '$bms %0\'a indi',
    'discharge_stopped' => 'Aküden çekilen yük sıfırlandı',
    'discharge_started' => 'Aküden yük çekilmeye başlandı',
    'bms_discharge_off' => '$bms deşarjı kesti',
    'bms_discharge_on' => '$bms deşarjı yeniden açtı',
    'bms_charge_off' => '$bms şarjı kesti',
    'bms_charge_on' => '$bms şarjı yeniden açtı',
    'bms_protection' => '$bms koruma durumu',
    'bms_protection_cleared' => '$bms koruma durumu temizlendi',
    'bms_offline' => '$bms bağlantısı koptu',
    'bms_online' => '$bms yeniden bağlandı',
    'soc_mismatch' => 'Aküler arası SOC farkı',
    _ => entry.type,
  };
}

/// One extra line of context for types where the numbers alone are not
/// enough; null when there is nothing to add.
String? eventDescription(EventLogEntry entry) {
  return switch (entry.type) {
    'boot' => 'Sebep: ${resetReasonLabel(entry.detail)}',
    'discharge_stopped' =>
      entry.system.current > 0.3
          ? 'Aküler şarja geçti (${entry.system.current.toStringAsFixed(1)} A)'
          : 'İnverter veya BMS yükü kesti',
    'bms_protection' => protectionFlagLabels(entry.detail).join(', '),
    'soc_mismatch' =>
      'Fark: %${entry.system.socDelta.toStringAsFixed(0)} '
          '(BMS 1 %${entry.bms1.soc} · BMS 2 %${entry.bms2.soc})',
    _ => null,
  };
}

/// ESP-IDF esp_reset_reason_t values.
String resetReasonLabel(int reason) => switch (reason) {
  1 => 'Güç verildi',
  2 => 'Reset butonu',
  3 => 'Yazılım yeniden başlatması',
  4 => 'Yazılım çökmesi (panic)',
  5 => 'Watchdog (kesme)',
  6 => 'Watchdog (görev)',
  7 => 'Watchdog',
  8 => 'Uyku modundan uyanma',
  9 => 'Besleme gerilimi düştü (brownout)',
  10 => 'SDIO',
  _ => 'Bilinmiyor ($reason)',
};

/// JBD protection status bits (register 0x03, bytes 20-21).
const _protectionBits = [
  'Hücre aşırı gerilim',
  'Hücre düşük gerilim',
  'Paket aşırı gerilim',
  'Paket düşük gerilim',
  'Şarj aşırı sıcaklık',
  'Şarj düşük sıcaklık',
  'Deşarj aşırı sıcaklık',
  'Deşarj düşük sıcaklık',
  'Şarj aşırı akım',
  'Deşarj aşırı akım',
  'Kısa devre',
  'Ön uç IC hatası',
  'MOS yazılım kilidi',
];

List<String> protectionFlagLabels(int flags) {
  final labels = <String>[
    for (var bit = 0; bit < _protectionBits.length; bit++)
      if (flags & (1 << bit) != 0) _protectionBits[bit],
  ];
  if (labels.isEmpty) {
    return ['0x${flags.toRadixString(16).padLeft(4, '0').toUpperCase()}'];
  }
  return labels;
}

String formatDateTime(DateTime? value) {
  if (value == null) return '-';
  final local = value.toLocal();
  String two(int n) => n.toString().padLeft(2, '0');
  return '${two(local.day)}.${two(local.month)}.${local.year} '
      '${two(local.hour)}:${two(local.minute)}:${two(local.second)}';
}
