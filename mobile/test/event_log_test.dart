import 'dart:async';

import 'package:bmsmonitor/features/event_log/data/event_log_repository.dart';
import 'package:bmsmonitor/features/event_log/domain/models/event_log_entry.dart';
import 'package:bmsmonitor/features/event_log/presentation/event_log_page.dart';
import 'package:bmsmonitor/features/event_log/presentation/event_log_texts.dart';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';

class _MockEventLogRepository extends Mock implements EventLogRepository {}

Map<String, Object?> _bms({
  required int soc,
  required double voltage,
  double current = -4.0,
  bool online = true,
}) => {
  'online': online,
  'soc': soc,
  'voltage': voltage,
  'current': current,
  'power': voltage * current,
  'remainingAh': soc.toDouble(),
  'minCellVoltage': 3.195,
  'maxCellVoltage': 3.21,
  'temperature': 24.5,
  'chargeMos': true,
  'dischargeMos': true,
  'protectionFlags': 0,
};

/// Shape written by the ESP32 firmware (FirebaseService::startLogWrite).
Map<String, Object?> _avgLowDoc() => {
  'type': 'avg_soc_low',
  'severity': 'warning',
  'createdAt': Timestamp.fromMillisecondsSinceEpoch(1790540901000),
  'bmsIndex': 0,
  'detail': 0,
  'firmwareVersion': '0.5.0-eventlog',
  'uptimeSeconds': 3605,
  'system': {
    'voltage': 25.61,
    'current': -8.02,
    'power': -205.4,
    'soc': 19.8,
    'remainingAh': 39.6,
    'socDelta': 1,
  },
  'bms1': _bms(soc: 19, voltage: 25.58),
  'bms2': _bms(soc: 20, voltage: 25.64),
};

void main() {
  group('EventLogEntry.fromJson', () {
    test('parses a firmware document with both packs', () {
      final entry = EventLogEntry.fromJson('1790540901-avg_soc_low-0', {
        ..._avgLowDoc(),
      });

      expect(entry.type, 'avg_soc_low');
      expect(entry.severity, EventSeverity.warning);
      expect(entry.category, EventCategory.battery);
      expect(
        entry.createdAt,
        DateTime.fromMillisecondsSinceEpoch(1790540901000),
      );
      expect(entry.system.voltage, 25.61);
      expect(entry.system.soc, 19.8);
      expect(entry.bms1.soc, 19);
      expect(entry.bms2.voltage, 25.64);
      expect(entry.bms1.cellDeltaMv, 15);
      expect(entry.before, isNull);
    });

    test('Firestore integers arrive as int and still parse as double', () {
      final entry = EventLogEntry.fromJson('x', {
        'type': 'boot',
        'severity': 'error',
        'detail': 6,
        'system': {'voltage': 26, 'soc': 50},
        'bms1': {'soc': 50, 'voltage': 26},
      });

      expect(entry.system.voltage, 26.0);
      expect(entry.bms1.voltage, 26.0);
      expect(entry.severity, EventSeverity.error);
      expect(entry.category, EventCategory.system);
      expect(entry.createdAt, isNull);
      expect(entry.bms2.online, isFalse);
    });

    test('discharge_stopped keeps the under-load reading', () {
      final entry = EventLogEntry.fromJson('x', {
        ..._avgLowDoc(),
        'type': 'discharge_stopped',
        'system': {'voltage': 25.95, 'current': 0.05},
        'before': {
          'voltage': 25.15,
          'current': -8,
          'power': -201.2,
          'soc': 12.5,
          'bms1Voltage': 25.1,
          'bms2Voltage': 25.2,
        },
      });

      expect(entry.category, EventCategory.load);
      expect(entry.before?.voltage, 25.15);
      expect(entry.before?.current, -8.0);
      expect(entry.before?.bms2Voltage, 25.2);
      expect(eventDescription(entry), 'İnverter veya BMS yükü kesti');
    });

    test('unknown severity and missing maps are null-safe', () {
      final entry = EventLogEntry.fromJson('x', null);
      expect(entry.type, 'unknown');
      expect(entry.severity, EventSeverity.info);
      expect(entry.system.voltage, 0);
    });
  });

  group('texts', () {
    test('per-BMS titles name the pack', () {
      final entry = EventLogEntry.fromJson('x', {
        ..._avgLowDoc(),
        'type': 'bms_soc_low',
        'bmsIndex': 2,
      });
      expect(eventTitle(entry), "BMS 2 %20'ye düştü");
    });

    test('JBD protection bits are decoded', () {
      expect(protectionFlagLabels(0x0001), ['Hücre aşırı gerilim']);
      expect(protectionFlagLabels(0x0402), [
        'Hücre düşük gerilim',
        'Kısa devre',
      ]);
      expect(protectionFlagLabels(0x8000), ['0x8000']);
    });

    test('reset reasons', () {
      expect(resetReasonLabel(1), 'Güç verildi');
      expect(resetReasonLabel(9), contains('brownout'));
    });

    test('solar takeover is described as charging', () {
      final entry = EventLogEntry.fromJson('x', {
        'type': 'discharge_stopped',
        'system': {'current': 6.2},
      });
      expect(eventDescription(entry), 'Aküler şarja geçti (6.2 A)');
    });
  });

  group('EventLogPage', () {
    late _MockEventLogRepository repository;
    late StreamController<List<EventLogEntry>> controller;

    setUp(() {
      repository = _MockEventLogRepository();
      controller = StreamController<List<EventLogEntry>>();
      when(
        () => repository.watchLogs(limit: any(named: 'limit')),
      ).thenAnswer((_) => controller.stream);
      when(() => repository.watchLogs()).thenAnswer((_) => controller.stream);
    });

    tearDown(() => controller.close());

    Future<void> pump(WidgetTester tester) => tester.pumpWidget(
      MaterialApp(home: EventLogPage(repository: repository)),
    );

    testWidgets('shows both packs and the average for an entry', (
      tester,
    ) async {
      await pump(tester);
      expect(find.byType(CircularProgressIndicator), findsOneWidget);

      controller.add([EventLogEntry.fromJson('a', _avgLowDoc())]);
      await tester.pump();

      expect(find.text("Ortalama SOC %20'ye düştü"), findsOneWidget);
      expect(find.text('%19.8'), findsOneWidget);
      expect(find.text('%19'), findsOneWidget);
      expect(find.text('%20'), findsOneWidget);
      expect(find.text('25.61 V'), findsOneWidget);
      expect(find.text('25.58 V'), findsOneWidget);
      expect(find.text('25.64 V'), findsOneWidget);
      expect(find.text('UYARI'), findsOneWidget);

      await tester.tap(find.text("Ortalama SOC %20'ye düştü"));
      await tester.pumpAndSettle();
      expect(find.text('Hücre min / maks'), findsNWidgets(2));
      expect(find.text('0.5.0-eventlog'), findsOneWidget);
    });

    testWidgets('filters by category', (tester) async {
      await pump(tester);
      controller.add([
        EventLogEntry.fromJson('a', _avgLowDoc()),
        EventLogEntry.fromJson('b', {
          ..._avgLowDoc(),
          'type': 'bms_offline',
          'bmsIndex': 1,
        }),
      ]);
      await tester.pump();
      expect(find.byType(EventLogCard), findsNWidgets(2));

      await tester.tap(find.widgetWithText(ChoiceChip, 'BMS'));
      await tester.pump();
      expect(find.byType(EventLogCard), findsOneWidget);
      expect(find.text('BMS 1 bağlantısı koptu'), findsOneWidget);
    });

    testWidgets('empty and error states', (tester) async {
      await pump(tester);
      controller.add(const []);
      await tester.pump();
      expect(find.text('Kayıt yok'), findsOneWidget);

      controller.addError('permission-denied');
      await tester.pump();
      expect(find.text('Kayıtlar alınamadı'), findsOneWidget);
    });
  });
}
