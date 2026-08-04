"""Helper utilities"""
from datetime import datetime


def format_timestamp():
    """Return current timestamp string HH:MM:SS.mmm"""
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def bytes_to_hex(data):
    """Convert bytes to space-separated hex string"""
    if isinstance(data, bytes):
        return ' '.join(f'{b:02X}' for b in data)
    return data


def hex_to_bytes(hex_str):
    """Convert hex string to bytes"""
    hex_str = hex_str.replace(' ', '').replace('\n', '')
    return bytes.fromhex(hex_str)