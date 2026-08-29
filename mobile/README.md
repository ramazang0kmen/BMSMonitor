# BMS Monitor

ESP32 BMS gateway'in Firebase Realtime Database telemetrisini Android ve iOS'ta canlı izleyen Flutter uygulamasıdır. Firebase Email/Password Authentication, Material 3, sistem dark mode, `go_router` ve `flutter_bloc` kullanır.

## Yapılandırma

- Firebase project: `bmsapp-3bcff`
- Database: `https://bmsapp-3bcff-default-rtdb.europe-west1.firebasedatabase.app/`
- Device: `home-battery-01`
- Android/iOS kimliği: `com.ramazangokmen.bmsmonitor`
- Status: `/devices/home-battery-01/status`
- Live: `/devices/home-battery-01/live`
- Remote session request: `/devices/home-battery-01/control/remoteSession/requestedAt`

Verilen web ayarları `lib/firebase_options.dart` içinde açık Firebase client config olarak bulunur. Production/native kurulum için Firebase Console'da aynı package/bundle ID ile Android ve iOS uygulamalarını kaydedin, ardından Firebase CLI ve FlutterFire CLI kurulu bir ortamda proje kökünden çalıştırın:

```text
flutterfire configure --project=bmsapp-3bcff --platforms=android,ios
```

Bu işlem doğru native app ID'leri ile `firebase_options.dart`, Android `google-services.json` ve iOS `GoogleService-Info.plist` dosyalarını üretir. Bu bilgisayarda Firebase CLI bulunmadığı için native uygulama kaydı otomatik yapılamamıştır.

Firebase Console > Authentication > Sign-in method altında Email/Password'ü etkinleştirin. Ardından mobil kullanım için ayrı bir kullanıcı oluşturun. ESP32 cihaz hesabını veya parolasını mobil uygulamaya koymayın; uygulamada kayıt ekranı bilinçli olarak yoktur.

## Çalıştırma ve build

```text
flutter pub get
flutter analyze
flutter test
flutter run
flutter build apk --debug
```

Debug APK çıktısı `build/app/outputs/flutter-apk/app-debug.apk` yolundadır.

## Canlı veri ve lifecycle

Gateway online durumu `status.online` yerine `now - status.lastSeen <= 60 saniye` ile hesaplanır. Live timestamp yaşı 0–5 saniye CANLI, 5–15 saniye GECİKMELİ, 15 saniye üzeri VERİ ESKİ olarak gösterilir; gateway timeout'u OFFLINE durumunu önceler.

Dashboard ilk açıldığında veya uygulama background'dan foreground'a döndüğünde `control/remoteSession/requestedAt` alanına bir kez `ServerValue.timestamp` yazılır. ESP32 bu istekten itibaren 60 saniye canlı telemetri gönderir. Uygulama foreground'da açık kaldığı sürece periyodik write yapılmaz. Kullanıcı dashboard'daki **1 dk canlı izle** butonuyla yeni bir oturum isteyebilir. Live subscription görünür ekran süresince, status stream ise tanılama için sürekli dinlenebilir.

RTDB stream olduğu için pull-to-refresh yoktur. Bağlantı kesilirse son bilinen değer korunur ve UI çevrimdışı/eski veri olarak etiketler.

## Güvenlik

Mobil uygulama yalnız `control/remoteSession/requestedAt` alanına yazacak şekilde tasarlanmıştır; `status` ve `live` yalnız okunur. İstemci davranışı tek başına güvenlik sınırı değildir. Production RTDB rules önerisi için [FIREBASE_RULES_RECOMMENDATION.md](docs/FIREBASE_RULES_RECOMMENDATION.md) dosyasını uygulayın ve gerekli custom claim'leri tanımlayın.

Firebase API key gizli bir sunucu parolası değildir; yine de Firebase rules ve Authentication zorunludur. Mobil veya ESP32 kullanıcı parolaları kaynak kodda tutulmamalıdır.

## Akım işareti

JBD akım yönü gerçek yük altında henüz fiziksel olarak doğrulanmadı. Tüm UI işaret davranışı `lib/core/constants/app_constants.dart` içindeki `invertCurrentSign` ayarından tek noktada ters çevrilebilir.

## Testler

`test/fixtures/live_telemetry.json` gerçek örnek yapıyı içerir. Birim testleri int/double dönüşümü, eksik alan, boş/sparse hücreler, offline veri, protection flags, freshness sınırları, tek seferlik server timestamp isteği, lifecycle resume ve manuel session yenilemeyi kapsar.
