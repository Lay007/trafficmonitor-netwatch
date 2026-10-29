"""Byte-for-byte checks against immutable upstream files, with no network access."""
import hashlib
from pathlib import Path

root = Path(__file__).resolve().parents[1]
expected = {
    "vendor/PluginInterface.h": "1760d2cdf15da70a567bff367e79e470a8ee6ab33084c19a0e61a50153958f9b",
    "vendor/TrafficMonitor.LICENSE": "7523fa7601abdf090a91ca365d3a768e5418292dc151038d0629e67240a33fe6",
}
for name, digest in expected.items():
    actual = hashlib.sha256((root / name).read_bytes()).hexdigest()
    if actual != digest:
        raise SystemExit(f"FAIL upstream file changed: {name}: {actual}")
    print(f"PASS pinned upstream bytes: {name}")
