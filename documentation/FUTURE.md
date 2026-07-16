# Towards Enterprise-Grade Remote Access over a Data-Diode: Hardware-Enforced Client Verification

**Author:** Maurice Snoeren
**Status:** design article / future work (not implemented)
**Version:** working draft, 2026

> **Disclaimer:** This is a design article describing *future work*. Nothing in this
> document is implemented in the repository yet. It is meant to guide the next step
> of the research and to be honest about what is possible, what is not, and where the
> hard limits are. Do not read any part of this as a finished, trustworthy security
> control.

## 1. Where we are, and what "enterprise-grade" would add

The current solution (see [PAPER.md](PAPER.md) and the [main README](../README.md)) already
does the hard part: it carries interactive remote access over a data-diode architecture
while keeping the two networks physically separated, and it filters the Guacamole protocol
on a guard so that only keystrokes, mouse movements and screen updates cross. Under an
"assume breach" model it stops lateral movement and it limits what can be sent to input
and video.

There is one risk class that the diode does **not** solve, because it lives on the
application layer (L7), in front of the diode. Two terms first, to avoid confusion. A
**remote user** is the person seeking remote access from the low side (a vendor or a
maintenance engineer, for example). An **operator** is the trusted high-side person
responsible for security, who approves incoming connections. These are different roles on
different sides of the diode; the risks below concern the remote user's side.

1. **Credential theft / phishing.** If an attacker obtains a remote user's credentials, they
   can log in from any device. The brokers and the guard have no idea *who* is on the other
   end; they forward the keystrokes regardless.
2. **Compromise of the internet-facing web tier or the low-side broker.** If the Guacamole
   web application (Tomcat) or the `gmlbroker` is taken over through a vulnerability, the
   attacker can generate keystroke streams directly on that machine and push them at the
   diode. The guard will happily filter and forward them, because they *are* valid
   keystrokes.

There is a related control already on the roadmap that is worth placing next to these risks:
the **approval process** (see [PAPER.md](PAPER.md) §8 and the approval example in the proxies).
Approval puts a human on the trusted OT side in the loop — nothing crosses the guard until an
operator explicitly grants the session. That is a strong, independent check, and it belongs
exactly where it is: on the trusted side, where a compromised low side cannot reach it. But
against the two risks above it has a fundamental weakness. Approval only answers *"should this
session be allowed?"*, and that decision is made by a person who can be deceived. An attacker
with stolen credentials, or one sitting on a compromised web tier, can present a
plausible-looking session and try to get the operator to click *approve*. Approval mitigates by
requiring consent; it does not verify *who* is actually on the other end.

Hardware-enforced client verification is the complementary — and stronger — control: it
authenticates the *client* cryptographically **before a connection can even be established**, so
a session backed by an unapproved key never reaches the operator's approval decision in the
first place. The two are best used together. Authentication removes the impostors up front —
only a holder of an approved hardware key can open a session at all — and approval then lets a
trusted-side human still consciously grant or refuse each individual, *already-authenticated*
session. Authentication answers *"who is this?"*; approval answers *"do we allow them right
now?"*. Neither replaces the other, and the combination is far harder to defeat than either
alone: an attacker would have to both possess an approved remote user's hardware key *and*
convince an operator on the trusted side to approve the session.

Commercial products in this space (Waterfall HERA, DataFlowX) close this gap by binding the
remote-access session to **hardware the remote user physically holds** — a smart card, a
YubiKey, or a TPM-backed key — and by verifying that binding on the trusted side. This
article is about how to bring that same property, *hardware-enforced client verification*,
to this open-source project.

The goal, stated precisely:

> A session must only be accepted on the OT (high) side if the remote user can prove
> **fresh possession of an approved hardware key**, and that proof must be verified
> **independently on the trusted side of the diode** — so that even a fully compromised
> web tier and low-side broker cannot fabricate an accepted session.

## 2. This is not in Apache Guacamole — and that is deliberate

The natural first question is: does Guacamole already do this, so that we only have to turn
it on? The answer, after going through the source, the protocol reference, the extension
documentation and the maintainers' own statements, is **no** — and it is a deliberate
design decision, not a gap we can fill with configuration.

**guacd authenticates nothing.** There is no authentication mechanism between the web
application and guacd. guacd trusts whatever connects to it; security relies entirely on
network isolation and firewalling. The `guacd-ssl` option is transport encryption
(server-side TLS), not client verification [1].

**The handshake carries only an *unverified* identity.** The Guacamole handshake has a
`name` instruction: an optional, human-readable user name that the web application sends to
guacd. guacd does not verify it — it is meant for display and auditing in shared sessions,
not for security [2]. So an identity field *does* reach guacd, but it is an unauthenticated
assertion by the (untrusted) low side. (Note: `name` is already on the guard's opcode
allowlist, see [`guard_opcode_parser.cpp`](../proxies/gmguard/src/guard_opcode_parser.cpp).)

**One mitigation this architecture already provides for free.** guacd's blind trust in its
peer is normally a real weakness, but it depends on one assumption: that guacd is only
reachable by something trusted. In a normal deployment that assumption rests on firewall rules
— software, and therefore fallible. In this architecture it is enforced *physically*. guacd is
not reachable from the outside, and not even from the high-side network in general: its only
peer is the `gcdbroker`, and the `gcdbroker`'s guacd-bound input arrives *exclusively* over the
data-diode from the guard. There is no network route by which an attacker — on the low side, or
anywhere else — can reach guacd directly or inject into it except through the guard's filter.
So the "guacd authenticates nothing" risk is contained here in a way it is not in a normal
Guacamole install: the one thing guacd trusts is fed only by filtered, diode-separated traffic.
This does not remove the need for the client-verification work below (a compromised low side
can still send *valid* keystrokes that pass the filter), but it does mean the attack surface of
guacd itself is far smaller than the plain "guacd trusts whatever connects to it" statement
suggests.

**`auth-sso-ssl` stops at the web-app login, by design and on the record.** The SSL/smart-card
SSO extension authenticates the user *to the Guacamole web application* (it derives a
username from the certificate's subject DN) and nothing more. The author of the extension
states it explicitly [3][4]:

> "This *does not* mean that you can pass through that authentication result to an RDP
> server that accepts smart cards. That's an entirely different can of worms."

**Smart-card redirection to the remote (RDP) session is not implemented.** FreeRDP supports
smart-card pass-through, but Guacamole does not expose it, and the maintainers confirm it is
not implemented [5].

**The whole authentication surface is the web-app login.** The 1.6.0 manual's extension list
— LDAP/Active Directory, MFA (Duo, TOTP), SSO (CAS, OpenID, SAML, smart cards/SSL), RADIUS,
external auth (JSON, HTTP header), vault — is, without exception, about establishing the
user's identity *at the web-app login* [6]. None of it carries a verified identity down to
guacd or to the remote session.

The conclusion is important for framing this work: **what we want does not exist anywhere in
Guacamole, in any form, and that is by design.** guacd is meant to sit behind a trust
boundary and trust the web app blindly. That is not a setback for this project — it is the
justification for it. A guard behind the diode would not be "an extra check on top of
Guacamole's verification"; it would be the *only* place where client identity beyond the
web-app login is ever cryptographically verified. That is precisely the gap the commercial
products fill and the open-source stack leaves open.

## 3. The core design problem: a certificate is not a proof

It is tempting to think: "let the reverse proxy do mTLS with the remote user's YubiKey, and
forward the client certificate across the diode; the guard checks it against a whitelist of
approved certificates. Done." This is attractive and it is **wrong** in the exact way that
matters, so it is worth being explicit about why.

### 3.1 Where the proof of possession happens

Mutual TLS proves one thing: that the client holds the *private key* matching the
certificate. In the `auth-sso-ssl` setup this proof is verified at the **reverse proxy**,
which then forwards the *public* client certificate to the web app in the
`X-Client-Certificate` header (URL-encoded PEM) plus an `X-Client-Verified: SUCCESS` flag
[3]. The proof of possession is *consumed at the proxy*. It does not travel any further.

### 3.2 Why forwarding the certificate to the guard buys almost nothing against compromise

A certificate is public by nature — it is not a secret. Worse, in this setup the full PEM of
every remote user flows through the web app in the `X-Client-Certificate` header, so a
compromised web app has literally seen all of them. If the guard's only check is "is this
forwarded fingerprint on my whitelist?", then:

- An attacker who has compromised the **web app or the `gmlbroker`** is already *behind* the
  proxy. They do not connect through the proxy to reach the bridge; the low side opens that
  connection itself, and the bridge verifies no client key. Now, the whitelist itself lives on
  the **guard**, on the trusted side, so the compromised web app cannot *read* it — that is a
  real and important property, and it is worth stating clearly. But it does not save this
  scheme, because the attacker does not need to read the whitelist. The value being
  whitelisted is a **public certificate**, not a secret, and at least one such value passes
  through the attacker's own compromised machine every time a legitimate remote user connects
  (the full PEM arrives in the `X-Client-Certificate` header). The attacker simply keeps a copy
  of a real user's certificate and re-asserts its fingerprint. That fingerprint is guaranteed to
  be on the whitelist — it belongs to a legitimate user — even though the attacker never saw the
  whitelist. Certificates are public by design (that is the whole point of PKI: only the private
  key is secret), so a whitelist of certificates is an *identity list*, not a proof, and copying
  a listed identity is trivial for anyone positioned to observe one.
- The requirement to "show a valid certificate" only bites for someone *in front of* the
  mTLS termination, i.e. an outsider with a stolen password but no YubiKey. That person is
  stopped — which is real anti-phishing value — but a compromised web tier is past that
  point.

So: **"the attacker still needs to show a good certificate" is only true in front of the
proxy.** Once you are inside a compromised Guacamole, the certificate degrades from a proof
into a copyable string. To defend against compromise you need a *fresh proof of possession
of the private key*, verified by the guard, over a value the guard itself chose.

### 3.3 The one-way diode, and the return channel that makes challenge-response possible

Normally a fresh proof of possession is a challenge-response: the verifier sends a random
nonce, the client signs it. The obvious objection is that a data-diode is one-way, so the
trusted-side guard cannot send a nonce back toward the client.

That objection is **not** fatal here, and this is the key architectural insight. The bridge
already has a return path: the guard is forward-only, but the `gcdbroker` reflects messages
onto the **return diode** back toward the low side. This is exactly how the `APPROVAL`
verdict travels today — guard → (forward diode) → `gcdbroker` → (return diode) → `gmlbroker`
([`gmguard/src/main.cpp`](../proxies/gmguard/src/main.cpp), the `APPROVAL` emission). And the
Guacamole protocol between the browser and guacd is itself bidirectional (keystrokes forward,
screen updates back), relayed by the web app.

So a guard-chosen nonce *can* physically reach the browser, and a signed response *can*
travel back forward to the guard. Challenge-response across the diode is a design choice, not
a physical impossibility. This is what makes the strong, enterprise-grade property
achievable.

## 4. Two tiers, and what each is honestly worth

There are two levels of ambition. Both are useful; only one delivers the anti-compromise
property. The project should build them in this order and *label them honestly*.

### 4.1 Tier 1 — forward the verified identity (anti-phishing + audit)

Keep mTLS at the reverse proxy with `auth-sso-ssl`, and forward the remote user's verified
identity (certificate fingerprint and/or subject DN) across the diode to the guard as
connection metadata. The guard enforces an allowlist of approved DNs/fingerprints as
defence-in-depth.

- **What it buys:** strong anti-phishing (no YubiKey, no login at the proxy), a real audit
  trail of which identity opened which session, and a coarse "only these identities may ever
  connect" gate.
- **What it does *not* buy:** protection against a compromised web tier / `gmlbroker`, because
  the forwarded fingerprint is a public, replayable value (Section 3.2).

Tier 1 is a genuine improvement and is cheap to build, but it must not be sold as
anti-compromise. It raises the bar for outsiders, not for a breached low side.

### 4.2 Tier 2 — challenge-response to the hardware key (the real property)

The guard issues a fresh nonce that travels the return channel to the browser; the browser
has the remote user's YubiKey/TPM sign it (e.g. via WebAuthn/FIDO2 or PIV); the signature
travels forward to the guard; the guard verifies it against an **enrolled public key held on
the trusted side**.

- **What it buys:** the target property. A compromised web tier or `gmlbroker` cannot produce
  a valid response, because it does not hold the private key, and the nonce is fresh and
  chosen by the trusted side, so a previously observed response cannot be replayed. This is
  the property Waterfall HERA and DataFlowX provide.
- **What it costs:** a browser-side signing component that Guacamole does not provide (see
  Section 6.3), one extra round trip at *session setup* (not per keystroke), and new
  verification logic on the guard.

Tier 2 is the enterprise-grade goal. Tier 1 is the stepping stone and the fallback when a
browser-side signing component is not acceptable in a given deployment.

## 5. Target architecture

```
[ Operator + YubiKey / TPM ]
        │  mTLS client auth (proof of possession, consumed here)
        ▼
[ Reverse proxy / SSL termination ]  ── X-Client-Verified / X-Client-Certificate ──▶ web app
        │
        ▼
[ Guacamole web app + auth-sso-ssl ]   (identity established for the LOGIN only)
        │  Guacamole protocol (browser ⇄ web app ⇄ "guacd")
        ▼
[ gmlbroker (LOW, untrusted) ]  ── carries nonce/response as metadata, forges nothing ──
        │                                              ▲
   (forward diode)                              (return diode: nonce reflected by gcdbroker)
        ▼                                              │
[ gmguard (TRUSTED, between diodes) ] ── issues nonce, verifies signature vs local enrolment
        │  only a verified, approved session is allowed to proceed
        ▼
[ gcdbroker (HIGH) ] ──▶ [ guacd ] ──▶ [ target PC (RDP/SSH/VNC) ]
```

The two anchors of trust are both on the OT side and both physically separated from the
attacker under the three-node design:

- **The enrolled public keys** (the approved hardware identities) live on a local, read-only
  store on the **guard**, never supplied by the low side.
- **The nonce** is generated on the **guard**, guaranteeing freshness from the trusted side.

## 6. Implementation proposal in this repository

This is deliberately *not* a new subsystem. It is an extension of the mechanism that already
exists: the `CREATE → Approver → APPROVAL` gate. Today that gate answers one coarse question
("is the global switch on?"); the proposal makes it answer a per-connection, cryptographic
question ("did an approved hardware key freshly sign my nonce for this channel?"). This also
directly advances the future-work item in [PAPER.md](PAPER.md) §8: moving from a single global
approve/deny switch to trustworthy, per-connection approval.

### 6.1 New bridge messages

The `BridgeMessage` wire format
([`multiplexer.h`](../proxies/shared/include/network/multiplexer.h)) has two spare bits in the
flags byte and room for a 1200-byte payload, so it can carry the new control traffic without a
format change beyond defining new actions. Two new `ChannelAction` values (or a sub-typed
`APPROVAL`) are needed:

- **`CHALLENGE`** (guard → `gcdbroker` → reflected on the return diode → `gmlbroker` → web app
  → browser). Payload: the channel id plus a fresh, guard-generated nonce (256-bit secure
  random, with an issue timestamp). This reuses the exact reflection path the `APPROVAL`
  verdict already uses.
- **`ATTESTATION`** (browser → web app → `gmlbroker` → forward diode → guard). Payload: the
  signed response (signature, the credential/key id, and whatever the chosen scheme requires
  to verify — e.g. WebAuthn `authenticatorData` + `clientDataJSON`).

The response should be terminated as **bridge control metadata**, not passed through as
Guacamole toward the real guacd. That keeps the guacd-bound handshake clean and keeps the
handshake-count that the [`handshake_forger`](../proxies/gmlbroker/src/handshake_forger.cpp)
and the real guacd rely on intact. (If instead you choose to smuggle the attestation inside
the existing `name` handshake instruction — which is already allow-listed — the `gmlbroker`
must strip it before replaying the handshake to guacd.)

### 6.2 Changed flow, mapped to the code

1. **`gmlbroker`** — on a new connection it already emits `CREATE` with an inert `request_id`.
   No change to what it *generates*; it must additionally relay the `CHALLENGE` it receives on
   the return path up to the web app/browser, and relay the browser's `ATTESTATION` forward.
   The `gmlbroker` signs nothing and generates no token — it is a pure conduit, so a compromise
   of it cannot forge a proof. The return path passes through
   [`return_filter`](../proxies/gmlbroker/src/return_filter.cpp), which must be extended to
   permit the `CHALLENGE` message.
2. **`gmguard`** — in the `CREATE` branch of
   [`gmguard/src/main.cpp`](../proxies/gmguard/src/main.cpp) (today it calls
   `approver.HandleRequest` and emits an `A`/`D` verdict), the logic becomes a small state
   machine per pending channel:
   - On `CREATE`: generate a nonce, store it against the channel with an expiry, emit
     `CHALLENGE`. Do **not** approve yet; no Guacamole crosses (the guard already drops
     traffic on unapproved channels).
   - On `ATTESTATION`: verify (Section 6.4). On success, emit the existing `APPROVAL`/`A` and
     insert the channel into `approved`; on failure or timeout, emit `D` and tear the channel
     down (the deny path with `SHUTDOWN_CHANNEL` already exists).
   The existing global approve/deny switch stays as an orthogonal operator override on top of
   the cryptographic gate.
3. **`gcdbroker`** — already reflects `APPROVAL` onto the return diode; it needs to reflect
   `CHALLENGE` the same way, and it must not dial guacd until the `APPROVAL` verdict arrives
   (which it already respects).

### 6.3 The browser-side signing component (the genuinely new, non-C++ part)

This is the piece Guacamole does not provide and the main unknown. Options, roughly in order
of cleanliness:

- **WebAuthn / FIDO2 assertion.** The remote user's YubiKey is enrolled as a FIDO2 credential;
  the guard's nonce is used as the WebAuthn challenge; the browser calls
  `navigator.credentials.get()` and the authenticator signs. The guard verifies the assertion
  signature against the enrolled credential public key. This is the most modern and
  browser-native path and needs no local middleware.
- **PIV / smart card via a browser extension or native messaging host.** More flexible for
  existing PIV YubiKeys, but needs a local component installed on the remote user's machine.

Whichever is chosen, it must be wired so the signing happens at connection setup and the
result rides the `ATTESTATION` message. `auth-sso-ssl` is still used — but only as the login
/ identity layer; the diode-crossing proof is this new component.

### 6.4 What the guard verifies (four checks, verify-only)

Add a small verify-only module to `shared` (OpenSSL is already a dependency; keep the guard
*verify-only* to minimise its attack surface). On `ATTESTATION` the guard checks, in order:

1. **Signature validity** over the exact nonce it issued for this channel (Ed25519/ECDSA, or
   the WebAuthn assertion structure).
2. **Enrolment**: the signing public key is in the guard's local, read-only allowlist of
   approved hardware identities.
3. **Freshness**: the nonce is the one issued for this channel and is still within its short
   validity window.
4. **Single use**: the nonce has not already been consumed (a per-channel one-shot; keep a
   short-lived seen-nonce set to reject exact duplicates).

Only when all four pass does the channel become `approved`.

### 6.5 Enrolment and key management (operational, do not hand-wave it)

The guard needs the approved public keys, delivered out-of-band to the trusted side (never via
the low side). Enrolling, rotating and revoking these keys is an operational process that must
be part of the design: a simple signed enrolment file on the guard host, updated through the
same trusted channel the operator uses, is a reasonable start. This mirrors, and should
eventually merge with, the per-connection approval console that is already future work.

### 6.6 Suggested slices

- **Slice A (Tier 1):** forward identity + guard DN/fingerprint allowlist. Fast; delivers
  anti-phishing and audit. Ship it labelled as such.
- **Slice B0 (plumbing):** `CHALLENGE`/`ATTESTATION` bridge messages, return-path reflection,
  `return_filter` update, guard pending-channel state machine — with a *stub* verifier. Proves
  the round trip end-to-end over the diode before any crypto.
- **Slice B1 (crypto):** the verify-only module and the four checks on the guard.
- **Slice B2 (browser):** the WebAuthn/PIV signing component and enrolment.

Build Slice B0 first: the biggest risk is the data path (does a guard-issued nonce actually
reach the browser and a response come back over the real bridge?), not the cryptography.

## 7. Residual risks and honest limits

- **Denial of service, not impersonation.** A compromised low side can *refuse* to relay the
  challenge or the response, or corrupt it. It cannot forge a valid response. So Tier 2 buys
  integrity/authenticity of accepted sessions, not availability — a breached low side can
  still stop sessions from being set up. That is the correct and expected trade-off.
- **The remote user's endpoint is trusted to the extent of the key's protection.** Hardware keys
  (YubiKey/TPM) resist extraction, which is the point, but malware on the remote user's machine
  could ask the key to sign while the remote user is present. Hence the value of *per-connection*
  approval by an operator on the trusted side on top of the cryptographic proof.
- **Freshness depends on a short window and single-use nonces**, enforced by the guard. This
  is robust because both the nonce source and the verifier are on the trusted side; it is not
  a weak timestamp-only scheme.
- **This does not remove the need for monitoring and approval.** As in the base design, it is
  still a remote-access point; a legitimate remote user can still be socially engineered into
  connecting. The hardware proof binds the session to an approved key; it does not judge
  intent.
- **Assurance.** The guard gains cryptographic verification code. It must stay small,
  verify-only, unit-tested (in the style of the existing parser tests), and ideally covered by
  the CodeQL scan already in place.

## 8. Summary

Enterprise-grade remote access over a data-diode needs one property this project does not yet
have: a session accepted on the OT side only when a remote user proves *fresh possession of an
approved hardware key*, verified *independently on the trusted side*. Apache Guacamole does not
provide this and, by design, never will — every authentication mechanism it has stops at the
web-app login, and guacd trusts the web app blindly. That makes the guard behind the diode the
natural and only place to enforce it.

The realistic path is not to forward a (public, replayable) certificate — that is anti-phishing
at best and worthless against a compromised web tier. It is a challenge-response to the
remote user's hardware key, made possible by the bridge's existing return channel, terminated and
verified on the trusted-side guard against a locally enrolled set of approved keys. In this
repository that is an extension of the existing `CREATE → Approver → APPROVAL` gate, plus a new
browser-side signing component, built in slices with the data path proven before the crypto.
Done this way, the open-source solution reaches the same class of guarantee as the commercial
Waterfall HERA and DataFlowX products — in a form anyone can inspect, test and improve.

## References

[1] The Apache Software Foundation, "Configuring Guacamole" (guacd, `guacd-ssl`, trust model),
Apache Guacamole Manual v1.6.0. https://guacamole.apache.org/doc/gug/configuring-guacamole.html

[2] The Apache Software Foundation, "The Guacamole protocol" (handshake, the `name`
instruction), Apache Guacamole Manual v1.6.0.
https://guacamole.apache.org/doc/gug/guacamole-protocol.html

[3] The Apache Software Foundation, "Signing in with smart cards or certificates"
(`auth-sso-ssl`, `X-Client-Certificate` / `X-Client-Verified`), Apache Guacamole Manual v1.6.0.
https://guacamole.apache.org/doc/gug/ssl-auth.html

[4] M. Jumper, "GUACAMOLE-839: Add webapp SSO support for certificates / smart cards", PR #797,
apache/guacamole-client. https://github.com/apache/guacamole-client/pull/797

[5] Apache Guacamole user mailing list, "Does Guacamole support PKI/Smartcard authentication for
RDP (instead of username/password)?".
https://www.mail-archive.com/user@guacamole.apache.org/msg08365.html

[6] The Apache Software Foundation, Apache Guacamole Manual v1.6.0 (extension / authentication
chapter index). https://guacamole.apache.org/doc/1.6.0/gug/

[7] Waterfall Security Solutions, "Hardware-Enforced Remote Access (HERA) — Under the Hood".
https://waterfall-security.com/ot-insights-center/ot-cybersecurity-insights-center/hardware-enforced-remote-access-hera-under-the-hood/

[8] DataFlowX, "Secure Remote Access". https://www.dataflowx.com/en/secure-remote-access
