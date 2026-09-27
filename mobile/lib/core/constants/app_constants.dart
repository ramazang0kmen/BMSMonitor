abstract final class AppConstants {
  static const appName = 'BMS Monitor';
  static const deviceId = 'home-battery-01';
  static const databaseUrl =
      'https://bmsapp-3bcff-default-rtdb.europe-west1.firebasedatabase.app/';
  static const deviceRoot = '/devices/$deviceId';
  static const statusPath = '$deviceRoot/status';
  static const livePath = '$deviceRoot/live';
  static const remoteSessionRequestedAtPath =
      '$deviceRoot/control/remoteSession/requestedAt';

  // Firestore, not RTDB: gateway event log (SOC thresholds, load cuts, BMS
  // faults) kept as history.
  static const eventLogCollection = 'devices/$deviceId/logs';
  static const eventLogPageSize = 200;

  static const remoteSessionDuration = Duration(seconds: 60);
  static const gatewayOnlineTimeout = Duration(seconds: 60);
  static const liveThreshold = Duration(seconds: 5);
  static const delayedThreshold = Duration(seconds: 15);

  // Flip this once the JBD sign convention is verified under a real load.
  static const invertCurrentSign = false;
}
