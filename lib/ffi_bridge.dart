import 'dart:ffi' as ffi;
import 'package:ffi/ffi.dart';
import 'dart:io';

// 1. Подгружаем скомпилированную C++ библиотеку
final ffi.DynamicLibrary serverLib = Platform.isAndroid
    ? ffi.DynamicLibrary.open('libserver_core.so')
    : ffi.DynamicLibrary.process();

// 2. Объясняем типы для функции start_cell_server
typedef StartServerC = ffi.Int32 Function(ffi.Int32 port);
typedef StartServerDart = int Function(int port);

final StartServerDart startServer = serverLib
    .lookup<ffi.NativeFunction<StartServerC>>('start_cell_server')
    .asFunction();

// 3. Объясняем типы для работы со строками (самое сложное в FFI)
typedef SearchLogC = ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8> trigger);
typedef SearchLogDart = ffi.Pointer<Utf8> Function(ffi.Pointer<Utf8> trigger);

final SearchLogDart _searchLogNative = serverLib
    .lookup<ffi.NativeFunction<SearchLogC>>('search_log_trigger')
    .asFunction();

// Удобная обертка для поиска, чтобы интерфейс работал с обычными String
String searchLog(String query) {
  final queryPointer = query.toNativeUtf8(); // Превращаем Dart строку в C-указатель
  final resultPointer = _searchLogNative(queryPointer); // Вызываем C++ функцию
  
  final resultString = resultPointer.toDartString(); // Читаем ответ от C++
  calloc.free(queryPointer); // Очищаем память, чтобы не было утечек
  
  return resultString;
}

