function Philosophy() {
  return (
    <div className="page page-narrow">
      <article className="article">
        <header className="article-header">
          <p className="article-kicker">How this is being built</p>
          <h1 className="article-title">Letting autonomous agents build software</h1>
        </header>

        <div className="prose">
          <p className="lead">
            Vibrowser is not just a browser. It's an experiment in 
            autonomous software development—using AI agents that work 
            continuously, building complex systems through iterative 
            refinement.
          </p>

          <h2>The Claude Estate Protocol</h2>
          
          <p>
            This project follows the <strong>Claude Estate protocol</strong>—a 
            method for perpetual autonomous development. Here's how it works:
          </p>

          <ol>
            <li>
              <strong>Read the estate</strong> — The agent reads 
              <code>.claude/claude-estate.md</code> and{" "}
              <code>.claude/claude-estate.local.md</code> to understand 
              current state and priorities.
            </li>
            <li>
              <strong>Do the work</strong> — Using subagents, it breaks down 
              tasks and implements them iteratively.
            </li>
            <li>
              <strong>Log everything</strong> — All work is logged to{" "}
              <code>.claude/estate-logs/</code> for transparency.
            </li>
            <li>
              <strong>Update the ledger</strong> — State is persisted so the 
              next session can pick up seamlessly.
            </li>
            <li>
              <strong>Loop</strong> — The process repeats, continuously 
              improving the software.
            </li>
          </ol>

          <h2>Why this approach?</h2>

          <p>
            Traditional software development happens in bursts—a few hours 
            here, a weekend there. But complex systems like browsers need 
            sustained, consistent effort. The estate protocol enables that 
            through:
          </p>

          <ul>
            <li>
              <strong>Continuity</strong> — Work persists across sessions. 
              The agent remembers where it left off.
            </li>
            <li>
              <strong>Iterative refinement</strong> — Small, continuous 
              improvements compound over time.
            </li>
            <li>
              <strong>Documentation as process</strong> — The estate files 
              aren't just records; they're the steering mechanism.
            </li>
            <li>
              <strong>Subagent parallelism</strong> — Complex tasks are 
              delegated to specialized subagents that work in parallel.
            </li>
          </ul>

          <h2>What this means for the code</h2>

          <p>
            Code written through this process has different characteristics 
            than traditionally authored software:
          </p>

          <ul>
            <li>
              <strong>Highly documented</strong> — The agent generates 
              explanations as it works, making the codebase unusually 
              readable.
            </li>
            <li>
              <strong>Modular by necessity</strong> — Subagents work best 
              with clear boundaries, so the architecture tends toward 
              clean separation.
            </li>
            <li>
              <strong>Test-heavy</strong> — The agent generates tests to 
              verify its own work, resulting in good coverage.
            </li>
            <li>
              <strong>Iterative quality</strong> — Early iterations may be 
              rough, but continuous refinement improves quality over time.
            </li>
          </ul>

          <h2>The human role</h2>

          <p>
            This isn't about replacing human developers. It's about 
            augmenting them. The human provides:
          </p>

          <ul>
            <li>High-level direction and priorities</li>
            <li>Code review and architectural decisions</li>
            <li>Creative problem-solving for novel challenges</li>
            <li>The final say on what gets merged</li>
          </ul>

          <p>
            The agent handles implementation, documentation, testing, and 
            the continuous grind of software development.
          </p>

          <h2>A new way to build</h2>

          <p>
            Vibrowser is both a product and a process. The browser is the 
            artifact, but the method is the contribution. If we can build 
            something as complex as a browser this way, what else becomes 
            possible?
          </p>

          <p>
            This is vibecoding at scale—not just generating snippets, but 
            architecting complete systems through sustained, autonomous 
            effort.
          </p>
        </div>
      </article>
    </div>
  );
}

export default Philosophy;
