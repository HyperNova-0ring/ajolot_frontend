## Warning!

Work In Progress! somethings can be messed up and can't be accuraty with future commits.

## License

This project is released under the **LGPLv3**. See [`LICENSE`](LICENSE) for the full text.

The LGPLv3 guarantees that the core frontend remains free and open source, while allowing external modules (apps and games) to be distributed under any license, including proprietary ones.

---

## Architecture Overview

```
Frontend (LGPLv3)
│
├── GUI Engine (LGPLv3)          ← built-in renderer, always open source
├── Built-in Apps (LGPLv3)       ← apps that ship with the frontend
│
├─ fork() ──→ External App       ← independent process, any license
│               ├── manifest.json
│               ├── icon.png
│               ├── banner.jpg
│               ├── preview.mp4
│               └── app.bin / app.py / ...
│
└─ fork() ──→ External Game      ← independent process, any license
                ├── manifest.json
                ├── icon.png
                ├── banner.jpg
                ├── preview.mp4
                └── game.bin
```

External modules are launched as **separate processes** via `fork()`. They never link against the frontend's code, which means the LGPLv3 does **not** extend to them.

---

## What Is Covered by LGPLv3

| Component | License | Notes |
|---|---|---|
| Frontend core | LGPLv3 | Always open source |
| GUI engine | LGPLv3 | Built into the frontend |
| Built-in apps | LGPLv3 | Ship with the frontend |
| `manifest.json` format | Free | Data format, no license restrictions |

Any modifications to the frontend core or GUI engine **must** be released under LGPLv3.

---

## What Is NOT Covered by LGPLv3

| Component | License | Notes |
|---|---|---|
| External apps (fork) | Any ✅ | Independent process |
| External games (fork) | Any ✅ | Independent process |
| Module assets (images, audio, video) | Any ✅ | Owned by the developer |
| Module executables (.bin, .so, etc.) | Any ✅ | Not linked to frontend |

Because all external modules run as independent processes and communicate with the frontend only through standard OS mechanisms (signals, exit codes, IPC), they are considered **separate works** and are not subject to the terms of the LGPLv3.

---

## Module Manifest

Every external module (app or game) must include a `manifest.json` file. The frontend reads this file to render the module's presentation in the menu — loading assets like icons, banners, and preview videos. The manifest is a data file with no license implications.

Example `manifest.json`:

```json
{
  "name": "My App",
  "version": "1.0.0",
  "description": "A short description of the module.",
  "author": "Developer Name",
  "license": "Proprietary",
  "icon": "icon.png",
  "banner": "banner.jpg",
  "preview": "preview.mp4",
  "executable": "app.bin"
}
```

All asset paths are relative to the module's root directory. The frontend handles rendering; the module only needs to provide the files.

---

## How Module Launching Works

The frontend manages external modules entirely through standard OS primitives. Modules do not need to implement any special interface or link against any frontend library.

| Action | Mechanism |
|---|---|
| Launch module | `fork()` + `exec()` |
| Pause module (menu overlay) | `SIGSTOP` |
| Resume module | `SIGCONT` |
| Detect crash | `waitpid()` + exit code |
| Debug screen (dev mode) | `stderr` capture + exit code |
| Menu overlay background | Screenshot of framebuffer + blur |

When the user opens the menu while a module is running, the frontend sends `SIGSTOP` to pause it, captures a screenshot, applies a blur effect, and renders the menu on top. On resume, `SIGCONT` is sent and the module continues normally.

If a module crashes and **dev mode** is enabled, the frontend catches the exit code and/or stderr output and displays a debug screen instead of returning silently to the menu.

---

## Developing a Module

To create a module compatible with this frontend:

1. Create a directory for your module.
2. Add a `manifest.json` following the format above.
3. Include your assets (icon, banner, preview video).
4. Point `"executable"` to your binary or script.

Your module can be written in any language and compiled to any target. It does not need to include any headers, link any libraries, or follow any specific internal API from this project.

**You may distribute your module under any license, including proprietary or commercial licenses.**

---

## Contributing

Contributions to the frontend core are welcome and must be submitted under the LGPLv3. By submitting a pull request, you agree that your contribution will be licensed under the same terms.

---

## Disclaimer

This README provides a general overview of how the LGPLv3 applies to this project's architecture. It is not legal advice. If you have specific licensing concerns, consult a qualified attorney.
