/* ==========================================================================
   SchedLab — Interactive Engine Client & Visualizer
   ========================================================================== */

// Harmonious 10-color system for distinct process visualization
const PROCESS_COLORS = [
  '#f43f5e', // P1: Coral Rose
  '#0d9488', // P2: Teal
  '#6366f1', // P3: Royal Indigo
  '#f59e0b', // P4: Warm Amber
  '#10b981', // P5: Emerald
  '#0284c7', // P6: Sky Blue
  '#8b5cf6', // P7: Purple
  '#ec4899', // P8: Pink
  '#06b6d4', // P9: Cyan
  '#ea580c'  // P10: Sunset Orange
];

const rowsContainer = document.querySelector('#rows');
const processCountBadge = document.querySelector('#process-count');
const quantumInput = document.querySelector('#quantum');
const messageBanner = document.querySelector('#message');
const btnRun = document.querySelector('#run');

// Escape helper to prevent XSS
const escapeHtml = value => String(value ?? '').replace(/[&<>"']/g, c => ({
  '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
}[c]));

/**
 * Returns the color assigned to a process by its index
 */
function getColorForIndex(index) {
  return PROCESS_COLORS[index % PROCESS_COLORS.length];
}

/**
 * Updates process row indicators and live count
 */
function updateWorkloadMetadata() {
  const trs = [...rowsContainer.querySelectorAll('tr')];
  processCountBadge.textContent = trs.length;

  trs.forEach((tr, index) => {
    const color = getColorForIndex(index);
    tr.style.setProperty('--row-accent', color);
    const tag = tr.querySelector('.process-color-tag');
    if (tag) tag.style.backgroundColor = color;
  });
}

/**
 * Adds a process row to the workload studio
 */
function addProcessRow(process = null) {
  const currentIndex = rowsContainer.children.length;
  const defaultId = `P${currentIndex + 1}`;
  const data = process || { id: defaultId, arrival: 0, burst: 5, priority: 1 };
  const assignedColor = getColorForIndex(currentIndex);

  const tr = document.createElement('tr');
  tr.dataset.index = currentIndex;

  // 1. Process ID Cell with Color Swatch
  const tdId = document.createElement('td');
  const idWrap = document.createElement('div');
  idWrap.className = 'process-id-cell';

  const colorTag = document.createElement('span');
  colorTag.className = 'process-color-tag';
  colorTag.style.backgroundColor = assignedColor;

  const idInput = document.createElement('input');
  idInput.type = 'text';
  idInput.className = 'input-field input-id';
  idInput.dataset.key = 'id';
  idInput.value = data.id;
  idInput.maxLength = 24;
  idInput.placeholder = 'P1';
  idInput.setAttribute('aria-label', `Process ID for row ${currentIndex + 1}`);

  idWrap.append(colorTag, idInput);
  tdId.append(idWrap);

  // 2. Arrival Time Cell
  const tdArr = document.createElement('td');
  const arrInput = document.createElement('input');
  arrInput.type = 'number';
  arrInput.className = 'input-field';
  arrInput.dataset.key = 'arrival';
  arrInput.min = '0';
  arrInput.max = '1000000';
  arrInput.value = data.arrival;
  arrInput.setAttribute('aria-label', `Arrival time for ${data.id}`);
  tdArr.append(arrInput);

  // 3. Burst Time Cell
  const tdBurst = document.createElement('td');
  const burstInput = document.createElement('input');
  burstInput.type = 'number';
  burstInput.className = 'input-field';
  burstInput.dataset.key = 'burst';
  burstInput.min = '1';
  burstInput.max = '1000000';
  burstInput.value = data.burst;
  burstInput.setAttribute('aria-label', `Burst time for ${data.id}`);
  tdBurst.append(burstInput);

  // 4. Priority Cell
  const tdPri = document.createElement('td');
  const priInput = document.createElement('input');
  priInput.type = 'number';
  priInput.className = 'input-field';
  priInput.dataset.key = 'priority';
  priInput.min = '0';
  priInput.max = '1000000';
  priInput.value = data.priority ?? 1;
  priInput.setAttribute('aria-label', `Priority for ${data.id}`);
  tdPri.append(priInput);

  // 5. Delete Action Cell
  const tdAction = document.createElement('td');
  tdAction.style.textAlign = 'center';
  const btnDel = document.createElement('button');
  btnDel.type = 'button';
  btnDel.className = 'btn-delete-row';
  btnDel.innerHTML = '&times;';
  btnDel.title = `Remove ${data.id}`;
  btnDel.onclick = () => {
    tr.remove();
    updateWorkloadMetadata();
  };
  tdAction.append(btnDel);

  tr.append(tdId, tdArr, tdBurst, tdPri, tdAction);
  rowsContainer.append(tr);
  updateWorkloadMetadata();
}

/**
 * Extracts normalized workload array from the DOM
 */
function getWorkload() {
  return [...rowsContainer.querySelectorAll('tr')].map(tr => {
    const inputs = [...tr.querySelectorAll('input')];
    return Object.fromEntries(inputs.map(input => {
      const key = input.dataset.key;
      const val = input.type === 'number' ? Number(input.value) : input.value.trim();
      return [key, val];
    }));
  });
}

/**
 * Loads a preset list of processes
 */
function loadPreset(processes, quantum = 2) {
  rowsContainer.innerHTML = '';
  quantumInput.value = quantum;
  processes.forEach(p => addProcessRow(p));
  hideMessage();
}

// Preset definitions
const PRESETS = {
  default: [
    { id: 'P1', arrival: 0, burst: 7, priority: 2 },
    { id: 'P2', arrival: 2, burst: 4, priority: 1 },
    { id: 'P3', arrival: 4, burst: 1, priority: 3 },
    { id: 'P4', arrival: 5, burst: 4, priority: 2 }
  ],
  variance: [
    { id: 'P1', arrival: 0, burst: 18, priority: 3 },
    { id: 'P2', arrival: 1, burst: 2, priority: 1 },
    { id: 'P3', arrival: 2, burst: 3, priority: 2 },
    { id: 'P4', arrival: 3, burst: 1, priority: 1 }
  ],
  staggered: [
    { id: 'P1', arrival: 0, burst: 5, priority: 2 },
    { id: 'P2', arrival: 4, burst: 4, priority: 1 },
    { id: 'P3', arrival: 8, burst: 2, priority: 3 },
    { id: 'P4', arrival: 12, burst: 6, priority: 2 }
  ]
};

// Event Listeners for Presets
document.querySelector('#btn-preset-default').onclick = () => loadPreset(PRESETS.default, 2);
document.querySelector('#btn-preset-variance').onclick = () => loadPreset(PRESETS.variance, 4);
document.querySelector('#btn-preset-staggered').onclick = () => loadPreset(PRESETS.staggered, 3);
document.querySelector('#btn-clear-workload').onclick = () => {
  rowsContainer.innerHTML = '';
  updateWorkloadMetadata();
};

// Quick quantum chips
document.querySelectorAll('.btn-chip').forEach(chip => {
  chip.onclick = () => {
    quantumInput.value = chip.dataset.q;
  };
});

document.querySelector('#add').onclick = () => addProcessRow();
document.querySelector('#refresh').onclick = loadHistory;

function showMessage(text, type = 'error') {
  messageBanner.className = `alert-banner ${type}`;
  messageBanner.textContent = text;
}

function hideMessage() {
  messageBanner.className = 'alert-banner';
  messageBanner.textContent = '';
}

/**
 * Simulation Runner (POST /api/simulate/)
 */
btnRun.onclick = async () => {
  const workload = getWorkload();
  const quantum = Number(quantumInput.value);

  // Client-side validations
  if (workload.length === 0) {
    showMessage('Please add at least 1 process to the workload.', 'error');
    return;
  }
  if (!Number.isInteger(quantum) || quantum < 1) {
    showMessage('Round Robin quantum must be a positive integer (≥ 1).', 'error');
    return;
  }

  // Ensure unique IDs
  const idSet = new Set();
  for (const p of workload) {
    if (!p.id) {
      showMessage('Each process must have a non-empty Process ID.', 'error');
      return;
    }
    if (idSet.has(p.id)) {
      showMessage(`Duplicate Process ID detected: "${p.id}". All process IDs must be unique.`, 'error');
      return;
    }
    idSet.add(p.id);
  }

  btnRun.disabled = true;
  btnRun.innerHTML = '<span class="run-icon">◌</span> <span>Simulating Workload...</span>';
  showMessage('Sending workload to scheduling engine...', 'success');

  try {
    const csrfTokenEl = document.querySelector('[name=csrfmiddlewaretoken]');
    const csrfToken = csrfTokenEl ? csrfTokenEl.value : '';

    const response = await fetch('/api/simulate/', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'X-CSRFToken': csrfToken
      },
      body: JSON.stringify({ processes: workload, quantum })
    });

    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || 'Simulation failed. Check workload inputs and try again.');
    }

    renderSimulationResults(data);
    showMessage(`Simulation complete! Run #${data.id} saved to database history.`, 'success');
    document.querySelector('#results-count-hint').textContent = '4 Policies Evaluated';
    loadHistory();

    // Smooth scroll to recommendation
    document.querySelector('#recommendation').scrollIntoView({ behavior: 'smooth', block: 'start' });
  } catch (err) {
    showMessage(err.message || 'Error communicating with scheduler server.', 'error');
  } finally {
    btnRun.disabled = false;
    btnRun.innerHTML = '<span class="run-icon">▶</span> <span>Run All Algorithms</span>';
  }
};

/**
 * Render all simulation results (Recommendation, Comparison, Gantt, Metrics)
 */
function renderSimulationResults(data) {
  const { results, recommendation, workload } = data;

  // Build processId -> color mapping based on workload order
  const processColorMap = new Map();
  (workload || []).forEach((p, idx) => {
    processColorMap.set(p.id, getColorForIndex(idx));
  });

  // 1. Render ML Recommendation Card
  renderRecommendationCard(recommendation);

  // 2. Render Aggregate Comparison Matrix
  renderComparisonMatrix(results, recommendation);

  // 3. Render Algorithm Result Cards (Gantt + Process Metrics)
  renderAlgorithmCards(results, recommendation, workload, processColorMap);
}

/**
 * Section 2: Recommendation Card
 */
function renderRecommendationCard(rec) {
  const card = document.querySelector('#recommendation');
  const recAlg = rec.algorithm;
  const score = typeof rec.score === 'number' ? rec.score.toFixed(3) : rec.score;

  // Format feature chips
  const featuresHtml = Object.entries(rec.features || {}).map(([key, val]) => {
    const label = key.replace(/_/g, ' ');
    const num = typeof val === 'number' ? val.toFixed(2) : val;
    return `<span class="feature-chip">${escapeHtml(label)}: <b>${escapeHtml(num)}</b></span>`;
  }).join('');

  card.innerHTML = `
    <div class="rec-container">
      <div class="rec-left">
        <div class="rec-icon-badge" title="Machine Learning Predictor">✦</div>
        <div class="rec-content">
          <span class="rec-overline">ML Powered Algorithm Recommendation</span>
          <h2 class="rec-title">${escapeHtml(recAlg)}</h2>
          <p class="rec-objective">
            ${escapeHtml(rec.objective)} &bull; ${escapeHtml(rec.model_status)}
          </p>
          <div class="rec-features-chips">
            ${featuresHtml}
          </div>
        </div>
      </div>
      <div class="rec-score-box">
        <div class="rec-score-val">${escapeHtml(score)}</div>
        <div class="rec-score-label">NORMALIZED WORKLOAD SCORE</div>
      </div>
    </div>
  `;
  card.classList.remove('hidden');
}

/**
 * Section 3: Aggregate Comparison Table
 */
function renderComparisonMatrix(results, recommendation) {
  const section = document.querySelector('#comparison-section');
  const tbody = document.querySelector('#comparison-tbody');
  tbody.innerHTML = '';

  const entries = Object.entries(results);
  if (!entries.length) return;

  // Find best values for highlights
  const minWait = Math.min(...entries.map(([, r]) => r.aggregate.average_waiting_time));
  const minTAT = Math.min(...entries.map(([, r]) => r.aggregate.average_turnaround_time));
  const minRT = Math.min(...entries.map(([, r]) => r.aggregate.average_response_time));
  const maxTP = Math.max(...entries.map(([, r]) => r.aggregate.throughput));
  const minSwitches = Math.min(...entries.map(([, r]) => r.aggregate.context_switches));
  const maxUtil = Math.max(...entries.map(([, r]) => r.aggregate.cpu_utilization));

  entries.forEach(([name, res]) => {
    const ag = res.aggregate;
    const isRecommended = name.toLowerCase() === String(recommendation.algorithm).toLowerCase();

    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td>
        <span class="comparison-alg-name">
          <span class="alg-dot" style="background:${isRecommended ? '#0d9488' : '#6366f1'}"></span>
          <strong>${escapeHtml(name)}</strong>
          ${isRecommended ? '<span class="badge-recommended">BEST FIT</span>' : ''}
        </span>
      </td>
      <td><span class="${ag.average_waiting_time === minWait ? 'best-val' : ''}">${ag.average_waiting_time.toFixed(2)}</span></td>
      <td><span class="${ag.average_turnaround_time === minTAT ? 'best-val' : ''}">${ag.average_turnaround_time.toFixed(2)}</span></td>
      <td><span class="${ag.average_response_time === minRT ? 'best-val' : ''}">${ag.average_response_time.toFixed(2)}</span></td>
      <td><span class="${ag.throughput === maxTP ? 'best-val' : ''}">${ag.throughput.toFixed(3)}</span></td>
      <td><span class="${ag.context_switches === minSwitches ? 'best-val' : ''}">${ag.context_switches}</span></td>
      <td><span class="${ag.cpu_utilization === maxUtil ? 'best-val' : ''}">${ag.cpu_utilization.toFixed(1)}%</span></td>
    `;
    tbody.append(tr);
  });

  section.classList.remove('hidden');
}

/**
 * Section 4: Result Cards with Gantt Charts and Process Metrics
 */
function renderAlgorithmCards(results, recommendation, workload, processColorMap) {
  const container = document.querySelector('#results');
  container.innerHTML = '';

  // Process legend HTML shared across cards
  const legendHtml = (workload || []).map((p, idx) => {
    const color = processColorMap.get(p.id) || getColorForIndex(idx);
    return `
      <span class="legend-item">
        <span class="legend-color-dot" style="background:${color};"></span>
        <span>${escapeHtml(p.id)}</span>
      </span>
    `;
  }).join('');

  Object.entries(results).forEach(([name, res], idx) => {
    const isRecommended = name.toLowerCase() === String(recommendation.algorithm).toLowerCase();
    const card = document.createElement('article');
    card.className = `result-card ${isRecommended ? 'is-recommended' : ''}`;

    const ag = res.aggregate;
    const timeline = res.timeline || [];
    const totalDuration = Math.max(...timeline.map(s => s.end), 1);

    // Build Gantt Segments
    const segmentsHtml = timeline.map(segment => {
      const isIdle = segment.process_id === 'IDLE';
      const duration = segment.end - segment.start;
      const flexGrow = Math.max(duration, 0.2);
      const color = isIdle ? '#94a3b8' : (processColorMap.get(segment.process_id) || '#6366f1');
      const title = isIdle
        ? `IDLE: time ${segment.start} to ${segment.end} (${duration} units)`
        : `${segment.process_id}: time ${segment.start} to ${segment.end} (burst ${duration} units)`;

      return `
        <div class="gantt-segment ${isIdle ? 'is-idle' : ''}"
             style="flex:${flexGrow}; background-color:${color};"
             title="${escapeHtml(title)}">
          <span>${escapeHtml(segment.process_id)}</span>
          <span class="segment-range">${segment.start}-${segment.end}</span>
        </div>
      `;
    }).join('');

    // Per-process metrics rows
    const procRowsHtml = (res.process_metrics || []).map(p => `
      <tr>
        <td><strong>${escapeHtml(p.process_id)}</strong></td>
        <td>${escapeHtml(p.completion_time)}</td>
        <td>${escapeHtml(p.waiting_time)}</td>
        <td>${escapeHtml(p.turnaround_time)}</td>
        <td>${escapeHtml(p.response_time)}</td>
      </tr>
    `).join('');

    card.innerHTML = `
      <div class="card-top">
        <div class="card-title-group">
          <span class="alg-dot" style="background:${getColorForIndex(idx)}"></span>
          <h3>${escapeHtml(name)}</h3>
        </div>
        ${isRecommended ? '<span class="badge-recommended">✦ ML RECOMMENDED</span>' : ''}
      </div>

      <!-- Gantt Chart -->
      <div class="timeline-block">
        <div class="timeline-chart" aria-label="${escapeHtml(name)} timeline">
          ${segmentsHtml}
        </div>
        <div class="timeline-ticks">
          <span>0</span>
          <span>${totalDuration} time units</span>
        </div>
      </div>

      <!-- Process Legend -->
      <div class="process-legend-bar">
        ${legendHtml}
      </div>

      <!-- Stat Tiles -->
      <div class="metrics-stat-grid">
        <div class="stat-tile">
          <div class="stat-tile-val">${ag.average_waiting_time.toFixed(2)}</div>
          <div class="stat-tile-label">AVG WAITING</div>
        </div>
        <div class="stat-tile">
          <div class="stat-tile-val">${ag.average_turnaround_time.toFixed(2)}</div>
          <div class="stat-tile-label">AVG TURNAROUND</div>
        </div>
        <div class="stat-tile">
          <div class="stat-tile-val">${ag.average_response_time.toFixed(2)}</div>
          <div class="stat-tile-label">AVG RESPONSE</div>
        </div>
        <div class="stat-tile">
          <div class="stat-tile-val">${ag.throughput.toFixed(3)}</div>
          <div class="stat-tile-label">THROUGHPUT</div>
        </div>
        <div class="stat-tile">
          <div class="stat-tile-val">${ag.context_switches}</div>
          <div class="stat-tile-label">SWITCHES</div>
        </div>
        <div class="stat-tile">
          <div class="stat-tile-val">${ag.cpu_utilization.toFixed(1)}%</div>
          <div class="stat-tile-label">CPU UTILIZATION</div>
        </div>
      </div>

      <!-- Detailed Metrics Table -->
      <div class="process-metrics-wrap">
        <table class="process-metrics-table">
          <thead>
            <tr>
              <th>PROCESS</th>
              <th>COMPLETION</th>
              <th>WAITING</th>
              <th>TURNAROUND</th>
              <th>RESPONSE</th>
            </tr>
          </thead>
          <tbody>
            ${procRowsHtml}
          </tbody>
        </table>
      </div>
    `;

    container.append(card);
  });
}

/**
 * Section 5: Load Run History (GET /api/history/)
 */
async function loadHistory() {
  const container = document.querySelector('#history');
  try {
    const res = await fetch('/api/history/');
    if (!res.ok) throw new Error('History service unavailable');
    const data = await res.json();

    if (!data.runs || !data.runs.length) {
      container.innerHTML = '<div class="history-empty">No simulations saved yet. Complete a simulation above to record history.</div>';
      return;
    }

    const rowsHtml = data.runs.map(run => {
      const dateStr = new Date(run.created_at).toLocaleString();
      const workloadJson = JSON.stringify(run.workload || []).replace(/"/g, '&quot;');
      const q = run.quantum || 2;

      return `
        <tr>
          <td><span class="history-run-id">#${run.id}</span></td>
          <td>${escapeHtml(dateStr)}</td>
          <td>${run.process_count} processes</td>
          <td>q=${q}</td>
          <td><strong>${escapeHtml(run.recommendation)}</strong></td>
          <td style="text-align: right;">
            <button type="button" class="btn btn-ghost" style="padding:4px 8px; font-size:10px;"
                    onclick="restoreRunWorkload('${workloadJson}', ${q})">
              Load Workload
            </button>
          </td>
        </tr>
      `;
    }).join('');

    container.innerHTML = `
      <div class="table-container">
        <table class="history-table">
          <thead>
            <tr>
              <th>RUN ID</th>
              <th>TIMESTAMP</th>
              <th>WORKLOAD</th>
              <th>QUANTUM</th>
              <th>RECOMMENDED</th>
              <th style="text-align: right;">ACTION</th>
            </tr>
          </thead>
          <tbody>
            ${rowsHtml}
          </tbody>
        </table>
      </div>
    `;
  } catch {
    container.innerHTML = '<div class="history-empty">Could not load run history. Click Refresh to retry.</div>';
  }
}

/**
 * Restores a past workload from history into the studio
 */
window.restoreRunWorkload = function(jsonStr, quantum) {
  try {
    const unescaped = jsonStr.replace(/&quot;/g, '"');
    const processes = JSON.parse(unescaped);
    loadPreset(processes, quantum);
    showMessage(`Loaded workload from historical run into editor.`, 'success');
    document.querySelector('#workload-section').scrollIntoView({ behavior: 'smooth' });
  } catch (err) {
    showMessage('Could not parse historical workload data.', 'error');
  }
};

// Initialize with default balanced sample and load initial history
loadPreset(PRESETS.default, 2);
loadHistory();

// Auto-run simulation if URL query parameter autorun=1 is passed
if (new URLSearchParams(window.location.search).get('autorun') === '1') {
  setTimeout(() => document.querySelector('#run').click(), 250);
}
