"""Main Window - Split layout with serial config + log on left, data tabs on right"""
from PySide6.QtWidgets import (
    QWidget, QHBoxLayout, QVBoxLayout, QSplitter, QSizePolicy
)
from PySide6.QtCore import Qt

from .serial_panel import SerialPanel
from .log_panel import LogPanel
from .tab_widget import TabWidget
from .tabs.data_collection_tab import DataCollectionTab
from .tabs.placeholder_tab import PlaceholderTab


class MainWindow(QWidget):
    """Main window - left: serial config + log, right: data collection tab"""

    def __init__(self, serial_manager, log_manager):
        super().__init__()
        self._serial_manager = serial_manager
        self._log_manager = log_manager
        self._setup_ui()
        self._connect_signals()

    def _setup_ui(self):
        self.setWindowTitle("Data Collection Tool")
        self.setGeometry(100, 100, 1200, 800)

        main_layout = QHBoxLayout(self)

        # Create splitter
        splitter = QSplitter(Qt.Orientation.Horizontal)
        main_layout.addWidget(splitter)

        # Left panel (vertical: serial config + log)
        left_widget = QWidget()
        left_layout = QVBoxLayout(left_widget)
        left_layout.setContentsMargins(0, 0, 0, 0)

        # Serial config (fixed height)
        self.serial_panel = SerialPanel()
        self.serial_panel.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)
        left_layout.addWidget(self.serial_panel)

        # Log panel (stretches to fill remaining space)
        self.log_panel = LogPanel()
        left_layout.addWidget(self.log_panel, stretch=1)

        # Right side tabs
        self.tab_widget = TabWidget()
        self.data_tab = DataCollectionTab(self._log_manager)
        self.tab_widget.add_tab(self.data_tab, "Data Collection")

        # Add to splitter
        splitter.addWidget(left_widget)
        splitter.addWidget(self.tab_widget)

        # Set left:right ratio 1:4
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 4)
        splitter.setSizes([250, 750])

        # Size policies
        left_widget.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Expanding)
        self.tab_widget.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Expanding)

    def _connect_signals(self):
        # Bind log manager
        self._log_manager.set_text_browser(self.log_panel.get_log_browser())

        # Serial panel signals
        self.serial_panel.btn_refresh.clicked.connect(self._on_refresh_ports)
        self.serial_panel.btn_connect.clicked.connect(self._on_toggle_connection)

        # Connection state changes
        self._serial_manager.connected.connect(self._on_serial_connected)
        self._serial_manager.disconnected.connect(self._on_serial_disconnected)
        self._serial_manager.error_occurred.connect(self._on_serial_error)
        self._serial_manager.data_received.connect(self._on_data_received)

        # Log buttons
        self.log_panel.btn_clear.clicked.connect(self._log_manager.clear)
        self.log_panel.btn_save.clicked.connect(self._on_save_log)

    def _on_refresh_ports(self):
        """Refresh serial port list"""
        import serial.tools.list_ports
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.serial_panel.set_port_list(ports)

    def _on_toggle_connection(self):
        """Toggle serial connection state"""
        if self._serial_manager.is_connected:
            self._serial_manager.disconnect()
        else:
            port = self.serial_panel.get_port()
            baudrate = self.serial_panel.get_baudrate()
            if not port:
                self._log_manager.warning("Please select a serial port first")
                return
            self._serial_manager.connect(port, baudrate, self._log_manager)

    def _on_serial_connected(self):
        self.serial_panel.set_connected_state(True)
        self._log_manager.info("Serial port connected")

    def _on_serial_disconnected(self):
        self.serial_panel.set_connected_state(False)
        self._log_manager.info("Serial port disconnected")

    def _on_serial_error(self, msg):
        self._log_manager.error(msg)

    def _on_data_received(self, data):
        # Forward to the active tab
        current = self.tab_widget.currentWidget()
        if hasattr(current, 'on_data_received'):
            current.on_data_received(data)

    def _on_save_log(self):
        from PySide6.QtWidgets import QFileDialog
        path, _ = QFileDialog.getSaveFileName(self, "Save Log", "", "Text Files (*.txt)")
        if path:
            self._log_manager.save_to_file(path)

    def send_data(self, data):
        """Send data over serial"""
        return self._serial_manager.send(data)

    def get_log_manager(self):
        return self._log_manager