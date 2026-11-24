import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;

void main() {
  runApp(const PetFeederApp());
}

class PetFeederApp extends StatelessWidget {
  const PetFeederApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Pet Feeder',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.teal),
        useMaterial3: true,
      ),
      home: const RtcTimeScreen(),
    );
  }
}

class RtcTimeScreen extends StatefulWidget {
  const RtcTimeScreen({super.key});

  @override
  State<RtcTimeScreen> createState() => _RtcTimeScreenState();
}

class _RtcTimeScreenState extends State<RtcTimeScreen> {
  static const String _rtcEndpointUrl =
      'https://simple-test-number-1-default-rtdb.firebaseio.com/pet_feeder/rtc_time.json';
  static const String _commandEndpointUrl =
      'https://simple-test-number-1-default-rtdb.firebaseio.com/pet_feeder/command.json';

  String _rtcTime = 'Waiting for RTC timestamp...';
  String _statusText = 'Connecting to Firebase...';
  String _lastCommandStatus = 'No command sent yet';
  bool _isSendingCommand = false;
  Timer? _pollTimer;

  @override
  void initState() {
    super.initState();
    _fetchRtcTime();
    _pollTimer = Timer.periodic(const Duration(seconds: 5), (_) {
      _fetchRtcTime();
    });
  }

  Future<void> _fetchRtcTime() async {
    try {
      final response = await http
          .get(Uri.parse(_rtcEndpointUrl))
          .timeout(const Duration(seconds: 10));

      if (!mounted) return;

      if (response.statusCode == 200) {
        final decoded = jsonDecode(response.body);
        setState(() {
          if (decoded != null && decoded.toString().isNotEmpty) {
            _rtcTime = decoded.toString();
            _statusText = 'Synced from Firebase RTC';
          } else {
            _rtcTime = 'No RTC data yet';
            _statusText = 'Waiting for ESP32 upload';
          }
        });
      } else {
        setState(() {
          _statusText = 'HTTP ${response.statusCode}';
        });
      }
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _statusText = 'Network error: $e';
      });
    }
  }

  Future<void> _sendFeedNowCommand() async {
    setState(() {
      _isSendingCommand = true;
      _lastCommandStatus = 'Sending Feed Now command...';
    });

    try {
      final payload = jsonEncode({
        'action': 'feed_now',
        'grams': 25,
        'requested_at': _rtcTime,
      });

      final response = await http
          .put(
        Uri.parse(_commandEndpointUrl),
        headers: {'Content-Type': 'application/json'},
        body: payload,
      )
          .timeout(const Duration(seconds: 10));

      if (!mounted) return;

      if (response.statusCode == 200) {
        setState(() {
          _lastCommandStatus = 'Sent Feed Now (25g) to ESP32!';
        });
      } else {
        setState(() {
          _lastCommandStatus = 'Failed to send (HTTP ${response.statusCode})';
        });
      }
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _lastCommandStatus = 'Error sending command: $e';
      });
    } finally {
      if (mounted) {
        setState(() {
          _isSendingCommand = false;
        });
      }
    }
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Pet Feeder — RTC & Control'),
      ),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Text(
                'ESP32 RTC Time',
                style: TextStyle(fontSize: 18, color: Colors.grey),
              ),
              const SizedBox(height: 12),
              Text(
                _rtcTime,
                style: const TextStyle(
                  fontSize: 28,
                  fontWeight: FontWeight.bold,
                ),
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 12),
              Text(
                _statusText,
                style: const TextStyle(fontSize: 14),
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 20),
              ElevatedButton.icon(
                onPressed: _fetchRtcTime,
                icon: const Icon(Icons.refresh),
                label: const Text('Refresh RTC Now'),
              ),
              const SizedBox(height: 28),
              FilledButton.icon(
                onPressed: _isSendingCommand ? null : _sendFeedNowCommand,
                icon: const Icon(Icons.pets),
                label: const Text('Feed Now (25g)'),
              ),
              const SizedBox(height: 12),
              Text(
                _lastCommandStatus,
                style: const TextStyle(fontSize: 14, fontWeight: FontWeight.w500),
                textAlign: TextAlign.center,
              ),
            ],
          ),
        ),
      ),
    );
  }
}