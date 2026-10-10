# GitHub Pages Studio demo

The static demo reuses the Studio UI and replaces its API calls with browser-side
JavaScript. It needs no Python HTTP API, C library, service worker, API key, or
server at runtime. Map contents are processed locally and are not uploaded.
The existing Python/C Studio remains available through `make studio`.

## Build and preview

Requirements: Python 3.9+ to build; Node.js 18+ to run the demo API tests.

```sh
make demo-check
make demo
make demo-serve
```

Open http://localhost:8080. The generated site is in `build/demo/`, with relative
script paths that work at a GitHub project URL such as
`https://kokutbon.github.io/xROUTE/`. To test that subpath locally, run
`python3 -m http.server 8080 --directory build` and open
http://localhost:8080/demo/. Preview over HTTP rather than opening a file URL.

The builder extracts the current sample XML from `server.py` and copies the UI;
edit `python/xroute/web/index.html` or `demo/demo-api.js`, then rebuild. Generated
files are ignored by Git. Styling and icons use the Studio's existing Tailwind
and Font Awesome CDNs, so visual styling requires internet access.

## Demo behavior

The adapter handles `/api/default_map`, `/api/parse`, `/api/route`, `/api/fleet`,
and `/api/convert`. Requests return the Studio response schema, including errors.
It accepts XML, JSON, CSV/TSV (including legacy and coordinate tables), and ASCII
grids. Export JSON, SVG export, editable map JSON, point routing, bay routing,
fleet placement/dispatch, hazards, zoom, and routing settings work in the browser.

Routing uses a JavaScript A* simulation with directed departures, physical cell
dimensions, heading-aware turn costs, two seconds per turn, and accessible bay
neighbors. Fleet dispatch selects the reachable route with the lowest total cost,
keeping the first vehicle on ties. Equal-cost paths may differ from the C engine.
This demo does not execute or benchmark the native library. State is in memory
and resets on reload; this is a planning demonstration, not vehicle control.

Try selecting aisle 1 / bay 1, placing a start on the floor, adding a hazard to
the route, and clearing hazards. For fleet dispatch, click floor cells to place
vehicles and click a rack to select its target bay, then Dispatch Nearest Operator.
Use Map Studio to edit the current canonical JSON, then submit it as a new map.

## Publish later (manual only)

Nothing is published by building or previewing, and the Pages workflow has no
push or pull request trigger.

1. Commit and push these files to your GitHub repository when ready.
2. In repository **Settings → Pages**, select **GitHub Actions** as the source.
3. In **Actions**, select **Publish Studio demo (manual)** and **Run workflow**
   on the branch containing these changes.
4. The deployment job reports the resulting Pages URL. The repository name
   determines the project path; it is not hardcoded to `/xROUTE/`.

Alternatively, upload the contents of `build/demo/` to your chosen Pages branch
and configure branch-based publishing yourself. Include `.nojekyll`.
