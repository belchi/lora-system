import sqlite3
import os

DB_PATH = os.path.join(os.path.dirname(__file__), "hub_data.db")

def init_db():
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS readings (
                id        INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT    NOT NULL,
                sender    TEXT    NOT NULL,
                value     REAL    NOT NULL,
                rssi      INTEGER NOT NULL
            )
        """)
        conn.commit()

def insert_reading(timestamp: str, sender: str, value: float, rssi: int):
    with sqlite3.connect(DB_PATH) as conn:
        conn.execute(
            "INSERT INTO readings (timestamp, sender, value, rssi) VALUES (?, ?, ?, ?)",
            (timestamp, sender, value, rssi)
        )
        conn.commit()

def get_readings(limit: int = 200):
    with sqlite3.connect(DB_PATH) as conn:
        cur = conn.execute(
            "SELECT timestamp, sender, value, rssi FROM readings ORDER BY id DESC LIMIT ?",
            (limit,)
        )
        return cur.fetchall()
