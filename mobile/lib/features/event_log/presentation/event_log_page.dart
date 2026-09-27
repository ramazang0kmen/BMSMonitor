import 'package:flutter/material.dart';

import '../../../core/utils/formatters.dart';
import '../../../core/widgets/status_badge.dart';
import '../data/event_log_repository.dart';
import '../domain/models/event_log_entry.dart';
import 'event_log_texts.dart';

enum _Filter { all, battery, load, bms, problems }

class EventLogPage extends StatefulWidget {
  const EventLogPage({required this.repository, super.key});

  final EventLogRepository repository;

  @override
  State<EventLogPage> createState() => _EventLogPageState();
}

class _EventLogPageState extends State<EventLogPage> {
  late final Stream<List<EventLogEntry>> _logs = widget.repository.watchLogs();
  _Filter _filter = _Filter.all;

  bool _matches(EventLogEntry entry) => switch (_filter) {
    _Filter.all => true,
    _Filter.battery => entry.category == EventCategory.battery,
    _Filter.load => entry.category == EventCategory.load,
    _Filter.bms => entry.category == EventCategory.bms,
    _Filter.problems => entry.severity != EventSeverity.info,
  };

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Olay Kayıtları')),
      body: Column(
        children: [
          SingleChildScrollView(
            scrollDirection: Axis.horizontal,
            padding: const EdgeInsets.fromLTRB(16, 4, 16, 8),
            child: Row(
              children: [
                for (final (filter, label) in const [
                  (_Filter.all, 'Tümü'),
                  (_Filter.battery, 'Batarya'),
                  (_Filter.load, 'Yük'),
                  (_Filter.bms, 'BMS'),
                  (_Filter.problems, 'Uyarı ve hatalar'),
                ])
                  Padding(
                    padding: const EdgeInsets.only(right: 8),
                    child: ChoiceChip(
                      label: Text(label),
                      selected: _filter == filter,
                      onSelected: (_) => setState(() => _filter = filter),
                    ),
                  ),
              ],
            ),
          ),
          Expanded(
            child: StreamBuilder<List<EventLogEntry>>(
              stream: _logs,
              builder: (context, snapshot) {
                if (snapshot.hasError) {
                  return _CenteredMessage(
                    icon: Icons.cloud_off_outlined,
                    title: 'Kayıtlar alınamadı',
                    message: '${snapshot.error}',
                  );
                }
                if (!snapshot.hasData) {
                  return const Center(child: CircularProgressIndicator());
                }
                final entries = snapshot.data!.where(_matches).toList();
                if (entries.isEmpty) {
                  return const _CenteredMessage(
                    icon: Icons.history_rounded,
                    title: 'Kayıt yok',
                    message:
                        'Gateway SOC eşikleri, yük değişimleri ve BMS '
                        'hatalarında buraya kayıt ekler.',
                  );
                }
                return ListView.separated(
                  padding: const EdgeInsets.fromLTRB(16, 0, 16, 32),
                  itemCount: entries.length,
                  separatorBuilder: (_, _) => const SizedBox(height: 10),
                  itemBuilder: (context, index) =>
                      EventLogCard(entry: entries[index]),
                );
              },
            ),
          ),
        ],
      ),
    );
  }
}

class EventLogCard extends StatefulWidget {
  const EventLogCard({required this.entry, super.key});

  final EventLogEntry entry;

  @override
  State<EventLogCard> createState() => _EventLogCardState();
}

class _EventLogCardState extends State<EventLogCard> {
  bool _expanded = false;

  @override
  Widget build(BuildContext context) {
    final entry = widget.entry;
    final colors = Theme.of(context).colorScheme;
    final (color, icon, label) = switch (entry.severity) {
      EventSeverity.error => (colors.error, Icons.error_outline, 'HATA'),
      EventSeverity.warning => (
        colors.tertiary,
        Icons.warning_amber_rounded,
        'UYARI',
      ),
      EventSeverity.info => (colors.primary, Icons.info_outline, 'BİLGİ'),
    };
    final description = eventDescription(entry);
    final textTheme = Theme.of(context).textTheme;

    return Card(
      clipBehavior: Clip.antiAlias,
      child: InkWell(
        onTap: () => setState(() => _expanded = !_expanded),
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Icon(icon, color: color),
                  const SizedBox(width: 10),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          eventTitle(entry),
                          style: textTheme.titleMedium?.copyWith(
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                        const SizedBox(height: 2),
                        Text(
                          '${formatDateTime(entry.createdAt)} · '
                          '${relativeTime(entry.createdAt?.millisecondsSinceEpoch ?? 0, DateTime.now())}',
                          style: textTheme.bodySmall,
                        ),
                      ],
                    ),
                  ),
                  StatusBadge(label: label, color: color),
                ],
              ),
              if (description != null) ...[
                const SizedBox(height: 8),
                Text(description),
              ],
              const SizedBox(height: 12),
              _ReadingsTable(entry: entry),
              if (entry.before != null) ...[
                const SizedBox(height: 10),
                _BeforeLoadCut(before: entry.before!),
              ],
              if (_expanded) ...[
                const Divider(height: 24),
                _Details(entry: entry),
              ],
              Align(
                alignment: Alignment.centerRight,
                child: Icon(
                  _expanded ? Icons.expand_less : Icons.expand_more,
                  size: 20,
                  color: colors.outline,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

/// SOC / voltage / current for the average and both packs, side by side -
/// the numbers needed for tuning inverter cut-off and restart voltages.
class _ReadingsTable extends StatelessWidget {
  const _ReadingsTable({required this.entry});

  final EventLogEntry entry;

  @override
  Widget build(BuildContext context) {
    final textTheme = Theme.of(context).textTheme;
    final header = textTheme.labelMedium?.copyWith(
      color: Theme.of(context).colorScheme.outline,
    );
    final value = textTheme.bodyMedium?.copyWith(fontWeight: FontWeight.w700);

    String pack(EventBmsSnapshot bms, String Function(EventBmsSnapshot) f) =>
        bms.online ? f(bms) : '-';

    TableRow row(String label, String avg, String bms1, String bms2) =>
        TableRow(
          children: [
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 3),
              child: Text(label, style: header),
            ),
            Text(avg, style: value, textAlign: TextAlign.end),
            Text(bms1, style: value, textAlign: TextAlign.end),
            Text(bms2, style: value, textAlign: TextAlign.end),
          ],
        );

    return Table(
      columnWidths: const {
        0: FlexColumnWidth(1.1),
        1: FlexColumnWidth(),
        2: FlexColumnWidth(),
        3: FlexColumnWidth(),
      },
      defaultVerticalAlignment: TableCellVerticalAlignment.middle,
      children: [
        TableRow(
          children: [
            const SizedBox.shrink(),
            Text('Ortalama', style: header, textAlign: TextAlign.end),
            Text('BMS 1', style: header, textAlign: TextAlign.end),
            Text('BMS 2', style: header, textAlign: TextAlign.end),
          ],
        ),
        row(
          'SOC',
          '%${entry.system.soc.toStringAsFixed(1)}',
          pack(entry.bms1, (b) => '%${b.soc}'),
          pack(entry.bms2, (b) => '%${b.soc}'),
        ),
        row(
          'Voltaj',
          '${entry.system.voltage.toStringAsFixed(2)} V',
          pack(entry.bms1, (b) => '${b.voltage.toStringAsFixed(2)} V'),
          pack(entry.bms2, (b) => '${b.voltage.toStringAsFixed(2)} V'),
        ),
        row(
          'Akım',
          '${displayCurrent(entry.system.current).toStringAsFixed(1)} A',
          pack(
            entry.bms1,
            (b) => '${displayCurrent(b.current).toStringAsFixed(1)} A',
          ),
          pack(
            entry.bms2,
            (b) => '${displayCurrent(b.current).toStringAsFixed(1)} A',
          ),
        ),
      ],
    );
  }
}

class _BeforeLoadCut extends StatelessWidget {
  const _BeforeLoadCut({required this.before});

  final EventBeforeSnapshot before;

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: colors.tertiary.withValues(alpha: 0.08),
        borderRadius: BorderRadius.circular(12),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'Yük kesilmeden hemen önce',
            style: Theme.of(
              context,
            ).textTheme.labelMedium?.copyWith(fontWeight: FontWeight.w700),
          ),
          const SizedBox(height: 4),
          Text(
            'Ortalama ${before.voltage.toStringAsFixed(2)} V · '
            '${displayCurrent(before.current).toStringAsFixed(1)} A · '
            '${displayPower(before.power).toStringAsFixed(0)} W · '
            '%${before.soc.toStringAsFixed(1)}',
          ),
          Text(
            'BMS 1 ${before.bms1Voltage.toStringAsFixed(2)} V · '
            'BMS 2 ${before.bms2Voltage.toStringAsFixed(2)} V',
          ),
        ],
      ),
    );
  }
}

class _Details extends StatelessWidget {
  const _Details({required this.entry});

  final EventLogEntry entry;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        _DetailRow(
          'Güç',
          '${displayPower(entry.system.power).toStringAsFixed(0)} W',
        ),
        _DetailRow(
          'Kalan kapasite',
          '${entry.system.remainingAh.toStringAsFixed(1)} Ah',
        ),
        for (final (number, bms) in [(1, entry.bms1), (2, entry.bms2)])
          if (bms.online) ...[
            const SizedBox(height: 8),
            Text(
              'BMS $number',
              style: Theme.of(
                context,
              ).textTheme.labelLarge?.copyWith(fontWeight: FontWeight.w800),
            ),
            _DetailRow(
              'Hücre min / maks',
              '${bms.minCellVoltage.toStringAsFixed(3)} / '
                  '${bms.maxCellVoltage.toStringAsFixed(3)} V '
                  '(Δ ${bms.cellDeltaMv} mV)',
            ),
            _DetailRow('Sıcaklık', '${bms.temperature.toStringAsFixed(1)} °C'),
            _DetailRow(
              'Şarj / deşarj MOS',
              '${bms.chargeMos ? 'Açık' : 'Kapalı'} / '
                  '${bms.dischargeMos ? 'Açık' : 'Kapalı'}',
            ),
            _DetailRow(
              'Koruma',
              bms.protectionFlags == 0
                  ? 'Yok'
                  : protectionFlagLabels(bms.protectionFlags).join(', '),
            ),
          ] else ...[
            const SizedBox(height: 8),
            _DetailRow('BMS $number', 'Çevrimdışı'),
          ],
        const SizedBox(height: 8),
        _DetailRow('Firmware', entry.firmwareVersion),
        _DetailRow('Gateway çalışma süresi', formatUptime(entry.uptimeSeconds)),
      ],
    );
  }
}

class _DetailRow extends StatelessWidget {
  const _DetailRow(this.label, this.value);

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Expanded(child: Text(label)),
          const SizedBox(width: 12),
          Flexible(
            child: Text(
              value,
              textAlign: TextAlign.end,
              style: const TextStyle(fontWeight: FontWeight.w600),
            ),
          ),
        ],
      ),
    );
  }
}

class _CenteredMessage extends StatelessWidget {
  const _CenteredMessage({
    required this.icon,
    required this.title,
    required this.message,
  });

  final IconData icon;
  final String title;
  final String message;

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(icon, size: 48, color: Theme.of(context).colorScheme.outline),
            const SizedBox(height: 12),
            Text(title, style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 6),
            Text(message, textAlign: TextAlign.center),
          ],
        ),
      ),
    );
  }
}
