import 'package:firebase_database/firebase_database.dart';

import '../../../core/constants/app_constants.dart';
import '../../../core/services/remote_session_service.dart';
import '../domain/models/gateway_status.dart';
import '../domain/models/live_telemetry.dart';

class FirebaseBmsRepository {
  FirebaseBmsRepository(
    FirebaseDatabase database, {
    RemoteSessionService? remoteSessionService,
  }) : _database = database,
       _remoteSessionService =
           remoteSessionService ?? RemoteSessionService(database);

  final FirebaseDatabase _database;
  final RemoteSessionService _remoteSessionService;

  Stream<GatewayStatus> watchStatus() {
    return _database
        .ref(AppConstants.statusPath)
        .onValue
        .map((event) => GatewayStatus.fromJson(event.snapshot.value));
  }

  Stream<LiveTelemetry?> watchLive() {
    return _database.ref(AppConstants.livePath).onValue.map((event) {
      if (!event.snapshot.exists || event.snapshot.value == null) return null;
      return LiveTelemetry.fromJson(event.snapshot.value);
    });
  }

  Future<void> requestRemoteSession() =>
      _remoteSessionService.requestRemoteSession();
}
