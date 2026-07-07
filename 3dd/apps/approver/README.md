## approver — the operator's approval console

A bare-bones web UI, co-located with the **guard** on the OT side, that lets an
operator drive the guard's global **approve/deny** switch. Clicking a button
sends a plaintext `approve`/`deny` datagram straight to the guard's control port
(`4999`) — the operator commands the guard directly, with no broker in between.

This is the OT-side counterpart to nettest: nettest (IT-side) drives traffic but
can no longer touch approvals, so **nothing on the untrusted IT side can
influence the gate**. A global **deny** also disconnects live sessions, not just
future requests (handled at the guard).

### Running

Included automatically in any configuration with the guard
(`docker/1node`, `docker/3node` guardnode). Open <http://localhost:8082>.

Configuration via environment:

    GUARD_HOST (guard)  GUARD_CONTROL_PORT (4999)  HTTP_PORT (8082)

### Under the hood

```
apps/approver/
├── server.py          stdlib HTTP server + UDP sender
└── static/index.html  Approve/Deny UI
```

- `POST /api/approval` `{"mode": "approve"|"deny"}` → sends the datagram to the
  guard and remembers the last command for the session.
- `GET /api/state` → the last command sent this session (the approver cannot read
  the guard's switch back; the guard defaults to DENY on startup).

The switch is deliberately global and coarse — a real per-request operator
decision is future work.
