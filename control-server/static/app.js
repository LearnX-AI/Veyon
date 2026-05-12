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
    machines: [],
    expandedFileId: null,
    distributeFileId: null,
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
        STATE.machines = machines;
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

// ============================================================
// Files
// ============================================================

function formatBytes(n) {
    if (n < 1024) return `${n} B`;
    if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
    if (n < 1024 * 1024 * 1024) return `${(n / (1024 * 1024)).toFixed(1)} MB`;
    return `${(n / (1024 * 1024 * 1024)).toFixed(2)} GB`;
}

function timeSince(iso) {
    const seconds = Math.floor((Date.now() - new Date(iso).getTime()) / 1000);
    if (seconds < 60)   return `${seconds}s ago`;
    if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
    if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
    return `${Math.floor(seconds / 86400)}d ago`;
}

async function refreshFiles() {
    try {
        const files = await apiRequest("/files");
        const list = $("files-list");
        const totalBytes = files.reduce((s, f) => s + f.size_bytes, 0);
        $("files-summary").textContent =
            files.length
                ? `${files.length} file${files.length !== 1 ? "s" : ""} · ${formatBytes(totalBytes)}`
                : "";

        if (files.length === 0) {
            list.innerHTML = '<li class="empty">No files uploaded yet.</li>';
            return;
        }

        list.innerHTML = files.map(f => `
            <li class="file-row ${STATE.expandedFileId === f.id ? "expanded" : ""}" data-file-id="${f.id}">
                <div class="file-row-head" data-toggle-file="${f.id}">
                    <div>
                        <div class="file-name">${escapeHtml(f.filename)}</div>
                        <div class="file-meta">
                            ${formatBytes(f.size_bytes)} ·
                            Uploaded ${timeSince(f.uploaded_at)}${f.note ? " · " + escapeHtml(f.note) : ""}
                        </div>
                    </div>
                    <div class="file-actions">
                        <button class="btn btn-primary" data-distribute-id="${f.id}" data-distribute-name="${escapeHtml(f.filename)}">Distribute</button>
                        <button class="btn btn-danger"  data-delete-file-id="${f.id}" data-delete-file-name="${escapeHtml(f.filename)}">Delete</button>
                    </div>
                </div>
                <div class="file-row-detail" id="file-detail-${f.id}">
                    <div class="muted">Loading status...</div>
                </div>
            </li>
        `).join("");

        // If a row was open, repopulate it
        if (STATE.expandedFileId) {
            loadFileDistributions(STATE.expandedFileId);
        }
    } catch (err) {
        if (err.status !== 401) console.error("refreshFiles:", err);
        throw err;
    }
}

async function loadFileDistributions(fileId) {
    const target = $(`file-detail-${fileId}`);
    if (!target) return;
    try {
        const dists = await apiRequest(`/files/${fileId}/distributions`);
        if (dists.length === 0) {
            target.innerHTML = '<div class="muted">Not distributed yet.</div>';
            return;
        }
        const machinesById = new Map(STATE.machines.map(m => [m.id, m]));
        target.innerHTML = dists.map(d => {
            const m = machinesById.get(d.machine_id);
            const label = m ? (m.label || m.hostname) : `Machine ${d.machine_id}`;
            const pct = d.bytes_received && (d.status === "downloading" || d.status === "delivered")
                ? Math.min(100, Math.floor((d.bytes_received / Math.max(1, fileSize)) * 100))
                : (d.status === "delivered" ? 100 : (d.status === "failed" ? 100 : 0));
            const barClass = d.status === "delivered" ? "status-bar-delivered"
                           : d.status === "failed" ? "status-bar-failed" : "";
            return `
                <div class="dist-row">
                    <div>${escapeHtml(label)}</div>
                    <div class="dist-bar ${barClass}"><span style="width:${pct}%"></span></div>
                    <div class="status-${d.status}">${d.status}</div>
                </div>
            `;
        }).join("");
    } catch (err) {
        target.innerHTML = '<div class="muted">Could not load status.</div>';
    }
}

function toggleFileRow(fileId) {
    STATE.expandedFileId = STATE.expandedFileId === fileId ? null : fileId;
    refreshFiles();
}

async function deleteFile(fileId, fileName) {
    if (!confirm(`Delete "${fileName}"? This also clears its distributions.`)) return;
    try {
        await apiRequest(`/files/${fileId}`, { method: "DELETE" });
        showToast(`Deleted ${fileName}`, "success");
        if (STATE.expandedFileId === fileId) STATE.expandedFileId = null;
        await refreshAll();
    } catch (err) {
        showToast(err.message, "error");
    }
}

// ----- Upload (XHR so we get progress events) -----

function uploadFile(fileObj, note) {
    return new Promise((resolve, reject) => {
        const form = new FormData();
        form.append("file", fileObj);
        if (note) form.append("note", note);

        const xhr = new XMLHttpRequest();
        xhr.open("POST", `/api/v1${"/files/upload"}`);
        xhr.setRequestHeader("Authorization", `Bearer ${STATE.token}`);

        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) {
                const pct = Math.floor((e.loaded / e.total) * 100);
                $("upload-progress-fill").style.width = `${pct}%`;
                $("upload-progress-label").textContent =
                    `${formatBytes(e.loaded)} / ${formatBytes(e.total)} (${pct}%)`;
            }
        };

        xhr.onload = () => {
            if (xhr.status >= 200 && xhr.status < 300) {
                resolve(JSON.parse(xhr.responseText));
            } else {
                let msg = `Upload failed (${xhr.status})`;
                try { msg = JSON.parse(xhr.responseText).detail || msg; } catch {}
                reject(new Error(msg));
            }
        };
        xhr.onerror   = () => reject(new Error("Upload network error"));
        xhr.onabort   = () => reject(new Error("Upload cancelled"));

        xhr.send(form);
    });
}

// ----- Distribute dialog -----

function openDistributeDialog(fileId, fileName) {
    STATE.distributeFileId = fileId;
    $("distribute-dialog-title").textContent = "Distribute file";
    $("distribute-dialog-sub").textContent = fileName;

    const listEl = $("distribute-machine-list");
    if (STATE.machines.length === 0) {
        listEl.innerHTML = '<li class="empty">No machines registered yet.</li>';
    } else {
        listEl.innerHTML = STATE.machines.map(m => `
            <li data-machine-id="${m.id}">
                <input type="checkbox" data-machine-id="${m.id}">
                <span>
                    ${escapeHtml(m.label || m.hostname)}
                    <small class="muted">${escapeHtml(m.hostname)}</small>
                </span>
            </li>
        `).join("");
    }
    $("distribute-dialog").classList.remove("hidden");
}

function closeDistributeDialog() {
    $("distribute-dialog").classList.add("hidden");
    STATE.distributeFileId = null;
}

function getSelectedDistributeMachineIds() {
    return Array.from(
        document.querySelectorAll("#distribute-machine-list input[type=checkbox]:checked")
    ).map(cb => parseInt(cb.dataset.machineId, 10));
}

async function sendDistribute() {
    const ids = getSelectedDistributeMachineIds();
    if (ids.length === 0) {
        showToast("Pick at least one machine", "error");
        return;
    }
    try {
        await apiRequest(`/files/${STATE.distributeFileId}/distribute`, {
            method: "POST",
            body: { machine_ids: ids },
        });
        showToast(`Queued for ${ids.length} machine${ids.length !== 1 ? "s" : ""}`, "success");
        closeDistributeDialog();
        await refreshAll();
    } catch (err) {
        showToast(err.message, "error");
    }
}

async function refreshAll() {
    try {
        await Promise.all([refreshBlocklist(), refreshMachines(), refreshFiles(), refreshLog()]);
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

        // File row expand/collapse
        const toggleFile = e.target.closest("[data-toggle-file]");
        if (toggleFile && !e.target.closest("[data-distribute-id]")
                       && !e.target.closest("[data-delete-file-id]")) {
            toggleFileRow(parseInt(toggleFile.dataset.toggleFile, 10));
            return;
        }
        // Open Distribute dialog
        const distBtn = e.target.dataset.distributeId;
        if (distBtn) {
            openDistributeDialog(parseInt(distBtn, 10),
                                 e.target.dataset.distributeName);
            return;
        }
        // Delete file
        const delFileId = e.target.dataset.deleteFileId;
        if (delFileId) {
            deleteFile(parseInt(delFileId, 10), e.target.dataset.deleteFileName);
            return;
        }

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


    // ---- Files: upload, file picker label, delegated row clicks ----

    $("file-input").addEventListener("change", (e) => {
        const f = e.target.files[0];
        const label = $("file-input-label-text");
        const parent = label.parentElement;
        if (f) {
            label.textContent = `${f.name} (${formatBytes(f.size)})`;
            parent.classList.add("has-file");
        } else {
            label.textContent = "Choose file...";
            parent.classList.remove("has-file");
        }
    });

    $("upload-form").addEventListener("submit", async (e) => {
        e.preventDefault();
        const fileObj = $("file-input").files[0];
        if (!fileObj) return;
        const note = $("file-note-input").value.trim() || null;

        const progress = $("upload-progress");
        const fill     = $("upload-progress-fill");
        const label    = $("upload-progress-label");
        const btn      = $("upload-btn");

        btn.disabled = true;
        progress.classList.remove("hidden");
        fill.style.width = "0%";
        label.textContent = "Starting...";

        try {
            await uploadFile(fileObj, note);
            showToast(`Uploaded ${fileObj.name}`, "success");
            $("upload-form").reset();
            $("file-input-label-text").textContent = "Choose file...";
            $("file-input-label-text").parentElement.classList.remove("has-file");
            await refreshAll();
        } catch (err) {
            showToast(err.message, "error");
        } finally {
            btn.disabled = false;
            progress.classList.add("hidden");
        }
    });

    // Distribute dialog buttons
    $("distribute-cancel").addEventListener("click", closeDistributeDialog);
    $("distribute-send").addEventListener("click", sendDistribute);
    $("distribute-select-all").addEventListener("click", () => {
        document.querySelectorAll("#distribute-machine-list input[type=checkbox]")
            .forEach(cb => cb.checked = true);
    });
    $("distribute-clear").addEventListener("click", () => {
        document.querySelectorAll("#distribute-machine-list input[type=checkbox]")
            .forEach(cb => cb.checked = false);
    });
    $("distribute-dialog").addEventListener("click", (e) => {
        if (e.target.id === "distribute-dialog") closeDistributeDialog();
    });
    // Clicking a machine row in the dialog toggles its checkbox
    $("distribute-machine-list").addEventListener("click", (e) => {
        const li = e.target.closest("li[data-machine-id]");
        if (!li) return;
        if (e.target.tagName !== "INPUT") {
            const cb = li.querySelector("input[type=checkbox]");
            if (cb) cb.checked = !cb.checked;
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
