const axios = require('axios');

const ROUTER_URL = "http://localhost:5000";

const NUM_CLIENTS = 5000;          // increase gradually
const REQUESTS_PER_CLIENT = 10;

// stats
let totalRequests = 0;
let success = 0;
let failed = 0;

let setSuccess = 0;
let getSuccess = 0;

// ================= REQUEST =================
async function sendRequest(type, key, value) {
    totalRequests++;

    try {
        let res;

        if (type === "SET") {
            res = await axios.post(`${ROUTER_URL}/set`, {
                key,
                value
            });
        } else {
            res = await axios.get(`${ROUTER_URL}/get?key=${key}`);
        }

        if (res.data) {
            success++;

            if (type === "SET") setSuccess++;
            else getSuccess++;
        } else {
            failed++;
        }

    } catch (err) {
        failed++;
    }
}

// ================= CLIENT =================
async function clientTask(clientId) {
    for (let i = 0; i < REQUESTS_PER_CLIENT; i++) {
        const key = `user:${clientId}_${i}`;

        await sendRequest("SET", key, `value${i}`);
        await sendRequest("GET", key);
    }
}

// ================= MAIN =================
async function runLoadTest() {
    console.log("🚀 Starting Load Test...\n");

    const start = Date.now();

    const clients = [];

    for (let i = 0; i < NUM_CLIENTS; i++) {
        clients.push(clientTask(i));
    }

    await Promise.all(clients);

    const end = Date.now();
    const duration = (end - start) / 1000;

    console.log("=============================");
    console.log("Total Clients:", NUM_CLIENTS);
    console.log("Requests per Client:", REQUESTS_PER_CLIENT);
    console.log("Total Requests:", totalRequests);

    console.log("✅ Success:", success);
    console.log("   SET Success:", setSuccess);
    console.log("   GET Success:", getSuccess);

    console.log("❌ Failed:", failed);

    console.log("Time Taken:", duration, "sec");

    console.log(
        "Throughput:",
        (success / duration).toFixed(2),
        "req/sec"
    );

    console.log(
        "Success Rate:",
        ((success / totalRequests) * 100).toFixed(2),
        "%"
    );

    console.log("=============================");
}

runLoadTest();