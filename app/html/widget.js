let widgetTimer = null;

async function fetchWidgetStatus() {
  const response = await fetch('/api/status');
  const text = await response.text();
  const data = text ? JSON.parse(text) : {};
  if (!response.ok) {
    throw new Error(data.error || `HTTP ${response.status}`);
  }
  return data;
}

function formatWidgetMeta(state) {
  const scenario = state.scenario_name || 'No scenario selected';
  const type = state.scenario_type || 'unknown scenario type';
  const timestamp = state.last_update ? `updated ${state.last_update}` : 'live count';
  return `${scenario} | ${type} | ${timestamp}`;
}

async function refreshWidget() {
  try {
    const status = await fetchWidgetStatus();
    document.getElementById('widget-label').textContent = status.config.Label;
    document.getElementById('widget-count').textContent = status.state.count;
    document.getElementById('widget-meta').textContent = formatWidgetMeta(status.state);
    const interval = Math.max(Number(status.config.PollIntervalMs) || 1000, 500);
    widgetTimer = window.setTimeout(refreshWidget, interval);
  } catch (error) {
    document.getElementById('widget-meta').textContent = error.message;
    widgetTimer = window.setTimeout(refreshWidget, 1000);
  }
}

refreshWidget();
