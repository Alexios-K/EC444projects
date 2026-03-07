const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const express = require('express');
const http = require('http');
const { Server } = require('socket.io');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.get('/', (req, res) => {
  res.sendFile(__dirname + '/index.html');
});

// Serial Port Setup
const port = new SerialPort({ path: 'COM7', baudRate: 115200 });
const parser = port.pipe(new ReadlineParser({ delimiter: '\r\n' }));

parser.on('data', (data) => {
  const time = Date.now();
  
  // Parse Acceleration Data (X, Y, Z)
  if (data.includes("X:")) {
    const matches = data.match(/X:\s*([-+]?[0-9]*\.?[0-9]+).*Y:\s*([-+]?[0-9]*\.?[0-9]+).*Z:\s*([-+]?[0-9]*\.?[0-9]+)/);
    if (matches) {
      io.emit('accelData', {
        time: time,
        x: parseFloat(matches[1]),
        y: parseFloat(matches[2]),
        z: parseFloat(matches[3])
      });
    }
  } else if (data.includes("roll:")) {
    // Parse Orientation Data (Roll, Pitch)
    const matches = data.match(/roll:\s*([-+]?[0-9]*\.?[0-9]+).*pitch:\s*([-+]?[0-9]*\.?[0-9]+)/);
    if (matches) {
      io.emit('orientationData', {
        time: time,
        roll: parseFloat(matches[1]),
        pitch: parseFloat(matches[2])
      });
    }
  }
});

server.listen(3000, () => {
  console.log('Graph available at http://localhost:3000');
});