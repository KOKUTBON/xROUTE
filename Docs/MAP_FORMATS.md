# Map interchange formats

The examples under `examples/maps/` are synthetic grids for development and testing. Keep actual facility layouts and customer imports outside this repository.

## Coordinates and dimensions

Coordinates are zero-based: `x` increases rightward and `y` downward. Cell dimensions are in centimetres. A map specifies its row and column counts; loaders reject invalid inputs using the public status codes in `include/xroute/types.h`.

## CSV

The CSV loader accepts comma-separated rows. The first line contains row and column counts (`rows,cols`); the second line is consumed as a separator before the matrix. The first value identifies the aisle (`corsia`); the remaining values represent map columns. An empty value or `0` is a transit cell, `x` or `X` is an obstacle, and a positive integer identifies a rack bay (`campata`). Rack access is inferred from adjacent transit cells. The caller supplies cell dimensions; zero selects the loader defaults (350 cm width and 200 cm height).

## XML

The root `Magazzino` declares `numRighe`, `numColonne`, `dimColonne` and `dimRighe`. Each `Cella` declares `x`, `y` and `Tipo`, with `Corsia` and `Campata` for rack identifiers. `LatoAccessibile` values identify accessible neighbouring cells: `sx` = left, `up` = above, `dx` = right, `dw` = below. Existing Italian element names form the compatibility contract and remain supported.

See `examples/maps/warehouse.xml` for a synthetic example and `include/xroute/parser.h` for loading, saving and conversion APIs. Tests cover CSV import and XML round trips. File import must not be treated as authorization to expose the imported layout publicly.
