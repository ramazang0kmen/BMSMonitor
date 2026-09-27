import 'package:cloud_firestore/cloud_firestore.dart';

import '../../../core/constants/app_constants.dart';
import '../domain/models/event_log_entry.dart';

class EventLogRepository {
  EventLogRepository(FirebaseFirestore firestore) : _firestore = firestore;

  final FirebaseFirestore _firestore;

  /// Newest first. The gateway only writes on state changes, so a few hundred
  /// entries cover weeks of history.
  Stream<List<EventLogEntry>> watchLogs({
    int limit = AppConstants.eventLogPageSize,
  }) {
    return _firestore
        .collection(AppConstants.eventLogCollection)
        .orderBy('createdAt', descending: true)
        .limit(limit)
        .snapshots()
        .map(
          (snapshot) => snapshot.docs
              .map((doc) => EventLogEntry.fromJson(doc.id, doc.data()))
              .toList(growable: false),
        );
  }
}
