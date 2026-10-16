# AeroNex laptop dashboard

## Start the dashboard

1. Download `PosturePal_Laptop_Dashboard_Updated.zip` and **extract the whole ZIP**. Do not run the `.bat` from inside the ZIP, and do not download only the `.bat`; it needs the neighboring files, including `team.jpeg` and `robotics-club-logo.jpeg`.
2. Connect the laptop to the ESP8266 Wi-Fi network `AerNex` (password `Posture123`).
3. Open the extracted folder and double-click `start-dashboard.bat` (or the top-level `START_POSTUREPAL_DASHBOARD.bat` if supplied beside the folder).
4. Keep the black helper window open. Your browser should open at `http://127.0.0.1:8787/`. If it does not, enter that address manually. Press Ctrl+C in the helper window to stop the server, then close that window.
5. Click **Enable sound**, calibrate your normal posture, then click **Start camera** and allow camera access when prompted.

The refreshed dashboard takes visual cues from the Robotics Club MMMUT site's cinematic dark palette, expressive headings, and team storytelling. The MMMUT Robotics Club crest appears at the upper-right of the header, and the supplied AeroNex team image remains in the team section with its full wide composition visible. Live readings refresh about 2–3 times per second, while the trend chart samples once per second. A brief missed response shows a retry state and keeps the last valid readings; the dashboard reports offline only after no successful sensor response for about four seconds. The launcher checks for required files and leaves an error visible if something is missing. It starts the PowerShell helper in the same visible window; the helper tries to open your browser and prints the local address if Windows cannot do that automatically. The helper uses Windows PowerShell already included with Windows; Node.js is not needed. It listens only on this laptop at `127.0.0.1:8787` and forwards tilt data and calibration/reset requests to the ESP8266. Camera video stays in the browser on the laptop; the helper does not record or send video to the ESP and does not request microphone access.

The phone can still open `http://192.168.4.1` for sensor readings and controls. That direct ESP page does not open a camera because browsers require camera access to come from a secure browser context.

## Upload the firmware

In Arduino IDE, open `firmware/PosturePal_Dashboard_teri/PosturePal_Dashboard_teri.ino`, select the ESP8266 board and serial port, and upload. This file is byte-for-byte the same as the latest saved `PosturePal_Dashboard_teri(1).ino`. It retains the PosturePal access point, MPU6050 logic, LEDs, IR reminder acknowledgement, calibration, and reset.

## Prototype pins

- MPU6050 SDA `D2`, SCL `D1`
- Green, yellow, red LEDs: `D5`, `D6`, `D7`
- IR sensor output: `D0`

The firmware and dashboard source were inspected and the PowerShell helper was started locally. A browser-to-helper connection, webcam permission, and live ESP/hardware operation were not verified on physical hardware.
