"""Log Manager - Unified log formatting with colored output"""
from datetime import datetime


class LogManager:
    """Log manager providing unified log format and color-coded output"""

    COLOR_MAP = {
        "INFO": "#808080",    # gray
        "SEND": "#1E90FF",    # dodger blue
        "RECV": "green",      # green
        "ERROR": "red",       # red
        "WARNING": "orange",  # orange
    }

    def __init__(self, text_browser=None):
        self._text_browser = text_browser

    def set_text_browser(self, text_browser):
        """Set the log display widget"""
        self._text_browser = text_browser

    def log(self, message, msg_type="INFO"):
        """Append a log entry"""
        if self._text_browser is None:
            return

        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        color = self.COLOR_MAP.get(msg_type, "black")
        formatted_msg = f'<span style="color: {color}">[{timestamp}] [{msg_type}] {message}</span>'

        self._text_browser.append(formatted_msg)
        # Auto-scroll to bottom
        cursor = self._text_browser.textCursor()
        cursor.movePosition(cursor.MoveOperation.End)
        self._text_browser.setTextCursor(cursor)

    def info(self, message):
        self.log(message, "INFO")

    def send(self, message):
        self.log(message, "SEND")

    def recv(self, message):
        self.log(message, "RECV")

    def error(self, message):
        self.log(message, "ERROR")

    def warning(self, message):
        self.log(message, "WARNING")

    def clear(self):
        """Clear all log entries"""
        if self._text_browser:
            self._text_browser.clear()

    def save_to_file(self, filepath):
        """Save log content to file"""
        if self._text_browser is None:
            return False
        try:
            with open(filepath, "w", encoding="utf-8") as f:
                f.write(self._text_browser.toPlainText())
            return True
        except Exception:
            return False