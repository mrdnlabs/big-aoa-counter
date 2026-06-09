const log = (message) => {
  const el = document.getElementById('status-log');
  el.textContent = `${new Date().toLocaleTimeString()} ${message}\n${el.textContent}`.trim();
};

let refreshTimer = null;
let refreshInFlight = false;
let formDirty = false;
let latestStatus = null;
let overlayDrag = null;

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

function clamp(value, min, max) {
  return Math.min(Math.max(value, min), max);
}

function numberOrDefault(value, defaultValue) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : defaultValue;
}

function formatNormalized(value) {
  const rounded = Math.abs(value) < 0.0005 ? 0 : value;
  return rounded.toFixed(3).replace(/(\.\d*?)0+$/, '$1').replace(/\.$/, '');
}

function showScenarioNameEnabled(value) {
  return value !== 'false';
}

function syncScenarioNameToggle(value) {
  const checkbox = document.getElementById('show-scenario-name');
  const hidden = document.getElementById('show-scenario-name-value');
  const enabled = showScenarioNameEnabled(value);
  checkbox.checked = enabled;
  hidden.value = enabled ? 'true' : 'false';
}

function getOverlayMetrics(rect) {
  const form = document.getElementById('config-form');
  const widthPercent = clamp(numberOrDefault(form.elements.OverlayWidthPercent.value, 100), 33, 100);
  const scale = clamp(numberOrDefault(form.elements.OverlayScalePercent.value, 100), 50, 200);
  const width = rect.width * (widthPercent / 100);
  const height = clamp((rect.width / 7) * (scale / 100), 42, rect.height);
  return { width, height };
}

function setOverlayPositionFromCenter(centerX, centerY) {
  const preview = document.getElementById('overlay-preview');
  const rect = preview.getBoundingClientRect();
  if (!rect.width || !rect.height) {
    return;
  }

  const metrics = getOverlayMetrics(rect);
  const minX = metrics.width / 2;
  const minY = metrics.height / 2;
  const maxX = rect.width - minX;
  const maxY = rect.height - minY;
  const clampedCenterX = minX > maxX ? rect.width / 2 : clamp(centerX, minX, maxX);
  const clampedCenterY = minY > maxY ? rect.height / 2 : clamp(centerY, minY, maxY);

  document.getElementById('overlay-x').value = formatNormalized((clampedCenterX / rect.width) * 2 - 1);
  document.getElementById('overlay-y').value = formatNormalized((clampedCenterY / rect.height) * 2 - 1);
  updateOverlayPreview();
}

function updateOverlayPreview() {
  const form = document.getElementById('config-form');
  const preview = document.getElementById('overlay-preview');
  const sample = document.getElementById('overlay-sample');
  const rect = preview.getBoundingClientRect();
  if (!rect.width || !rect.height) {
    return;
  }

  const metrics = getOverlayMetrics(rect);
  const x = clamp(numberOrDefault(form.elements.OverlayX.value, 0), -1, 1);
  const y = clamp(numberOrDefault(form.elements.OverlayY.value, -0.72), -1, 1);
  const minX = metrics.width / 2;
  const minY = metrics.height / 2;
  const maxX = rect.width - minX;
  const maxY = rect.height - minY;
  let centerX = ((x + 1) / 2) * rect.width;
  let centerY = ((y + 1) / 2) * rect.height;
  centerX = minX > maxX ? rect.width / 2 : clamp(centerX, minX, maxX);
  centerY = minY > maxY ? rect.height / 2 : clamp(centerY, minY, maxY);

  form.elements.OverlayX.value = formatNormalized((centerX / rect.width) * 2 - 1);
  form.elements.OverlayY.value = formatNormalized((centerY / rect.height) * 2 - 1);
  sample.style.width = `${metrics.width}px`;
  sample.style.height = `${metrics.height}px`;
  sample.style.left = `${centerX - metrics.width / 2}px`;
  sample.style.top = `${centerY - metrics.height / 2}px`;

  const count = latestStatus?.state?.count ?? document.getElementById('count').textContent;
  const label = form.elements.Label.value || latestStatus?.config?.Label || 'Object count';
  const scenario = latestStatus?.state?.scenario_name || 'AOA scenario';
  const scenarioVisible = showScenarioNameEnabled(form.elements.ShowScenarioName.value);
  document.getElementById('overlay-preview-count').textContent = count;
  document.getElementById('overlay-preview-label').textContent = label;
  document.getElementById('overlay-preview-scenario').textContent = scenario;
  document.getElementById('overlay-preview-scenario').hidden = !scenarioVisible;
  sample.classList.toggle('scenario-hidden', !scenarioVisible);
  sample.dataset.position = `x ${form.elements.OverlayX.value}, y ${form.elements.OverlayY.value}`;
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
  latestStatus = status;
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
    syncScenarioNameToggle(status.config.ShowScenarioName);
    updateDynamicSlotHint(form.elements.DynamicTextSlot.value);
  }
  updateOverlayPreview();
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

document.querySelector('[name="Label"]').addEventListener('input', updateOverlayPreview);
document.querySelector('[name="OverlayScalePercent"]').addEventListener('input', updateOverlayPreview);
document.querySelector('[name="OverlayWidthPercent"]').addEventListener('change', updateOverlayPreview);

document.getElementById('show-scenario-name').addEventListener('change', (event) => {
  formDirty = true;
  syncScenarioNameToggle(event.target.checked ? 'true' : 'false');
  updateOverlayPreview();
});

document.getElementById('overlay-sample').addEventListener('pointerdown', (event) => {
  if (event.button !== undefined && event.button !== 0) {
    return;
  }
  const previewRect = document.getElementById('overlay-preview').getBoundingClientRect();
  const sampleRect = event.currentTarget.getBoundingClientRect();
  overlayDrag = {
    offsetX: event.clientX - (sampleRect.left + sampleRect.width / 2),
    offsetY: event.clientY - (sampleRect.top + sampleRect.height / 2)
  };
  event.currentTarget.setPointerCapture(event.pointerId);
  setOverlayPositionFromCenter(
    event.clientX - previewRect.left - overlayDrag.offsetX,
    event.clientY - previewRect.top - overlayDrag.offsetY
  );
  formDirty = true;
  event.preventDefault();
});

document.getElementById('overlay-sample').addEventListener('pointermove', (event) => {
  if (!overlayDrag) {
    return;
  }
  const previewRect = document.getElementById('overlay-preview').getBoundingClientRect();
  setOverlayPositionFromCenter(
    event.clientX - previewRect.left - overlayDrag.offsetX,
    event.clientY - previewRect.top - overlayDrag.offsetY
  );
  formDirty = true;
});

document.getElementById('overlay-sample').addEventListener('pointerup', () => {
  overlayDrag = null;
});

document.getElementById('overlay-sample').addEventListener('pointercancel', () => {
  overlayDrag = null;
});

document.getElementById('overlay-sample').addEventListener('keydown', (event) => {
  const step = event.shiftKey ? 0.1 : 0.02;
  let deltaX = 0;
  let deltaY = 0;
  if (event.key === 'ArrowLeft') {
    deltaX = -step;
  } else if (event.key === 'ArrowRight') {
    deltaX = step;
  } else if (event.key === 'ArrowUp') {
    deltaY = -step;
  } else if (event.key === 'ArrowDown') {
    deltaY = step;
  } else {
    return;
  }
  const form = document.getElementById('config-form');
  form.elements.OverlayX.value = formatNormalized(
    clamp(numberOrDefault(form.elements.OverlayX.value, 0) + deltaX, -1, 1)
  );
  form.elements.OverlayY.value = formatNormalized(
    clamp(numberOrDefault(form.elements.OverlayY.value, -0.72) + deltaY, -1, 1)
  );
  updateOverlayPreview();
  formDirty = true;
  event.preventDefault();
});

window.addEventListener('resize', updateOverlayPreview);

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
