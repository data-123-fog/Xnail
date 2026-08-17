import 'package:flutter/material.dart';
import 'ffi_bridge.dart'; // Наш мост к C++ ядру

void main() {
  runApp(const CyberServerApp());
}

class CyberServerApp extends StatelessWidget {
  const CyberServerApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Xnail Server',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark().copyWith(
        scaffoldBackgroundColor: const Color(0xFF0F172A), // Тёмный фон
        colorScheme: const ColorScheme.dark(
          primary: Color(0xFF06B6D4), // Неоновый циан
          surface: Color(0xFF1E293B),
        ),
      ),
      home: const ServerHomeScreen(),
    );
  }
}

class ServerHomeScreen extends StatefulWidget {
  const ServerHomeScreen({super.key});

  @override
  State<ServerHomeScreen> createState() => _ServerHomeScreenState();
}

class _ServerHomeScreenState extends State<ServerHomeScreen> {
  final TextEditingController _cellController = TextEditingController(text: "messages");
  final TextEditingController _portController = TextEditingController(text: "8080");
  final TextEditingController _userIdController = TextEditingController(text: "47");
  final TextEditingController _msgController = TextEditingController();
  final TextEditingController _searchController = TextEditingController();

  String _statusText = "Сервер остановлен";
  String _logOutput = "Логи появятся здесь...";
  bool _isServerRunning = false;

  void _startServer() {
    int port = int.tryParse(_portController.text) ?? 8080;
    int res = startServerSocket(port, _cellController.text);
    if (res == 1) {
      setState(() {
        _isServerRunning = true;
        _statusText = "Сервер запущен на порту $port [Ячейка: ${_cellController.text}]";
      });
    }
  }

  void _sendAndEncrypt() {
    if (_msgController.text.isEmpty) return;
    int uid = int.tryParse(_userIdController.text) ?? 1;
    
    // Вызов C++ ядра через FFI
    String res = processIncomingMessage(_cellController.text, uid, _msgController.text);
    setState(() {
      _logOutput = res;
      _msgController.clear();
    });
  }

  void _searchTrigger() {
    if (_searchController.text.isEmpty) return;
    
    // Поиск лога в C++ хэш-индексе за 0.001 секунды
    String res = searchLogTrigger(_cellController.text, _searchController.text);
    setState(() {
      _logOutput = res;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("XNAIL CELL SERVER", style: TextStyle(fontWeight: FontWeight.bold, letterSpacing: 1.5)),
        centerTitle: true,
        backgroundColor: const Color(0xFF1E293B),
        elevation: 4,
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Блок статуса и запуска сервера
            Card(
              color: const Color(0xFF1E293B),
              shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  children: [
                    Row(
                      children: [
                        Icon(
                          _isServerRunning ? Icons.radar : Icons.power_settings_new,
                          color: _isServerRunning ? Colors.greenAccent : Colors.redAccent,
                        ),
                        const SizedBox(width: 10),
                        Expanded(child: Text(_statusText, style: const TextStyle(fontSize: 14))),
                      ],
                    ),
                    const SizedBox(height: 12),
                    Row(
                      children: [
                        Expanded(
                          child: TextField(
                            controller: _cellController,
                            decoration: const InputDecoration(labelText: "Имя Ячейки", border: OutlineInputBorder()),
                          ),
                        ),
                        const SizedBox(width: 10),
                        Expanded(
                          child: TextField(
                            controller: _portController,
                            keyboardType: TextInputType.number,
                            decoration: const InputDecoration(labelText: "Порт", border: OutlineInputBorder()),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 12),
                    ElevatedButton.icon(
                      style: ElevatedButton.styleFrom(
                        backgroundColor: _isServerRunning ? Colors.grey : const Color(0xFF06B6D4),
                        minimumSize: const Size.fromHeight(45),
                      ),
                      onPressed: _isServerRunning ? null : _startServer,
                      icon: const Icon(Icons.play_arrow),
                      label: const Text("ЗАПУСТИТЬ C++ СОКЕТ"),
                    )
                  ],
                ),
              ),
            ),

            const SizedBox(height: 16),

            // Блок отправки и шифрования данных
            Card(
              color: const Color(0xFF1E293B),
              shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text("Запись зашифрованного лога (XOR Key)", style: TextStyle(fontWeight: FontWeight.bold)),
                    const SizedBox(height: 10),
                    TextField(
                      controller: _userIdController,
                      keyboardType: TextInputType.number,
                      decoration: const InputDecoration(labelText: "ID Пользователя (Ключ)", border: OutlineInputBorder()),
                    ),
                    const SizedBox(height: 10),
                    TextField(
                      controller: _msgController,
                      decoration: const InputDecoration(labelText: "Текст сообщения", border: OutlineInputBorder()),
                    ),
                    const SizedBox(height: 10),
                    ElevatedButton(
                      style: ElevatedButton.styleFrom(backgroundColor: const Color(0xFF10B981), minimumSize: const Size.fromHeight(45)),
                      onPressed: _sendAndEncrypt,
                      child: const Text("Зашифровать и записать в ячейку"),
                    ),
                  ],
                ),
              ),
            ),

            const SizedBox(height: 16),

            // Поиск по индексу-триггеру
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _searchController,
                    decoration: const InputDecoration(
                      hintText: "Поисковый триггер (напр. 'жопа')",
                      filled: true,
                      fillColor: Color(0xFF1E293B),
                      border: OutlineInputBorder(),
                    ),
                  ),
                ),
                const SizedBox(width: 8),
                IconButton.filled(
                  style: IconButton.styleFrom(backgroundColor: const Color(0xFF06B6D4)),
                  icon: const Icon(Icons.search),
                  onPressed: _searchTrigger,
                )
              ],
            ),

            const SizedBox(height: 16),

            // Терминал вывода логов
            Container(
              padding: const EdgeInsets.all(12),
              height: 180,
              decoration: BoxDecoration(
                color: Colors.black,
                borderRadius: BorderRadius.circular(8),
                border: Border.all(color: const Color(0xFF06B6D4), width: 0.5),
              ),
              child: SingleChildScrollView(
                child: Text(
                  _logOutput,
                  style: const TextStyle(fontFamily: 'monospace', color: Colors.greenAccent, fontSize: 13),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

