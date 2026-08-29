import 'dart:async';

import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../core/constants/app_constants.dart';
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
    this.sessionRequestInProgress = false,
    this.sessionRequestedAt,
    this.errorMessage,
  });

  final GatewayStatus? status;
  final LiveTelemetry? telemetry;
  final DateTime? now;
  final bool internetAvailable;
  final bool liveViewActive;
  final bool sessionRequestInProgress;
  final DateTime? sessionRequestedAt;
  final String? errorMessage;

  Duration get remoteSessionRemaining {
    final requestedAt = sessionRequestedAt;
    final currentTime = now;
    if (requestedAt == null || currentTime == null) return Duration.zero;
    final remaining =
        AppConstants.remoteSessionDuration -
        currentTime.difference(requestedAt);
    return remaining.isNegative ? Duration.zero : remaining;
  }

  bool get waitingForStatus => status == null && errorMessage == null;

  DashboardState copyWith({
    GatewayStatus? status,
    LiveTelemetry? telemetry,
    DateTime? now,
    bool? internetAvailable,
    bool? liveViewActive,
    bool? sessionRequestInProgress,
    DateTime? sessionRequestedAt,
    String? errorMessage,
    bool clearError = false,
  }) {
    return DashboardState(
      status: status ?? this.status,
      telemetry: telemetry ?? this.telemetry,
      now: now ?? this.now,
      internetAvailable: internetAvailable ?? this.internetAvailable,
      liveViewActive: liveViewActive ?? this.liveViewActive,
      sessionRequestInProgress:
          sessionRequestInProgress ?? this.sessionRequestInProgress,
      sessionRequestedAt: sessionRequestedAt ?? this.sessionRequestedAt,
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
    sessionRequestInProgress,
    sessionRequestedAt,
    errorMessage,
  ];
}

class DashboardCubit extends Cubit<DashboardState> {
  DashboardCubit(this._repository, {Connectivity? connectivity})
    : _connectivity = connectivity ?? Connectivity(),
      super(DashboardState(now: DateTime.now())) {
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
  StreamSubscription<GatewayStatus>? _statusSubscription;
  StreamSubscription<LiveTelemetry?>? _liveSubscription;
  StreamSubscription<List<ConnectivityResult>>? _connectivitySubscription;
  Timer? _clock;

  void setLiveViewActive(bool active) {
    if (active == state.liveViewActive) return;
    emit(state.copyWith(liveViewActive: active));
    if (active) {
      _liveSubscription ??= _repository.watchLive().listen(
        (telemetry) =>
            emit(state.copyWith(telemetry: telemetry, clearError: true)),
        onError: _onStreamError,
      );
    } else {
      final subscription = _liveSubscription;
      _liveSubscription = null;
      unawaited(subscription?.cancel());
    }
  }

  Future<void> requestRemoteSession() async {
    if (state.sessionRequestInProgress) return;
    emit(state.copyWith(sessionRequestInProgress: true, clearError: true));
    try {
      await _repository.requestRemoteSession();
      if (isClosed) return;
      emit(
        state.copyWith(
          sessionRequestInProgress: false,
          sessionRequestedAt: DateTime.now(),
          clearError: true,
        ),
      );
    } catch (_) {
      if (isClosed) return;
      emit(
        state.copyWith(
          sessionRequestInProgress: false,
          errorMessage: 'Canlı izleme oturumu başlatılamadı.',
        ),
      );
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
    _clock?.cancel();
    await _statusSubscription?.cancel();
    await _liveSubscription?.cancel();
    await _connectivitySubscription?.cancel();
    return super.close();
  }
}
