import { useEffect, useRef, useState } from "react";

const CYCLES = 404;
const LOG_LINES = [
  { t: "estate", msg: "Reading .claude/claude-estate.md ..." },
  { t: "estate", msg: "Ledger loaded. Last cycle: 403. Resuming." },
  { t: "spawn", msg: "Spawning subagent → harden HTTP/2 status-line parsing" },
  { t: "agent", msg: "Grep: scanning src/network/http_parser.cpp ..." },
  { t: "agent", msg: "Found 3 whitespace edge-cases in status-line tokenizer" },
  { t: "agent", msg: "Edit: http_parser.cpp:247 — fail-closed on NUL byte" },
  { t: "agent", msg: "Edit: http_parser.cpp:312 — reject vertical-tab in reason-phrase" },
  { t: "test", msg: "Running test_request_contracts ... 147/147 passed" },
  { t: "agent", msg: "Write: tests/test_http2_status_whitespace.cpp — 6 new cases" },
  { t: "test", msg: "Running full suite ... 892/892 passed ✓" },
  { t: "commit", msg: 'git commit -m "Cycle 404: harden HTTP/2 status-line whitespace"' },
  { t: "estate", msg: "Ledger updated. Cycle 404 complete." },
  { t: "estate", msg: "Scanning for next priority ..." },
  { t: "spawn", msg: "Spawning subagent → CSS cascade specificity edge-cases" },
  { t: "agent", msg: "Read: src/css/cascade.cpp — analyzing !important ordering" },
];

function useTypingLog() {
  const [lines, setLines] = useState([]);
  const idx = useRef(0);

  useEffect(() => {
    const interval = setInterval(() => {
      if (idx.current < LOG_LINES.length) {
        setLines((prev) => [...prev, LOG_LINES[idx.current]]);
        idx.current++;
      } else {
        // loop
        idx.current = 0;
        setLines([]);
      }
    }, 800);
    return () => clearInterval(interval);
  }, []);

  return lines;
}

function LogTerminal() {
  const lines = useTypingLog();
  const ref = useRef(null);

  useEffect(() => {
    if (ref.current) ref.current.scrollTop = ref.current.scrollHeight;
  }, [lines]);

  const colors = {
    estate: "var(--log-estate)",
    spawn: "var(--log-spawn)",
    agent: "var(--log-agent)",
    test: "var(--log-test)",
    commit: "var(--log-commit)",
  };

  return (
    <div className="terminal" ref={ref}>
      <div className="terminal-header">
        <span className="terminal-dot terminal-dot--red" />
        <span className="terminal-dot terminal-dot--yellow" />
        <span className="terminal-dot terminal-dot--green" />
        <span className="terminal-title">claude-estate — cycle {CYCLES}</span>
      </div>
      <div className="terminal-body">
        {lines.map((l, i) => (
          <div key={i} className="terminal-line">
            <span className="terminal-tag" style={{ color: colors[l.t] }}>
              [{l.t}]
            </span>{" "}
            <span className="terminal-msg">{l.msg}</span>
          </div>
        ))}
        <span className="terminal-cursor">█</span>
      </div>
    </div>
  );
}

function Home() {
  return (
    <div className="page">
      {/* Hero */}
      <section className="hero">
        <h1 className="hero-title">
          A web browser<br />
          written by autonomous agents.
        </h1>
        <p className="hero-lead">
          No WebKit/Chromium, but a C++17 browser engine built from
          nothing by Claude Code running in ultralong horizon — {CYCLES} cycles and
          counting.
        </p>
        <div className="hero-actions">
          <a
            href="https://github.com/MercuriusDream/Vibrowser"
            className="btn btn-primary"
            target="_blank"
            rel="noopener noreferrer"
          >
            View the browser
          </a>
          <a
            href="https://github.com/MercuriusDream/claude-estate"
            className="btn"
            target="_blank"
            rel="noopener noreferrer"
          >
            View the harness
          </a>
          <a href="/docs" className="btn btn-ghost">
            Docs
          </a>
        </div>
      </section>

      {/* Stats */}
      <section className="stats-bar">
        <div className="stat">
          <span className="stat-number">{CYCLES}+</span>
          <span className="stat-label">autonomous cycles</span>
        </div>
        <div className="stat">
          <span className="stat-number">100+</span>
          <span className="stat-label">hours of agent runtime</span>
        </div>
        <div className="stat">
          <span className="stat-number">0</span>
          <span className="stat-label">lines hand-written</span>
        </div>
        <div className="stat">
          <span className="stat-number">892</span>
          <span className="stat-label">tests passing</span>
        </div>
      </section>

      {/* Live terminal */}
      <section className="section">
        <h2 className="section-title">This is what it looks like</h2>
        <p className="section-desc">
          Every cycle, the agent reads its estate, picks the highest-priority
          task, spawns subagents, writes code, runs tests, commits, and loops.
          This is a real cycle replayed.
        </p>
        <LogTerminal />
      </section>

      {/* The harness */}
      <section className="section">
        <h2 className="section-title">The harness: claude-estate</h2>
        <div className="prose">
          <p>
            <strong>
              <a
                href="https://github.com/MercuriusDream/claude-estate"
                target="_blank"
                rel="noopener noreferrer"
              >
                claude-estate
              </a>
            </strong>{" "}
            is the autonomous development harness that drives this. It's not a
            wrapper or a prompt template. It's a perpetual loop protocol that
            gives Claude Code persistent memory, task prioritization, subagent
            orchestration, and the ability to run for days without human
            intervention.
          </p>
        </div>
        <div className="estate-grid">
          <div className="estate-card">
            <div className="estate-card-label">Read</div>
            <div className="estate-card-detail">
              Agent loads <code>.claude/claude-estate.md</code> — the living
              document that defines project state, priorities, and architectural
              decisions.
            </div>
          </div>
          <div className="estate-card">
            <div className="estate-card-label">Plan</div>
            <div className="estate-card-detail">
              Picks highest-priority task from the ledger. Breaks it into
              subtasks. Decides which subagents to spawn.
            </div>
          </div>
          <div className="estate-card">
            <div className="estate-card-label">Execute</div>
            <div className="estate-card-detail">
              Subagents work in parallel — one writes code, another writes
              tests, another reviews. All through Claude Code's tool system.
            </div>
          </div>
          <div className="estate-card">
            <div className="estate-card-label">Verify</div>
            <div className="estate-card-detail">
              Full test suite runs. If anything breaks, the agent debugs and
              fixes before committing. Fail-closed.
            </div>
          </div>
          <div className="estate-card">
            <div className="estate-card-label">Commit</div>
            <div className="estate-card-detail">
              Clean atomic commit with descriptive message. Ledger updated.
              Estate file refreshed. Next cycle begins.
            </div>
          </div>
        </div>
      </section>

      {/* What the agents built */}
      <section className="section">
        <h2 className="section-title">What 400+ cycles produced</h2>
        <p className="section-desc">
          The agent didn't just write a toy parser. It built a real rendering
          pipeline from raw TCP to pixels.
        </p>
        <div className="pipeline-full">
          <div className="pipeline-stage">
            <div className="pipeline-stage-name">Network</div>
            <div className="pipeline-stage-desc">
              HTTP/1.1 + HTTP/2 fetch with hardened status-line parsing,
              chunked transfer, redirect following
            </div>
          </div>
          <div className="pipeline-arrow-v" />
          <div className="pipeline-stage">
            <div className="pipeline-stage-name">HTML Parser</div>
            <div className="pipeline-stage-desc">
              Tokenizer → tree construction following the spec. Handles
              malformed HTML, implicit closes, void elements
            </div>
          </div>
          <div className="pipeline-arrow-v" />
          <div className="pipeline-stage">
            <div className="pipeline-stage-name">CSS Engine</div>
            <div className="pipeline-stage-desc">
              Selector matching, cascade resolution, specificity calculation,
              inheritance, computed style generation
            </div>
          </div>
          <div className="pipeline-arrow-v" />
          <div className="pipeline-stage">
            <div className="pipeline-stage-name">Layout</div>
            <div className="pipeline-stage-desc">
              Block and inline formatting contexts, box model with margins,
              padding, borders, text wrapping
            </div>
          </div>
          <div className="pipeline-arrow-v" />
          <div className="pipeline-stage">
            <div className="pipeline-stage-name">Paint</div>
            <div className="pipeline-stage-desc">
              Rasterization to pixel buffer — backgrounds, borders, text
              rendering, color blending → .ppm output
            </div>
          </div>
          <div className="pipeline-arrow-v" />
          <div className="pipeline-stage">
            <div className="pipeline-stage-name">JavaScript</div>
            <div className="pipeline-stage-desc">
              Subset interpreter for DOM access, style mutation,
              querySelector, event basics — enough to be dangerous
            </div>
          </div>
        </div>
      </section>

      {/* Proof */}
      <section className="section">
        <h2 className="section-title">Don't take our word for it</h2>
        <p className="section-desc">
          The entire git history is the proof. Every commit was made by an
          agent. Every cycle is logged.
        </p>
        <pre className="code-block">
          <code>{`$ git log --oneline -20
ef73d86 Cycle 404: harden HTTP/2 status-line whitespace handling
d94c5b5 Cycle 403: harden HTTP status-line NUL separator regression coverage
e1ebdcd Cycle 402: harden HTTP status-line CR/LF separator regressions
27a79b7 test: add vertical-tab HTTP status-line fail-closed coverage
...
# 400+ commits. All autonomous. All tested.`}</code>
        </pre>
        <div className="proof-links">
          <a
            href="https://github.com/MercuriusDream/Vibrowser/commits/main"
            className="btn btn-ghost"
            target="_blank"
            rel="noopener noreferrer"
          >
            Full commit history
          </a>
          <a
            href="https://github.com/MercuriusDream/claude-estate"
            className="btn btn-ghost"
            target="_blank"
            rel="noopener noreferrer"
          >
            claude-estate source
          </a>
        </div>
      </section>

      {/* Quick start */}
      <section className="section">
        <h2 className="section-title">Try it</h2>
        <pre className="code-block">
          <code>{`git clone https://github.com/MercuriusDream/Vibrowser.git
cd Vibrowser
cmake -S . -B build && cmake --build build
./build/vibrowser --input test.html --output render.ppm`}</code>
        </pre>
        <p className="section-note">
          Requires macOS 13+ or Linux, C++17 (Clang 15+ / GCC 12+), CMake
          3.20+.
        </p>
      </section>

      {/* License */}
      <section className="section section-last">
        <p className="section-note">
          GNU AGPLv3 License.{" "}
          <a href="https://github.com/MercuriusDream/Vibrowser/blob/main/LICENSE">
            See LICENSE
          </a>
          .
        </p>
      </section>
    </div>
  );
}

export default Home;
