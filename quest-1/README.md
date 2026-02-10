# Controllable 4 legged walker

Authors: Jason Redick, Alexios Kontothanassis, Ivan Chen
Date: 2026-02-09

### Summary

In this quest, we built a four-legged walker using an ESP32 and two servos that moves at fixed time intervals. The ESP32 keeps accurate time using timers and updates a display every second to show how long until the next walk. When the timer reaches zero, the servos move back and forth for a set number of steps to push the walker forward, then stop. The time interval and number of steps can be changed dynamically using buttons or serial input, rather than being hard-coded.

### Solution Design

Describe how you built your solution. Use hardware and software
diagrams, show data flow, include control flow or state charts if
appropriate.

The walker was built using two small breadboards with the servos sandwiched between them. The ESP32 was mounted on the bottom breadboard, while the I2C alphanumeric display was mounted on the top breadboard. Two servos were connected to separate GPIO pins on the ESP32 and powered from the board. Bent paper clips and cardboard were attached to the servo horns to form the 2 legs, allowing the back-and-forth servo motion to translate into forward movement.
<p align="center">
<img src="./images/ece444.png" width="50%">
</p>
<p align="center">
</p>

### Quest Summary
Summarize your results and call out any challenges or things you would
have done differently.

### Investigative questions 
1)The time interval can be made dynamic by using a single button with multi-click detection. The ESP32 measures the time between button presses and interprets double or triple clicks within a short time window as different interval settings. This allows the user to change the walking interval without hard-coding values or using the serial console. The selected interval is stored in a variable and applied immediately by the timer logic.

2)If the servos were attached to two different microcontrollers, they could share a single button or trigger signal. When the button is pressed, it sends a signal to a GPIO pin on both microcontrollers at the same time. Each microcontroller detects the signal and starts its servo routine simultaneously, keeping the servos synchronized and moving the walker forward together.

### Supporting Artifacts
- Link to video technical presentation. Not to exceed 120s
- Link to video demo. Not to exceed 120s

### Self-Assessment 

| Objective Criterion | Rating | Max Value  | 
|---------------------------------------------|:-----------:|:---------:|
| Objective One |  |  1     | 
| Objective Two |  |  1     | 
| Objective Three |  |  1     | 
| Objective Four |  |  1     | 
| Objective Five |  |  1     | 
| Objective Six |  |  1     | 
| Objective Seven |  |  1     | 



### AI and Open Source Code Assertions

- We have documented in our code readme.md and in our code any software that we have adopted from elsewhere
- We used AI for coding and this is documented in our code as indicated by comments "AI generated" 


