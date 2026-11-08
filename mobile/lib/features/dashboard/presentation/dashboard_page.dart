import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:go_router/go_router.dart';

import '../../../core/constants/app_strings.dart';
import '../../../core/utils/formatters.dart';
import '../../../core/utils/telemetry_state.dart';
import '../../../core/widgets/live_view_lifecycle.dart';
import '../../../core/widgets/metric_tile.dart';
import '../../../core/widgets/status_badge.dart';
import '../data/firebase_bms_repository.dart';
import '../domain/models/bms_battery.dart';
import '../domain/models/system_battery.dart';
import 'dashboard_cubit.dart';

class DashboardPage extends StatelessWidget {
  const DashboardPage({required this.repository, this.connectivity, super.key});

  final FirebaseBmsRepository repository;
  final Connectivity? connectivity;

  @override
  Widget build(BuildContext context) {
    return BlocProvider(
      create: (_) => DashboardCubit(repository, connectivity: connectivity),
      child: Builder(
        builder: (context) => LiveViewLifecycle(
          onActiveChanged: context.read<DashboardCubit>().setLiveViewActive,
          onSessionRequested: context
              .read<DashboardCubit>()
              .requestRemoteSession,
          child: const _DashboardView(),
        ),
      ),
    );
  }
}

class _DashboardView extends StatelessWidget {
  const _DashboardView();

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('BMS Monitor'),
        actions: [
          BlocBuilder<DashboardCubit, DashboardState>(
            buildWhen: (previous, current) =>
                previous.sessionRequestInProgress !=
                current.sessionRequestInProgress,
            builder: (context, state) => IconButton(
              tooltip: '1 dk canlı izle',
              onPressed: state.sessionRequestInProgress
                  ? null
                  : context.read<DashboardCubit>().requestRemoteSession,
              icon: state.sessionRequestInProgress
                  ? const SizedBox.square(
                      dimension: 20,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    )
                  : const Icon(Icons.refresh_rounded),
            ),
          ),
          IconButton(
            tooltip: 'Olay kayıtları',
            onPressed: () => context.push('/logs'),
            icon: const Icon(Icons.history_rounded),
          ),
          IconButton(
            tooltip: 'Tanılama ve ayarlar',
            onPressed: () => context.push('/settings'),
            icon: const Icon(Icons.monitor_heart_outlined),
          ),
        ],
      ),
      body: BlocBuilder<DashboardCubit, DashboardState>(
        builder: (context, state) {
          if (state.waitingForStatus) {
            return const Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  CircularProgressIndicator(),
                  SizedBox(height: 16),
                  Text('Gateway durumu alınıyor...'),
                ],
              ),
            );
          }
          final now = state.now ?? DateTime.now();
          final gatewayOnline = isGatewayOnline(state.status, now);
          final freshness = liveFreshness(state.status, state.telemetry, now);
          final telemetry = state.telemetry;
          return ListView(
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 32),
            children: [
              _GatewayHeader(
                online: gatewayOnline,
                freshness: freshness,
                wifiRssi: state.status?.wifiRssi ?? 0,
                sessionSecondsRemaining: state.remoteSessionRemaining.inSeconds,
              ),
              if (!state.internetAvailable)
                const _MessageBanner(
                  icon: Icons.wifi_off_rounded,
                  title: AppStrings.noInternet,
                  message: AppStrings.showingLastData,
                ),
              if (state.errorMessage != null)
                _MessageBanner(
                  icon: Icons.cloud_off_outlined,
                  title: 'Bağlantı uyarısı',
                  message: state.errorMessage!,
                ),
              const SizedBox(height: 14),
              if (telemetry == null) ...[
                _MessageBanner(
                  icon: Icons.hourglass_top_rounded,
                  title: gatewayOnline ? 'Gateway bağlı' : 'Gateway çevrimdışı',
                  message: AppStrings.waitingForLive,
                ),
              ] else ...[
                Opacity(
                  opacity:
                      freshness == LiveFreshness.stale ||
                          freshness == LiveFreshness.offline
                      ? 0.62
                      : 1,
                  child: _SystemBatteryCard(system: telemetry.system),
                ),
                if (telemetry.system.socMismatch) ...[
                  const SizedBox(height: 14),
                  _SocMismatchCard(
                    system: telemetry.system,
                    batteries: telemetry.batteries,
                  ),
                ],
                if (telemetry.batteries.isEmpty)
                  const _MessageBanner(
                    icon: Icons.bluetooth_searching_rounded,
                    title: 'BMS bulunamadı',
                    message:
                        'Gateway Bluetooth taramasının tamamlanması bekleniyor.',
                  )
                else
                  for (
                    var index = 0;
                    index < telemetry.batteries.length;
                    index++
                  ) ...[
                    const SizedBox(height: 12),
                    _BmsSummaryCard(
                      number: index + 1,
                      battery: telemetry.batteries[index],
                      gatewayOnline: gatewayOnline,
                      now: now,
                      onTap: () =>
                          context.push('/bms/${telemetry.batteries[index].id}'),
                    ),
                  ],
              ],
            ],
          );
        },
      ),
    );
  }
}

class _GatewayHeader extends StatelessWidget {
  const _GatewayHeader({
    required this.online,
    required this.freshness,
    required this.wifiRssi,
    required this.sessionSecondsRemaining,
  });

  final bool online;
  final LiveFreshness freshness;
  final int wifiRssi;
  final int sessionSecondsRemaining;

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    final liveColor = switch (freshness) {
      LiveFreshness.live => colors.primary,
      LiveFreshness.delayed => colors.tertiary,
      LiveFreshness.waiting => colors.secondary,
      _ => colors.error,
    };
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: [
            Icon(
              online ? Icons.router_outlined : Icons.cloud_off_outlined,
              color: online ? colors.primary : colors.error,
            ),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    online ? 'Gateway Online' : 'Gateway Offline',
                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                  Text(
                    'WiFi $wifiRssi dBm · ${rssiQuality(wifiRssi)}',
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                  if (sessionSecondsRemaining > 0)
                    Text(
                      'Canlı yayın · $sessionSecondsRemaining sn kaldı',
                      style: Theme.of(
                        context,
                      ).textTheme.bodySmall?.copyWith(color: colors.primary),
                    ),
                ],
              ),
            ),
            StatusBadge(label: freshnessLabel(freshness), color: liveColor),
          ],
        ),
      ),
    );
  }
}

class _SystemBatteryCard extends StatelessWidget {
  const _SystemBatteryCard({required this.system});

  final SystemBattery system;

  @override
  Widget build(BuildContext context) {
    final soc = system.soc.clamp(0, 100) / 100;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          children: [
            Text(
              'TOPLAM BATARYA',
              style: Theme.of(context).textTheme.labelLarge?.copyWith(
                letterSpacing: 1.2,
                fontWeight: FontWeight.w700,
              ),
            ),
            const SizedBox(height: 12),
            Text(
              '%${system.soc.toStringAsFixed(1)}',
              style: Theme.of(context).textTheme.displayMedium?.copyWith(
                fontWeight: FontWeight.w800,
                color: Theme.of(context).colorScheme.primary,
              ),
            ),
            const SizedBox(height: 8),
            ClipRRect(
              borderRadius: BorderRadius.circular(99),
              child: LinearProgressIndicator(value: soc, minHeight: 10),
            ),
            const SizedBox(height: 22),
            Row(
              children: [
                Expanded(
                  child: MetricTile(
                    label: 'Voltaj',
                    value: '${system.voltage.toStringAsFixed(2)} V',
                  ),
                ),
                Expanded(
                  child: MetricTile(
                    label: 'Akım',
                    value:
                        '${displayCurrent(system.current).toStringAsFixed(2)} A',
                  ),
                ),
                Expanded(
                  child: MetricTile(
                    label: 'Güç',
                    value: '${displayPower(system.power).toStringAsFixed(0)} W',
                  ),
                ),
              ],
            ),
            const SizedBox(height: 18),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  '${system.remainingAh.toStringAsFixed(2)} / '
                  '${system.totalCapacityAh.toStringAsFixed(0)} Ah',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w700,
                  ),
                ),
                StatusBadge(
                  label: powerDirection(system.current),
                  color: Theme.of(context).colorScheme.secondary,
                  icon: Icons.electric_bolt_rounded,
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _SocMismatchCard extends StatelessWidget {
  const _SocMismatchCard({required this.system, required this.batteries});

  final SystemBattery system;
  final List<BmsBattery> batteries;

  @override
  Widget build(BuildContext context) {
    final color = Theme.of(context).colorScheme.tertiary;
    final onlineBatteries =
        batteries.where((battery) => battery.online).toList()
          ..sort((a, b) => a.soc.compareTo(b.soc));
    final range = onlineBatteries.length < 2
        ? 'Fark: %${system.socDelta.toStringAsFixed(0)}'
        : '${onlineBatteries.first.name}: %${onlineBatteries.first.soc.toStringAsFixed(0)} · '
              '${onlineBatteries.last.name}: %${onlineBatteries.last.soc.toStringAsFixed(0)} · '
              'Fark: %${system.socDelta.toStringAsFixed(0)}';
    return Card(
      color: color.withValues(alpha: 0.09),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Icon(Icons.balance_rounded, color: color),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'BMS SOC farkı yüksek',
                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                  const SizedBox(height: 5),
                  Text(range),
                  const SizedBox(height: 5),
                  const Text('Paketlerin SOC kalibrasyonunu kontrol edin.'),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _BmsSummaryCard extends StatelessWidget {
  const _BmsSummaryCard({
    required this.number,
    required this.battery,
    required this.gatewayOnline,
    required this.now,
    required this.onTap,
  });

  final int number;
  final BmsBattery battery;
  final bool gatewayOnline;
  final DateTime now;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    final online = gatewayOnline && battery.online;
    final waitingForTurn =
        gatewayOnline && battery.online && !battery.connected;
    final colors = Theme.of(context).colorScheme;
    return Opacity(
      opacity: online ? 1 : 0.58,
      child: Card(
        clipBehavior: Clip.antiAlias,
        child: InkWell(
          onTap: onTap,
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              children: [
                Row(
                  children: [
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            'BMS $number',
                            style: Theme.of(context).textTheme.titleLarge
                                ?.copyWith(fontWeight: FontWeight.w800),
                          ),
                          Text(battery.name),
                        ],
                      ),
                    ),
                    StatusBadge(
                      label: waitingForTurn
                          ? 'SIRASINI BEKLİYOR'
                          : online
                          ? 'ONLINE'
                          : 'OFFLINE · SON VERİ',
                      color: waitingForTurn
                          ? colors.tertiary
                          : online
                          ? colors.primary
                          : colors.error,
                    ),
                    const SizedBox(width: 6),
                    const Icon(Icons.chevron_right),
                  ],
                ),
                const Divider(height: 24),
                Row(
                  children: [
                    Expanded(
                      child: MetricTile(
                        label: 'SOC',
                        value: '%${battery.soc.toStringAsFixed(0)}',
                      ),
                    ),
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
                  ],
                ),
                if (battery.lastSeen > 0) ...[
                  const SizedBox(height: 8),
                  Align(
                    alignment: Alignment.centerLeft,
                    child: Text(
                      'Son veri: ${relativeTime(battery.lastSeen, now)}',
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
                const SizedBox(height: 14),
                Row(
                  children: [
                    Expanded(child: Text('Delta ${battery.cellDeltaMv} mV')),
                    Expanded(
                      child: Text(
                        '${battery.temperatures.firstOrNull?.toStringAsFixed(1) ?? '-'}°C',
                      ),
                    ),
                    Text('RSSI ${battery.rssi} dBm'),
                  ],
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class _MessageBanner extends StatelessWidget {
  const _MessageBanner({
    required this.icon,
    required this.title,
    required this.message,
  });

  final IconData icon;
  final String title;
  final String message;

  @override
  Widget build(BuildContext context) {
    final color = Theme.of(context).colorScheme.error;
    return Padding(
      padding: const EdgeInsets.only(top: 12),
      child: Card(
        color: color.withValues(alpha: 0.07),
        child: ListTile(
          leading: Icon(icon, color: color),
          title: Text(title),
          subtitle: Text(message),
        ),
      ),
    );
  }
}
