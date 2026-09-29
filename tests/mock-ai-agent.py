"""Offline process fixture. Never connects to an AI provider."""
import json, sys
from pathlib import Path
request=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
marker=Path(request['rateStateDir'])/'offline-retry.marker'
marker.parent.mkdir(parents=True,exist_ok=True)
if not marker.exists():
    marker.write_text('first attempt')
    print('Offline simulated failure',file=sys.stderr)
    sys.exit(1)
assert not request['messages'], 'Retry duplicated the previous user prompt'
print(json.dumps({'type':'answer','text':'OFFLINE_RETRY_OK'}),flush=True)
