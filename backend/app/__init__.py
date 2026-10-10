from flask import Flask, jsonify
from flask_cors import CORS


def create_app():
    app = Flask(__name__)
    CORS(app)  

    from app.api.readings import bp as readings_bp
    app.register_blueprint(readings_bp)

    from app.realtime import socketio
    socketio.init_app(app)

    @app.get("/api/health")
    def health():
        return jsonify({"status": "ok"})

    return app
