# Waypoint Ai drive

Authors: Jason Redick, Alexios Kontothanassis, Ivan Chen
Date: 2026 05 03

### Summary

Through this Limo quest, our team learned how to build an end-to-end imitation learning pipeline that teaches an autonomous vehicle to drive by mimicking a human expert. We started by tele-operating the AgileX Limo to collect demonstration data — pairing sensor inputs (LiDAR, distance estimates) with our driving actions — then used that data to train a model that predicts future waypoints from the current sensor state. We also learned how to separate planning from control: the neural network decides where the car should go (waypoints), while a PID controller handles how to get there by smoothly converting those waypoints into steering and throttle commands. Along the way, we gained practical experience with the full ML workflow (recording, training, deploying), state discretization, safety overrides like obstacle-based stops and speed caps, and the importance of working within RASTIC's constraints when operating real hardware.

### Solution Design

We built a four-stage pipeline that teaches the AgileX Limo to drive autonomously by imitating a human expert. The system runs across two networked machines: a Mac laptop handles tele-op, data logging, and offline training, while the Limo runs the verified RASTIC TCP server that exposes motor control and YDLidar X2 data. The four scripts form a linear pipeline — limo_drive.py (pure tele-op for testing the connection), limo_record.py (tele-ops and logs sensor/action pairs), limo_train.py (learns a policy offline from the JSON recordings), and limo_replay.py (executes the policy onboard the Limo with safety overrides).
During recording, a background thread runs at 20 Hz: it sends the current (velocity, steering) over TCP, receives a JSON dump of LiDAR points back, computes minimum distances in front/left/right sectors, dead-reckons a distance estimate, and appends a timestamped sample to the buffer. Decoupling this from the keyboard thread keeps the data rate constant regardless of how fast the human types. Training uses a simple tabular behavioral cloning approach rather than a neural network — for our data volume (~30 reachable states), a lookup table was sufficient and far easier to debug since we could print the entire policy and reason about every entry.
The most important design decision was choosing the right state representation. We initially included steering in the state, but "stopped + neutral steering" appeared in every recording's idle prefix and dominated the dataset, causing the learned policy to map that state to "stop forever." We fixed this by excluding steering from the state and filtering out the idle prefix before training. The final state is a 2-tuple of (distance_bin, front_obstacle_bin), and actions are bucketed into five discrete labels (stop, forward, reverse, turn_left, turn_right), with the policy storing the majority action and average (vel, steer) per state.
At runtime, limo_replay.py reads live LiDAR every 50 ms, discretizes the state using the same bin edges as training, looks up the action, and sends it to the motors. Two safety layers wrap the policy: a hard front-distance cutoff at 300 mm that forces velocity to zero regardless of policy output, and a 20-second max-runtime timeout. Unknown states default to stop rather than guessing, and all speed limits are capped well below the Limo's maximum (MAX_SPEED = 0.2) per RASTIC's constraint of never speeding up the car.


### Quest Summary
We successfully trained the Limo to drive autonomously through our obstacle course: it moved forward, executed turns accurately based on the learned policy, and used the PID-based safety layer to detect obstacles via LiDAR and stop before collisions. The full record → train → replay pipeline worked end-to-end.
The biggest challenge was state representation. Our first version included steering in the state, which caused the car to get stuck immediately — every recording started with stopped + neutral steering, so that state dominated the dataset and mapped to stop forever. Excluding steering and filtering out the idle prefix fixed it, but cost us debugging time we hadn't budgeted for. We also had to manually sync state bin edges between limo_train.py and limo_replay.py, which felt fragile.
If we did this again, we'd collect more demonstrations across varied starting positions to reduce unseen states (which currently default to stop), centralize shared constants in a config file, and eventually replace the tabular lookup with a small neural network for better generalization at bin boundaries.

### Supporting Artifacts
- [Link to video technical presentation](). Not to exceed 120s
- [Link to video demo](https://drive.google.com/file/d/1OYyTZQWQuQ7EO7ElAq1mYqbcRdc1w7JB/view?usp=drive_link). Not to exceed 120s

### Self-Assessment 

| Objective Criterion | Rating | Max Value  | 
|---------------------------------------------|:-----------:|:---------:|
| Objective One | 1 |  1     | 
| Objective Two | 1 |  1     | 
| Objective Three | 1 |  1     | 
| Objective Four | 1 |  1     | 
| Objective Five | 1 |  1     | 
| Objective Six | 1 |  1     | 
| Objective Seven | 1 |  1     | 


### AI and Open Source Code Assertions

- We have documented in our code readme.md and in our code any software that we have adopted from elsewhere
- We used AI for coding and this is documented in our code as indicated by comments "AI generated" 


