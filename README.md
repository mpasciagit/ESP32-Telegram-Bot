ESP32 Telegram Control Bot (Upgrade)
This project is a high-level upgrade of the wifi_manager_pro_v2 (Reference Link). While the base system handles the complex heavy lifting of WiFi persistence and provisioning, this version introduces a Remote Control Layer via the Telegram Bot API.

🚀 The Upgrade: What's New?
Approximately 90% of the core logic (NVS Storage, WiFi State Machine, Captive Portal, and Hardware Scanning) remains identical to wifi_manager_pro_v2. For detailed information on the underlying network management, please refer to that project's documentation.

The core addition in this repository is the Telegram Integration Module, which transforms the ESP32 into a remotely manageable IoT node without requiring port forwarding or a public IP.

📦 New Module: app_telegram.c / .h
This is the heart of the upgrade. It handles the secure asynchronous communication between the ESP32 and the Telegram Servers.

Key Features:
. Asynchronous Polling: Non-blocking getUpdates implementation that runs as a dedicated FreeRTOS task.
. Session-Key Security: A dynamic 6-character hex key generated at boot. Only users providing this key via /start or /help gain Admin privileges for that session.
. Remote Commands:
    . /status: Real-time RSSI, IP, and System Uptime.
    . /scan: Triggers a hardware scan and returns the top 10 visible networks.
    . /test SSID,PASS: Remotely instructs the ESP32 to attempt a new connection.
    . /reboot: Secure remote hardware reset.

🛠 System Architecture (State Machine)
The system relies on a strictly defined State Machine to ensure the Bot only attempts to connect when the internet stack is fully ready.

Fragmento de código
graph TD
    classDef state fill:#f9f,stroke:#333,stroke-width:2px;
    classDef decision fill:#e1f5fe,stroke:#01579b,stroke-width:2px;

    S_BOOT[SYSTEM_STATE_BOOT] --> S_SCAN[SYSTEM_STATE_SCANNING]
    
    S_SCAN --> Q_MATCH{Known Network?}
    Q_MATCH -- YES --> S_TRY[SYSTEM_STATE_TRY_STA]
    Q_MATCH -- NO --> S_PROV[SYSTEM_STATE_PROVISIONING]

    S_PROV -->|New Credentials| S_SCAN
    
    S_TRY --> Q_OK{Connected?}
    Q_OK -- YES --> S_CONN[SYSTEM_STATE_CONNECTED]
    Q_OK -- NO --> S_DISC[SYSTEM_STATE_DISCONNECTED]

    S_CONN -->|Launch| BOT_TASK((Telegram Bot Task))
    S_DISC -->|Retry 3x| S_TRY
    S_DISC -->|Fail| S_SCAN

    class S_BOOT,S_SCAN,S_PROV,S_TRY,S_CONN,S_DISC state;
    class Q_MATCH,Q_OK decision;

🔧 Installation & Setup
1. Base Requirements: Same as wifi_manager_pro_v2 (ESP-IDF v5.x).
2. Bot Token: You must obtain a BOT_TOKEN from @BotFather.
3. Configuration:
    . Open main/app_telegram.h.
    . Insert your BOT_TOKEN.
    . Insert your MY_CHAT_ID (to ensure only you receive critical boot alerts).
4. Flash: idf.py build flash monitor.

🛡 Security Note
This project uses Session Keys. Every time the ESP32 reboots, a new key is generated. To interact with the bot, you must send the command /help or /start to see the current key (if you are the Admin) or provide it if you are a new user. This prevents unauthorized users from rebooting or reconfiguring your device.