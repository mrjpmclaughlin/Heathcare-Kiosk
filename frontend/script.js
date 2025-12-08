const APPOINTMENTS_URL = "appointments.json";
const STATUS_URL = "status.json";
const LOG_URL = "log.txt";


async function fetchJson(url) {
    const res = await fetch(url + "?t=" + Date.now()).catch(() => null);
    if (!res || !res.ok) return null;
    try {
    return await res.json();
    } catch (e) {
    return null;
    }
}

async function fetchText(url) {
    const res = await fetch(url + "?t=" + Date.now()).catch(() => null);
    if (!res || !res.ok) return null;
    try {
    return await res.text();
    } catch (e) {
    return null;
    }
}

function renderKiosks(kioskStats) {
    const container = document.getElementById("kioskGrid");
    container.innerHTML = "";

    if (!Array.isArray(kioskStats) || kioskStats.length === 0) {
    container.innerHTML = "<p class='small-label'>No kiosk data available yet.</p>";
    return;
    }

    kioskStats.forEach(stat => {
    const div = document.createElement("div");
    div.className = "card kiosk-card";
    div.innerHTML = `
        <div class="kiosk-title">
        <span>Kiosk ${stat.kiosk_id ?? "?"}</span>
        <span class="pill">Thread #${stat.kiosk_id ?? "?"}</span>
        </div>
        <div class="kiosk-stat">
        Patients checked in: <strong>${stat.patients_checked_in ?? 0}</strong>
        </div>
    `;
    container.appendChild(div);
    });
}

function renderMetrics(status, appointments) {
    const metricsEl = document.getElementById("metrics");
    metricsEl.innerHTML = "";

    const totalPatients = status?.total_patients ?? (appointments?.waiting?.length || 0) + (appointments?.being_seen?.length || 0);
    const avgWait = status?.average_wait_seconds;
    const lastUpdate = status?.last_update ?? "n/a";

    function metric(label, value) {
    const div = document.createElement("div");
    div.className = "metric-box";
    div.innerHTML = `<strong>${value}</strong> ${label}`;
    metricsEl.appendChild(div);
    }

    metric("patients total", totalPatients);
    metric("in queue", appointments?.waiting?.length ?? 0);
    metric("being seen", appointments?.being_seen?.length ?? 0);
    if (typeof avgWait === "number") {
    metric("avg wait (sec)", avgWait.toFixed(0));
    }
    const div = document.createElement("div");
    div.className = "metric-box";
    div.textContent = "Last update: " + lastUpdate;
    metricsEl.appendChild(div);

    const policyLabel = document.getElementById("policyLabel");
    policyLabel.textContent = status?.scheduling_policy ?? "Unknown (status.json missing)";
}

function renderQueue(waiting) {
    const tbody = document.getElementById("queueBody");
    tbody.innerHTML = "";

    if (!Array.isArray(waiting) || waiting.length === 0) {
    const tr = document.createElement("tr");
    const td = document.createElement("td");
    td.colSpan = 5;
    td.className = "small-label";
    td.textContent = "No patients currently waiting.";
    tr.appendChild(td);
    tbody.appendChild(tr);
    return;
    }

    waiting.forEach(p => {
    const tr = document.createElement("tr");

    const tdName = document.createElement("td");
    tdName.textContent = p.name ?? "Unknown";
    tr.appendChild(tdName);

    const tdTriage = document.createElement("td");
    const triage = p.triage ?? "?";
    const span = document.createElement("span");
    span.className = "triage triage-" + triage;
    span.textContent = triage;
    tdTriage.appendChild(span);
    tr.appendChild(tdTriage);

    const tdArrival = document.createElement("td");
    tdArrival.textContent = p.arrival_time ?? "-";
    tr.appendChild(tdArrival);

    const tdKiosk = document.createElement("td");
    tdKiosk.textContent = p.kiosk_id != null ? `#${p.kiosk_id}` : "-";
    tr.appendChild(tdKiosk);

    const tdStatus = document.createElement("td");
    const statusSpan = document.createElement("span");
    const s = (p.status || "waiting").toLowerCase();
    statusSpan.textContent = s;
    statusSpan.className = "pill " + (s === "waiting" ? "waiting" : s === "being_seen" ? "being-seen" : "");
    tdStatus.appendChild(statusSpan);
    tr.appendChild(tdStatus);

    tbody.appendChild(tr);
    });
}

function renderBeingSeen(beingSeen) {
    const ul = document.getElementById("beingSeenList");
    ul.innerHTML = "";

    if (!Array.isArray(beingSeen) || beingSeen.length === 0) {
    const li = document.createElement("li");
    li.className = "small-label";
    li.textContent = "No patients currently being seen.";
    ul.appendChild(li);
    return;
    }

    beingSeen.forEach(p => {
    const li = document.createElement("li");
    const room = p.room ? ` in ${p.room}` : "";
    li.innerHTML = `<strong>${p.name ?? "Unknown"}</strong> (triage ${p.triage ?? "?"})${room}`;
    ul.appendChild(li);
    });
}

function renderLog(text) {
    const container = document.getElementById("logContainer");
    const status = document.getElementById("logStatus");

    if (text == null) {
    status.textContent = "Log unavailable";
    status.className = "status-error";
    container.textContent = "Unable to read log.txt. Make sure your C program is writing this file.";
    return;
    }

    status.textContent = "Live";
    status.className = "status-ok";
    container.textContent = text.trim() === "" ? "(Log is empty)" : text;
    container.scrollTop = container.scrollHeight;
}

async function refresh() {
    const [appointments, status, logText] = await Promise.all([
    fetchJson(APPOINTMENTS_URL),
    fetchJson(STATUS_URL),
    fetchText(LOG_URL)
    ]);

    renderKiosks(appointments?.kiosk_stats || []);
    renderMetrics(status || {}, appointments || {});
    renderQueue(appointments?.waiting || []);
    renderBeingSeen(appointments?.being_seen || []);
    renderLog(logText);
}

// Poll every second
refresh();
setInterval(refresh, 1000);