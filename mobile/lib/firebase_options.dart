import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/foundation.dart'
    show TargetPlatform, defaultTargetPlatform, kIsWeb;

abstract final class DefaultFirebaseOptions {
  static FirebaseOptions get currentPlatform {
    if (kIsWeb) return web;
    return switch (defaultTargetPlatform) {
      TargetPlatform.android => android,
      TargetPlatform.iOS => ios,
      _ => web,
    };
  }

  // Auth and Realtime Database are initialized explicitly from the supplied
  // Firebase project settings. Native app registrations can later replace the
  // shared appId through flutterfire configure without changing app code.
  static const web = FirebaseOptions(
    apiKey: 'AIzaSyDdT_47AvRyb6TC-iLkb_Y71Q7KPOF4Mw0',
    appId: '1:803865181162:web:f5343c30aa7c239f7a5093',
    messagingSenderId: '803865181162',
    projectId: 'bmsapp-3bcff',
    authDomain: 'bmsapp-3bcff.firebaseapp.com',
    databaseURL:
        'https://bmsapp-3bcff-default-rtdb.europe-west1.firebasedatabase.app/',
    storageBucket: 'bmsapp-3bcff.firebasestorage.app',
  );

  static const android = FirebaseOptions(
    apiKey: 'AIzaSyDdT_47AvRyb6TC-iLkb_Y71Q7KPOF4Mw0',
    appId: '1:803865181162:web:f5343c30aa7c239f7a5093',
    messagingSenderId: '803865181162',
    projectId: 'bmsapp-3bcff',
    databaseURL:
        'https://bmsapp-3bcff-default-rtdb.europe-west1.firebasedatabase.app/',
    storageBucket: 'bmsapp-3bcff.firebasestorage.app',
  );

  static const ios = FirebaseOptions(
    apiKey: 'AIzaSyDdT_47AvRyb6TC-iLkb_Y71Q7KPOF4Mw0',
    appId: '1:803865181162:web:f5343c30aa7c239f7a5093',
    messagingSenderId: '803865181162',
    projectId: 'bmsapp-3bcff',
    databaseURL:
        'https://bmsapp-3bcff-default-rtdb.europe-west1.firebasedatabase.app/',
    storageBucket: 'bmsapp-3bcff.firebasestorage.app',
    iosBundleId: 'com.ramazangokmen.bmsmonitor',
  );
}
