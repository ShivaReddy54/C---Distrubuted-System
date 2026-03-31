const express = require('express');

const app = express();
app.use(express.json());

// ================= STORE =================
const store = new Map();

// ================= ROUTES =================

// SET key value
app.post('/set', (req, res) => {
    const { key, value } = req.body;

    if (!key || value === undefined) {
        return res.status(400).send("INVALID");
    }

    store.set(key, value);
    return res.send("OK");
});

// GET key
app.get('/get', (req, res) => {
    const key = req.query.key;

    if (!key) {
        return res.status(400).send("INVALID");
    }

    if (store.has(key)) {
        return res.send(store.get(key));
    } else {
        return res.send("NOT_FOUND");
    }
});

// ================= START =================
const DEFAULT_PORT = 5001;
const requestedPort = process.argv[2] || process.env.PORT;
const PORT = parseInt(requestedPort, 10) || DEFAULT_PORT;

if (!requestedPort) {
    console.log(`No port supplied; using default ${DEFAULT_PORT}`);
} else if (isNaN(PORT) || PORT < 1 || PORT > 65535) {
    console.warn(`Invalid port '${requestedPort}'. Falling back to ${DEFAULT_PORT}.`);
}

const server = app.listen(PORT, () => {
    console.log(`🚀 KV Server (Express) running on port ${PORT}`);
});

server.on('error', (err) => {
    console.error(`KV Server failed to start on port ${PORT}:`, err);
    process.exit(1);
});

process.on('uncaughtException', (err) => {
    console.error('Uncaught exception in KV Server:', err);
});

process.on('unhandledRejection', (reason) => {
    console.error('Unhandled rejection in KV Server:', reason);
});