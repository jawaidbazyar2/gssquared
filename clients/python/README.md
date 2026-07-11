# gs2debug

Python client for the GSSquared external debug protocol ([Docs/DebugProtocol.md](../../Docs/DebugProtocol.md)).

## Install (editable)

```bash
cd clients/python
pip install -e .
```

## Smoke test

With the emulator listening:

```bash
# terminal 1
./build/GSSquared --debug /tmp/gs2.sock -p 1

# terminal 2
cd clients/python
PYTHONPATH=src python examples/hello_ping.py /tmp/gs2.sock
```

## Tests (framing only)

```bash
cd clients/python
pip install -e ".[dev]"
pytest
```
