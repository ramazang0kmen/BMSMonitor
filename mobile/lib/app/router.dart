import 'dart:async';

import 'package:firebase_auth/firebase_auth.dart';
import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';

import '../core/services/app_route_observer.dart';
import '../features/auth/data/auth_repository.dart';
import '../features/auth/presentation/login_page.dart';
import '../features/bms_detail/presentation/bms_detail_page.dart';
import '../features/dashboard/data/firebase_bms_repository.dart';
import '../features/dashboard/presentation/dashboard_page.dart';
import '../features/event_log/data/event_log_repository.dart';
import '../features/event_log/presentation/event_log_page.dart';
import '../features/settings/presentation/settings_page.dart';

class AuthStateNotifier extends ChangeNotifier {
  AuthStateNotifier(AuthRepository repository) {
    _subscription = repository.authStateChanges().listen((user) {
      currentUser = user;
      isReady = true;
      notifyListeners();
    });
  }

  late StreamSubscription<User?> _subscription;
  User? currentUser;
  bool isReady = false;

  @override
  void dispose() {
    unawaited(_subscription.cancel());
    super.dispose();
  }
}

GoRouter createRouter({
  required AuthRepository authRepository,
  required FirebaseBmsRepository bmsRepository,
  required EventLogRepository eventLogRepository,
  required AuthStateNotifier authState,
}) {
  return GoRouter(
    initialLocation: '/splash',
    refreshListenable: authState,
    observers: [appRouteObserver],
    redirect: (context, state) {
      if (!authState.isReady) {
        return state.matchedLocation == '/splash' ? null : '/splash';
      }

      final isAuthenticationRoute =
          state.matchedLocation == '/login' ||
          state.matchedLocation == '/splash';
      if (authState.currentUser == null) {
        return state.matchedLocation == '/login' ? null : '/login';
      }
      return isAuthenticationRoute ? '/dashboard' : null;
    },
    routes: [
      GoRoute(
        path: '/splash',
        builder: (context, state) => const _SplashPage(),
      ),
      GoRoute(
        path: '/login',
        builder: (context, state) => LoginPage(authRepository: authRepository),
      ),
      GoRoute(
        path: '/dashboard',
        builder: (context, state) => DashboardPage(repository: bmsRepository),
      ),
      GoRoute(
        path: '/bms/:id',
        builder: (context, state) => BmsDetailPage(
          repository: bmsRepository,
          bmsId: state.pathParameters['id'] ?? '',
        ),
      ),
      GoRoute(
        path: '/logs',
        builder: (context, state) =>
            EventLogPage(repository: eventLogRepository),
      ),
      GoRoute(
        path: '/settings',
        builder: (context, state) => SettingsPage(
          repository: bmsRepository,
          authRepository: authRepository,
        ),
      ),
    ],
  );
}

class _SplashPage extends StatelessWidget {
  const _SplashPage();

  @override
  Widget build(BuildContext context) {
    return const Scaffold(
      body: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.battery_charging_full, size: 64),
            SizedBox(height: 20),
            CircularProgressIndicator(),
          ],
        ),
      ),
    );
  }
}
