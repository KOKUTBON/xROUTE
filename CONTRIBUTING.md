# Contributing to xROUTE

Thank you for your interest in contributing to **xROUTE**! We welcome contributions ranging from bug fixes and algorithm enhancements to documentation improvements and language bindings.

---

## Getting Started

1. **Fork the Repository**: Create your own fork on GitHub.
2. **Clone Locally**:
   ```bash
   git clone https://github.com/KOKUTBON/xROUTE.git
   cd xROUTE
   ```
3. **Build and Test**:
   ```bash
   make all
   make test
   ```

---

## Development Workflow

### Coding Standards
- **C Standard**: Pure **ANSI C99 / C11**. Do not use platform-specific headers like `<conio.h>` or non-standard compiler extensions.
- **Memory Safety**: Every allocation (`malloc`, `calloc`) must have corresponding bounds checks, error paths, and clean lifecycle management (`free`).
- **Sanitizers**: All code must compile and pass tests cleanly with Clang AddressSanitizer and UndefinedBehaviorSanitizer:
  ```bash
  make clean
  make test CFLAGS="-std=c99 -Wall -Wextra -pedantic -fsanitize=address,undefined -g" LDFLAGS="-fsanitize=address,undefined"
  ```
- **Naming Conventions**:
  - Public types and functions are prefixed with `xroute_` or `XROUTE_`.
  - Opaque pointers should hide internal structure definitions inside `src/`.

### Adding Features & Algorithms
- Any new pathfinding heuristic, kinematics model, or map loader must include corresponding unit tests in `tests/test_main.c`.
- Keep the public API consistently namespaced with `xroute_` and `XROUTE_`.

---

## Submitting Pull Requests

1. Create a feature branch:
   ```bash
   git checkout -b feat/my-new-feature
   ```
2. Commit your changes following conventional commit syntax:
   ```bash
   git commit -m "feat(router): add bidirectional A* search"
   ```
3. Push to your fork:
   ```bash
   git push origin feat/my-new-feature
   ```
4. Open a Pull Request against `main` describing:
   - The rationale behind the changes.
   - Test results and verification steps.
   - Any benchmark or performance impact.

## Community participation

Follow the [communication guidelines](COMMUNICATION.md) and [organizational governance](GOVERNANCE.md). Keep security and conduct reports confidential and use a verified private channel; never include sensitive details in a public issue. Maintainers verify the reporting contacts and channels listed in the [readiness checklist](RELEASE_CHECKLIST.md).

## KOKUTBON contribution principles

Contribute under the existing **MIT** license in [LICENSE](LICENSE). You retain copyright in your work. Submit only work you are entitled to share, identify third-party sources and preserve their notices. No copyright assignment or paid membership is required.

Open an issue for substantial proposals, then submit a focused pull request from a branch or fork. Describe the problem, implementation, relevant checks and any privacy, accessibility, migration or safety impact. Use synthetic fixtures and sanitized logs. Maintainer review and passing relevant CI checks are expected before merging. A sole maintainer may merge their own change after checks pass and must record the rationale; no fictitious second reviewer is required.

Small documentation, translation, accessibility and reproducibility improvements are welcome. Funding does not determine review priority. Follow [communication guidelines](COMMUNICATION.md) and [the sponsorship policy](SPONSORSHIP.md).
