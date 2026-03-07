const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const fs = require('fs');

// 1. Configure the Port
const port = new SerialPort({
  path: 'COM6',
  baudRate: 115200,
});

// 2. Set up the Parser (reads until it hits a newline)
const parser = port.pipe(new ReadlineParser({ delimiter: '\r\n' }));

// 3. Prepare the CSV file (Add headers if file is new)
const CSV_FILE = 'sensor_data.csv';
if (!fs.existsSync(CSV_FILE)) {
  fs.writeFileSync(CSV_FILE, 'Timestamp,SensorValue\n');
}

console.log('--- Logging started. Press Ctrl+C to stop. ---');

// 4. Handle incoming data
parser.on('data', (data) => {
  const timestamp = new Date().toISOString(); // High-precision ISO time
  const cleanData = data.trim(); // Remove any extra spaces
  
  const logEntry = `${timestamp},${cleanData}`;
  
  // Print to console
  console.log(logEntry);
  
  // Append to CSV file
  fs.appendFile(CSV_FILE, logEntry + '\n', (err) => {
    if (err) console.error('Error writing to file:', err);
  });
});

// Error handling
port.on('error', (err) => {
  console.error('Serial Port Error: ', err.message);
});