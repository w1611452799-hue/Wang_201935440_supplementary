"""Serial Manager - Thread-safe serial communication"""
import threading
from PySide6.QtCore import QObject, Signal, QThread


class SerialWorker(QObject):
    """Serial worker thread, handles low-level serial I/O"""

    connected = Signal()
    disconnected = Signal()
    error_occurred = Signal(str)
    data_received = Signal(str)
    data_sent = Signal(str)

    def __init__(self):
        super().__init__()
        self.serial_port = None
        self.is_running = False
        self._lock = threading.Lock()

    def open_serial(self, port_name, baud_rate):
        """Open serial port"""
        import serial
        try:
            self.serial_port = serial.Serial(
                port=port_name,
                baudrate=int(baud_rate),
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1
            )
            self.is_running = True
            self.connected.emit()
        except Exception as e:
            self.error_occurred.emit(f"Failed to open serial port: {str(e)}")

    def close_serial(self):
        """Close serial port"""
        self.is_running = False
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
        self.disconnected.emit()

    def send_data(self, data):
        """Send data over serial"""
        if not self.is_running or not self.serial_port or not self.serial_port.is_open:
            return False
        try:
            with self._lock:
                self.serial_port.write(data.encode('utf-8'))
            self.data_sent.emit(data.strip())
            return True
        except Exception as e:
            self.error_occurred.emit(f"Failed to send data: {str(e)}")
            return False

    def read_data(self):
        """Read serial data (called by timer)"""
        if not self.is_running or not self.serial_port or not self.serial_port.is_open:
            return
        try:
            if self.serial_port.in_waiting > 0:
                data = self.serial_port.read(self.serial_port.in_waiting)
                if data:
                    decoded = data.decode('utf-8', errors='ignore')
                    self.data_received.emit(decoded)
        except Exception as e:
            self.error_occurred.emit(f"Failed to read data: {str(e)}")


class SerialManager(QObject):
    """Serial communication manager - unified external interface"""

    connected = Signal()
    disconnected = Signal()
    error_occurred = Signal(str)
    data_received = Signal(str)
    data_sent = Signal(str)

    def __init__(self):
        super().__init__()
        self._worker = None
        self._thread = None
        self._is_connected = False
        self._timer = None

    @property
    def is_connected(self):
        return self._is_connected

    def connect(self, port_name, baud_rate, log_manager=None):
        """Connect to serial port"""
        if self._is_connected:
            return False

        self._thread = QThread()
        self._worker = SerialWorker()
        self._worker.moveToThread(self._thread)

        self._worker.connected.connect(self._on_connected)
        self._worker.disconnected.connect(self._on_disconnected)
        self._worker.error_occurred.connect(lambda e: self._on_error(e, log_manager))
        self._worker.data_received.connect(lambda d: self._on_data_received(d, log_manager))
        self._worker.data_sent.connect(lambda d: self._on_sent(d, log_manager))

        from PySide6.QtCore import QTimer
        self._timer = QTimer()
        self._timer.timeout.connect(self._worker.read_data)

        self._thread.started.connect(
            lambda: self._worker.open_serial(port_name, baud_rate)
        )
        self._thread.start()
        self._timer.start(10)
        return True

    def disconnect(self):
        """Disconnect from serial port"""
        if self._worker:
            self._worker.close_serial()
        if self._timer:
            self._timer.stop()
        if self._thread:
            self._thread.quit()
            self._thread.wait()
        self._is_connected = False

    def send(self, data):
        """Send data over serial"""
        if self._worker and self._is_connected:
            return self._worker.send_data(data)
        return False

    def _on_connected(self):
        self._is_connected = True
        self.connected.emit()

    def _on_disconnected(self):
        self._is_connected = False
        self.disconnected.emit()

    def _on_error(self, msg, log_manager):
        self.error_occurred.emit(msg)
        if log_manager:
            log_manager.error(msg)

    def _on_data_received(self, data, log_manager):
        self.data_received.emit(data)
        if log_manager:
            log_manager.recv(data)

    def _on_sent(self, data, log_manager):
        self.data_sent.emit(data)
        if log_manager:
            log_manager.send(data)