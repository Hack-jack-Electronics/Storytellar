# 🧸 Storytellar

### Screen-Free AI-Enabled Storytelling Device

**Storytellar** is a screen-free storytelling device built around the **ESP32-S3**. The ESP32 acts as the embedded client, communicating with a remote server over Wi-Fi to fetch data and interact with external AI services. AI processing is performed externally, not on the ESP32.

The project combines embedded firmware, wireless connectivity, audio playback, device provisioning, authentication, and server communication into a dedicated physical AI interface.

---

## 🏗️ Architecture

```text
        External AI Services
                ▲
                │ API
                ▼
        ┌──────────────┐
        │    Server    │
        │  Data + APIs │
        └──────┬───────┘
               │ HTTP / Wi-Fi
               ▼
        ┌──────────────┐
        │   ESP32-S3   │
        │              │
        │ Wi-Fi / BLE  │
        │ Audio / I2S  │
        │ TFT / Touch  │
        │ Authentication│
        └──────┬───────┘
               │
               ▼
            Speaker
```

---

## ✨ Key Features

- 🎧 **I2S Audio** — MP3 playback and network audio streaming.
- 📡 **Wi-Fi Communication** — HTTP requests to the remote server for data and content.
- 🔵 **BLE Provisioning** — Configure Wi-Fi credentials and Story IDs.
- 🔐 **Device Authentication** — HMAC-SHA256 request signing and JWT-based authentication.
- 📱 **TFT + Touch** — Prototype interface using TFT_eSPI and XPT2046.
- 🌐 **Server Integration** — Embedded device communicates with backend APIs for data retrieval and external service integration.
- 🤖 **External AI Integration** — AI functionality is handled through remote services rather than local inference.

---

## 🔄 Data Flow

```text
ESP32-S3
    │
    │ HTTP Request
    ▼
  Server
    │
    │ Data / AI API
    ▼
External Service
    │
    │ Response
    ▼
  Server
    │
    │ HTTP Response
    ▼
ESP32-S3
    │
    ▼
Audio / Device Action
```

---

## 🛠️ Hardware

| Component | Purpose |
|---|---|
| ESP32-S3 | Main controller |
| TFT Display | Prototype UI |
| XPT2046 | Touch controller |
| I2S Amplifier | Audio output |
| Speaker | Story playback |
| Wi-Fi | Server connectivity |
| BLE | Device provisioning |
| SD Card | Media experiments |

---

## 💻 Tech Stack

**Embedded:** ESP32-S3, C/C++, Arduino, ESP-IDF, I2S, SPI, GPIO

**Connectivity:** Wi-Fi, BLE, HTTP, REST APIs

**Audio:** MP3, I2S, network streaming

**Security:** HMAC-SHA256, JWT, device authentication

**Display:** TFT_eSPI, XPT2046_Touchscreen

**AI:** External AI APIs, server-mediated AI integration

---

## 👨‍💻 My Contribution

As a **Founding Embedded Engineer at EchoTales**, I worked on:

- ESP32-S3 firmware development and hardware integration.
- I2S-based audio playback and network audio streaming.
- Wi-Fi and BLE-based device provisioning.
- Server communication and API-based data retrieval.
- Device authentication using HMAC and JWT.
- TFT and touch interface integration.
- Integration of the embedded device with externally hosted AI services.

---

## 🔗 Repository

[GitHub — Storytellar](https://github.com/Hack-jack-Electronics/Storytellar)
[Prototype-Video](https://youtube.com/shorts/uQehqIdOT2Q)
