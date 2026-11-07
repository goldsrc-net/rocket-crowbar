# rockcrowbar/ - mod content directory

The runtime game directory for mod-rockcrowbar, run next to the stock `valve/`
directory with `-game rockcrowbar`.

```
rockcrowbar/
  liblist.gam     # mod manifest (this repo)
  dlls/hl.so      # server game library built from this repo
  ...             # maps, .lec configs, rc.ini and anything else RC overrides;
                  #   everything not here falls back to valve/
```

The original's content (rc.ini, the per-map `.lec` entity configs, pak0.pak) is in
the v19 release zip; see `../../reference/README.md`. RC 1.9 has no client
library of its own, so there is no `cl_dlls/` here.
