import 'package:bmsmonitor/features/dashboard/data/firebase_bms_repository.dart';
import 'package:bmsmonitor/features/dashboard/presentation/dashboard_page.dart';
import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';

class _MockRepository extends Mock implements FirebaseBmsRepository {}

class _MockConnectivity extends Mock implements Connectivity {}

void main() {
  late _MockRepository repository;
  late _MockConnectivity connectivity;

  setUp(() {
    repository = _MockRepository();
    connectivity = _MockConnectivity();
    when(repository.watchStatus).thenAnswer((_) => const Stream.empty());
    when(repository.watchLive).thenAnswer((_) => const Stream.empty());
    when(repository.requestRemoteSession).thenAnswer((_) async {});
    when(
      () => connectivity.onConnectivityChanged,
    ).thenAnswer((_) => const Stream.empty());
    when(
      connectivity.checkConnectivity,
    ).thenAnswer((_) async => [ConnectivityResult.wifi]);
  });

  testWidgets(
    'dashboard requests once, does not repeat, resumes, and refreshes manually',
    (tester) async {
      await tester.pumpWidget(
        MaterialApp(
          home: DashboardPage(
            repository: repository,
            connectivity: connectivity,
          ),
        ),
      );
      await tester.pump();
      verify(repository.requestRemoteSession).called(1);

      await tester.pump(const Duration(seconds: 60));
      verifyNever(repository.requestRemoteSession);

      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.paused);
      await tester.pump();
      tester.binding.handleAppLifecycleStateChanged(AppLifecycleState.resumed);
      await tester.pump();
      verify(repository.requestRemoteSession).called(1);

      await tester.tap(find.byTooltip('1 dk canlı izle'));
      await tester.pump();
      verify(repository.requestRemoteSession).called(1);
    },
  );
}
