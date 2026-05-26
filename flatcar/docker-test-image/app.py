from flask import Flask
import socket
import datetime

app = Flask(__name__)

@app.route("/")
def home():
    hostname = socket.gethostname()
    now = datetime.datetime.now()

    return f"""
    <html>
    <head>
        <title>Flatcar Test Server</title>
        <style>
            body {{
                background: #0f172a;
                color: #e2e8f0;
                font-family: Arial;
                padding: 40px;
            }}

            .card {{
                background: #1e293b;
                padding: 30px;
                border-radius: 12px;
                max-width: 700px;
                margin: auto;
                box-shadow: 0 0 20px rgba(0,0,0,0.4);
            }}

            h1 {{
                color: #38bdf8;
            }}

            code {{
                color: #facc15;
            }}
        </style>
    </head>
    <body>
        <div class="card">
            <h1>🚀 Flatcar Docker Test</h1>

            <p>Container draait succesvol.</p>

            <p><b>Hostname:</b> <code>{hostname}</code></p>

            <p><b>Tijd:</b> <code>{now}</code></p>

            <hr>

            <p>✅ Flatcar werkt</p>
            <p>✅ Docker werkt</p>
            <p>✅ Networking werkt</p>
            <p>✅ Container startup werkt</p>

        </div>
    </body>
    </html>
    """

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=80)