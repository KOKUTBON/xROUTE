# Security Policy

## Supported Versions

Security updates and critical patches are actively applied to the latest stable release of **xROUTE**:

| Version | Supported          |
| ------- | ------------------ |
| 2.0.x   | :white_check_mark: |
| < 2.0   | :x:                |

---

## Reporting a Vulnerability

The xROUTE team takes safety, memory boundaries, and execution integrity seriously, especially in safety-critical automated warehouse and robotic environments.

If you discover a security vulnerability (such as a memory corruption bug, buffer overflow in file parsing, integer overflow, or Denial of Service vulnerability):

1. **Do not create a public issue**.
2. Submit your report privately via GitHub Security Advisories at [https://github.com/KOKUTBON/xROUTE/security/advisories](https://github.com/KOKUTBON/xROUTE/security/advisories) or contact the project maintainers directly.
3. Please include:
   - Detailed description of the vulnerability.
   - Proof of concept (PoC) file (e.g., malformed XML/CSV map or reproducing command line arguments).
   - Expected vs. actual behavior.
   - Your environment (OS, architecture, compiler version).

We will acknowledge receipt within 48 hours and work with you to test and release a security advisory and fix.

## Private reporting contact

Email [kokutbon@gmail.com](mailto:kokutbon@gmail.com) with subject `[xROUTE] Security report`. Include affected version/commit, reproduction using synthetic data, preconditions and impact. Do not include live credentials or personal data. This mailbox is the fallback while the repository is private or GitHub's reporting form is unavailable.

After publication, prefer [GitHub private vulnerability reporting](https://github.com/KOKUTBON/xROUTE/security/advisories/new) when enabled. Acknowledgment and remediation depend on maintainer availability; no response deadline is guaranteed. Coordinate disclosure and fixes privately.
