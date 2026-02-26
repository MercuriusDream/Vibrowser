import { useParams, Link } from "react-router-dom";

const docList = [
  { slug: "what-is-vibrowser", title: "What is Vibrowser?" },
  { slug: "installation", title: "Installation" },
  { slug: "tutorial", title: "Getting Started Tutorial" },
  { slug: "dependencies", title: "Dependencies and Requirements" },
  { slug: "architecture", title: "Architecture" },
  { slug: "license", title: "License" },
];

// Simple markdown-like content
const docContent = {
  "what-is-vibrowser": `Vibrowser is a web browser built from scratch in C++17.

No WebKit. No Blink. No Gecko. Just raw code, built line by line, in public.

## Why?

Because browsers are too important to be black boxes. We use them for everything—work, entertainment, communication—yet most of us have no idea how they actually work. Vibrowser exists to change that.

## What exists today

- HTML/CSS parser pipeline
- Selector matching and style resolution  
- JavaScript subset for DOM/style mutation
- Layout engine and paint system
- Output to .ppm image files
- Scripted smoke tests and fixtures

## What we're building toward

A complete, usable browser engine.`,

  "installation": `## Requirements

- **OS**: macOS 13+ (development currently targets macOS)
- **Compiler**: Clang 15+ or GCC 12+
- **CMake**: 3.20+
- **Make**: BSD Make or GNU Make

## Build from source

    cmake -S . -B build
    cmake --build build
    ctest --test-dir build

## Running

Currently, Vibrowser renders to static .ppm files:

    ./build/vibrowser --input test.html --output render.ppm`,

  "tutorial": `## Getting Started

### Step 1: Create a test HTML file

Create hello.html with some basic HTML and CSS.

### Step 2: Render it

    ./build/vibrowser --input hello.html --output hello.ppm

### Step 3: View the output

Open hello.ppm in any image viewer, or convert it:

    convert hello.ppm hello.png

## Understanding the output

The rendering pipeline:

1. Fetch — Load the HTML file
2. Parse — Build the DOM tree
3. Style — Apply CSS and compute styles
4. Layout — Calculate element positions
5. Paint — Rasterize to pixels
6. Output — Save as PPM image`,

  "dependencies": `## Build Dependencies

| Dependency | Minimum | Purpose |
|------------|---------|---------|
| CMake | 3.20 | Build system |
| C++ Compiler | C++17 | Core language |
| Make | any | Build tool |

## macOS

    xcode-select --install
    brew install cmake

## Linux

    sudo apt-get install cmake build-essential

## Runtime Dependencies

None. Vibrowser is self-contained.

## Hardware Requirements

- RAM: 4GB minimum, 8GB recommended
- Storage: 500MB for build artifacts
- Architecture: ARM64 or x86_64`,

  "architecture": `## Pipeline

    HTML/CSS → Parse → DOM → Style → Layout → Paint → PPM
                   ↑                           ↓
                   └──── JavaScript (subset) ──┘

## Components

### HTML Parser
- Incremental tokenizer
- Spec-compliant tree construction
- Handles malformed HTML gracefully

### CSS Parser
- Selector parsing
- Property/value parsing
- Cascade and specificity calculation

### Style Engine
- Selector matching
- Computed style resolution

### Layout Engine
- Box model implementation
- Flow layout (block and inline)

### Paint System
- Display list generation
- Rasterization`,

  "license": `GNU Affero General Public License v3.0 (AGPL-3.0).

The full license text is in the root LICENSE file.`,
};

function Docs() {
  const { slug } = useParams();
  const currentSlug = slug || "what-is-vibrowser";
  const content = docContent[currentSlug];
  const currentDoc = docList.find(d => d.slug === currentSlug);

  return (
    <div className="page page-docs">
      <aside className="docs-sidebar">
        <h2 className="docs-sidebar-title">Documentation</h2>
        <nav className="docs-nav">
          {docList.map((doc) => (
            <Link
              key={doc.slug}
              to={`/docs/${doc.slug}`}
              className={doc.slug === currentSlug ? "active" : ""}
            >
              {doc.title}
            </Link>
          ))}
        </nav>
      </aside>
      <main className="docs-content">
        <article className="article">
          <h1 className="article-title">{currentDoc?.title}</h1>
          <div className="prose">
            {content.split('\n\n').map((para, i) => {
              if (para.startsWith('## ')) {
                return <h2 key={i}>{para.replace('## ', '')}</h2>;
              }
              if (para.startsWith('| ')) {
                // Simple table rendering
                const lines = para.split('\n');
                return (
                  <table key={i} className="simple-table">
                    <tbody>
                      {lines.filter(l => l.startsWith('|')).map((line, j) => {
                        const cells = line.split('|').filter(c => c.trim());
                        if (j === 1) return null; // Skip separator
                        return (
                          <tr key={j}>
                            {cells.map((cell, k) => (
                              <td key={k}>{cell.trim()}</td>
                            ))}
                          </tr>
                        );
                      })}
                    </tbody>
                  </table>
                );
              }
              if (para.startsWith('    ')) {
                return (
                  <pre key={i} className="code-block">
                    <code>{para.replace(/^    /gm, '')}</code>
                  </pre>
                );
              }
              if (para.startsWith('- ')) {
                return (
                  <ul key={i} className="simple-list">
                    {para.split('\n').map((item, j) => (
                      <li key={j}>{item.replace('- ', '')}</li>
                    ))}
                  </ul>
                );
              }
              if (para.startsWith('1. ')) {
                return (
                  <ol key={i}>
                    {para.split('\n').map((item, j) => (
                      <li key={j}>{item.replace(/^\d+\. /, '')}</li>
                    ))}
                  </ol>
                );
              }
              return <p key={i}>{para}</p>;
            })}
          </div>
        </article>
      </main>
    </div>
  );
}

export default Docs;
