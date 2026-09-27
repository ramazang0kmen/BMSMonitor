import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../core/constants/app_constants.dart';
import '../features/auth/data/auth_repository.dart';
import '../features/dashboard/data/firebase_bms_repository.dart';
import '../features/event_log/data/event_log_repository.dart';
import 'router.dart';
import 'theme.dart';

class BmsMonitorApp extends StatefulWidget {
  const BmsMonitorApp({
    required this.authRepository,
    required this.bmsRepository,
    required this.eventLogRepository,
    super.key,
  });

  final AuthRepository authRepository;
  final FirebaseBmsRepository bmsRepository;
  final EventLogRepository eventLogRepository;

  @override
  State<BmsMonitorApp> createState() => _BmsMonitorAppState();
}

class _BmsMonitorAppState extends State<BmsMonitorApp> {
  late final AuthStateNotifier _authState;
  late final GoRouter _router;

  @override
  void initState() {
    super.initState();
    _authState = AuthStateNotifier(widget.authRepository);
    _router = createRouter(
      authRepository: widget.authRepository,
      bmsRepository: widget.bmsRepository,
      eventLogRepository: widget.eventLogRepository,
      authState: _authState,
    );
  }

  @override
  void dispose() {
    _router.dispose();
    _authState.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp.router(
      title: AppConstants.appName,
      debugShowCheckedModeBanner: false,
      theme: AppTheme.light(),
      darkTheme: AppTheme.dark(),
      themeMode: ThemeMode.system,
      routerConfig: _router,
    );
  }
}
