#include <pgmspace.h>
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 SD File Manager</title>
  <style>
    body { font-family: Arial; background: #f0f0f0; padding: 20px; }
    h1, h2 { color: #333; }
    input[type="text"], input[type="file"] {
      padding: 5px;
      margin: 5px;
      width: 300px;
    }
    button { padding: 8px 12px; margin: 5px; }
    ul { list-style-type: none; padding: 0; }
    li { margin: 6px 0; background: #fff; padding: 8px; border: 1px solid #ccc; cursor: pointer; }
    li:hover { background: #eee; }

    #status {
      background: #fff;
      border: 1px solid #aaa;
      padding: 10px;
      margin-top: 20px;
      font-weight: bold;
      transition: background 0.3s;
    }

    img { max-width: 100%; margin-top: 10px; border: 1px solid #999; }
  </style>
</head>
<body>

  <h1>ESP32 SD File Manager</h1>

  <div id="status">Status: Ready</div>

  <!-- Upload -->
  <h2>Upload File</h2>
  <input type="file" id="uploadFile">
  <button onclick="uploadFile()">Upload</button>

  <!-- File Manager -->
  <h2>File Management</h2>

  <div>
    <input type="text" id="deletePath" placeholder="/pictures/filename.jpg">
    <button onclick="deleteFile()">Delete</button>
  </div>

  <div>
    <input type="text" id="oldPath" placeholder="/pictures/old.jpg">
    <input type="text" id="newPath" placeholder="/pictures/new.jpg">
    <button onclick="renameFile()">Rename</button>
  </div>

  <div>
    <input type="text" id="createPath" placeholder="/pictures/newfolder">
    <button onclick="createFile()">Create</button>
  </div>

  <div>
    <input type="text" id="downloadPath" placeholder="/pictures/filename.jpg">
    <button onclick="downloadFile()">Download</button>
  </div>

  <h2>Display Image on TFT</h2>
  <input type="text" id="displayPath" placeholder="/pictures/filename.jpg">
  <button onclick="displayImage()">Display on TFT</button>

  <h2>Preview Image</h2>
  <input type="text" id="viewPath" placeholder="/pictures/filename.jpg">
  <button onclick="viewImage()">Show in Browser</button>
  <div id="imagePreview"></div>

  <h2>Files in /pictures</h2>
  <button onclick="listFiles()">List Files</button>
  <ul id="fileList"></ul>

  <h2>List Files in Custom Path</h2>
  <input type="text" id="customPath" placeholder="/any/path">
  <button onclick="listCustomPath()">List Files in Path</button>
  <ul id="customFileList"></ul>

<script>
// --- Utility: random color generator ---
function getRandomColor() {
  const letters = '0123456789ABCDEF';
  let color = '#';
  for(let i=0; i<6; i++) {
    color += letters[Math.floor(Math.random() * 16)];
  }
  return color;
}

// --- Update status with random background ---
function setStatus(msg) {
  const statusDiv = document.getElementById("status");
  statusDiv.innerText = "Status: " + msg;
  statusDiv.style.backgroundColor = getRandomColor();
}

// --- UPLOAD FILE ---
function uploadFile() {
  const fileInput = document.getElementById("uploadFile");
  if(!fileInput.files.length) return setStatus("No file selected");
  const formData = new FormData();
  formData.append("upload", fileInput.files[0]);

  fetch("/upload", { method: "POST", body: formData })
    .then(res => res.text())
    .then(msg => setStatus(msg))
    .catch(err => setStatus("Upload error: " + err));
}

// --- DELETE ---
function deleteFile() {
  const path = document.getElementById('deletePath').value;
  fetch(`/delete?path=${encodeURIComponent(path)}`)
    .then(res => res.text())
    .then(msg => setStatus(msg))
    .catch(err => setStatus("Delete error: " + err));
}

// --- RENAME ---
function renameFile() {
  const oldPath = document.getElementById('oldPath').value;
  const newPath = document.getElementById('newPath').value;
  fetch(`/rename?oldPath=${encodeURIComponent(oldPath)}&newPath=${encodeURIComponent(newPath)}`)
    .then(res => res.text())
    .then(msg => setStatus(msg))
    .catch(err => setStatus("Rename error: " + err));
}

// --- CREATE ---
function createFile() {
  const path = document.getElementById('createPath').value;
  fetch(`/create?path=${encodeURIComponent(path)}`, { method: "POST" })
    .then(res => res.text())
    .then(msg => setStatus(msg))
    .catch(err => setStatus("Create error: " + err));
}

// --- DOWNLOAD ---
function downloadFile() {
  const path = document.getElementById('downloadPath').value;
  window.open(`/download?path=${encodeURIComponent(path)}`, "_blank");
  setStatus("Download started");
}

// --- DISPLAY ON TFT ---
function displayImage() {
  const path = document.getElementById('displayPath').value;
  fetch(`/display?path=${encodeURIComponent(path)}`)
    .then(res => res.text())
    .then(msg => setStatus(msg))
    .catch(err => setStatus("Display error: " + err));
}

// --- PREVIEW ---
function viewImage() {
  const path = document.getElementById('viewPath').value;
  document.getElementById('imagePreview').innerHTML =
    `<img src="/image?path=${encodeURIComponent(path)}">`;
  setStatus("Preview loaded");
}

// --- LIST /pictures ---
function listFiles() {
  fetch("/list?dir=/pictures")
    .then(res => res.json())
    .then(files => {
      const list = document.getElementById('fileList');
      list.innerHTML = "";
      files.forEach(file => {
        const li = document.createElement('li');
        const path = `/pictures/${file.name}`;
        li.innerHTML = `<strong>${file.type.toUpperCase()}</strong>: ${file.name}`;
        li.onclick = () => {
          document.getElementById('imagePreview').innerHTML =
            `<img src="/image?path=${encodeURIComponent(path)}">`;
          fetch(`/display?path=${encodeURIComponent(path)}`)
            .then(res => res.text())
            .then(msg => setStatus("Preview + TFT OK"))
            .catch(err => setStatus("TFT error: " + err));
        };
        list.appendChild(li);
      });
      setStatus("List loaded");
    })
    .catch(err => setStatus("List error: " + err));
}

// --- LIST CUSTOM PATH ---
function listCustomPath() {
  const path = document.getElementById('customPath').value;
  fetch(`/list?dir=${encodeURIComponent(path)}`)
    .then(res => res.json())
    .then(files => {
      const list = document.getElementById('customFileList');
      list.innerHTML = "";
      files.forEach(file => {
        const li = document.createElement('li');
        li.innerHTML = `<strong>${file.type.toUpperCase()}</strong>: ${file.name}`;
        list.appendChild(li);
      });
      setStatus("Custom list loaded");
    })
    .catch(err => setStatus("List error: " + err));
}

</script>
</body>
</html>
)rawliteral";
