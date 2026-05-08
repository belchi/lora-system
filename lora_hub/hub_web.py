from flask import Flask, jsonify, render_template
from hub_db import get_readings, init_db

app = Flask(__name__)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/api/data")
def api_data():
    rows = get_readings(200)
    return jsonify([
        {"timestamp": r[0], "sender": r[1], "value": r[2], "rssi": r[3]}
        for r in rows
    ])

if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=5000, debug=False)
