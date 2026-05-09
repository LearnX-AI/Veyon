/**
 * Veyon Control Dashboard - frontend logic.
 *
 * Single-file vanilla JS, no frameworks. Communicates with the
 * FastAPI backend over fetch().
 */

// ============================================================
// State
// ============================================================
const STATE = {
    token: null,
    refreshTimer: null,
};

const TOKEN_KEY = "veyon_admin_token";

// ============================================================
// API client
// ============================================================
const API_BASE = "/api/v1";

async function apiRequest(path, { method = "GET", body = null } = {}) {
    const headers = {
        "Authorization": `Bearer ${STATE.token}`,
    };
    if (body !== null) headers["Content-Type"] = "application/json";

    const response = await fetch(API_BASE + path, {
        method,
        headers,
        body: body !== null ? JSON.stringify(body) : null,
    });

    // 204 No Content -> nothing to parse
    if (response.status === 204) return null;

    let data = null;
    try {
        data = await response.json();
    } catch {
        // non-JSON response
    }

    if (!response.ok) {
        const msg = data?.detail
            ? (typeof data.detail === "string" ? data.detail : JSON.stringify(data.detail))
            : `Request failed (${response.status})`;
        const err = new Error(msg);
        err.status = response.status;
        throw err;
    }

    return data;
}

// ============================================================
// Helpers
// ============================================================
function $(id) { return document.getElementById(id); }

function escapeHtml(s) {
    if (s === null || s === undefined) return "";
    return String(s)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#39;");
}

function formatTime(isoString) {
    if (!isoString) return "—";
    const d = new Date(isoString);
    return d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" });
}

function showToast(message, type = "info") {
    const toast = $("toast");
    toast.textContent = message;
    toast.className = `toast ${type}`;
    setTimeout(() => toast.classList.add("hidden"), 3500);
}

function setStatus(ok, text) {
    const pill = $("status-pill");
    pill.className = `pill ${ok ? "pill-ok" : "pill-error"}`;
    pill.innerHTML = `●&nbsp;${escapeHtml(text)}`;
}

// ============================================================
// Login / logout
// ============================================================
async function attemptLogin(token) {
    STATE.token = token;
    // Verify by hitting a protected endpoint
    await apiRequest("/blocklist");
    sessionStorage.setItem(TOKEN_KEY, token);
}

function logout() {
    STATE.token = null;
    sessionStorage.removeItem(TOKEN_KEY);
    if (STATE.refreshTimer) {
        clearInterval(STATE.refreshTimer);
        STATE.refreshTimer = null;
    }
    $("login-overlay").classList.remove("hidden");
    $("app").classList.add("hidden");
    $("token-input").value = "";
    $("login-error").textContent = "";
}

function showApp() {
    $("login-overlay").classList.add("hidden");
    $("app").classList.remove("hidden");
    refreshAll();
    STATE.refreshTimer = setInterval(refreshAll, 10_000);
}

// ============================================================
// Renderers
// ============================================================
async function refreshBlocklist() {
    try {
        const entries = await apiRequest("/blocklist");
        const list = $("blocklist");
        $("blocklist-count").textContent = `${entries.length} domain${entries.length !== 1 ? "s" : ""}`;
        if (entries.length === 0) {
            list.innerHTML = '<li class="empty">No domains blocked yet.</li>';
            return;
        }
        list.innerHTML = entries.map(e => `
            <li>
                <div>
                    <div>${escapeHtml(e.domain)}</div>
                    ${e.note ? `<div class="meta">${escapeHtml(e.note)}</div>` : ""}
                </div>
                <button class="btn btn-danger" data-remove-id="${e.id}">Remove</button>
            </li>
        `).join("");
    } catch (err) {
        setStatus(false, "Disconnected");
        throw err;
    }
}

async function refreshMachines() {
    try {
        const machines = await apiRequest("/machines");
        const list = $("machines");
        $("machines-count").textContent = `${machines.length} machine${machines.length !== 1 ? "s" : ""}`;
        if (machines.length === 0) {
            list.innerHTML = '<li class="empty">No machines registered yet.</li>';
            return;
        }
        list.innerHTML = machines.map(m => `
            <li class="machine-row">
                <div class="label">
                    ${escapeHtml(m.label || m.hostname)}
                    <small>${escapeHtml(m.hostname)}${m.ip_address ? " · " + escapeHtml(m.ip_address) : ""}</small>
                </div>
                <div class="toggle ${m.focus_mode_active ? "on" : ""}"
                     data-toggle-id="${m.id}"
                     data-toggle-current="${m.focus_mode_active}"
                     title="Focus Mode"></div>
            </li>
        `).join("");
    } catch (err) {
        setStatus(false, "Disconnected");
        throw err;
    }
}

async function refreshLog() {
    try {
        const entries = await apiRequest("/admin/log?limit=20");
        const tbody = $("log-rows");
        if (entries.length === 0) {
            tbody.innerHTML = '<tr><td colspan="4" style="text-align:center;color:var(--text-muted);padding:20px;">No activity yet.</td></tr>';
            return;
        }
        tbody.innerHTML = entries.map(e => `
            <tr>
                <td>${formatTime(e.occurred_at)}</td>
                <td><span class="action-tag">${escapeHtml(e.action)}</span></td>
                <td>${escapeHtml(e.target || "—")}</td>
                <td class="muted">${escapeHtml(e.actor_ip || "—")}</td>
            </tr>
        `).join("");
    } catch (err) {
        setStatus(false, "Disconnected");
        throw err;
    }
}

async function refreshAll() {
    try {
        await Promise.all([refreshBlocklist(), refreshMachines(), refreshLog()]);
        setStatus(true, "Connected");
    } catch (err) {
        if (err.status === 401) {
            showToast("Session expired. Please sign in again.", "error");
            logout();
        }
    }
}

// ============================================================
// Event wiring
// ============================================================
document.addEventListener("DOMContentLoaded", () => {

    // Login form
    $("login-form").addEventListener("submit", async (e) => {
        e.preventDefault();
        const token = $("token-input").value.trim();
        if (!token) return;
        try {
            await attemptLogin(token);
            showApp();
        } catch (err) {
            $("login-error").textContent = err.message || "Login failed";
        }
    });

    // Logout button
    $("logout-btn").addEventListener("click", logout);

    // Add domain
    $("add-domain-form").addEventListener("submit", async (e) => {
        e.preventDefault();
        const domain = $("domain-input").value.trim();
        const note = $("note-input").value.trim() || null;
        if (!domain) return;
        try {
            await apiRequest("/blocklist", {
                method: "POST",
                body: { domain, note },
            });
            $("domain-input").value = "";
            $("note-input").value = "";
            showToast(`Added ${domain}`, "success");
            await refreshAll();
        } catch (err) {
            showToast(err.message, "error");
        }
    });

    // Delegated click handler for remove + toggle buttons (re-rendered dynamically)
    document.body.addEventListener("click", async (e) => {
        const removeId = e.target.dataset.removeId;
        if (removeId) {
            if (!confirm("Remove this domain from the blocklist?")) return;
            try {
                await apiRequest(`/blocklist/${removeId}`, { method: "DELETE" });
                showToast("Domain removed", "success");
                await refreshAll();
            } catch (err) {
                showToast(err.message, "error");
            }
            return;
        }

        const toggle = e.target.closest("[data-toggle-id]");
        if (toggle) {
            const id = toggle.dataset.toggleId;
            const newState = toggle.dataset.toggleCurrent !== "true";
            try {
                await apiRequest(`/machines/${id}/focus-mode?enabled=${newState}`, { method: "POST" });
                showToast(`Focus Mode ${newState ? "enabled" : "disabled"}`, "success");
                await refreshAll();
            } catch (err) {
                showToast(err.message, "error");
            }
        }
    });

    // Refresh log
    $("refresh-log-btn").addEventListener("click", refreshLog);

    // Auto-login if a token is in sessionStorage
    const saved = sessionStorage.getItem(TOKEN_KEY);
    if (saved) {
        STATE.token = saved;
        apiRequest("/blocklist")
            .then(showApp)
            .catch(() => {
                STATE.token = null;
                sessionStorage.removeItem(TOKEN_KEY);
            });
    }
});
