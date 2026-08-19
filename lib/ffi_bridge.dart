import 'dart:ffi' as ffi;
import 'package:ffi/ffi.dart';
import 'dart:io';

// 1. Подгружаем C++ библиотеку
final ffi.DynamicLibrary serverLib = Platform.isAndroid
    ? ffi.DynamicLibrary.open('libserver_core.so')
    : ffi.DynamicLibrary.process();

// 2. Запуск сервера
typedef StartServerC = ffi.Int32 Function(ffi.Int32 port, ffi.Pointer<Utf8> cellName);
typedef StartServerDart = int Function(int port, ffi.Pointer<Utf8> cellName);

final StartServerDart _startServerSocketNative = serverLib
    .lookup<ffi.NativeFunction<StartServerC>>('start_cell_server')
    .asFunction();

int startServerSocket(int port, String cellName) {
  final cellPtr = cellName.toNativeUtf8();
  final res = _startServerSocketNative(port, cellPtr);
  calloc.free(cellPtr);
  return res;
}

// 3. Шифрование и запись лога
typedef ProcessMsgC = ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8> cell, ffi.Int32 uid, ffi.Pointer<Utf8> msg);
typedef ProcessMsgDart = ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8> cell, int uid, ffi.Pointer<Utf8> msg);

final ProcessMsgDart _processMsgNative = serverLib
    .lookup<ffi.NativeFunction<ProcessMsgC>>('process_incoming_message')
    .asFunction();

String processIncomingMessage(String cell, int uid, String msg) {
  final cellPtr = cell.toNativeUtf8();
  final msgPtr = msg.toNativeUtf8();
  final resPtr = _processMsgNative(cellPtr, uid, msgPtr);
  final resStr = resPtr.toDartString();
  calloc.free(cellPtr);
  calloc.free(msgPtr);
  return resStr;
}

// 4. Поиск по триггеру
typedef SearchLogC = ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8> cell, ffi.Pointer<Utf8> trigger);
typedef SearchLogDart = ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8> cell, ffi.Pointer<Utf8> trigger);

final SearchLogDart _searchLogNative = serverLib
    .lookup<ffi.NativeFunction<SearchLogC>>('search_log_trigger')
    .asFunction();

String searchLogTrigger(String cell, String trigger) {
  final cellPtr = cell.toNativeUtf8();
  final triggerPtr = trigger.toNativeUtf8();
  final resPtr = _searchLogNative(cellPtr, triggerPtr);
  final resStr = resPtr.toDartString();
  calloc.free(cellPtr);
  calloc.free(triggerPtr);
  return resStr;
}
