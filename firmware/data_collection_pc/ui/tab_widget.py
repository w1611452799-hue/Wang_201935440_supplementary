"""Tab Widget - Right-side tab container"""
from PySide6.QtWidgets import QTabWidget


class TabWidget(QTabWidget):
    """Tab widget container"""

    def __init__(self):
        super().__init__()
        self.setTabPosition(QTabWidget.TabPosition.North)

    def add_tab(self, widget, title):
        """Add a new tab"""
        self.addTab(widget, title)