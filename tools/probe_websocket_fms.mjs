// Node 22+: read-only X-Plane Web API probe; no packages required.
import { mkdir, writeFile } from 'node:fs/promises';
const base = 'http://127.0.0.1:8086';
const endpoint = 'ws://127.0.0.1:8086/api/v3';
const get = async path => {
  const response = await fetch(base + path, {
    headers: { Connection: 'close' }, signal: AbortSignal.timeout(5000),
  });
  if (!response.ok) throw new Error(`${path}: HTTP ${response.status}`);
  return response.json();
};
const capabilities = await get('/api/capabilities');
const { data: registry } = await get('/api/v3/datarefs');
const names = [
  'AirbusFBW/WPT_ID', 'AirbusFBW/WPT_ID_FO', 'AirbusFBW/DistToDest',
  'AirbusFBW/MCDU1titlew', 'AirbusFBW/MCDU1stitlew',
  'AirbusFBW/MCDU1cont1g', 'AirbusFBW/MCDU1cont6w',
  'AirbusFBW/MCDU2titlew', 'sim/time/total_running_time_sec',
];
const candidates = [
  'departure_icao', 'destination_icao', 'alternate_icao', 'no_wp', 'wpid', 'dest_index',
].map(name => 'toliss_airbus/flightplan/' + name);
const selected = registry.filter(ref => names.includes(ref.name));
if (!selected.length) throw new Error('No probe datarefs registered');
const byId = new Map(selected.map(ref => [String(ref.id), ref]));
const report = {
  started_at: new Date().toISOString(), endpoint, capabilities,
  registry_count: registry.length,
  candidate_presence: Object.fromEntries(candidates.map(name => [name, registry.some(ref => ref.name === name)])),
  route_related_metadata: registry.filter(ref => /flight.?plan|fms.*(entry|waypoint|latitude|longitude)|gps.*(next|prev)|WPT_ID|DistToDest/i.test(ref.name)),
  selected, requests: [], messages: [], latest: {},
};
let socket;
let samplingTimer;
try {
  await new Promise((resolve, reject) => {
    socket = new WebSocket(endpoint);
    const deadline = setTimeout(() => { socket.close(); reject(new Error('WebSocket probe timed out')); }, 15000);
    const send = message => { report.requests.push(message); socket.send(JSON.stringify(message)); };
    socket.addEventListener('open', () => send({
      req_id: 1, type: 'dataref_subscribe_values',
      params: { datarefs: selected.map(ref => ({ id: ref.id })) },
    }));
    socket.addEventListener('message', event => {
      try {
        const message = JSON.parse(event.data);
        report.messages.push({ received_at: new Date().toISOString(), message });
        if (message.type === 'result' && message.success === false) throw new Error(JSON.stringify(message));
        if (message.type === 'result' && message.req_id === 1) {
          samplingTimer = setTimeout(() => send({
            req_id: 2, type: 'dataref_unsubscribe_values', params: { datarefs: 'all' },
          }), 8000);
        }
        if (message.type === 'dataref_update_values') {
          for (const [id, value] of Object.entries(message.data)) {
            const ref = byId.get(id);
            if (ref) report.latest[ref.name] = ref.value_type === 'data'
              ? { raw: value, text: Buffer.from(value, 'base64').toString('utf8').replace(/\0.*$/s, '') }
              : { value };
          }
        }
        if (message.type === 'result' && message.req_id === 2) socket.close();
      } catch (error) { socket.close(); clearTimeout(deadline); reject(error); }
    });
    socket.addEventListener('error', event => { clearTimeout(deadline); reject(new Error(`WebSocket error: ${event.message}; ${event.error}; ${event.error?.cause ?? ''}`)); });
    socket.addEventListener('close', event => {
      clearTimeout(deadline);
      const unsubscribed = report.messages.some(({ message }) => message.req_id === 2 && message.success);
      if (unsubscribed) resolve();
      else reject(new Error(`Socket closed before unsubscribe acknowledgement: ${event.code}`));
    });
  });
  if (!Object.keys(report.latest).length) throw new Error('No dataref values received');
} catch (error) {
  report.error = String(error);
  process.exitCode = 1;
} finally {
  clearTimeout(samplingTimer);
  if (socket && socket.readyState < 2) socket.close();
  report.ended_at = new Date().toISOString();
  const path = new URL('../reports/runtime/websocket-fms-20261027.json', import.meta.url);
  await mkdir(new URL('.', path), { recursive: true });
  await writeFile(path, JSON.stringify(report, null, 2) + '\n');
  console.log(JSON.stringify({ error: report.error, capabilities, candidate_presence: report.candidate_presence,
    message_count: report.messages.length, latest: report.latest, output: path.pathname }, null, 2));
}
