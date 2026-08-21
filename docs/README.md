# Operator docs

HTML lives in [`html/`](html/). PDF guides live in [`pdf/`](pdf/). Edit the
HTML under [`html/`](html/), then rebuild.

## Guides

The software manual and the wiring guide are sibling PDFs in `pdf/`. They
are not chapters of each other.

| Guide | File |
|-------|------|
| Software User Manual | [pdf/Software_User_Manual.pdf](pdf/Software_User_Manual.pdf) |
| Software User Manual (HTML) | [html/software-manual.html](html/software-manual.html) |
| Wiring Guide | [pdf/Wiring_Guide.pdf](pdf/Wiring_Guide.pdf) |
| Wiring Guide (HTML) | [html/wiring-guide.html](html/wiring-guide.html) |

## Software chapters (HTML)

| Page | Chapter PDF |
|------|-------------|
| [Overview](html/index.html) | [pdf/software/index.pdf](pdf/software/index.pdf) |
| [Main](html/main.html) | [pdf/software/main.pdf](pdf/software/main.pdf) |
| [Calibration](html/calibration.html) | [pdf/software/calibration.pdf](pdf/software/calibration.pdf) |
| [Diagnostics](html/diagnostics.html) | [pdf/software/diagnostics.pdf](pdf/software/diagnostics.pdf) |
| [Settings](html/settings.html) | [pdf/software/settings.pdf](pdf/software/settings.pdf) |

## Wiring chapters (HTML)

| Page | Chapter PDF |
|------|-------------|
| [Board A](html/board-a/index.html) | [pdf/wiring/board-a.pdf](pdf/wiring/board-a.pdf) |
| [Board B](html/board-b/index.html) | [pdf/wiring/board-b.pdf](pdf/wiring/board-b.pdf) |
| [Board C](html/board-c/index.html) | [pdf/wiring/board-c.pdf](pdf/wiring/board-c.pdf) |

## Rebuild

From this directory:

```bash
npm install
./node_modules/.bin/playwright install chromium
npm run build
```

`html/` is the source. The script rebuilds the combined HTML pages and the
PDFs (`pdf/Software_User_Manual.pdf` and `pdf/Wiring_Guide.pdf`).
