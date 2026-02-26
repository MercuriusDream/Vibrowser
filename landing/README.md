# Vibrowser

A web browser built from scratch in C++17.

**Vibrowser is a web browser engine built from scratch in C++17.** No WebKit. No Blink. No Gecko. Just raw code, built line by line, in public.

## Why?

Because browsers are too important to be black boxes. We use them for everything—work, entertainment, communication—yet most of us have no idea how they actually work. Vibrowser exists to change that; Yet barely working for now.

## What exists today

- HTML/CSS parser pipeline
- Selector matching and style resolution
- JavaScript subset for DOM/style mutation
- Layout engine and paint system
- Output to .ppm image files
- Scripted smoke tests and fixtures

## Quick start

```bash
cmake -S . -B build
cmake --build build
./build/vibrowser --input test.html --output render.ppm
```

See [docs/installation.md](docs/installation.md) for detailed setup instructions.

## Documentation

- [What is Vibrowser?](docs/what-is-vibrowser.md)
- [Installation](docs/installation.md)
- [Getting Started Tutorial](docs/tutorial.md)
- [Dependencies](docs/dependencies.md)
- [Architecture](docs/architecture.md)
- [License](docs/license.md)

## How it's built

Vibrowser is built using the **Claude Estate protocol**—autonomous AI agents working iteratively to build complex software. Read more in [docs/philosophy.md](landing/src/pages/Philosophy.jsx) or visit the [Philosophy page](https://vibrowser.dev/philosophy).

## License

MIT License. See [LICENSE](LICENSE) for details.
