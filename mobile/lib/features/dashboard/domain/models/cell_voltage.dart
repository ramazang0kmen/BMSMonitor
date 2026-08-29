import 'package:equatable/equatable.dart';

class CellVoltage extends Equatable {
  const CellVoltage({
    required this.index,
    required this.voltage,
    required this.isMinimum,
    required this.isMaximum,
  });

  final int index;
  final double voltage;
  final bool isMinimum;
  final bool isMaximum;

  @override
  List<Object?> get props => [index, voltage, isMinimum, isMaximum];
}
