"""Data Collection Tab - Real-time sensor data display with Start/Stop CSV recording"""
from datetime import datetime
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout,
    QGroupBox, QPushButton, QLabel, QTableWidget,
    QTableWidgetItem, QHeaderView, QFileDialog, QMessageBox,
    QAbstractItemView
)
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor


class DataCollectionTab(QWidget):
    """Data collection tab with live table, Start/Stop recording, and CSV export"""

    COLUMNS = ["#", "Time", "PT100 (°C)", "BME280 T (°C)", "Humidity (%)", "Pressure (hPa)"]

    def __init__(self, log_manager=None):
        super().__init__()
        self._log_manager = log_manager
        self._recording = False
        self._rows = []          # list of (timestamp, [val1, val2, val3, val4])
        self._line_buffer = ""   # partial line buffer for serial data

        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)

        # ── Control bar: Start / Stop / Status ──
        ctrl_group = QGroupBox("Recording Control")
        ctrl_layout = QHBoxLayout(ctrl_group)

        self.btn_start = QPushButton("▶ Start Recording")
        self.btn_start.setMinimumHeight(36)
        self.btn_start.setStyleSheet("font-weight: bold; font-size: 13px;")
        ctrl_layout.addWidget(self.btn_start)

        self.btn_stop = QPushButton("Stop Recording")
        self.btn_stop.setMinimumHeight(36)
        self.btn_stop.setEnabled(False)
        self.btn_stop.setStyleSheet("font-weight: bold; font-size: 13px;")
        ctrl_layout.addWidget(self.btn_stop)

        self.btn_clear_table = QPushButton("Clear Table")
        self.btn_clear_table.setMinimumHeight(36)
        ctrl_layout.addWidget(self.btn_clear_table)

        ctrl_layout.addStretch()

        self.lbl_status = QLabel("● Idle")
        self.lbl_status.setStyleSheet("color: gray; font-weight: bold; font-size: 13px;")
        ctrl_layout.addWidget(self.lbl_status)

        layout.addWidget(ctrl_group)

        # ── Data table ──
        table_group = QGroupBox("Sensor Data")
        table_layout = QVBoxLayout(table_group)

        self.table = QTableWidget()
        self.table.setColumnCount(len(self.COLUMNS))
        self.table.setHorizontalHeaderLabels(self.COLUMNS)
        self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.setAlternatingRowColors(True)

        # Column widths
        header = self.table.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        for col in range(2, len(self.COLUMNS)):
            header.setSectionResizeMode(col, QHeaderView.ResizeMode.Stretch)

        table_layout.addWidget(self.table)
        layout.addWidget(table_group, stretch=1)

        # ── Connect signals ──
        self.btn_start.clicked.connect(self._on_start)
        self.btn_stop.clicked.connect(self._on_stop)
        self.btn_clear_table.clicked.connect(self._on_clear)

    # ── Public API ─────────────────────────────────────────────────

    def on_data_received(self, data):
        """Called when serial data arrives. Parse CSV lines and display."""
        self._line_buffer += data

        # Process complete lines
        while "\n" in self._line_buffer:
            line, self._line_buffer = self._line_buffer.split("\n", 1)
            line = line.strip()
            if not line:
                continue
            self._process_line(line)

    # ── Internal: line parsing ─────────────────────────────────────

    def _process_line(self, line):
        """Parse one line of CSV data from the ESP32 firmware."""
        parts = [p.strip() for p in line.split(",")]

        # Skip the CSV header line from the ESP32
        if len(parts) >= 1 and parts[0].lower().startswith("pt100"):
            if self._log_manager:
                self._log_manager.info(f"Detected CSV header: {line}")
            return

        # Expect 4 numeric values
        if len(parts) < 4:
            if self._log_manager:
                self._log_manager.warning(f"Skipping malformed line: {line}")
            return

        try:
            values = [float(parts[i]) for i in range(4)]
        except ValueError:
            if self._log_manager:
                self._log_manager.warning(f"Skipping non-numeric line: {line}")
            return

        # Record with current system timestamp
        now = datetime.now()
        time_str = now.strftime("%Y-%m-%d %H:%M:%S.") + f"{now.microsecond // 1000:03d}"

        if self._recording:
            self._add_row(time_str, values)
        else:
            # When not recording, just log to console (data is visible in the Communication Log)
            pass

    def _add_row(self, time_str, values):
        """Append a row to the table and the in-memory buffer."""
        row_idx = self.table.rowCount()
        self.table.insertRow(row_idx)

        # Row number
        item_num = QTableWidgetItem(str(row_idx + 1))
        item_num.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
        self.table.setItem(row_idx, 0, item_num)

        # Timestamp
        item_time = QTableWidgetItem(time_str)
        item_time.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
        self.table.setItem(row_idx, 1, item_time)

        # Sensor values
        for col_idx, val in enumerate(values):
            item = QTableWidgetItem(f"{val:.2f}")
            item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            if val < -900.0:
                item.setForeground(QColor("red"))
                item.setText("ERROR")
            self.table.setItem(row_idx, col_idx + 2, item)

        # Auto-scroll to latest row
        self.table.scrollToBottom()

        # Store in buffer
        self._rows.append((time_str, values))

        # Update status
        self.lbl_status.setText(f"● Recording — {len(self._rows)} rows")
        self.lbl_status.setStyleSheet("color: red; font-weight: bold; font-size: 13px;")

    # ── Button handlers ────────────────────────────────────────────

    def _on_start(self):
        """Start recording data."""
        self._recording = True
        self._rows.clear()
        self.table.setRowCount(0)

        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(True)

        self.lbl_status.setText("● Recording — 0 rows")
        self.lbl_status.setStyleSheet("color: red; font-weight: bold; font-size: 13px;")

        if self._log_manager:
            self._log_manager.info("Recording started. Data will be saved when stopped.")

    def _on_stop(self):
        """Stop recording and save data to CSV."""
        self._recording = False
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)

        self.lbl_status.setText(f"● Stopped — {len(self._rows)} rows collected")
        self.lbl_status.setStyleSheet("color: gray; font-weight: bold; font-size: 13px;")

        if not self._rows:
            if self._log_manager:
                self._log_manager.warning("No data recorded. Nothing to save.")
            QMessageBox.information(self, "No Data", "No data was recorded during this session.")
            return

        # Generate default filename with current timestamp
        default_name = f"data_{datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}.csv"
        path, _ = QFileDialog.getSaveFileName(
            self, "Save Data as CSV", default_name, "CSV Files (*.csv)"
        )
        if not path:
            if self._log_manager:
                self._log_manager.info("Save cancelled by user.")
            return

        try:
            self._save_csv(path)
            if self._log_manager:
                self._log_manager.info(f"Data saved to: {path} ({len(self._rows)} rows)")
            QMessageBox.information(
                self, "Saved",
                f"Data saved successfully.\n\n"
                f"File: {path}\n"
                f"Rows: {len(self._rows)}"
            )
        except Exception as e:
            if self._log_manager:
                self._log_manager.error(f"Failed to save CSV: {e}")
            QMessageBox.critical(self, "Save Error", f"Failed to save file:\n{e}")

    def _on_clear(self):
        """Clear the table display (does not affect buffered data if currently recording)."""
        self.table.setRowCount(0)
        if not self._recording:
            self._rows.clear()
            self.lbl_status.setText("● Idle")
            self.lbl_status.setStyleSheet("color: gray; font-weight: bold; font-size: 13px;")

    def _save_csv(self, filepath):
        """Write all buffered rows to a CSV file."""
        with open(filepath, "w", encoding="utf-8", newline="") as f:
            # Header
            f.write(",".join(self.COLUMNS) + "\n")
            # Data rows
            for i, (time_str, values) in enumerate(self._rows, 1):
                f.write(f"{i},{time_str},")
                f.write(",".join(f"{v:.2f}" for v in values))
                f.write("\n")
