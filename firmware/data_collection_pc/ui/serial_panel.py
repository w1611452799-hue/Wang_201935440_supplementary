"""Serial Panel - Serial port configuration UI"""
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QGroupBox, QLabel, QComboBox, QPushButton, QSizePolicy
)
from PySide6.QtCore import Qt


class SerialPanel(QWidget):
    """Serial port configuration panel - port selection, baud rate, connect button"""

    def __init__(self):
        super().__init__()
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)

        group = QGroupBox("Serial Config")
        grid = QGridLayout(group)

        # Port selection
        grid.addWidget(QLabel("Port:"), 0, 0)
        self.combo_ports = QComboBox()
        self.combo_ports.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        grid.addWidget(self.combo_ports, 0, 1)

        # Refresh button
        self.btn_refresh = QPushButton("Refresh")
        self.btn_refresh.setFixedWidth(60)
        grid.addWidget(self.btn_refresh, 0, 2)

        # Baud rate selection
        grid.addWidget(QLabel("Baudrate:"), 1, 0)
        self.combo_baudrate = QComboBox()
        self.combo_baudrate.addItems(["9600", "19200", "38400", "57600", "115200"])
        self.combo_baudrate.setCurrentText("115200")
        self.combo_baudrate.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        grid.addWidget(self.combo_baudrate, 1, 1, 1, 2)

        # Connect button
        self.btn_connect = QPushButton("Connect")
        grid.addWidget(self.btn_connect, 2, 0, 1, 3)

        layout.addWidget(group)
        layout.addStretch()

    def get_port(self):
        return self.combo_ports.currentText()

    def get_baudrate(self):
        return self.combo_baudrate.currentText()

    def set_port_list(self, ports):
        """Set the serial port list"""
        self.combo_ports.clear()
        self.combo_ports.addItems(ports)

    def set_connected_state(self, connected):
        """Update UI state after connection change"""
        self.combo_ports.setEnabled(not connected)
        self.combo_baudrate.setEnabled(not connected)
        self.btn_refresh.setEnabled(not connected)
        self.btn_connect.setText("Disconnect" if connected else "Connect")