import 'package:firebase_database/firebase_database.dart';

import '../constants/app_constants.dart';

class RemoteSessionService {
  RemoteSessionService(FirebaseDatabase database) : _database = database;

  final FirebaseDatabase _database;

  Future<void> requestRemoteSession() {
    return _database
        .ref(AppConstants.remoteSessionRequestedAtPath)
        .set(ServerValue.timestamp);
  }
}
