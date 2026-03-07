const express = require('express');
const app = express();
const http = require('http');
const server = http.createServer(app);
const { Server } = require("socket.io");
const io = new Server(server);
const fs = require('fs');
const path = require('path');

// Configuration: File to read
const CSV_FILE = 'sensor_data1.csv'; 

// HTML Content served to the client
const htmlContent = `
<!DOCTYPE HTML>
<html>
<head>
<title>Sensor Data Graph</title>
<script src="https://canvasjs.com/assets/script/canvasjs.min.js"></script>
<script src="/socket.io/socket.io.js"></script>
<style>
    body { font-family: sans-serif; padding: 20px; }
    .chart-box { height: 370px; width: 100%; margin-bottom: 50px; }
</style>
<script>
window.onload = function () {
    // 1. Setup Accelerometer Chart
    var accelChart = new CanvasJS.Chart("accelChartContainer", {
        zoomEnabled: true,
        title: { text: "Accelerometer Data (Y Position)" },
        axisX: { 
            title: "Time", 
            valueFormatString: "mm:ss.fff",
            labelAngle: -30
        },
        axisY: { 
            title: "Position (m)",
            includeZero: false
        },
        data: [{ 
            type: "line", 
            xValueType: "dateTime",
            showInLegend: true,
            name: "Y Position",
            dataPoints: [] 
        }]
    });

    // 2. Setup LiDAR Chart
    var lidarChart = new CanvasJS.Chart("lidarChartContainer", {
        zoomEnabled: true,
        title: { text: "LiDAR Data (Distance)" },
        axisX: { 
            title: "Time", 
            valueFormatString: "mm:ss.fff",
            labelAngle: -30
        },
        axisY: { 
            title: "Distance (cm)",
            includeZero: false
        },
        data: [{ 
            type: "line", 
            xValueType: "dateTime",
            showInLegend: true,
            name: "Distance",
            color: "#C0504E", // Different color for distinction
            dataPoints: [] 
        }]
    });

    accelChart.render();
    lidarChart.render();

    // 3. Receive Data from Server
    var socket = io();
    socket.on('csvData', function (data) {
        console.log("Received data points:", data.accel.length + data.lidar.length);
        
        accelChart.options.data[0].dataPoints = data.accel;
        lidarChart.options.data[0].dataPoints = data.lidar;
        
        accelChart.render();
        lidarChart.render();
    });
}
</script>
</head>
<body>
    <h2>Sensor Data Visualization: ${CSV_FILE}</h2>
    <div id="accelChartContainer" class="chart-box"></div>
    <div id="lidarChartContainer" class="chart-box"></div>
</body>
</html>
`;

// Serve the HTML page
app.get('/', (req, res) => {
    res.send(htmlContent);
});

// Handle Socket Connection
io.on('connection', (socket) => {
    console.log('Client connected, parsing CSV...');
    
    const filePath = path.join(__dirname, CSV_FILE);

    fs.readFile(filePath, 'utf8', (err, data) => {
        if (err) {
            console.error(`Error reading file ${CSV_FILE}:`, err);
            return;
        }

        const accelPoints = [];
        const lidarPoints = [];
        
        const lines = data.split('\n');
        
        lines.forEach(line => {
            // Expected format: Timestamp,Message
            // e.g., 2026-03-06T01:19:01.646Z,[ACCEL] Y Pos(m): -0.00
            
            const parts = line.split(',');
            if (parts.length < 2) return;
            
            const timestampStr = parts[0].trim();
            // Rejoin the rest in case message contains commas
            const message = parts.slice(1).join(',').trim();
            
            const time = new Date(timestampStr).getTime();
            if (isNaN(time)) return; // Skip invalid lines or headers

            if (message.includes('[ACCEL]')) {
                // Regex to extract float value from: [ACCEL] Y Pos(m): -0.00
                const match = message.match(/Y Pos\(m\):\s*([-+]?[0-9]*\.?[0-9]+)/);
                if (match) {
                    accelPoints.push({ x: time, y: parseFloat(match[1]) });
                }
            } else if (message.includes('[LiDAR]')) {
                // Regex to extract integer value from: [LiDAR] Distance: 176 cm
                const match = message.match(/Distance:\s*([0-9]+)/);
                if (match) {
                    lidarPoints.push({ x: time, y: parseInt(match[1]) });
                }
            }
        });

        console.log(`Parsed ${accelPoints.length} Accel points and ${lidarPoints.length} LiDAR points.`);
        socket.emit('csvData', { accel: accelPoints, lidar: lidarPoints });
    });
});

server.listen(3000, () => {
    console.log('Server running at http://localhost:3000');
});
