import 'package:firebase_auth/firebase_auth.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../data/auth_repository.dart';

enum LoginStatus { initial, submitting, success, failure }

class LoginState {
  const LoginState({this.status = LoginStatus.initial, this.message});

  final LoginStatus status;
  final String? message;
}

class LoginCubit extends Cubit<LoginState> {
  LoginCubit(this._repository) : super(const LoginState());

  final AuthRepository _repository;

  Future<void> submit(String email, String password) async {
    if (email.trim().isEmpty || password.isEmpty) {
      emit(
        const LoginState(
          status: LoginStatus.failure,
          message: 'E-posta ve şifre gerekli.',
        ),
      );
      return;
    }
    emit(const LoginState(status: LoginStatus.submitting));
    try {
      await _repository.signIn(email: email, password: password);
      emit(const LoginState(status: LoginStatus.success));
    } on FirebaseAuthException catch (error) {
      emit(
        LoginState(
          status: LoginStatus.failure,
          message: firebaseAuthMessage(error.code),
        ),
      );
    } catch (_) {
      emit(
        const LoginState(
          status: LoginStatus.failure,
          message: 'Giriş sırasında beklenmeyen bir hata oluştu.',
        ),
      );
    }
  }
}

String firebaseAuthMessage(String code) => switch (code) {
  'invalid-credential' ||
  'wrong-password' ||
  'user-not-found' => 'E-posta veya şifre hatalı.',
  'invalid-email' => 'Geçerli bir e-posta adresi girin.',
  'network-request-failed' => 'İnternet bağlantısı kontrol edilmeli.',
  'too-many-requests' => 'Çok fazla deneme yapıldı. Lütfen bekleyin.',
  'user-disabled' => 'Bu kullanıcı hesabı devre dışı.',
  _ => 'Giriş yapılamadı. Lütfen tekrar deneyin.',
};
