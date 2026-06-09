const log = (message) => {
  const el = document.getElementById('status-log');
  el.textContent = `${new Date().toLocaleTimeString()} ${message}\n${el.textContent}`.trim();
};

let refreshTimer = null;
let refreshInFlight = false;
let formDirty = false;

const categoryOptions = [
  ['total', 'Overall count for the selected scenario.'],
  ['totalVehicle', 'Sum of car, bike, bus, truck, and other vehicle counts.'],
  ['totalHuman', 'Human detections counted by the scenario.'],
  ['totalCar', 'Car detections counted by the scenario.'],
  ['totalBike', 'Bike detections counted by the scenario.'],
  ['totalBus', 'Bus detections counted by the scenario.'],
  ['totalTruck', 'Truck detections counted by the scenario.'],
  ['totalOtherVehicle', 'Other vehicle detections that do not match the main vehicle classes.']
];

function setCategoryDescription(value) {
  const description = categoryOptions.find(([optionValue]) => optionValue === value)?.[1] ||
    'Overall count for the selected scenario.';
  document.getElementById('category-description').textContent = description;
}

function updateDynamicSlotHint(value) {
  const hint = document.getElementById('dynamic-slot-hint');
  const slot = Number(value);
  if (Number.isInteger(slot) && slot >= 1 && slot <= 16) {
    hint.textContent = `Axis overlay modifier for this slot: #D${slot}.`;
    return;
  }
  hint.textContent = 'Enter a slot from 1 to 16 to see the matching #D modifier.';
}

function populateCategorySelect(selectedValue) {
  const categorySelect = document.getElementById('category-select');
  categorySelect.innerHTML = '';
  categoryOptions.forEach(([value]) => {
    const option = document.createElement('option');
    option.value = value;
    option.textContent = value;
    categorySelect.appendChild(option);
  });
  categorySelect.value = selectedValue || 'total';
  setCategoryDescription(categorySelect.value);
}

function formatMeta(state) {
  const scenario = state.scenario_name || 'No scenario selected';
  const type = state.scenario_type || 'unknown scenario type';
  const timestamp = state.last_update ? `updated ${state.last_update}` : 'live count';
  return `${scenario} | ${type} | ${timestamp}`;
}

function updateWidgetUrl() {
  const widgetUrl = new URL('widget.html', window.location.href).href;
  const widgetInput = document.getElementById('widget-url');
  if (widgetInput) {
    widgetInput.value = widgetUrl;
  }
}

async function fetchJson(url, options) {
  const response = await fetch(url, options);
  const text = await response.text();
  const data = text ? JSON.parse(text) : {};
  if (!response.ok) {
    throw new Error(data.error || `HTTP ${response.status}`);
  }
  return data;
}

async function loadStatus() {
  const status = await fetchJson('api/status');
  document.getElementById('label').textContent = status.config.Label;
  document.getElementById('count').textContent = status.state.count;
  document.getElementById('meta').textContent = formatMeta(status.state);

  const form = document.getElementById('config-form');
  if (!formDirty) {
    const scenarioSelect = document.getElementById('scenario-select');
    populateCategorySelect(status.config.Category);
    scenarioSelect.innerHTML = '<option value="">Auto-select first countable scenario</option>';
    (status.available_scenarios || []).forEach((scenario) => {
      const option = document.createElement('option');
      option.value = scenario.id;
      option.textContent = `${scenario.name} (${scenario.type})`;
      scenarioSelect.appendChild(option);
    });

    Object.entries(status.config).forEach(([key, value]) => {
      if (form.elements[key]) {
        form.elements[key].value = value;
      }
    });
    updateDynamicSlotHint(form.elements.DynamicTextSlot.value);
  }
  return status;
}

function scheduleAutoRefresh(intervalMs) {
  const delay = Math.max(Number(intervalMs) || 1000, 500);
  if (refreshTimer) {
    window.clearTimeout(refreshTimer);
  }
  refreshTimer = window.setTimeout(async () => {
    if (!refreshInFlight) {
      refreshInFlight = true;
      try {
        const status = await loadStatus();
        scheduleAutoRefresh(status.config.PollIntervalMs);
      } catch (error) {
        log(error.message);
        scheduleAutoRefresh(delay);
      } finally {
        refreshInFlight = false;
      }
      return;
    }
    scheduleAutoRefresh(delay);
  }, delay);
}

document.getElementById('category-select').addEventListener('change', (event) => {
  formDirty = true;
  setCategoryDescription(event.target.value);
});

document.querySelector('[name="DynamicTextSlot"]').addEventListener('input', (event) => {
  formDirty = true;
  updateDynamicSlotHint(event.target.value);
});

document.getElementById('config-form').addEventListener('input', () => {
  formDirty = true;
});

document.getElementById('config-form').addEventListener('change', () => {
  formDirty = true;
});

document.getElementById('copy-widget-url').addEventListener('click', async () => {
  const widgetInput = document.getElementById('widget-url');
  try {
    await navigator.clipboard.writeText(widgetInput.value);
    log('Widget URL copied');
  } catch (error) {
    widgetInput.select();
    document.execCommand('copy');
    log('Widget URL copied');
  }
});

document.getElementById('config-form').addEventListener('submit', async (event) => {
  event.preventDefault();
  const payload = Object.fromEntries(new FormData(event.target).entries());
  await fetchJson('api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });
  formDirty = false;
  log('Config saved');
  const status = await loadStatus();
  scheduleAutoRefresh(status.config.PollIntervalMs);
});

document.querySelectorAll('button[data-action]').forEach((button) => {
  button.addEventListener('click', async () => {
    const action = button.dataset.action;
    if (action === 'refresh') {
      const status = await loadStatus();
      scheduleAutoRefresh(status.config.PollIntervalMs);
      log('Status refreshed');
      return;
    }

    if (action === 'discover') {
      await fetchJson('api/discover', { method: 'POST' });
      formDirty = false;
      log('Scenario selection reset to automatic');
      const status = await loadStatus();
      scheduleAutoRefresh(status.config.PollIntervalMs);
      return;
    }

    if (action === 'reset') {
      await fetchJson('api/reset', { method: 'POST' });
      formDirty = false;
      log('Accumulated count reset');
      const status = await loadStatus();
      scheduleAutoRefresh(status.config.PollIntervalMs);
      return;
    }

    if (action === 'alarm') {
      await fetchJson('api/send-alarm', { method: 'POST' });
      log('Alarm event sent');
      return;
    }
  });
});

loadStatus()
  .then((status) => {
    updateWidgetUrl();
    scheduleAutoRefresh(status.config.PollIntervalMs);
  })
  .catch((error) => log(error.message));
