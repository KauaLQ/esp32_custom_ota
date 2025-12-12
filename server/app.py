from flask import Flask, request, send_from_directory, render_template_string
import os
import re

app = Flask(__name__)

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UPLOAD_FOLDER = os.path.join(BASE_DIR, "firmware")
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

HTML = """
<!DOCTYPE html>
<html>
<body>
  <h2>Upload de Firmware (.bin)</h2>
  <form method="post" action="/upload" enctype="multipart/form-data">
      <input type="file" name="file">
      <input type="submit" value="Enviar">
  </form>
  <p>Versão atual do firmware: <b>{{ version }}</b></p>
  <p>URL para o ESP32 baixar:<br>
  <code>http://{{ host }}/firmware.bin</code>
  </p>
</body>
</html>
"""

def extract_version_from_bin(path):
    print("\n=== DEBUG: Procurando FW-V no firmware ===")

    with open(path, "rb") as f:
        data = f.read()

    # Procura especificamente por FW-V:x.y.z
    match = re.search(rb"FW-V:(\d+\.\d+\.\d+)", data)

    if match:
        version = match.group(1).decode()
        print(">> Versão encontrada:", version)
        print("=== FIM ===\n")
        return version

    print(">> Nenhuma tag FW-V encontrada.")
    print("=== FIM ===\n")
    return "0.0.0"

@app.route("/")
def index():
    version_file = os.path.join(UPLOAD_FOLDER, "version.txt")
    version = "0.0.0"

    if os.path.exists(version_file):
        with open(version_file, "r") as f:
            version = f.read().strip()

    return render_template_string(HTML, version=version, host=request.host)


@app.route("/upload", methods=["POST"])
def upload():
    file = request.files.get("file")
    if not file:
        return "Nenhum arquivo enviado."

    if not file.filename.endswith(".bin"):
        return "Envie apenas um arquivo .bin."

    # salva o firmware sempre com o mesmo nome
    fw_path = os.path.join(UPLOAD_FOLDER, "firmware.bin")
    file.save(fw_path)

    # extrai versão automaticamente
    version = extract_version_from_bin(fw_path)

    with open(os.path.join(UPLOAD_FOLDER, "version.txt"), "w") as f:
        f.write(version)

    return f"Upload OK! Versão detectada automaticamente: {version}"


@app.route("/version")
def version():
    version_file = os.path.join(UPLOAD_FOLDER, "version.txt")
    if not os.path.exists(version_file):
        return "0.0.0"

    with open(version_file, "r") as f:
        return f.read().strip()


@app.route("/firmware.bin")
def firmware():
    return send_from_directory(UPLOAD_FOLDER, "firmware.bin")


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=80)