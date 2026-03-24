🧠M.I.N.D Companion 
Mental Integrated Neural Defender Companion

Overview
There are many children around the world who struggle with negative thoughts, stress, and mental health challenges. One of the biggest issues is that it can be very difficult for them to express how they feel, and by the time someone notices, it may already be too late. 

Our goal with the M.I.N.D Companion is to provide a system that can step in before the user has to ask for help. This device acts as a real-time support system that monitors physical and behavioural signals to detect early signs of distress.

The idea is simple: give children something they can rely on at any time of day. By combining sensors, smart logic, and an IoT dashboard, the device can respond immediately with helpful actions such as guiding breathing, motivational audio, or alerts to guardians.

Project Purpose
The purpose of this project is to create a wearable companion that:
- Monitors the user's physical and emotional state in real time
- Detects early signs of stress, discomfort, or irregular behavior
- Provides immediate feedback (visual, audio, or physical) to help the user cope
- Allows communication between the user and the device through voice commands
- Sends important data to a dashboard so parents/guardians can stay informed

Instead of reacting after a problem occurs, this system focuses on early detection and prevention.

System Inputs
The device uses multiple inputs to understand the user's condition:

Microphone(Voice Commands)

The microphone allows the user to interact with the device using preset voice commands. 
For example:
- The user can request breathing exercises
- The device responds by activation guided breathing patterns

Camera (Facial Detection)

The camera is used to monitor facial expressions.
- The system can detect emotional cues from the user's face
- The vibration motor reminds the user (every hour) to show their face
- This reminder is disabled when the system detects the user is sleeping

Sleep detection System

This consists of three components working together:
- RTC (Real-Time Clock) -> tracks time
- MAX30102 (Heart Rate Sensor) -> monitors pulse
- MPU6050 (Motion Sensor) -> detects movement

These sensors help determine:
- Sleep quality
- Movement patterns
- Whether the user is resting properly or not

GSR Sensor (Stress Detection)

The Galvanic Skin Response sensor measures skin conductivity.
- Higher conductivity usually indicates sweating
- This can be linked to stress or anxiety
- The system uses this data to classify stress levels

Emergency Button
- A physical panic button for immediate help
- Sends an alert directly to the dashboard
- Can be used if the user is overwhelmed or in distress

System Outputs

Once the system processes the inputs, it responds using the following outputs:

Guided Breathing LED
- 4 LEDs act as a breathing guide
- Fade in and out smoothly
- Activated by:
    - Voice commands
    - Abnormal readings from sensors

TFT Display
- Shows real-time system data

Displays:
- Heart rate
- Stress level
- Sleep status
Also provides motivational messages for the user

Speaker
- Plays audio messages or motivational quotes
- Activated when higher stress levels are detected
- Helps calm or guide the user

Vibration Motor
-Provides physical alerts
-Vibrates:
    -Every hour (to prompt facial check)
    -When specific conditions are triggered

GPIO / Pin Configurations:
Below is a simplified explanation of how each component is connected:

Sensors & Inputs
- GPIO 3 -> GSR sensor (analog signal)
- GPIO 47(SDA) -> I2C data line (MPU6050, MAX30102, RTC)
- GPIO 48(SCL) -> I2C clock line (MPU6050, MAX30102, RTC)
- GPIO 41 -> Emergency button
- GPIO 45 -> Microphone SCK
- GPIO 38 -> Microphone WS
- GPIO 19 -> Microphone SD

Display(TFT)
- GPIO 39 -> Chip Select (CS)
- GPIO 1 -> Clock (SCK)
- GPIO 2 -> MOSI (Data)
- GPIO 40 -> Reset
- GPIO 42 -> Data/Command

Outputs
- GPIO 20 -> LED strip (via PNP transistor)
- GPIO 46 -> Vibration motor

Speaker(Amplifier)
- GPIO 14 -> Audio input(DIN)
- GPIO 21 -> Bit Clock(BCLK)
- GPIO 0 -> Left/Right Clock(LRC)

How the System Works
1. Sensors continuously collect data (heart rate, movement, stress, etc.)
2. The ESP32 processes this data in real time
3. The system determines the user's current state
4. If something unusual is detected:
    - LEDs activate (breathing guide)
    - Speaker plays audio
    - Vibration alerts the user
5. Data is sent to the IoT dashboard via MQTT
6. Parents/guardians can monitor updates remotely

Why This Project Matters
This project focuses on early intervention. Instead of waiting for a child to ask for help, the system provides support automatically based on real-time data.

By combining hardware, software, and IoT, the M.I.N.D Companion creates a reliable and responsive support system that can make a real different in everyday life.