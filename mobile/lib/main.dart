import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';

import 'app/app.dart';
import 'core/constants/app_constants.dart';
import 'features/auth/data/auth_repository.dart';
import 'features/dashboard/data/firebase_bms_repository.dart';
import 'features/event_log/data/event_log_repository.dart';
import 'firebase_options.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  try {
    final firebaseApp = await Firebase.initializeApp(
      options: DefaultFirebaseOptions.currentPlatform,
    );
    final database = FirebaseDatabase.instanceFor(
      app: firebaseApp,
      databaseURL: AppConstants.databaseUrl,
    );

    runApp(
      BmsMonitorApp(
        authRepository: AuthRepository(
          FirebaseAuth.instanceFor(app: firebaseApp),
        ),
        bmsRepository: FirebaseBmsRepository(database),
        eventLogRepository: EventLogRepository(
          FirebaseFirestore.instanceFor(app: firebaseApp),
        ),
      ),
    );
  } catch (error) {
    runApp(_BootstrapErrorApp(message: error.toString()));
  }
}

class _BootstrapErrorApp extends StatelessWidget {
  const _BootstrapErrorApp({required this.message});

  final String message;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      home: Scaffold(
        body: SafeArea(
          child: Center(
            child: Padding(
              padding: const EdgeInsets.all(24),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  const Icon(Icons.error_outline, size: 56, color: Colors.red),
                  const SizedBox(height: 16),
                  const Text(
                    'Firebase başlatılamadı',
                    style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold),
                  ),
                  const SizedBox(height: 8),
                  const Text(
                    'Platform Firebase yapılandırmasını kontrol edin.',
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 12),
                  SelectableText(message, textAlign: TextAlign.center),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}
