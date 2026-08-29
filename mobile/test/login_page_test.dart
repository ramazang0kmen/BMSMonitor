import 'package:bmsmonitor/features/auth/data/auth_repository.dart';
import 'package:bmsmonitor/features/auth/presentation/login_page.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';

class _MockFirebaseAuth extends Mock implements FirebaseAuth {}

void main() {
  testWidgets('login screen renders and password visibility toggles', (
    tester,
  ) async {
    await tester.pumpWidget(
      MaterialApp(
        home: LoginPage(authRepository: AuthRepository(_MockFirebaseAuth())),
      ),
    );

    expect(find.text('BMS Monitor'), findsOneWidget);
    expect(find.text('E-posta'), findsOneWidget);
    expect(find.text('Şifre'), findsOneWidget);
    expect(find.text('Giriş Yap'), findsOneWidget);

    var passwordField = tester
        .widgetList<EditableText>(find.byType(EditableText))
        .last;
    expect(passwordField.obscureText, isTrue);

    await tester.tap(find.byTooltip('Şifreyi göster'));
    await tester.pump();
    passwordField = tester
        .widgetList<EditableText>(find.byType(EditableText))
        .last;
    expect(passwordField.obscureText, isFalse);
  });
}
