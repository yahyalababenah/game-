/*
  ============================================================
  Serial -> WebSocket Bridge
  الطريقة الأثبت للبوث: يقرأ من الأردوينو ويبثه لصفحة الويب
  ============================================================
  التشغيل:
    npm init -y
    npm install serialport @serialport/parser-readline ws
    node serial_bridge.js COM5          (ويندوز)
    node serial_bridge.js /dev/ttyUSB0  (لينكس)

  لمعرفة اسم المنفذ: افتح Arduino IDE -> Tools -> Port
  ثم افتح tug_of_war.html في المتصفح (يتصل تلقائيا).
  ============================================================
*/

const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const { WebSocketServer } = require('ws');

const PORT_PATH = process.argv[2] || 'COM5';
const BAUD      = 115200;
const WS_PORT   = 8080;

const wss = new WebSocketServer({ port: WS_PORT });
let clients = new Set();

wss.on('connection', ws => {
  clients.add(ws);
  console.log('صفحة ويب اتصلت. العدد الحالي:', clients.size);
  ws.on('close', () => clients.delete(ws));
});

console.log(`WebSocket يعمل على ws://localhost:${WS_PORT}`);

function broadcast(line) {
  for (const ws of clients) {
    if (ws.readyState === 1) ws.send(line);
  }
}

// إعادة الاتصال تلقائيا اذا انفصل الكيبل  -> مهم جدا في البوث
function connectSerial() {
  const port = new SerialPort({ path: PORT_PATH, baudRate: BAUD }, err => {
    if (err) {
      console.error('تعذر فتح المنفذ:', err.message, '- إعادة المحاولة بعد ثانيتين');
      setTimeout(connectSerial, 2000);
    }
  });

  const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

  port.on('open',  () => console.log('تم الاتصال بالأردوينو على', PORT_PATH));
  parser.on('data', line => broadcast(line.trim()));

  port.on('close', () => {
    console.warn('انقطع الاتصال بالأردوينو - إعادة المحاولة...');
    setTimeout(connectSerial, 2000);
  });

  port.on('error', e => console.error('خطأ سيريال:', e.message));
}

connectSerial();
