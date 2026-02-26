import { BrowserRouter, Routes, Route, Link } from "react-router-dom";
import Home from "./pages/Home";
import Philosophy from "./pages/Philosophy";
import Docs from "./pages/Docs";

const basename = import.meta.env.BASE_URL.replace(/\/+$/, "");

function App() {
  return (
    <BrowserRouter basename={basename}>
      <nav className="nav">
        <div className="nav-links">
          <Link to="/">Home</Link>
          <Link to="/docs">Docs</Link>
          <Link to="/philosophy">Philosophy</Link>
          <a
            href="https://github.com/MercuriusDream/Vibrowser"
            target="_blank"
            rel="noopener noreferrer"
          >
            GitHub
          </a>
          <a
            href="https://github.com/MercuriusDream/claude-estate"
            target="_blank"
            rel="noopener noreferrer"
          >
            claude-estate
          </a>
        </div>
      </nav>
      <Routes>
        <Route path="/" element={<Home />} />
        <Route path="/philosophy" element={<Philosophy />} />
        <Route path="/docs" element={<Docs />} />
        <Route path="/docs/:slug" element={<Docs />} />
      </Routes>
    </BrowserRouter>
  );
}

export default App;
