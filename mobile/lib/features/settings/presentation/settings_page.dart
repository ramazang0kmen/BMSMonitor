import 'package:flutter/material.dart';
import 'package:package_info_plus/package_info_plus.dart';

import '../../../core/utils/formatters.dart';
import '../../../core/utils/telemetry_state.dart';
import '../../../core/widgets/status_badge.dart';
import '../../auth/data/auth_repository.dart';
import '../../dashboard/data/firebase_bms_repository.dart';
import '../../dashboard/domain/models/gateway_status.dart';

class SettingsPage extends StatelessWidget {
  const SettingsPage({
    required this.repository,
    required this.authRepository,
    super.key,
  });

  final FirebaseBmsRepository repository;
  final AuthRepository authRepository;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Tanılama')),
      body: StreamBuilder<GatewayStatus>(
        stream: repository.watchStatus(),
        builder: (context, snapshot) {
          final status = snapshot.data;
          final now = DateTime.now();
          final online = isGatewayOnline(status, now);
          return ListView(
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 32),
            children: [
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(18),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        children: [
                          Expanded(
                            child: Text(
                              'Gateway',
                              style: Theme.of(context).textTheme.titleLarge
                                  ?.copyWith(fontWeight: FontWeight.w800),
                            ),
                          ),
                          StatusBadge(
                            label: online ? 'ONLINE' : 'OFFLINE',
                            color: online
                                ? Theme.of(context).colorScheme.primary
                                : Theme.of(context).colorScheme.error,
                          ),
                        ],
                      ),
                      const Divider(height: 26),
                      if (snapshot.connectionState == ConnectionState.waiting &&
                          status == null)
                        const LinearProgressIndicator()
                      else ...[
                        _DiagnosticRow(
                          label: 'Son görülme',
                          value: relativeTime(status?.lastSeen ?? 0, now),
                        ),
                        _DiagnosticRow(
                          label: 'Firmware',
                          value: status?.firmwareVersion ?? '-',
                        ),
                        _DiagnosticRow(label: 'IP', value: status?.ip ?? '-'),
                        _DiagnosticRow(
                          label: 'WiFi RSSI',
                          value:
                              '${status?.wifiRssi ?? 0} dBm · ${rssiQuality(status?.wifiRssi ?? 0)}',
                        ),
                        _DiagnosticRow(
                          label: 'Çalışma süresi',
                          value: formatUptime(status?.uptimeSeconds ?? 0),
                        ),
                        _DiagnosticRow(
                          label: 'Boş heap',
                          value: _formatBytes(status?.freeHeap ?? 0),
                        ),
                        _DiagnosticRow(
                          label: 'Minimum heap',
                          value: _formatBytes(status?.minFreeHeap ?? 0),
                        ),
                        _DiagnosticRow(
                          label: 'BMS 1',
                          value: status?.bms1Online == true
                              ? 'Online'
                              : 'Offline',
                        ),
                        _DiagnosticRow(
                          label: 'BMS 2',
                          value: status?.bms2Online == true
                              ? 'Online'
                              : 'Offline',
                        ),
                      ],
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 14),
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(18),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        'Uygulama',
                        style: Theme.of(context).textTheme.titleLarge?.copyWith(
                          fontWeight: FontWeight.w800,
                        ),
                      ),
                      const Divider(height: 26),
                      _DiagnosticRow(
                        label: 'Firebase kullanıcı',
                        value: authRepository.currentUserEmail ?? '-',
                      ),
                      FutureBuilder<PackageInfo>(
                        future: PackageInfo.fromPlatform(),
                        builder: (context, snapshot) => _DiagnosticRow(
                          label: 'Sürüm',
                          value: snapshot.hasData
                              ? 'BMS Monitor v${snapshot.data!.version}'
                              : 'BMS Monitor',
                        ),
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(height: 20),
              OutlinedButton.icon(
                onPressed: authRepository.signOut,
                icon: const Icon(Icons.logout_rounded),
                label: const Text('Çıkış Yap'),
              ),
            ],
          );
        },
      ),
    );
  }
}

class _DiagnosticRow extends StatelessWidget {
  const _DiagnosticRow({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 7),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Expanded(child: Text(label)),
          const SizedBox(width: 16),
          Flexible(
            child: Text(
              value,
              textAlign: TextAlign.end,
              style: const TextStyle(fontWeight: FontWeight.w700),
            ),
          ),
        ],
      ),
    );
  }
}

String _formatBytes(int bytes) => '${(bytes / 1024).toStringAsFixed(1)} KB';
