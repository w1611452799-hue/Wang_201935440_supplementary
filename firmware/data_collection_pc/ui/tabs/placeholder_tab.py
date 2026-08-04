"""Placeholder Tab - Reserved for future extensions"""
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QLabel
)
from PySide6.QtCore import Qt


class PlaceholderTab(QWidget):
    """Placeholder tab - for future feature development"""

    def __init__(self, title="Reserved"):
        super().__init__()
        layout = QVBoxLayout(self)
        label = QLabel(f"{title}\n\nUnder development...")
        label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        label.setStyleSheet("color: gray; font-size: 14px;")
        layout.addWidget(label)

    def on_data_received(self, data):
        """Reserved interface - override in subclass to handle received data"""
        pass