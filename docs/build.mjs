#!/usr/bin/env node
/**
 * Publish operator HTML into html/ and print PDFs into pdf/.
 *
 * Software chapters print to pdf/software/, then bind into
 * pdf/Software_User_Manual.pdf. Board A/B/C print to pdf/wiring/, then
 * bind into pdf/Wiring_Guide.pdf. Those two guides are siblings, not
 * chapters of each other.
 *
 * In the shop repo, sources are docs/Software/ and docs/Board_{A,B,C}/.
 * Edit those files, then run this script. In the published PrivateReserve
 * tree, html/ is the source; the script rebuilds the combined HTML pages
 * and the PDFs.
 *
 *   cd client/docs
 *   npm install
 *   ./node_modules/.bin/playwright install chromium
 *   npm run build
 */
import { chromium } from "playwright";
import { cp, mkdir, readdir, readFile, writeFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const shopDocs = join(here, "..", "..", "docs");
const shopSoftware = join(shopDocs, "Software");
const htmlDir = join(here, "html");
const softwareSrc = existsSync(join(shopSoftware, "index.html"))
  ? shopSoftware
  : htmlDir;
const dest = htmlDir;
const pdfDir = join(here, "pdf");
const softwarePdfDir = join(pdfDir, "software");
const wiringPdfDir = join(pdfDir, "wiring");
const softwareManualPdf = "Software_User_Manual.pdf";
const wiringGuidePdf = "Wiring_Guide.pdf";
const softwareManualHtml = "software-manual.html";
const wiringGuideHtml = "wiring-guide.html";
const softwarePdfHref = `../pdf/${softwareManualPdf}`;
const wiringPdfHrefFromSoftware = `../pdf/${wiringGuidePdf}`;
const wiringPdfHrefFromBoard = `../../pdf/${wiringGuidePdf}`;

const pages = [
  { html: "index.html", pdf: "index.pdf", title: "Overview" },
  { html: "main.html", pdf: "main.pdf", title: "Main" },
  { html: "calibration.html", pdf: "calibration.pdf", title: "Calibration" },
  { html: "diagnostics.html", pdf: "diagnostics.pdf", title: "Diagnostics" },
  { html: "settings.html", pdf: "settings.pdf", title: "Settings" },
];

const boards = [
  {
    id: "board-a",
    shop: "Board_A",
    title: "Board A — Earth-side control",
    files: ["index.html", "Board_A_placement.svg", "EC11_Onboard_Wiring.md"],
    extras: [
      {
        from: join(shopDocs, "LilyGo_T-Display-S3", "T-Display-S3_PermaProto_wired.jpg"),
        name: "T-Display-S3_PermaProto_wired.jpg",
      },
    ],
  },
  {
    id: "board-b",
    shop: "Board_B",
    title: "Board B — Floating gate driver",
    files: ["index.html", "Board_B_placement.svg", "Board_B_PermaProto.jpg"],
  },
  {
    id: "board-c",
    shop: "Board_C",
    title: "Board C — HV switch stage",
    files: ["index.html", "Board_C_placement.svg", "Board_C_PermaProto.jpg"],
  },
];

async function copySoftware() {
  if (softwareSrc === dest) {
    return;
  }
  await mkdir(dest, { recursive: true });
  for (const name of ["page.css", ...pages.map((p) => p.html)]) {
    await cp(join(softwareSrc, name), join(dest, name));
  }
  const shotsSrc = join(softwareSrc, "shots");
  const shotsDest = join(dest, "shots");
  await mkdir(shotsDest, { recursive: true });
  for (const name of await readdir(shotsSrc)) {
    if (!name.endsWith(".png")) {
      continue;
    }
    await cp(join(shotsSrc, name), join(shotsDest, name));
  }
}

async function copyBoards() {
  const shopA = join(shopDocs, "Board_A", "index.html");
  if (!existsSync(shopA)) {
    return;
  }
  for (const board of boards) {
    const fromDir = join(shopDocs, board.shop);
    const toDir = join(dest, board.id);
    await mkdir(toDir, { recursive: true });
    for (const name of board.files) {
      const from = join(fromDir, name);
      if (!existsSync(from)) {
        continue;
      }
      const to = join(toDir, name);
      if (name.endsWith(".md")) {
        let md = await readFile(from, "utf8");
        md = md
          .split("\n")
          .filter((line) => !line.includes("../"))
          .join("\n")
          .replace(/\n{3,}/g, "\n\n");
        await writeFile(to, md);
      } else {
        await cp(from, to);
      }
    }
    for (const extra of board.extras ?? []) {
      if (existsSync(extra.from)) {
        await cp(extra.from, join(toDir, extra.name));
      }
    }
  }
}

function rewriteBoardHrefs(html, from) {
  const hrefs =
    from === "software"
      ? {
          a: "board-a/index.html",
          b: "board-b/index.html",
          c: "board-c/index.html",
          software: "index.html",
        }
      : {
          a: "../board-a/index.html",
          b: "../board-b/index.html",
          c: "../board-c/index.html",
          software: "../index.html",
        };
  html = html.replaceAll('href="../Board_A/index.html"', `href="${hrefs.a}"`);
  html = html.replaceAll('href="../Board_B/index.html"', `href="${hrefs.b}"`);
  html = html.replaceAll('href="../Board_C/index.html"', `href="${hrefs.c}"`);
  html = html.replaceAll('href="../Software/index.html"', `href="${hrefs.software}"`);
  return html;
}

function stripShopOnlyLinks(html) {
  html = html.replace(/\s*<li>\s*<a href="\.\.\/[\s\S]*?<\/li>/g, "");
  html = html.replace(/<a href="(?:\.\.\/)+[^"]+"[^>]*>([\s\S]*?)<\/a>/g, "$1");
  return html;
}

function prepareSoftwareHtml(raw) {
  let html = rewriteBoardHrefs(raw, "software");
  html = stripShopOnlyLinks(html);
  html = html.replace(/\s*<a href="wiring-guide.html">Wiring<\/a>/g, "");
  html = html.replace(/\s*<a class="pdf-dl"[^>]*>[\s\S]*?<\/a>/g, "");
  html = html.replace(
    /(<nav class="sec"[^>]*>)([\s\S]*?)(<\/nav>)/,
    (_, open, inner, close) =>
      `${open}${inner.trimEnd()}\n      <a href="${wiringGuideHtml}">Wiring</a>\n      <a class="pdf-dl" href="${softwarePdfHref}">User Manual (PDF)</a>\n      <a class="pdf-dl" href="${wiringPdfHrefFromSoftware}">Wiring Guide (PDF)</a>\n    ${close}`,
  );
  html = html.replace(
    /<footer>[\s\S]*?<\/footer>/,
    `<footer>This page is a chapter of the <a href="${softwarePdfHref}">Software User Manual</a>.</footer>`,
  );
  return html;
}

function addPdfIndex(html) {
  html = html.replace(/\s*<p class="pdf-all">[\s\S]*?<\/p>/g, "");
  const softwareChapters = pages
    .map((p) => `<a href="../pdf/software/${p.pdf}">${p.title}</a>`)
    .join(" · ");
  const boardHtml = boards
    .map((b) => `<a href="${b.id}/index.html">${b.title.split(" — ")[0]}</a>`)
    .join(" · ");
  const boardPdfs = boards
    .map((b) => `<a href="../pdf/wiring/${b.id}.pdf">${b.title.split(" — ")[0]}</a>`)
    .join(" · ");
  const softwareBlock =
    `<p class="pdf-all"><strong>Software User Manual.</strong> ` +
    `<a href="${softwarePdfHref}">Download PDF</a> · ` +
    `<a href="${softwareManualHtml}">Single-page HTML</a><br>` +
    `Chapters: ${softwareChapters}</p>`;
  const wiringBlock =
    `<p class="pdf-all"><strong>Wiring Guide.</strong> ` +
    `<a href="${wiringPdfHrefFromSoftware}">Download PDF</a> · ` +
    `<a href="${wiringGuideHtml}">Single-page HTML</a><br>` +
    `Boards: ${boardHtml}<br>` +
    `Chapters: ${boardPdfs}</p>`;
  const blocks = `${softwareBlock}\n\n    ${wiringBlock}`;
  if (html.includes('<div class="warn">')) {
    return html.replace(
      /(<div class="warn">[\s\S]*?<\/div>)/,
      `$1\n\n    ${blocks}`,
    );
  }
  return html.replace("</nav>", `</nav>\n    ${blocks}`);
}

function articleInner(html) {
  const m = html.match(/<article class="page">([\s\S]*)<\/article>/);
  if (!m) {
    throw new Error("no article.page in HTML");
  }
  return m[1];
}

function chapterBody(html) {
  let inner = articleInner(html);
  inner = inner.replace(/<nav class="sec"[\s\S]*?<\/nav>/, "");
  inner = inner.replace(/<p class="pdf-all">[\s\S]*?<\/p>/g, "");
  inner = inner.replace(
    /<h2>[^<]*Related<\/h2>\s*<nav class="links">[\s\S]*?<\/nav>/,
    "",
  );
  inner = inner.replace(/<footer>[\s\S]*?<\/footer>/, "");
  return inner.trim();
}

function boardRelated(id) {
  const items = [
    {
      id: "board-a",
      html: `<li><a href="../board-a/index.html">Board A</a> — earth-side control (T-Display-S3, EC11, field I/O)</li>`,
    },
    {
      id: "board-b",
      html: `<li><a href="../board-b/index.html">Board B</a> — floating gate driver</li>`,
    },
    {
      id: "board-c",
      html: `<li><a href="../board-c/index.html">Board C</a> — HV switch stage</li>`,
    },
  ];
  const extra =
    id === "board-a"
      ? `<li><a href="EC11_Onboard_Wiring.md">EC11 on-board wiring</a> — GPIO11–13; T17–T19 are NC</li>\n        `
      : "";
  const boardItems = items.map((item) => item.html).join("\n        ");
  return `<h2>Related</h2>
    <nav class="links">
      <ul>
        ${boardItems}
        ${extra}<li><a href="../${wiringGuideHtml}">Wiring Guide</a> — boards A, B, and C on one page</li>
        <li><a href="../index.html">Software</a> — operator screens</li>
      </ul>
    </nav>`;
}

function ensureBoardChromeCss(html) {
  html = html.replace(/\s*\/\* client-nav \*\/[\s\S]*?\/\* \/client-nav \*\//, "");
  if (!html.includes("</style>")) {
    return html;
  }
  return html.replace(
    "</style>",
    `    /* client-nav */
    nav.sec { display: flex; flex-wrap: wrap; gap: 0.85rem; margin: 0 0 1.25rem; font-size: 0.9rem; }
    nav.sec a { color: var(--earth); }
    nav.sec a[aria-current="page"] { color: var(--ink); font-weight: 600; text-decoration: none; }
    nav.sec a.pdf-dl { font-weight: 600; }
    .pdf-all { margin: 0 0 1.25rem; font-size: 0.9rem; }
    .pdf-all a { color: var(--earth); }
    .manual-cover { min-height: 18rem; padding: 3rem 0 2rem; break-after: page; }
    .manual-cover h1 { font-size: 2rem; margin: 0.2rem 0 0.8rem; }
    .manual-cover .toc { margin-top: 2rem; }
    .chapter { break-before: page; }
    @media print {
      nav.sec, nav.links, .pdf-dl, .pdf-all { display: none !important; }
      nav.links a::after { content: none; }
      table { break-inside: avoid; }
    }
    /* /client-nav */
  </style>`,
  );
}

function boardNav(id) {
  const link = (boardId, label) => {
    const href = `../${boardId}/index.html`;
    if (boardId === id) {
      return `<a href="${href}" aria-current="page">${label}</a>`;
    }
    return `<a href="${href}">${label}</a>`;
  };
  return `    <nav class="sec" aria-label="Wiring pages">
      ${link("board-a", "Board A")}
      ${link("board-b", "Board B")}
      ${link("board-c", "Board C")}
      <a href="../${wiringGuideHtml}">All boards</a>
      <a href="../index.html">Software</a>
      <a class="pdf-dl" href="${wiringPdfHrefFromBoard}">Wiring Guide (PDF)</a>
    </nav>

    <p class="pdf-all"><strong>Wiring Guide.</strong> <a href="${wiringPdfHrefFromBoard}">Download PDF</a> · <a href="../${wiringGuideHtml}">Single-page HTML</a></p>`;
}

function prepareBoardHtml(raw, id) {
  let html = rewriteBoardHrefs(raw, "board");
  html = html.replace(
    'src="../LilyGo_T-Display-S3/T-Display-S3_PermaProto_wired.jpg"',
    'src="T-Display-S3_PermaProto_wired.jpg"',
  );
  html = stripShopOnlyLinks(html);
  html = html.replace(
    /<h2>[^<]*Related<\/h2>\s*<nav class="links">[\s\S]*?<\/nav>/,
    boardRelated(id),
  );
  html = html.replace(/<footer>[\s\S]*?<\/footer>/, 
    `<footer>This page is a chapter of the <a href="${wiringPdfHrefFromBoard}">Wiring Guide</a>. Electrical wiring truth for the motor stack remains the gate-driver schematic, not the placement drawing.</footer>`,
  );
  html = html.replace(/\s*<nav class="sec"[\s\S]*?<\/nav>/, "");
  html = html.replace(/\s*<p class="pdf-all">[\s\S]*?<\/p>/g, "");
  html = html.replace(
    /(<h1>[^<]*<\/h1>\s*<p>[\s\S]*?<\/p>)/,
    `$1\n\n${boardNav(id)}\n`,
  );
  html = ensureBoardChromeCss(html);
  return html;
}

function prefixBoardAssets(inner, id) {
  inner = inner.replace(
    /\b(src|href)="(?!https?:|\/|\.\.\/|#)([^"]+)"/g,
    (_, attr, path) => `${attr}="${id}/${path}"`,
  );
  return inner;
}

async function writeSoftwareManualHtml() {
  const chapters = [];
  for (const pageSpec of pages) {
    const html = await readFile(join(dest, pageSpec.html), "utf8");
    chapters.push(
      `    <section class="chapter" id="${pageSpec.html.replace(".html", "")}">\n` +
        `${chapterBody(html)}\n` +
        `    </section>`,
    );
  }
  const toc = pages.map((p) => `<li>${p.title}</li>`).join("\n          ");
  const out = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Private Reserve — Software User Manual</title>
  <link rel="stylesheet" href="page.css">
</head>
<body>
  <article class="page">
    <section class="manual-cover">
      <p class="lede">Private Reserve · DIN controller</p>
      <h1>Software User Manual</h1>
      <p>Operator screens for production image <code>tdisplay_s3_prod</code> on the LilyGo T-Display-S3 (Board&nbsp;A).</p>
      <p>The LCD and the phone web page share one controller state.</p>
      <ol class="toc">
          ${toc}
      </ol>
    </section>
${chapters.join("\n")}
  </article>
</body>
</html>
`;
  await writeFile(join(dest, softwareManualHtml), out);
}

async function writeWiringGuideHtml() {
  const first = await readFile(join(dest, "board-a", "index.html"), "utf8");
  const styleMatch = first.match(/<style>([\s\S]*?)<\/style>/);
  const style = styleMatch ? styleMatch[1] : "";
  const chapters = [];
  for (const board of boards) {
    const html = await readFile(join(dest, board.id, "index.html"), "utf8");
    chapters.push(
      `    <section class="chapter" id="${board.id}">\n` +
        `${prefixBoardAssets(chapterBody(html), board.id)}\n` +
        `    </section>`,
    );
  }
  const toc = boards.map((b) => `<li>${b.title}</li>`).join("\n          ");
  const out = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Private Reserve — Wiring Guide</title>
  <style>${style}
  </style>
</head>
<body>
  <article class="page">
    <section class="manual-cover">
      <p class="lede">Private Reserve · DIN controller</p>
      <h1>Wiring Guide</h1>
      <p>Boards A, B, and C as built: earth-side control, floating gate driver, and HV switch stage.</p>
      <p>Screw-terminal IDs on these pages are the names used on the operator screens.</p>
      <ol class="toc">
          ${toc}
      </ol>
    </section>
${chapters.join("\n")}
  </article>
</body>
</html>
`;
  await writeFile(join(dest, wiringGuideHtml), out);
}

async function waitForImages(page) {
  await page.evaluate(async () => {
    const imgs = [...document.images];
    await Promise.all(
      imgs.map(
        (img) =>
          img.complete ||
          new Promise((resolve) => {
            img.addEventListener("load", resolve, { once: true });
            img.addEventListener("error", resolve, { once: true });
            setTimeout(resolve, 15000);
          }),
      ),
    );
  });
}

async function printOne(context, htmlFile, pdfPath, footerLabel) {
  const p = await context.newPage();
  const url = pathToFileURL(join(dest, htmlFile)).href;
  await p.goto(url, { waitUntil: "load", timeout: 60000 });
  await waitForImages(p);
  await mkdir(dirname(pdfPath), { recursive: true });
  await p.pdf({
    path: pdfPath,
    format: "Letter",
    printBackground: true,
    displayHeaderFooter: true,
    headerTemplate: `<div></div>`,
    footerTemplate: `<div style="font-size:9px;width:100%;padding:0 18mm;color:#475467;display:flex;justify-content:space-between;"><span>Private Reserve · ${footerLabel}</span><span><span class="pageNumber"></span> / <span class="totalPages"></span></span></div>`,
    margin: { top: "14mm", bottom: "18mm", left: "14mm", right: "14mm" },
  });
  await p.close();
}

async function printPdfs() {
  await mkdir(softwarePdfDir, { recursive: true });
  await mkdir(wiringPdfDir, { recursive: true });
  const browser = await chromium.launch();
  const context = await browser.newContext({
    viewport: { width: 1200, height: 1600 },
  });
  for (const pageSpec of pages) {
    const path = join(softwarePdfDir, pageSpec.pdf);
    await printOne(
      context,
      pageSpec.html,
      path,
      `Software User Manual · ${pageSpec.title}`,
    );
    console.log(`wrote pdf/software/${pageSpec.pdf}`);
  }
  await printOne(
    context,
    softwareManualHtml,
    join(pdfDir, softwareManualPdf),
    "Software User Manual",
  );
  console.log(`wrote pdf/${softwareManualPdf}`);
  for (const board of boards) {
    const path = join(wiringPdfDir, `${board.id}.pdf`);
    await printOne(
      context,
      join(board.id, "index.html"),
      path,
      `Wiring Guide · ${board.title.split(" — ")[0]}`,
    );
    console.log(`wrote pdf/wiring/${board.id}.pdf`);
  }
  await printOne(
    context,
    wiringGuideHtml,
    join(pdfDir, wiringGuidePdf),
    "Wiring Guide",
  );
  console.log(`wrote pdf/${wiringGuidePdf}`);
  await browser.close();
}

async function main() {
  await mkdir(dest, { recursive: true });
  await copySoftware();
  await copyBoards();
  if (softwareSrc !== dest) {
    console.log(`copied software HTML from ${softwareSrc} to ${dest}`);
  }
  if (existsSync(join(shopDocs, "Board_A", "index.html"))) {
    console.log(`copied board HTML from ${shopDocs} to ${dest}`);
  }
  if (!existsSync(join(dest, "board-a", "index.html"))) {
    throw new Error(
      "missing html/board-a/index.html; need shop docs/Board_A or published board HTML",
    );
  }
  for (const pageSpec of pages) {
    const path = join(dest, pageSpec.html);
    let html = await readFile(path, "utf8");
    html = prepareSoftwareHtml(html);
    if (pageSpec.html === "index.html") {
      html = addPdfIndex(html);
    }
    await writeFile(path, html);
  }
  for (const board of boards) {
    const path = join(dest, board.id, "index.html");
    let html = await readFile(path, "utf8");
    html = prepareBoardHtml(html, board.id);
    await writeFile(path, html);
  }
  await writeSoftwareManualHtml();
  await writeWiringGuideHtml();
  await printPdfs();
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
