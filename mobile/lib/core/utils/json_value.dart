Map<String, dynamic> asStringMap(Object? value) {
  if (value is! Map) return const {};
  return value.map((key, item) => MapEntry(key.toString(), item));
}

double asDouble(Object? value, [double fallback = 0]) =>
    value is num ? value.toDouble() : fallback;

int asInt(Object? value, [int fallback = 0]) =>
    value is num ? value.toInt() : fallback;

bool asBool(Object? value, [bool fallback = false]) =>
    value is bool ? value : fallback;

String asText(Object? value, [String fallback = '']) =>
    value is String ? value : fallback;

List<Object?> asObjectList(Object? value) {
  if (value is List) return List<Object?>.from(value);
  if (value is Map) {
    final entries =
        value.entries
            .map(
              (entry) =>
                  MapEntry(int.tryParse(entry.key.toString()), entry.value),
            )
            .where((entry) => entry.key != null)
            .toList()
          ..sort((a, b) => a.key!.compareTo(b.key!));
    return entries.map((entry) => entry.value).toList();
  }
  return const [];
}
