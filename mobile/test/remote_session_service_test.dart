import 'package:bmsmonitor/core/constants/app_constants.dart';
import 'package:bmsmonitor/core/services/remote_session_service.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';

class _MockFirebaseDatabase extends Mock implements FirebaseDatabase {}

class _MockDatabaseReference extends Mock implements DatabaseReference {}

void main() {
  test('requestRemoteSession writes one Firebase server timestamp', () async {
    final database = _MockFirebaseDatabase();
    final reference = _MockDatabaseReference();
    when(
      () => database.ref(AppConstants.remoteSessionRequestedAtPath),
    ).thenReturn(reference);
    when(() => reference.set(ServerValue.timestamp)).thenAnswer((_) async {});

    final service = RemoteSessionService(database);
    await service.requestRemoteSession();

    verify(
      () => database.ref(AppConstants.remoteSessionRequestedAtPath),
    ).called(1);
    verify(() => reference.set(ServerValue.timestamp)).called(1);
    verifyNoMoreInteractions(reference);
  });
}
