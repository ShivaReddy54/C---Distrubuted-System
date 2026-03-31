const express = require('express');
const axios = require('axios');
const cors = require('cors')

const app = express();
app.use(express.json());
app.use(cors())

// ================= CONFIG =================
const MIN_SERVERS = 2;
const MAX_SERVERS = 4;

const SCALE_UP_THRESHOLD = 50;
const SCALE_DOWN_THRESHOLD = 10;

// ==========================================

// 🔥 Servers (no process spawning here)
let servers = [
    { port: 5001, healthy: true },
    { port: 5002, healthy: true }
];

// ==========================================

// 🔥 HASH
function hashKey(key) {
    let hash = 0;
    for (let c of key) {
        hash = (hash * 31 + c.charCodeAt(0)) % 100000;
    }
    return hash;
}

// ==========================================

// 🔥 HEALTH CHECK
async function isServerHealthy(port) {
    try {
        await axios.get(`http://localhost:${port}/get?key=health_check`);
        return true;
    } catch {
        return false;
    }
}

// 🔥 HEALTH MONITOR
setInterval(async () => {
    for (let s of servers) {
        const healthy = await isServerHealthy(s.port);

        if (!healthy && s.healthy) {
            console.log(`[HEALTH] DOWN: ${s.port}`);
            s.healthy = false;
        }
        else if (healthy && !s.healthy) {
            console.log(`[HEALTH] RECOVERED: ${s.port}`);
            s.healthy = true;
        }
    }
}, 2000);

// ==========================================

// 🔥 AUTO SCALE (SIMULATED)
function autoScale(queueSize) {

    // SCALE UP
    if (queueSize > SCALE_UP_THRESHOLD && servers.length < MAX_SERVERS) {
        const newPort = 5001 + servers.length;

        console.log(`[AUTO SCALE] Add server ${newPort}`);

        // ⚠️ Just register (not spawn process)
        servers.push({ port: newPort, healthy: true });
    }

    // SCALE DOWN
    if (queueSize < SCALE_DOWN_THRESHOLD && servers.length > MIN_SERVERS) {
        const removed = servers.pop();
        console.log(`[AUTO SCALE] Remove server ${removed.port}`);
    }
}

// ==========================================

// 🔥 GET ACTIVE SERVERS
function getActiveServers() {
    return servers.filter(s => s.healthy).map(s => s.port);
}

// ==========================================

// 🔥 FORWARD REQUEST
async function forwardRequest(command, key, value, port) {
    try {
        if (command === "SET") {
            const res = await axios.post(`http://localhost:${port}/set`, {
                key,
                value
            });
            return res.data;
        }

        if (command === "GET") {
            const res = await axios.get(`http://localhost:${port}/get?key=${key}`);
            return res.data;
        }

    } catch (err) {
        console.log(`Error forwarding to ${port}`);
        return "";
    }
}

// ==========================================

// 🔥 ROUTER ENDPOINT

app.post('/set', async (req, res) => {
    const { key, value } = req.body;

    if (!key || value === undefined) {
        return res.send("INVALID");
    }

    const active = getActiveServers();

    console.log("Active servers:", active.length);

    if (active.length === 0) {
        return res.send("NO_SERVER");
    }

    const port = active[hashKey(key) % active.length];

    console.log("Routing to port:", port);

    const response = await forwardRequest("SET", key, value, port);

    if (!response) return res.send("ERROR");

    autoScale(0); // simulate queue size

    res.send(response);
});

// ==========================================

app.get('/get', async (req, res) => {
    const key = req.query.key;

    if (!key) {
        return res.send("INVALID");
    }

    const active = getActiveServers();

    console.log("Active servers:", active.length);

    if (active.length === 0) {
        return res.send("NO_SERVER");
    }

    const port = active[hashKey(key) % active.length];

    console.log("Routing to port:", port);

    const response = await forwardRequest("GET", key, null, port);

    if (!response) return res.send("ERROR");

    autoScale(0);

    res.send(response);
});

// ==========================================

// START
const DEFAULT_PORT = 5000;
const requestedPort = process.argv[2] || process.env.PORT;
const PORT = parseInt(requestedPort, 10) || DEFAULT_PORT;

if (!requestedPort) {
    console.log(`No port supplied; using default ${DEFAULT_PORT}`);
} else if (isNaN(PORT) || PORT < 1 || PORT > 65535) {
    console.warn(`Invalid port '${requestedPort}'. Falling back to ${DEFAULT_PORT}.`);
}

const server = app.listen(PORT, () => {
    console.log(`🚀 Router (Express) running on port ${PORT}`);
});

server.on('error', (err) => {
    console.error(`Router failed to start on port ${PORT}:`, err);
    process.exit(1);
});

process.on('uncaughtException', (err) => {
    console.error('Uncaught exception in Router:', err);
});

process.on('unhandledRejection', (reason) => {
    console.error('Unhandled rejection in Router:', reason);
});