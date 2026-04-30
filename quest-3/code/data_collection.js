const fs = require('fs');
const dgram = require('dgram');
const mqtt = require('mqtt');

const server = dgram.createSocket('udp4');

const PORT = 41234;

// // MQTT Configuration
// const MOCAP_TOPIC = 'rb/Group_1_data'; // Update with the actual topic
// const mqttClient = mqtt.connect('mqtt://rasticvm.internal', { 
//     clientId: 'alexiosk',
// });

// Array to store incoming data before writing to JSON
const sensorDataLog = [];
const OUTPUT_FILE = 'new_arm_data.json';

// Helper function to handle saving logic
function logData(source, rawData) {
  let parsedData = rawData.toString();
  // Attempt to parse JSON if the sensors send stringified JSON
  try {
    parsedData = JSON.parse(parsedData);
  } catch (e) {
    // Keep as string if it's not JSON
  }

  sensorDataLog.push({
    timestamp: new Date().toISOString(),
    source: source,
    data: parsedData
  });
}

// Periodically write data to a JSON file every 2 seconds to avoid blocking I/O on every message
setInterval(() => {
  if (sensorDataLog.length > 0) {
    fs.writeFileSync(OUTPUT_FILE, JSON.stringify(sensorDataLog, null, 2));
  }
}, 2000);

// Handle UDP (Accelerometer) Messages
server.on('error', (err) => {
  console.log(`Server error:\n${err.stack}`);
  server.close();
});

server.on('message', (msg, rinfo) => {
  console.log(`Server got: "${msg}" from ${rinfo.address}:${rinfo.port}`);
  console.log(`UDP (Accel) got: "${msg}" from ${rinfo.address}:${rinfo.port}`);
  logData('accelerometer', msg);
});

server.on('listening', () => {
  const address = server.address();
  console.log(`Server listening on ${address.address}:${address.port}`);
  console.log(`UDP Server listening on ${address.address}:${address.port}`);
});

server.bind(PORT);

// // Handle MQTT (Mocap) Messages
// mqttClient.on('connect', () => {
//     console.log('Successfully connected to the Rastic MQTT Broker!');
    
//     mqttClient.subscribe(MOCAP_TOPIC, (err) => {
//         if (!err) {
//             console.log(`Subscribed to topic: ${MOCAP_TOPIC}`);
//         } else {
//             console.error('Subscription error:', err);
//         }
//     });
// });

// mqttClient.on('message', (topic, message) => {
//     console.log(`MQTT (Mocap) Topic: ${topic} | Data: ${message.toString()}`);
//     logData('mocap', message);
// });

// Handle graceful shutdown to ensure the final data is saved (e.g. CTRL+C)
process.on('SIGINT', () => {
  console.log('\nGracefully shutting down and saving data...');
  fs.writeFileSync(OUTPUT_FILE, JSON.stringify(sensorDataLog, null, 2));
  process.exit();
});