# Firebase RTDB güvenlik kuralları önerisi

Mevcut geliştirme kuralı, oturum açmış her hesaba cihazın tüm düğümlerini yazma izni verir. Bu nedenle bir mobil kullanıcı `status` veya `live` verisini değiştirebilir. Production ortamında cihaz ve mobil kullanıcı rolleri UID veya custom claim ile ayrılmalıdır.

## Custom claim olmadan önerilen yapı

Firebase Authentication içindeki ESP gateway hesabının UID değerini ve izin verilecek mobil kullanıcı UID'lerini Firebase Console/Admin SDK üzerinden öğrenin. Aşağıdaki yer tutucuları gerçek UID'lerle değiştirin:

```json
{
  "rules": {
    ".read": false,
    ".write": false,
    "devices": {
      "home-battery-01": {
        ".read": "auth != null && (auth.uid === 'ESP_GATEWAY_UID' || auth.uid === 'MOBILE_USER_UID')",
        "status": { ".write": "auth != null && auth.uid === 'ESP_GATEWAY_UID'" },
        "live": { ".write": "auth != null && auth.uid === 'ESP_GATEWAY_UID'" },
        "control": {
          "viewer": {
            "lastSeen": {
              ".write": "auth != null && auth.uid === 'MOBILE_USER_UID'",
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

Birden fazla mobil kullanıcı için UID tabanlı bir `/roles/viewers/{uid}: true` allowlist'i kullanılabilir; fakat kullanıcıların bu rol ağacına yazması kesinlikle engellenmelidir. Daha ölçeklenebilir production çözümü, yalnız güvenilir Admin SDK ortamından atanan `gateway` ve `viewer` custom claim'leridir.

Email veya şifreyi RTDB rules içinde rol ayrımı için kullanmak güvenli ve sürdürülebilir değildir. Authenticated-only mevcut yapıda cihaz/mobil ayrımı yapılamaz. Bu dosya yalnız öneridir; çalışan Firebase kuralları otomatik değiştirilmemiştir.

Tek `control/viewer/lastSeen` alanı birden fazla telefon tarafından ortak güncellenir. Herhangi bir aktif telefon gateway'i aktif tutar; mevcut firmware sözleşmesi gereği bu davranış korunmuştur.
