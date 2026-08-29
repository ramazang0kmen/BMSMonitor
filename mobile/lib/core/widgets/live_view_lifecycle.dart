import 'package:flutter/material.dart';

import '../services/app_route_observer.dart';

class LiveViewLifecycle extends StatefulWidget {
  const LiveViewLifecycle({
    required this.onActiveChanged,
    required this.onSessionRequested,
    required this.child,
    this.requestOnInitialActive = true,
    super.key,
  });

  final ValueChanged<bool> onActiveChanged;
  final VoidCallback onSessionRequested;
  final bool requestOnInitialActive;
  final Widget child;

  @override
  State<LiveViewLifecycle> createState() => _LiveViewLifecycleState();
}

class _LiveViewLifecycleState extends State<LiveViewLifecycle>
    with WidgetsBindingObserver, RouteAware {
  bool _routeVisible = true;
  bool _appResumed = false;
  bool? _lastValue;
  bool _initialRequestSent = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _appResumed =
        WidgetsBinding.instance.lifecycleState == null ||
        WidgetsBinding.instance.lifecycleState == AppLifecycleState.resumed;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      final route = ModalRoute.of(context);
      if (route != null) appRouteObserver.subscribe(this, route);
      _notify();
      _requestInitialSessionIfNeeded();
    });
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    final wasResumed = _appResumed;
    _appResumed = state == AppLifecycleState.resumed;
    _notify();
    if (!wasResumed && _appResumed && _routeVisible) {
      widget.onSessionRequested();
    }
  }

  @override
  void didPush() {
    _routeVisible = true;
    _notify();
  }

  @override
  void didPopNext() {
    _routeVisible = true;
    _notify();
  }

  @override
  void didPushNext() {
    _routeVisible = false;
    _notify();
  }

  @override
  void didPop() {
    _routeVisible = false;
    _notify();
  }

  void _notify() {
    final active = _routeVisible && _appResumed;
    if (_lastValue == active) return;
    _lastValue = active;
    widget.onActiveChanged(active);
  }

  void _requestInitialSessionIfNeeded() {
    if (_initialRequestSent ||
        !widget.requestOnInitialActive ||
        !_routeVisible ||
        !_appResumed) {
      return;
    }
    _initialRequestSent = true;
    widget.onSessionRequested();
  }

  @override
  void dispose() {
    appRouteObserver.unsubscribe(this);
    WidgetsBinding.instance.removeObserver(this);
    widget.onActiveChanged(false);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => widget.child;
}
