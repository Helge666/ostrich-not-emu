# Building Ostrich 32

Not buildable yet: the repository holds the submodules and the documents; the plugin
follows (ROADMAP.md). This file grows with it, after Ostrich 55's BUILDING.md.

## 1. Cloning

```bash
git clone --recurse-submodules https://github.com/Helge666/ostrich32.git
```

On Windows with `core.autocrlf=true`, set `core.autocrlf false` in the repository and in
`plugin/dpf` before the submodules are checked out (DPF-LOGAN.md); the Make build needs LF.

`git submodule status --recursive` must show a leading space on every line; `+` or `m`
means a submodule is not at its pinned state.

## 2. Submodules

| Path | Pinned to | Notes |
|---|---|---|
| `munt/` | [Helge666/munt](https://github.com/Helge666/munt), tag `libmt32emu_2_8_6` (`91d2205`) | upstream Munt, unmodified; only `munt/mt32emu` (libmt32emu, CMake) is built |
| `plugin/dpf/` | [Helge666/DPF](https://github.com/Helge666/DPF), tag `logan-2` (`b9424f1`) | see DPF-LOGAN.md |

To move munt to a newer release: fetch the tags into our fork (`git fetch upstream --tags`,
push the tag to `Helge666/munt`, together with the owner), then
`git -C munt checkout libmt32emu_X_Y_Z` and commit the new pointer. Read
`munt/mt32emu/NEWS.txt` first. Nothing in `munt/` is ever changed in place.

## 3. Before publishing

- Both submodule pins exist on their remotes (`git ls-remote`).
- The history holds no ROM, recording, SysEx or manual (`git log --all --stat`).
