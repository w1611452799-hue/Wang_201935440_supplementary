"""Log Panel - Communication log display"""
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout,
    QGroupBox, QTextBrowser, QPushButton
)
from PySide6.QtGui import QFont


class LogPanel(QWidget):
    """Log panel - displays communication logs with color-coded entries"""

    def __init__(self):
        super().__init__()
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)

        group = QGroupBox("Communication Log")
        vbox = QVBoxLayout(group)

        # Log display area
        self.log_browser = QTextBrowser()
        self.log_browser.setFont(QFont("Consolas", 11))
        vbox.addWidget(self.log_browser)

        # Control buttons
        btn_layout = QHBoxLayout()
        self.btn_clear = QPushButton("Clear Log")
        self.btn_save = QPushButton("Save Log")
        btn_layout.addWidget(self.btn_clear)
        btn_layout.addWidget(self.btn_save)
        btn_layout.addStretch()
        vbox.addLayout(btn_layout)

        layout.addWidget(group)

    def get_log_browser(self):
        return self.log_browser

    def clear(self):
        self.log_browser.clear()