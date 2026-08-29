import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../core/utils/formatters.dart';
import '../../../core/utils/telemetry_state.dart';
import '../../../core/widgets/live_view_lifecycle.dart';
import '../../../core/widgets/metric_tile.dart';
import '../../../core/widgets/status_badge.dart';
import '../../dashboard/data/firebase_bms_repository.dart';
import '../../dashboard/domain/models/bms_battery.dart';
import '../../dashboard/presentation/dashboard_cubit.dart';

class BmsDetailPage extends StatelessWidget {
  const BmsDetailPage({
    required this.repository,
    required this.bmsNumber,
    super.key,
  });

  final FirebaseBmsRepository repository;
  final int bmsNumber;

  @override
  Widget build(BuildContext context) {
    return BlocProvider(
      create: (_) => DashboardCubit(repository),
      child: Builder(
        builder: (context) => LiveViewLifecycle(
          onActiveChanged: context.read<DashboardCubit>().setLiveViewActive,
          onSessionRequested: context
              .read<DashboardCubit>()
              .requestRemoteSession,
          requestOnInitialActive: false,
          child: _BmsDetailView(bmsNumber: bmsNumber),
        ),
      ),
    );
  }
}

class _BmsDetailView extends StatelessWidget {
  const _BmsDetailView({required this.bmsNumber});

  final int bmsNumber;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text('BMS $bmsNumber Detay')),
      body: BlocBuilder<DashboardCubit, DashboardState>(
        builder: (context, state) {
          final telemetry = state.telemetry;
          if (telemetry == null) {
            return const Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  CircularProgressIndicator(),
                  SizedBox(height: 16),
                  Text('Canlı BMS verisi bekleniyor...'),
                ],
              ),
            );
          }
          final bms = bmsNumber == 1 ? telemetry.bms1 : telemetry.bms2;
          final gatewayOnline = isGatewayOnline(
            state.status,
            state.now ?? DateTime.now(),
          );
          final online = gatewayOnline && bms.online;
          return ListView(
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 32),
            children: [
              _DetailHeader(number: bmsNumber, battery: bms, online: online),
              const SizedBox(height: 14),
              Opacity(
                opacity: online ? 1 : 0.58,
                child: _SummaryCard(battery: bms),
              ),
              const SizedBox(height: 14),
              _CellsCard(battery: bms),
              const SizedBox(height: 14),
              _TemperatureCard(battery: bms),
              const SizedBox(height: 14),
              _SwitchesCard(battery: bms),
            ],
          );
        },
      ),
    );
  }
}

class _DetailHeader extends StatelessWidget {
  const _DetailHeader({
    required this.number,
    required this.battery,
    required this.online,
  });

  final int number;
  final BmsBattery battery;
  final bool online;

  @override
  Widget build(BuildContext context) {
    final color = online
        ? Theme.of(context).colorScheme.primary
        : Theme.of(context).colorScheme.error;
    return Row(
      children: [
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                'BMS $number',
                style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                  fontWeight: FontWeight.w800,
                ),
              ),
              Text(battery.name),
            ],
          ),
        ),
        StatusBadge(
          label: online ? 'ONLINE' : 'OFFLINE · SON BİLİNEN',
          color: color,
        ),
      ],
    );
  }
}

class _SummaryCard extends StatelessWidget {
  const _SummaryCard({required this.battery});

  final BmsBattery battery;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          children: [
            Row(
              children: [
                Expanded(
                  child: Text(
                    '%${battery.soc.toStringAsFixed(0)}',
                    style: Theme.of(context).textTheme.displaySmall?.copyWith(
                      color: Theme.of(context).colorScheme.primary,
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ),
                StatusBadge(
                  label: powerDirection(battery.current),
                  color: Theme.of(context).colorScheme.secondary,
                ),
              ],
            ),
            const Divider(height: 28),
            Row(
              children: [
                Expanded(
                  child: MetricTile(
                    label: 'Voltaj',
                    value: '${battery.voltage.toStringAsFixed(2)} V',
                  ),
                ),
                Expanded(
                  child: MetricTile(
                    label: 'Akım',
                    value:
                        '${displayCurrent(battery.current).toStringAsFixed(2)} A',
                  ),
                ),
                Expanded(
                  child: MetricTile(
                    label: 'Güç',
                    value:
                        '${displayPower(battery.power).toStringAsFixed(0)} W',
                  ),
                ),
              ],
            ),
            const SizedBox(height: 16),
            Align(
              alignment: Alignment.centerLeft,
              child: Text(
                '${battery.remainingAh.toStringAsFixed(2)} / '
                '${battery.capacityAh.toStringAsFixed(0)} Ah',
                style: Theme.of(
                  context,
                ).textTheme.titleMedium?.copyWith(fontWeight: FontWeight.w700),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _CellsCard extends StatelessWidget {
  const _CellsCard({required this.battery});

  final BmsBattery battery;

  @override
  Widget build(BuildContext context) {
    final cells = battery.cells;
    final lower = battery.minCellVoltage - 0.02;
    final upper = battery.maxCellVoltage + 0.02;
    final span = (upper - lower).abs() < 0.001 ? 0.04 : upper - lower;
    final deltaColor = battery.cellDeltaMv > 80
        ? Theme.of(context).colorScheme.error
        : battery.cellDeltaMv > 30
        ? Theme.of(context).colorScheme.tertiary
        : Theme.of(context).colorScheme.primary;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Expanded(
                  child: Text(
                    'Hücreler',
                    style: Theme.of(context).textTheme.titleLarge?.copyWith(
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                ),
                Column(
                  crossAxisAlignment: CrossAxisAlignment.end,
                  children: [
                    Text(
                      '${battery.cellDeltaMv} mV',
                      style: Theme.of(context).textTheme.titleLarge?.copyWith(
                        color: deltaColor,
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                    Text(cellDeltaLabel(battery.cellDeltaMv)),
                  ],
                ),
              ],
            ),
            const SizedBox(height: 16),
            if (cells.isEmpty)
              const Text('Hücre verisi bulunmuyor.')
            else
              for (final cell in cells)
                Padding(
                  padding: const EdgeInsets.only(bottom: 12),
                  child: Row(
                    children: [
                      SizedBox(
                        width: 58,
                        child: Text('Cell ${cell.index + 1}'),
                      ),
                      Expanded(
                        child: ClipRRect(
                          borderRadius: BorderRadius.circular(99),
                          child: LinearProgressIndicator(
                            value: ((cell.voltage - lower) / span).clamp(0, 1),
                            minHeight: 8,
                          ),
                        ),
                      ),
                      const SizedBox(width: 10),
                      SizedBox(
                        width: 64,
                        child: Text(
                          '${cell.voltage.toStringAsFixed(3)} V',
                          textAlign: TextAlign.end,
                        ),
                      ),
                      SizedBox(
                        width: 42,
                        child: Text(
                          cell.isMinimum
                              ? ' MIN'
                              : cell.isMaximum
                              ? ' MAX'
                              : '',
                          style: Theme.of(context).textTheme.labelSmall
                              ?.copyWith(
                                fontWeight: FontWeight.w800,
                                color: cell.isMinimum
                                    ? Theme.of(context).colorScheme.tertiary
                                    : Theme.of(context).colorScheme.primary,
                              ),
                        ),
                      ),
                    ],
                  ),
                ),
            const Divider(),
            Text(
              '0–10 mV iyi · 10–30 mV normal · '
              '30–80 mV dikkat · 80+ mV yüksek fark',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ],
        ),
      ),
    );
  }
}

class _TemperatureCard extends StatelessWidget {
  const _TemperatureCard({required this.battery});

  final BmsBattery battery;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Sıcaklıklar',
              style: Theme.of(
                context,
              ).textTheme.titleLarge?.copyWith(fontWeight: FontWeight.w800),
            ),
            const SizedBox(height: 14),
            Row(
              children: [
                for (var index = 0; index < 3; index++)
                  Expanded(
                    child: MetricTile(
                      label: 'Temp ${index + 1}',
                      value:
                          '${battery.temperatures[index].toStringAsFixed(1)}°C',
                      icon: Icons.thermostat_rounded,
                    ),
                  ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _SwitchesCard extends StatelessWidget {
  const _SwitchesCard({required this.battery});

  final BmsBattery battery;

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(18),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                StatusBadge(
                  label: 'Charge MOS ${battery.chargeMos ? 'ON' : 'OFF'}',
                  color: battery.chargeMos ? colors.primary : colors.outline,
                ),
                StatusBadge(
                  label: 'Discharge MOS ${battery.dischargeMos ? 'ON' : 'OFF'}',
                  color: battery.dischargeMos ? colors.primary : colors.outline,
                ),
                StatusBadge(
                  label: 'Balancing ${battery.balancing ? 'ON' : 'OFF'}',
                  color: battery.balancing ? colors.tertiary : colors.outline,
                ),
              ],
            ),
            const Divider(height: 28),
            ListTile(
              contentPadding: EdgeInsets.zero,
              leading: Icon(
                battery.protectionFlags == 0
                    ? Icons.verified_user_outlined
                    : Icons.warning_amber_rounded,
                color: battery.protectionFlags == 0
                    ? colors.primary
                    : colors.error,
              ),
              title: Text(protectionLabel(battery.protectionFlags)),
              subtitle: Text(
                'RSSI ${battery.rssi} dBm · ${rssiQuality(battery.rssi)}',
              ),
            ),
          ],
        ),
      ),
    );
  }
}
