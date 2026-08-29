import 'dart:async';

import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../core/constants/app_constants.dart';
import '../../../core/services/viewer_heartbeat_controller.dart';
import '../data/firebase_bms_repository.dart';
import '../domain/models/gateway_status.dart';
import '../domain/models/live_telemetry.dart';

class DashboardState extends Equatable {
  const DashboardState({
    this.status,
    this.telemetry,
    this.now,
    this.internetAvailable = true,
    this.liveViewActive = false,
    this.errorMessage,
  });

  final GatewayStatus? status;
  final LiveTelemetry? telemetry;
  final DateTime? now;
  final bool internetAvailable;
  final bool liveViewActive;
  final String? errorMessage;

  bool get waitingForStatus => status == null && errorMessage == null;

  DashboardState copyWith({
    GatewayStatus? status,
    LiveTelemetry? telemetry,
    DateTime? now,
    bool? internetAvailable,
    bool? liveViewActive,
    String? errorMessage,
    bool clearError = false,
  }) {
    return DashboardState(
      status: status ?? this.status,
      telemetry: telemetry ?? this.telemetry,
      now: now ?? this.now,
      internetAvailable: internetAvailable ?? this.internetAvailable,
      liveViewActive: liveViewActive ?? this.liveViewActive,
      errorMessage: clearError ? null : errorMessage ?? this.errorMessage,
    );
  }

  @override
  List<Object?> get props => [
    status,
    telemetry,
    now,
    internetAvailable,
    liveViewActive,
    errorMessage,
  ];
}

class DashboardCubit extends Cubit<DashboardState> {
  DashboardCubit(this._repository, {Connectivity? connectivity})
    : _connectivity = connectivity ?? Connectivity(),
      super(DashboardState(now: DateTime.now())) {
    _heartbeat = ViewerHeartbeatController(
      writeHeartbeat: _repository.sendViewerHeartbeat,
      interval: AppConstants.viewerHeartbeatInterval,
      onError: _onHeartbeatError,
    );
    _statusSubscription = _repository.watchStatus().listen(
      (status) => emit(state.copyWith(status: status, clearError: true)),
      onError: _onStreamError,
    );
    _connectivitySubscription = _connectivity.onConnectivityChanged.listen(
      _onConnectivityChanged,
    );
    _connectivity.checkConnectivity().then(_onConnectivityChanged);
    _clock = Timer.periodic(
      const Duration(seconds: 1),
      (_) => emit(state.copyWith(now: DateTime.now())),
    );
  }

  final FirebaseBmsRepository _repository;
  final Connectivity _connectivity;
  late final ViewerHeartbeatController _heartbeat;
  StreamSubscription<GatewayStatus>? _statusSubscription;
  StreamSubscription<LiveTelemetry?>? _liveSubscription;
  StreamSubscription<List<ConnectivityResult>>? _connectivitySubscription;
  Timer? _clock;

  void setLiveViewActive(bool active) {
    if (active == state.liveViewActive) return;
    emit(state.copyWith(liveViewActive: active));
    if (active) {
      _heartbeat.start();
      _liveSubscription ??= _repository.watchLive().listen(
        (telemetry) =>
            emit(state.copyWith(telemetry: telemetry, clearError: true)),
        onError: _onStreamError,
      );
    } else {
      _heartbeat.stop();
      final subscription = _liveSubscription;
      _liveSubscription = null;
      unawaited(subscription?.cancel());
    }
  }

  void _onConnectivityChanged(List<ConnectivityResult> results) {
    if (isClosed) return;
    emit(
      state.copyWith(
        internetAvailable: !results.contains(ConnectivityResult.none),
      ),
    );
  }

  void _onHeartbeatError(Object _) {
    if (isClosed) return;
    emit(state.copyWith(errorMessage: 'Viewer heartbeat gönderilemedi.'));
  }

  void _onStreamError(Object _) {
    if (isClosed) return;
    emit(
      state.copyWith(
        errorMessage: 'Firebase verisi alınamadı. Son veriler gösteriliyor.',
      ),
    );
  }

  @override
  Future<void> close() async {
    _heartbeat.dispose();
    _clock?.cancel();
    await _statusSubscription?.cancel();
    await _liveSubscription?.cancel();
    await _connectivitySubscription?.cancel();
    return super.close();
  }
}
