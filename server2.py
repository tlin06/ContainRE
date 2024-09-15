from flask import Flask, render_template, jsonify, request
from datetime import datetime
import threading
import time

app = Flask(__name__)

# Store the latest signal strength and timestamp
signal_history = []
lock = threading.Lock()

@app.route('/')
def index():
    return render_template('index3.html')

@app.route('/update_signal', methods=['GET', 'POST'])
def update_signal():
    station = request.form.get('station')
    kerberos = request.form.get('kerberos')
    id = request.form.get('id')
    io = request.form.get('io')
    print(kerberos, station, id, io)
    print(type(kerberos), type(station), type(id), type(io))
    if id:
        with lock:
            if io == 'in':
                signal_history.append({
                    'kerberos': kerberos,
                    'station': station,
                    'id': id,
                    'timestring': datetime.now().strftime("%Y-%m-%d %H:%M:%S"), 
                    'timestamp': time.time() * 1000 # Convert to milliseconds
                })
            elif io == 'out':
                for i in range(len(signal_history)):
                    if signal_history[i]['id'] == str(id):
                        del signal_history[i]
            else:
                return "Invalid io value", 100
        return "Signal updated", 200
    return "No data received", 400

@app.route('/data')
def get_data():
    with lock:
        return jsonify(signal_history)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)