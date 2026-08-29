# Firebase RTDB güvenlik kuralları önerisi

Yalnız `auth != null` kontrolü yapan geliştirme kuralları, oturum açmış her hesaba cihazın tüm düğümlerini yazma izni verir. Bu nedenle production ortamında cihaz ve mobil kullanıcı rolleri custom claim ile ayrılmalıdır. Repository'deki `esp32/firebase/database.rules.json` aşağıdaki claim tabanlı şablonu uygular; deploy öncesinde hesaplara claim atanması zorunludur.

## Custom claim ile önerilen yapı

ESP gateway hesabına yalnız güvenilir Admin SDK ortamından `gateway: true`, izin verilen mobil kullanıcılara `viewer: true` custom claim'i atayın:

```json
{
  "rules": {
    ".read": false,
    ".write": false,
    "devices": {
      "$deviceId": {
        ".read": "auth != null && (auth.token.gateway === true || auth.token.viewer === true)",
        "status": { ".write": "auth != null && auth.token.gateway === true" },
        "live": { ".write": "auth != null && auth.token.gateway === true" },
        "control": {
          "remoteSession": {
            "requestedAt": {
              ".write": "auth != null && auth.token.viewer === true",
              ".validate": "newData.isNumber() && newData.val() <= now + 5000 && newData.val() >= now - 10000"
            },
            "$other": { ".write": false }
          },
          "$other": { ".write": false }
        },
        "$other": { ".write": false }
      },
      "$other": { ".read": false, ".write": false }
    }
  }
}
```

Birden fazla mobil kullanıcı aynı `viewer` claim'ini kullanabilir. Claim'ler yalnız güvenilir Admin SDK ortamından atanmalı ve kullanıcıların herhangi bir rol ağacına yazması engellenmelidir.

Email veya şifreyi RTDB rules içinde rol ayrımı için kullanmak güvenli ve sürdürülebilir değildir. Authenticated-only mevcut yapıda cihaz/mobil ayrımı yapılamaz. Bu dosya yalnız öneridir; çalışan Firebase kuralları otomatik değiştirilmemiştir.

Tek `control/remoteSession/requestedAt` alanı birden fazla telefon tarafından ortak kullanılabilir. Her yeni server timestamp, önceki oturumu 60 saniyelik yeni bir izleme penceresiyle değiştirir; uygulama foreground'da kaldığı sürece periyodik write yapılmaz.
