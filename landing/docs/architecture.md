# Architecture

Vibrowser follows a traditional browser engine architecture with explicit stages.

## Pipeline

```
HTML/CSS → Parse → DOM → Style → Layout → Paint → PPM
   ↑                                     ↓
   └──────── JavaScript (subset) ────────┘
```

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
- Inheritance handling

### Layout Engine

- Box model implementation
- Flow layout (block and inline)
- Margin collapsing

### Paint System

- Display list generation
- Rasterization
- PPM output

## Code Organization

```
src/
├── html/        # HTML parsing
├── css/         # CSS parsing and styling
├── layout/      # Layout engine
├── paint/       # Rendering and output
├── js/          # JavaScript subset
└── util/        # Utilities
```
