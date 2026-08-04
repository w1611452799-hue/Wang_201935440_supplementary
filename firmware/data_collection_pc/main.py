"""Data Collection Application - Serial port data acquisition tool"""
import sys
import os
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QIcon

from core.serial_manager import SerialManager
from core.log_manager import LogManager
from ui.main_window import MainWindow


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("DataCollection")

    # Set application icon
    logo_path = os.path.join(os.path.dirname(__file__), "logo.png")
    if os.path.exists(logo_path):
        app.setWindowIcon(QIcon(logo_path))

    # Initialize core modules
    serial_manager = SerialManager()
    log_manager = LogManager()

    # Create main window
    window = MainWindow(serial_manager, log_manager)
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
