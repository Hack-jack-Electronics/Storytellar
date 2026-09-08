# 🧸 Storytellar

### A Screen-Free, AI-Powered Storytelling Device for Children

**Storytellar** is an embedded AI storytelling platform designed to make children's learning and entertainment more interactive without relying on a conventional screen.

Built around the **ESP32-S3**, Storytellar combines embedded firmware, wireless connectivity, high-quality audio playback, a touchscreen-based prototype interface, device authentication, and cloud/API communication to create a foundation for a dedicated **screen-free AI storytelling companion**.

The project was developed as part of the product development work at **EchoTales**, with a focus on bringing AI-powered interaction into a purpose-built embedded device rather than a conventional mobile or web application.

---

## 🚀 Project Vision

Modern children's devices are increasingly dependent on smartphones and screens.

Storytellar explores a different approach:

> **What if a child could interact with an intelligent storytelling companion without needing a screen?**

The goal is to build a physical device capable of:

- 🎙️ Voice-driven interaction
- 🤖 AI-generated and personalized stories
- 🔊 High-quality audio storytelling
- 🧠 Speech-processing capabilities
- 👨‍👩‍👧 Parent-controlled content
- 🔐 Secure device authorization
- 📡 Wireless connectivity
- 🧩 A dedicated embedded hardware experience

The current repository contains the firmware development and experimentation required to build these individual subsystems.

---

# 🏗️ System Architecture

```text
                    ┌──────────────────────────┐
                    │       Parent / App       │
                    │                          │
                    │  Wi-Fi Configuration     │
                    │  Story Selection         │
                    │  Device Provisioning     │
                    └────────────┬─────────────┘
                                 │
                            BLE / Wi-Fi
                                 │
                                 ▼
              ┌──────────────────────────────────┐
              │            ESP32-S3              │
              │                                  │
              │  ┌────────────────────────────┐  │
              │  │ Device Provisioning        │  │
              │  │ BLE + Wi-Fi                │  │
              │  └────────────────────────────┘  │
              │                                  │
              │  ┌────────────────────────────┐  │
              │  │ Authentication             │  │
              │  │ HMAC + JWT                 │  │
              │  └────────────────────────────┘  │
              │                                  │
              │  ┌────────────────────────────┐  │
              │  │ Story Management            │  │
              │  │ Story IDs / API Integration │  │
              │  └────────────────────────────┘  │
              │                                  │
              │  ┌────────────────────────────┐  │
              │  │ Audio Pipeline              │  │
              │  │ MP3 → I2S → Amplifier       │  │
              │  └────────────────────────────┘  │
              │                                  │
              │  ┌────────────────────────────┐  │
              │  │ User Interface              │  │
              │  │ TFT + Touch                 │  │
              │  └────────────────────────────┘  │
              └───────────────┬──────────────────┘
                              │
                              ▼
                    ┌─────────────────────┐
                    │   Audio Hardware    │
                    │                     │
                    │ I2S Amplifier       │
                    │      ↓              │
                    │    Speaker          │
                    └─────────────────────┘
```

---

# ✨ Key Features

## 🎧 Embedded Audio Pipeline

Storytellar uses the ESP32's **I2S peripheral** for digital audio output.

The firmware experiments with:

- MP3 decoding
- Network audio streaming
- I2S audio output
- Audio playback control
- Play / pause functionality
- Automatic stream restart
- Speaker/amplifier integration

The prototype uses the `AudioGeneratorMP3`, `AudioFileSourceICYStream`, and `AudioOutputI2S` components to create an end-to-end network-to-speaker audio pipeline.

```text
Internet / API
      │
      ▼
  MP3 Stream
      │
      ▼
 ESP32-S3
      │
 MP3 Decoder
      │
      ▼
    I2S
      │
      ▼
 I2S Amplifier
      │
      ▼
   Speaker
```

---

# 📱 TFT + Touch Interface

A touchscreen-based prototype was developed during the hardware bring-up phase.

The system integrates:

- TFT display
- XPT2046 resistive touch controller
- `TFT_eSPI`
- `XPT2046_Touchscreen`
- RGB565 image rendering
- Interactive UI buttons
- Touch-based playback control

The interface was used to validate the interaction and media pipeline before moving toward a more screen-free final product experience.

---

# 📡 BLE-Based Device Provisioning

Storytellar includes an ESP32-S3 BLE provisioning mechanism for configuring the device.

The BLE interface supports commands such as:

```text
WIFI:<SSID>,<PASSWORD>
STORY:<story_id>
STATUS
RESET
```

This allows an external device to:

1. Connect to the Storytellar through BLE.
2. Configure Wi-Fi credentials.
3. Assign a Story ID.
4. Query device status.
5. Reset the device remotely.

Once Wi-Fi and the Story ID are available, the device enters a fully configured state.

```text
             BLE
              │
              ▼
      ┌─────────────────┐
      │    ESP32-S3     │
      └────────┬────────┘
               │
       ┌───────┴────────┐
       │                │
       ▼                ▼
 Wi-Fi Credentials   Story ID
       │                │
       └───────┬────────┘
               ▼
       Device Configured
```

---

# 🔐 Device Authentication

A dedicated authentication prototype explores secure communication between the physical device and a backend service.

The implementation includes:

- Device identifiers
- Device secrets
- Timestamp-based requests
- HMAC-SHA256 request signing
- JWT session tokens
- Token expiry handling
- HTTP-based device authentication

The authentication flow is conceptually:

```text
ESP32-S3
   │
   │ Device ID
   │ Timestamp
   │ Request Body
   │ HMAC-SHA256 Signature
   ▼
Backend Authentication API
   │
   │ JWT + Expiration
   ▼
ESP32-S3
   │
   ▼
Authenticated Device Session
```

This provides the foundation for securely identifying individual Storytellar devices rather than treating every device as an anonymous client.

---

# 🌐 Wi-Fi Provisioning

The repository also contains an ESP-IDF based Wi-Fi provisioning implementation.

The ESP32 can create a temporary access point:

```text
SSID: ESP32_SETUP
Password: configureme
```

A lightweight HTTP server exposes a configuration page where the user can provide:

- Wi-Fi SSID
- Wi-Fi password
- Device claim token

The firmware then attempts to establish a station-mode Wi-Fi connection.

This creates a foundation for **first-time device onboarding** without requiring hard-coded network credentials.

---

# 🧩 Firmware Development Approach

Rather than developing the entire system as one monolithic firmware application, the repository contains multiple experimental implementations for individual subsystems.

This allowed different components to be developed and validated independently:

```text
                 Storytellar Firmware
                         │
       ┌─────────────────┼─────────────────┐
       │                 │                 │
       ▼                 ▼                 ▼
   Connectivity       Audio/UI          Security
       │                 │                 │
   ┌───┴───┐        ┌────┴────┐       ┌────┴────┐
   │ BLE   │        │ I2S     │       │ HMAC    │
   │ Wi-Fi │        │ MP3     │       │ JWT     │
   │ AP    │        │ TFT     │       │ Device  │
   └───────┘        │ Touch   │       │ Auth    │
                    └─────────┘       └─────────┘
```

This repository therefore acts as both a **working firmware base and an engineering workspace for hardware/software subsystem validation**.

---

# 🛠️ Hardware

The project is centered around an **ESP32-S3** development platform.

Typical prototype components include:

| Component | Purpose |
|---|---|
| ESP32-S3 | Main embedded controller |
| TFT Display | Prototype graphical interface |
| XPT2046 | Resistive touch controller |
| I2S Audio Amplifier | Digital audio amplification |
| Speaker | Story playback |
| Wi-Fi | Internet/backend connectivity |
| BLE | Device provisioning |
| SD Card | Local audio/media experiments |

> Hardware configuration may vary between prototype revisions.

---

# 💻 Software Stack

### Embedded

- C / C++
- ESP32-S3
- Arduino framework
- ESP-IDF
- ESP32 Wi-Fi
- Bluetooth Low Energy
- I2S
- HTTP
- NVS

### Audio

- MP3 decoding
- I2S audio output
- Network audio streaming
- Audio playback control

### Display / Interaction

- TFT_eSPI
- XPT2046_Touchscreen
- SPI
- RGB565 image rendering

### Security / Networking

- HMAC-SHA256
- JWT
- HTTP REST communication
- Wi-Fi provisioning
- BLE provisioning

### Development

- Git
- Arduino IDE
- ESP-IDF
- Serial debugging

---

# 📁 Repository Structure

The repository contains multiple firmware experiments representing different stages of development:

```text
Storytellar/
│
├── auth/
│   └── Device authentication / JWT / HMAC experiments
│
├── auth_JWT_register/
│   └── Authentication and device registration
│
├── ble_wifi_story_id/
│   └── BLE-based Wi-Fi and Story ID provisioning
│
├── wifi_connection_esp32s3_using_ble/
│   └── ESP32-S3 BLE → Wi-Fi configuration
│
├── tft_speaker_working_play_pause/
│   └── TFT + touch + I2S audio playback prototype
│
├── image_audio_working/
│   └── Image and audio integration experiments
│
├── image_jpeg/
│   └── JPEG/image handling experiments
│
├── mp3_stream_esp32s3/
│   └── Network MP3 streaming experiments
│
├── sd_card_wav_firebase_vol/
│   └── SD-card audio and volume experiments
│
├── multiple_images/
│   └── Multiple-image display experiments
│
├── multiple_wav_sd/
│   └── WAV audio playback experiments
│
├── heartbeat/
│   └── Device heartbeat experiments
│
├── buttons_vol_track_change/
│   └── Physical controls and playback interaction
│
├── main.c
│   └── ESP-IDF Wi-Fi provisioning prototype
│
└── ...
```

The repository intentionally preserves intermediate prototypes because each directory represents a specific hardware or firmware milestone.

---

# 🔄 Development Evolution

The project evolved through several hardware/software validation stages.

```text
                 Storytellar Development
                         │
                         ▼
               ┌──────────────────┐
               │ ESP32-S3 Bringup │
               └────────┬─────────┘
                        ▼
                ┌───────────────┐
                │ Audio Testing │
                │ I2S + Speaker │
                └───────┬───────┘
                        ▼
              ┌───────────────────┐
              │ MP3 Network Stream│
              └─────────┬─────────┘
                        ▼
             ┌────────────────────┐
             │ TFT + Touch UI     │
             └─────────┬──────────┘
                       ▼
             ┌────────────────────┐
             │ BLE Provisioning   │
             │ + Wi-Fi Setup      │
             └─────────┬──────────┘
                       ▼
             ┌────────────────────┐
             │ Story ID Management│
             └─────────┬──────────┘
                       ▼
             ┌────────────────────┐
             │ Device Security    │
             │ HMAC + JWT         │
             └─────────┬──────────┘
                       ▼
             ┌────────────────────┐
             │ AI Storytelling    │
             │ Product Platform   │
             └────────────────────┘
```

---

# 🎯 Engineering Challenges

Building an AI-enabled embedded product introduces constraints that do not exist in conventional software applications.

### 1. Audio on a resource-constrained device

The device must decode and continuously stream audio while maintaining responsive user interaction.

### 2. Wireless provisioning

A consumer device cannot depend on credentials being hard-coded into firmware.

Storytellar therefore explores BLE and SoftAP-based provisioning mechanisms.

### 3. Device identity

A deployed fleet requires individual device identities.

The authentication experiments introduce:

- Device IDs
- Device secrets
- Signed requests
- Timestamp validation
- JWT sessions

### 4. Embedded UI and media

The prototype combines graphics, touch input, networking and continuous audio playback on the same microcontroller.

### 5. Transition from prototype to product

The repository contains deliberately separated experiments because hardware products require individual subsystems to be validated before being integrated into a stable production firmware architecture.

---

# 📈 Future Development

The current repository represents the embedded foundation of Storytellar. Potential future development includes:

- [ ] Complete screen-free interaction model
- [ ] Voice input pipeline
- [ ] Speech-to-text integration
- [ ] AI story generation pipeline
- [ ] Text-to-speech / voice personalization
- [ ] Parent voice personalization
- [ ] Child-safe content filtering
- [ ] Production-grade secure provisioning
- [ ] Secure credential storage
- [ ] OTA firmware updates
- [ ] Power-management optimization
- [ ] Production PCB
- [ ] Enclosure and acoustic optimization
- [ ] Unified production firmware architecture
- [ ] Automated firmware testing
- [ ] Device fleet management

---

# 🧠 What This Project Demonstrates

Storytellar goes beyond simply connecting an ESP32 to an API.

The project demonstrates the engineering required to turn an AI concept into a **physical embedded product**, including:

**Embedded Systems**
- ESP32-S3 firmware
- Peripheral integration
- I2S
- SPI
- GPIO
- Memory/resource constraints

**Connectivity**
- Wi-Fi
- BLE
- HTTP
- Device provisioning

**Multimedia**
- MP3 streaming
- Audio decoding
- I2S audio output
- TFT graphics
- Touch interaction

**Security**
- HMAC-SHA256
- Device identity
- JWT authentication
- Session expiry

**Product Engineering**
- Hardware/software integration
- Prototype iteration
- Subsystem validation
- Consumer-device provisioning

---

# 👨‍💻 Role

**Founding Embedded Engineer — EchoTales**

Key contributions included:

- Designed the embedded architecture around ESP32.
- Developed firmware for audio streaming and I2S-based audio output.
- Worked on wireless provisioning using Wi-Fi and BLE.
- Developed device authentication mechanisms using HMAC and JWT.
- Integrated display, touch and audio subsystems.
- Developed firmware prototypes for speech/AI-enabled interaction.
- Worked toward a screen-free AI storytelling experience.

---

# 📚 Project Context

Storytellar is part of the broader **EchoTales** vision: an AI-powered, screen-free educational and storytelling companion for children.

The project focuses on bringing together:

```text
AI
 +
Embedded Systems
 +
Audio
 +
Wireless Connectivity
 +
Security
 =
Interactive Screen-Free Device
```

---

# ⚠️ Current Status

> **Development / Prototype**

This repository contains multiple working prototypes and subsystem experiments developed during the evolution of Storytellar.

Not every directory represents the final production firmware. Some implementations exist specifically to validate individual components such as:

- Audio playback
- TFT rendering
- Touch interaction
- BLE communication
- Wi-Fi provisioning
- Device authentication
- SD-card media handling

The repository is therefore best viewed as the **embedded development history and technical foundation of the Storytellar platform**.

---

# ⭐ Why Storytellar?

Most AI applications live inside phones, browsers, or computers.

Storytellar explores a different direction:

> **AI that lives inside a physical product.**

The project combines embedded engineering with AI-driven interaction to create technology that is designed around the user rather than around a conventional screen.

---

## 📌 Repository

**GitHub:**  
https://github.com/Hack-jack-Electronics/Storytellar

---

## 🏷️ Topics

```text
esp32
esp32-s3
embedded-systems
embedded-c
arduino
esp-idf
iot
ai
artificial-intelligence
audio-streaming
i2s
ble
wifi
tft
touchscreen
mp3
jwt
hmac
device-authentication
```

---

## 📄 License

Add the project's license here once the repository license is finalized.
