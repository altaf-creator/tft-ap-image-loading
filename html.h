const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>ESP32 Image Uploader</title>
<style>
  body {
    height: 100%;
    width: 100%;
    margin: 0;
  }
  canvas { border: 1px solid #ccc; margin-top: 10px; }
  .container {
    display: flex;
    height: 100%;
    flex-direction: row;
  }
  .container > div {
    flex: 1;
    text-align: center;
  }
</style>
</head>
<body>
  <h1>Configuration</h1>
  <div class="container">
    <div>
      <h2>Preview</h2>
      <canvas id="canvas" width="240" height="240"></canvas><br>
    </div>
    <div style="text-align: left;">
      <h2>Upload Image (240×240) as Background</h2>
      <input type="file" id="fileInput" accept="image/*"><br>
      <button id="uploadBtn">Upload Image to ESP32</button>
      <h2>Sync Time</h2>
      <button id="syncTimeBtn">Synchronise Time</button>
    </div>
  </div>
<script>
const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');
const fileInput = document.getElementById('fileInput');
const uploadBtn = document.getElementById('uploadBtn');
const syncBtn = document.getElementById('syncTimeBtn');

let bitmapData = null;

fileInput.addEventListener('change', async (e) => {
  const file = e.target.files[0];
  if (!file) return;

  const img = new Image();
  img.src = URL.createObjectURL(file);
  await img.decode();

  // Resize to 240x240
  ctx.clearRect(0, 0, 240, 240);
  ctx.drawImage(img, 0, 0, 240, 240);

  // Extract raw pixel data
  const imgData = ctx.getImageData(0, 0, 240, 240).data;

  // Convert to RGB565 bitmap
  const buf = new Uint16Array(240 * 240);
  for (let i = 0, j = 0; i < imgData.length; i += 4, j++) {
    const r = imgData[i] >> 3;
    const g = imgData[i + 1] >> 2;
    const b = imgData[i + 2] >> 3;
    buf[j] = (r << 11) | (g << 5) | b;
  }

  bitmapData = buf;
  console.log("Bitmap ready:", buf.length, "pixels");
});

uploadBtn.addEventListener('click', async () => {
  if (!bitmapData) return alert("Upload an image first.");

  // Convert to raw byte array
  const uint8 = new Uint8Array(bitmapData.buffer);

  try {
    const res = await fetch("http://192.168.4.1/upload", {
      method: "POST",
      headers: { "Content-Type": "application/octet-stream" },
      body: uint8,
    });
    const text = await res.text();
    alert("Response: " + text);
  } catch (err) {
    alert("Failed to send: " + err);
  }
});

syncBtn.addEventListener('click', async () => {
  try {
    const res = await fetch("http://192.168.4.1/settime", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: "t=" + Date.now()
    });
    const text = await res.text();
    alert("Response: " + text);
  } catch (err) {
    alert("Failed to send: " + err);
  }
});

</script>
</body>
</html>
)rawliteral";
