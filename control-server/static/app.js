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
    folders: [],
    expandedFolderId: null,
    uploadMaterialFolderId: null,
    editFolderId: null,
    sessions: [],
    expandedSessionId: null,
    extendSessionId: null,
    newSessionMode: "lab",
    countdownTimer: null,
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
        // Look up total file size for progress percentage display.
        const allFiles = await apiRequest("/files");
        const fileRec  = allFiles.find(f => f.id === fileId);
        const totalBytes = fileRec ? fileRec.size_bytes : 1;

        const dists = await apiRequest(`/files/${fileId}/distributions`);
        if (dists.length === 0) {
            target.innerHTML = '<div class="muted">Not distributed yet.</div>';
            return;
        }
        const machinesById = new Map(STATE.machines.map(m => [m.id, m]));
        target.innerHTML = dists.map(d => {
            const m = machinesById.get(d.machine_id);
            const label = m ? (m.label || m.hostname) : `Machine ${d.machine_id}`;
            const pct =
                  d.status === "delivered" ? 100
                : d.status === "failed"    ? 100
                : d.status === "downloading"
                    ? Math.min(99, Math.floor((d.bytes_received / Math.max(1, totalBytes)) * 100))
                    : 0;
            const barClass = d.status === "delivered" ? "status-bar-delivered"
                           : d.status === "failed"    ? "status-bar-failed" : "";
            return `
                <div class="dist-row">
                    <div>${escapeHtml(label)}</div>
                    <div class="dist-bar ${barClass}"><span style="width:${pct}%"></span></div>
                    <div class="status-${d.status}">${d.status}</div>
                </div>
            `;
        }).join("");
    } catch (err) {
        console.error("loadFileDistributions failed:", err);
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

// ============================================================
// Folders
// ============================================================

function formatDate(iso, fallback = "—") {
    if (!iso) return fallback;
    const d = new Date(iso);
    return d.toLocaleDateString(undefined, { year: "numeric", month: "short", day: "numeric" });
}

function formatDateTime(iso, fallback = "—") {
    if (!iso) return fallback;
    return new Date(iso).toLocaleString(undefined, {
        year: "numeric", month: "short", day: "numeric",
        hour: "2-digit", minute: "2-digit"
    });
}

function deadlineMeta(iso) {
    if (!iso) return "No deadline";
    const due = new Date(iso);
    const now = new Date();
    const passed = due < now;
    return passed
        ? `Due: ${formatDate(iso)} (passed)`
        : `Due: ${formatDate(iso)}`;
}

async function refreshFolders() {
    try {
        const folders = await apiRequest("/folders");
        STATE.folders = folders;
        const list = $("folders-list");

        if (folders.length === 0) {
            list.innerHTML = '<li class="empty">No shared folders yet.</li>';
            return;
        }

        list.innerHTML = folders.map(f => `
            <li class="folder-row ${STATE.expandedFolderId === f.id ? "expanded" : ""}"
                data-folder-id="${f.id}">
                <div class="folder-row-head" data-toggle-folder="${f.id}">
                    <div class="folder-info">
                        <div class="folder-name">
                            ${escapeHtml(f.name)}
                            <span class="status-badge status-${f.status}">${f.status}</span>
                        </div>
                        <div class="folder-meta">
                            ${deadlineMeta(f.deadline)} ·
                            ${f.machine_count} machine${f.machine_count !== 1 ? "s" : ""} ·
                            ${f.material_count} material${f.material_count !== 1 ? "s" : ""} ·
                            ${f.submission_count} submission${f.submission_count !== 1 ? "s" : ""}
                        </div>
                    </div>
                    <div class="folder-actions-head">
                        <button class="btn btn-ghost" data-edit-folder-id="${f.id}">Edit</button>
                        <button class="btn btn-danger" data-delete-folder-id="${f.id}" data-delete-folder-name="${escapeHtml(f.name)}">Delete</button>
                    </div>
                </div>
                <div class="folder-row-detail" id="folder-detail-${f.id}"></div>
            </li>
        `).join("");

        if (STATE.expandedFolderId) {
            renderFolderDetail(STATE.expandedFolderId);
        }
    } catch (err) {
        if (err.status !== 401) console.error("refreshFolders:", err);
        throw err;
    }
}

function toggleFolderRow(folderId) {
    STATE.expandedFolderId = STATE.expandedFolderId === folderId ? null : folderId;
    refreshFolders();
}

async function renderFolderDetail(folderId) {
    const target = $(`folder-detail-${folderId}`);
    if (!target) return;

    target.innerHTML = '<div class="empty-line">Loading...</div>';

    const folder = STATE.folders.find(f => f.id === folderId);
    if (!folder) {
        target.innerHTML = '<div class="empty-line">Folder not found.</div>';
        return;
    }

    try {
        // Fetch parallel: assigned machines, materials (distributions),
        // submissions. The assigned-machine list isn't a direct endpoint
        // but we derive it from the existing distributions for this folder.
        const [allDists, allFiles, submissions] = await Promise.all([
            // No "list assignments" endpoint; use file_distributions filtered by folder
            // via /files/{id}/distributions for each material - or we list folder's
            // distributions via SQL... we have neither. Use submissions endpoint and
            // machine_ids from a separate inference: read all distributions for the
            // folder. Use the file list instead (the materials' file_ids).
            apiRequest(`/files`),
            apiRequest(`/files`),
            apiRequest(`/folders/${folderId}/submissions`),
        ]);

        // Materials: we need the file_distributions for this folder, but we don't have
        // a dedicated endpoint. Fall back to listing all files and matching by folder
        // is wrong; we need actual material rows. Simplest: hit a new derived list -
        // we just iterate all known files and call /files/{id}/distributions, then
        // filter by folder. That's chatty. Let's keep it simple: just show counts and
        // a list of submissions; for materials, we read folder.material_count from
        // the summary and offer the upload button. If needed, expand later.

        const machineMap = new Map(STATE.machines.map(m => [m.id, m]));
        const submissionRows = submissions.length === 0
            ? '<li class="empty-line">No submissions yet.</li>'
            : submissions.map(s => {
                const m = machineMap.get(s.machine_id);
                const who = m ? (m.label || m.hostname) : `Machine ${s.machine_id}`;
                return `
                    <li>
                        <div>
                            <strong>${escapeHtml(s.filename)}</strong>
                            <div class="muted" style="font-size:0.8rem">
                                from ${escapeHtml(who)} · ${formatBytes(s.size_bytes)}
                                · ${timeSince(s.submitted_at)}
                            </div>
                        </div>
                        <div>${escapeHtml(s.note || "")}</div>
                        <button class="btn btn-ghost"
                                data-download-submission="${folderId}/${s.id}"
                                data-download-filename="${escapeHtml(s.filename)}">Download</button>
                    </li>
                `;
            }).join("");

        target.innerHTML = `
            ${folder.description ? `<p class="muted">${escapeHtml(folder.description)}</p>` : ""}

            <div class="subsection">
                <div class="subsection-head">
                    <span>Materials (${folder.material_count})</span>
                    <button class="btn btn-primary"
                            data-upload-material-id="${folderId}"
                            data-upload-material-name="${escapeHtml(folder.name)}">
                        + Upload Material
                    </button>
                </div>
                ${folder.material_count === 0
                    ? '<div class="empty-line">No materials uploaded yet.</div>'
                    : '<div class="empty-line">' + folder.material_count + ' material(s) distributed to assigned machines.</div>'}
            </div>

            <div class="subsection">
                <div class="subsection-head">
                    <span>Submissions (${submissions.length})</span>
                </div>
                <ul class="item-list">${submissionRows}</ul>
            </div>

            <div class="subsection">
                <div class="subsection-head">
                    <span>${folder.machine_count} machine(s) assigned</span>
                </div>
            </div>
        `;
    } catch (err) {
        console.error("renderFolderDetail failed:", err);
        target.innerHTML = '<div class="empty-line">Could not load folder detail.</div>';
    }
}

// ----- Upload material -----

function openUploadMaterialDialog(folderId, folderName) {
    STATE.uploadMaterialFolderId = folderId;
    $("upload-material-title").textContent = `Add Material to "${folderName}"`;
    $("upload-material-file").value = "";
    $("upload-material-file-label").textContent = "Choose file...";
    $("upload-material-note").value = "";
    $("upload-material-dialog").classList.remove("hidden");
}

function closeUploadMaterialDialog() {
    $("upload-material-dialog").classList.add("hidden");
    STATE.uploadMaterialFolderId = null;
}

async function submitUploadMaterial(e) {
    e.preventDefault();
    const folderId = STATE.uploadMaterialFolderId;
    const fileObj = $("upload-material-file").files[0];
    if (!folderId || !fileObj) return;

    const note = $("upload-material-note").value.trim() || null;
    const btn = $("upload-material-submit");
    btn.disabled = true;

    try {
        // Use XHR for the streaming upload (same pattern as Files panel)
        await uploadMaterialFile(folderId, fileObj, note);
        showToast(`Uploaded ${fileObj.name}`, "success");
        closeUploadMaterialDialog();
        await refreshAll();
    } catch (err) {
        showToast(err.message, "error");
    } finally {
        btn.disabled = false;
    }
}

function uploadMaterialFile(folderId, fileObj, note) {
    return new Promise((resolve, reject) => {
        const form = new FormData();
        form.append("file", fileObj);
        if (note) form.append("note", note);

        const xhr = new XMLHttpRequest();
        xhr.open("POST", `/api/v1/folders/${folderId}/materials`);
        xhr.setRequestHeader("Authorization", `Bearer ${STATE.token}`);
        xhr.onload = () => {
            if (xhr.status >= 200 && xhr.status < 300) {
                resolve(JSON.parse(xhr.responseText));
            } else {
                let msg = `Upload failed (${xhr.status})`;
                try { msg = JSON.parse(xhr.responseText).detail || msg; } catch {}
                reject(new Error(msg));
            }
        };
        xhr.onerror = () => reject(new Error("Upload network error"));
        xhr.send(form);
    });
}

// ----- Download submission -----

async function downloadSubmission(folderId, submissionId, filename) {
    try {
        const resp = await fetch(`/api/v1/folders/${folderId}/submissions/${submissionId}/download`, {
            headers: { Authorization: `Bearer ${STATE.token}` },
        });
        if (!resp.ok) throw new Error(`Download failed (${resp.status})`);
        const blob = await resp.blob();
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(url);
    } catch (err) {
        showToast(err.message, "error");
    }
}

// ----- Edit folder -----

function openEditFolderDialog(folderId) {
    const folder = STATE.folders.find(f => f.id === folderId);
    if (!folder) return;
    STATE.editFolderId = folderId;

    $("edit-folder-desc").value = folder.description || "";
    // datetime-local wants "YYYY-MM-DDTHH:mm" - strip timezone+seconds
    if (folder.deadline) {
        const d = new Date(folder.deadline);
        // Build a local-time string the input understands
        const pad = (n) => String(n).padStart(2, "0");
        const local = `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())}` +
                      `T${pad(d.getHours())}:${pad(d.getMinutes())}`;
        $("edit-folder-deadline").value = local;
    } else {
        $("edit-folder-deadline").value = "";
    }
    $("edit-folder-status").value = folder.status;
    $("edit-folder-dialog").classList.remove("hidden");
}

function closeEditFolderDialog() {
    $("edit-folder-dialog").classList.add("hidden");
    STATE.editFolderId = null;
}

async function submitEditFolder(e) {
    e.preventDefault();
    const folderId = STATE.editFolderId;
    if (!folderId) return;

    const body = {};
    body.description = $("edit-folder-desc").value.trim() || null;

    const deadlineRaw = $("edit-folder-deadline").value;
    body.deadline = deadlineRaw ? new Date(deadlineRaw).toISOString() : null;

    body.status = $("edit-folder-status").value;

    try {
        await apiRequest(`/folders/${folderId}`, { method: "PATCH", body });
        showToast("Folder updated", "success");
        closeEditFolderDialog();
        await refreshAll();
    } catch (err) {
        showToast(err.message, "error");
    }
}

// ----- New folder dialog -----

function openNewFolderDialog() {
    $("new-folder-name").value = "";
    $("new-folder-desc").value = "";
    $("new-folder-deadline").value = "";

    const listEl = $("new-folder-machine-list");
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
    $("new-folder-dialog").classList.remove("hidden");
}

function closeNewFolderDialog() {
    $("new-folder-dialog").classList.add("hidden");
}

async function submitNewFolder(e) {
    e.preventDefault();
    const name = $("new-folder-name").value.trim();
    if (!name) return;

    const machineIds = Array.from(
        document.querySelectorAll("#new-folder-machine-list input[type=checkbox]:checked")
    ).map(cb => parseInt(cb.dataset.machineId, 10));

    const deadlineRaw = $("new-folder-deadline").value;
    // datetime-local gives "YYYY-MM-DDTHH:mm" without timezone -- treat as local
    const deadline = deadlineRaw ? new Date(deadlineRaw).toISOString() : null;

    const body = {
        name,
        description: $("new-folder-desc").value.trim() || null,
        deadline,
        machine_ids: machineIds,
    };

    try {
        await apiRequest("/folders", { method: "POST", body });
        showToast(`Created folder "${name}"`, "success");
        closeNewFolderDialog();
        await refreshAll();
    } catch (err) {
        showToast(err.message, "error");
    }
}

// ----- Delete folder -----

async function deleteFolder(folderId, folderName) {
    if (!confirm(`Delete "${folderName}" and ALL its materials and submissions?`)) return;
    try {
        await apiRequest(`/folders/${folderId}`, { method: "DELETE" });
        showToast(`Deleted "${folderName}"`, "success");
        if (STATE.expandedFolderId === folderId) STATE.expandedFolderId = null;
        await refreshAll();
    } catch (err) {
        showToast(err.message, "error");
    }
}

// ============================================================
// Sessions
// ============================================================

const SESSION_MODE_PRESETS = {
    lab:  { duration: 60, warnings: "10, 1",          action: "lock_screen" },
    exam: { duration: 90, warnings: "30, 15, 5, 1",   action: "logout"      },
};

function fmtRemaining(ms) {
    if (ms <= 0) return "0:00";
    const s = Math.floor(ms / 1000);
    const m = Math.floor(s / 60);
    const ss = s % 60;
    return `${m}:${String(ss).padStart(2, "0")}`;
}

function updateRunningCountdowns() {
    // Update all rows tagged data-ends-at, no API hit needed.
    document.querySelectorAll("[data-ends-at]").forEach(el => {
        const endsAt = new Date(el.dataset.endsAt).getTime();
        const remaining = endsAt - Date.now();
        el.textContent = remaining > 0
            ? `ends in ${fmtRemaining(remaining)}`
            : "time up";
    });
}

async function refreshSessions() {
    try {
        const sessions = await apiRequest("/sessions");
        STATE.sessions = sessions;
        const list = $("sessions-list");
        if (sessions.length === 0) {
            list.innerHTML = '<li class="empty">No sessions yet.</li>';
            return;
        }
        list.innerHTML = sessions.map(s => renderSessionRow(s)).join("");
        if (STATE.expandedSessionId) renderSessionDetail(STATE.expandedSessionId);
        updateRunningCountdowns();
    } catch (err) {
        if (err.status !== 401) console.error("refreshSessions:", err);
        throw err;
    }
}

function renderSessionRow(s) {
    const isRunning = s.status === "running";
    const isPaused  = s.status === "paused";

    // Action buttons depending on state
    let actions = "";
    if (s.status === "scheduled") {
        actions += `<button class="btn btn-primary" data-start-session="${s.id}">Start</button>`;
        actions += `<button class="btn btn-danger" data-delete-session="${s.id}" data-delete-session-name="${escapeHtml(s.name)}">Delete</button>`;
    } else if (isRunning) {
        actions += `<button class="btn btn-ghost"   data-pause-session="${s.id}">Pause</button>`;
        actions += `<button class="btn btn-ghost"   data-extend-session="${s.id}" data-extend-session-name="${escapeHtml(s.name)}">+ Time</button>`;
        actions += `<button class="btn btn-danger"  data-cancel-session="${s.id}" data-cancel-session-name="${escapeHtml(s.name)}">End now</button>`;
    } else if (isPaused) {
        actions += `<button class="btn btn-primary" data-resume-session="${s.id}">Resume</button>`;
        actions += `<button class="btn btn-ghost"   data-extend-session="${s.id}" data-extend-session-name="${escapeHtml(s.name)}">+ Time</button>`;
        actions += `<button class="btn btn-danger"  data-cancel-session="${s.id}" data-cancel-session-name="${escapeHtml(s.name)}">End now</button>`;
    } else {
        // completed / cancelled
        actions += `<button class="btn btn-danger" data-delete-session="${s.id}" data-delete-session-name="${escapeHtml(s.name)}">Delete</button>`;
    }

    const countdown = isRunning && s.ends_at
        ? `<span class="session-countdown" data-ends-at="${s.ends_at}">…</span>`
        : "";

    const expanded = STATE.expandedSessionId === s.id ? "expanded" : "";

    return `
        <li class="session-row ${expanded}" data-session-id="${s.id}">
            <div class="session-row-head" data-toggle-session="${s.id}">
                <div class="session-info">
                    <div class="session-name">
                        ${escapeHtml(s.name)}
                        <span class="status-badge status-${s.status}">${s.status}</span>
                        <span class="muted" style="font-size:0.75rem">${s.mode}</span>
                        ${countdown}
                    </div>
                    <div class="session-meta">
                        ${s.duration_minutes} min ·
                        ${s.machine_count} machine${s.machine_count !== 1 ? "s" : ""} ·
                        action: ${s.timeout_action.replace("_", " ")}
                    </div>
                </div>
                <div class="session-actions">${actions}</div>
            </div>
            <div class="session-row-detail" id="session-detail-${s.id}"></div>
        </li>
    `;
}

function toggleSessionRow(sessionId) {
    STATE.expandedSessionId = STATE.expandedSessionId === sessionId ? null : sessionId;
    refreshSessions();
}

async function renderSessionDetail(sessionId) {
    const target = $(`session-detail-${sessionId}`);
    if (!target) return;
    try {
        const events = await apiRequest(`/sessions/${sessionId}/events`);
        if (events.length === 0) {
            target.innerHTML = '<div class="empty-line">No events yet.</div>';
            return;
        }
        const machineMap = new Map(STATE.machines.map(m => [m.id, m]));
        target.innerHTML = `
            <div class="subsection">
                <div class="subsection-head"><span>Event history (${events.length})</span></div>
                <ul class="event-timeline">${
                    events.map(e => {
                        const m = e.machine_id ? machineMap.get(e.machine_id) : null;
                        const who = m ? (m.label || m.hostname) : "session";
                        return `
                            <li>
                                <span class="event-type">${e.event_type}</span>
                                <span>
                                    <strong>${escapeHtml(who)}</strong>
                                    ${e.details ? ' · ' + escapeHtml(e.details) : ''}
                                </span>
                                <span class="muted">${formatDateTime(e.occurred_at)}</span>
                            </li>
                        `;
                    }).join("")
                }</ul>
            </div>
        `;
    } catch (err) {
        target.innerHTML = '<div class="empty-line">Could not load events.</div>';
    }
}

// ----- New Session dialog -----

function openNewSessionDialog() {
    STATE.newSessionMode = "lab";
    document.querySelectorAll("#new-session-dialog .mode-btn").forEach(b => {
        b.classList.toggle("active", b.dataset.mode === "lab");
    });
    applyModePreset("lab");
    $("new-session-name").value = "";

    const listEl = $("new-session-machine-list");
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
    $("new-session-dialog").classList.remove("hidden");
}

function applyModePreset(mode) {
    const p = SESSION_MODE_PRESETS[mode];
    if (!p) return;
    $("new-session-duration").value = p.duration;
    $("new-session-warnings").value = p.warnings;
    $("new-session-action").value   = p.action;
}

function closeNewSessionDialog() {
    $("new-session-dialog").classList.add("hidden");
}

function parseWarningList(raw) {
    return raw.split(",").map(s => parseInt(s.trim(), 10)).filter(n => n > 0);
}

async function submitNewSession(e) {
    e.preventDefault();
    const name = $("new-session-name").value.trim();
    if (!name) return;

    const duration = parseInt($("new-session-duration").value, 10);
    const warnings = parseWarningList($("new-session-warnings").value);

    const machineIds = Array.from(
        document.querySelectorAll("#new-session-machine-list input[type=checkbox]:checked")
    ).map(cb => parseInt(cb.dataset.machineId, 10));

    const body = {
        name,
        mode: STATE.newSessionMode,
        duration_minutes: duration,
        warning_minutes: warnings,
        timeout_action: $("new-session-action").value,
        machine_ids: machineIds,
    };

    try {
        await apiRequest("/sessions", { method: "POST", body });
        showToast(`Created session "${name}"`, "success");
        closeNewSessionDialog();
        await refreshAll();
    } catch (err) {
        showToast(err.message, "error");
    }
}

// ----- Lifecycle controls -----

async function controlSession(action, sessionId, opts = {}) {
    try {
        const path = `/sessions/${sessionId}/${action}`;
        const body = opts.body || undefined;
        await apiRequest(path, { method: "POST", body });
        showToast(`Session ${action}`, "success");
        await refreshAll();
    } catch (err) {
        showToast(err.message, "error");
    }
}

async function deleteSession(sessionId, name) {
    if (!confirm(`Delete "${name}"?`)) return;
    try {
        await apiRequest(`/sessions/${sessionId}`, { method: "DELETE" });
        showToast(`Deleted "${name}"`, "success");
        if (STATE.expandedSessionId === sessionId) STATE.expandedSessionId = null;
        await refreshAll();
    } catch (err) {
        showToast(err.message, "error");
    }
}

// ----- Extend dialog -----

function openExtendDialog(sessionId, name) {
    STATE.extendSessionId = sessionId;
    $("extend-session-name").textContent = name;
    $("extend-session-minutes").value = 5;
    $("extend-session-dialog").classList.remove("hidden");
}

function closeExtendDialog() {
    $("extend-session-dialog").classList.add("hidden");
    STATE.extendSessionId = null;
}

async function submitExtendSession(e) {
    e.preventDefault();
    const id = STATE.extendSessionId;
    if (!id) return;
    const minutes = parseInt($("extend-session-minutes").value, 10);
    if (!minutes || minutes < 1) return;
    await controlSession("extend", id, { body: { minutes } });
    closeExtendDialog();
}

async function refreshAll() {
    try {
        await Promise.all([refreshBlocklist(), refreshMachines(), refreshFiles(), refreshFolders(), refreshSessions(), refreshLog()]);
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

        // Folder row expand/collapse
        const toggleFolder = e.target.closest("[data-toggle-folder]");
        if (toggleFolder && !e.target.closest("[data-edit-folder-id]")
                         && !e.target.closest("[data-delete-folder-id]")) {
            toggleFolderRow(parseInt(toggleFolder.dataset.toggleFolder, 10));
            return;
        }
        const delFolderId = e.target.dataset.deleteFolderId;
        if (delFolderId) {
            deleteFolder(parseInt(delFolderId, 10), e.target.dataset.deleteFolderName);
            return;
        }
        const editFolderId = e.target.dataset.editFolderId;
        if (editFolderId) {
            openEditFolderDialog(parseInt(editFolderId, 10));
            return;
        }
        const uploadMatId = e.target.dataset.uploadMaterialId;
        if (uploadMatId) {
            openUploadMaterialDialog(parseInt(uploadMatId, 10),
                                     e.target.dataset.uploadMaterialName);
            return;
        }
        const downloadSub = e.target.dataset.downloadSubmission;
        if (downloadSub) {
            const [fid, sid] = downloadSub.split("/").map(n => parseInt(n, 10));
            downloadSubmission(fid, sid, e.target.dataset.downloadFilename);
            return;
        }

        // ---- Session controls ----
        const toggleSession = e.target.closest("[data-toggle-session]");
        if (toggleSession
            && !e.target.closest("[data-start-session]")
            && !e.target.closest("[data-pause-session]")
            && !e.target.closest("[data-resume-session]")
            && !e.target.closest("[data-extend-session]")
            && !e.target.closest("[data-cancel-session]")
            && !e.target.closest("[data-delete-session]")) {
            toggleSessionRow(parseInt(toggleSession.dataset.toggleSession, 10));
            return;
        }
        if (e.target.dataset.startSession) {
            controlSession("start", parseInt(e.target.dataset.startSession, 10));
            return;
        }
        if (e.target.dataset.pauseSession) {
            controlSession("pause", parseInt(e.target.dataset.pauseSession, 10));
            return;
        }
        if (e.target.dataset.resumeSession) {
            controlSession("resume", parseInt(e.target.dataset.resumeSession, 10));
            return;
        }
        if (e.target.dataset.extendSession) {
            openExtendDialog(parseInt(e.target.dataset.extendSession, 10),
                             e.target.dataset.extendSessionName);
            return;
        }
        if (e.target.dataset.cancelSession) {
            const id = parseInt(e.target.dataset.cancelSession, 10);
            const name = e.target.dataset.cancelSessionName;
            if (confirm(`End "${name}" now without firing the timeout action?`)) {
                controlSession("cancel", id);
            }
            return;
        }
        if (e.target.dataset.deleteSession) {
            deleteSession(parseInt(e.target.dataset.deleteSession, 10),
                          e.target.dataset.deleteSessionName);
            return;
        }

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




    // ---- Sessions: new session dialog ----
    $("new-session-btn").addEventListener("click", openNewSessionDialog);
    $("new-session-cancel").addEventListener("click", closeNewSessionDialog);
    $("new-session-form").addEventListener("submit", submitNewSession);
    $("new-session-select-all").addEventListener("click", () => {
        document.querySelectorAll("#new-session-machine-list input[type=checkbox]")
            .forEach(cb => cb.checked = true);
    });
    $("new-session-clear").addEventListener("click", () => {
        document.querySelectorAll("#new-session-machine-list input[type=checkbox]")
            .forEach(cb => cb.checked = false);
    });
    $("new-session-dialog").addEventListener("click", (e) => {
        if (e.target.id === "new-session-dialog") closeNewSessionDialog();
    });
    $("new-session-machine-list").addEventListener("click", (e) => {
        const li = e.target.closest("li[data-machine-id]");
        if (!li) return;
        if (e.target.tagName !== "INPUT") {
            const cb = li.querySelector("input[type=checkbox]");
            if (cb) cb.checked = !cb.checked;
        }
    });
    document.querySelectorAll("#new-session-dialog .mode-btn").forEach(btn => {
        btn.addEventListener("click", () => {
            STATE.newSessionMode = btn.dataset.mode;
            document.querySelectorAll("#new-session-dialog .mode-btn").forEach(b => {
                b.classList.toggle("active", b === btn);
            });
            applyModePreset(btn.dataset.mode);
        });
    });

    // ---- Sessions: extend dialog ----
    $("extend-session-cancel").addEventListener("click", closeExtendDialog);
    $("extend-session-form").addEventListener("submit", submitExtendSession);
    $("extend-session-dialog").addEventListener("click", (e) => {
        if (e.target.id === "extend-session-dialog") closeExtendDialog();
    });

    // ---- Sessions: per-second countdown ticker ----
    STATE.countdownTimer = setInterval(updateRunningCountdowns, 1000);

    // ---- Folders: new folder dialog ----
    $("new-folder-btn").addEventListener("click", openNewFolderDialog);
    $("new-folder-cancel").addEventListener("click", closeNewFolderDialog);
    $("new-folder-form").addEventListener("submit", submitNewFolder);
    $("new-folder-select-all").addEventListener("click", () => {
        document.querySelectorAll("#new-folder-machine-list input[type=checkbox]")
            .forEach(cb => cb.checked = true);
    });
    $("new-folder-clear").addEventListener("click", () => {
        document.querySelectorAll("#new-folder-machine-list input[type=checkbox]")
            .forEach(cb => cb.checked = false);
    });
    $("new-folder-dialog").addEventListener("click", (e) => {
        if (e.target.id === "new-folder-dialog") closeNewFolderDialog();
    });
    $("new-folder-machine-list").addEventListener("click", (e) => {
        const li = e.target.closest("li[data-machine-id]");
        if (!li) return;
        if (e.target.tagName !== "INPUT") {
            const cb = li.querySelector("input[type=checkbox]");
            if (cb) cb.checked = !cb.checked;
        }
    });


    // ---- Folders: edit + upload material + download submission ----
    $("upload-material-cancel").addEventListener("click", closeUploadMaterialDialog);
    $("upload-material-form").addEventListener("submit", submitUploadMaterial);
    $("upload-material-dialog").addEventListener("click", (e) => {
        if (e.target.id === "upload-material-dialog") closeUploadMaterialDialog();
    });
    $("upload-material-file").addEventListener("change", (e) => {
        const f = e.target.files[0];
        $("upload-material-file-label").textContent =
            f ? `${f.name} (${formatBytes(f.size)})` : "Choose file...";
    });

    $("edit-folder-cancel").addEventListener("click", closeEditFolderDialog);
    $("edit-folder-form").addEventListener("submit", submitEditFolder);
    $("edit-folder-dialog").addEventListener("click", (e) => {
        if (e.target.id === "edit-folder-dialog") closeEditFolderDialog();
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
