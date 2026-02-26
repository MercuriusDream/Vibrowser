# Getting Started Tutorial

This tutorial walks you through rendering your first HTML file with Vibrowser.

## Step 1: Create a test HTML file

Create `hello.html`:

```html
<!DOCTYPE html>
<html>
<head>
  <style>
    body {
      background: #f0f0f0;
      font-family: sans-serif;
      padding: 20px;
    }
    h1 {
      color: #333;
    }
  </style>
</head>
<body>
  <h1>Hello, Vibrowser!</h1>
  <p>This is my first render.</p>
</body>
</html>
```

## Step 2: Render it

```bash
./build/vibrowser --input hello.html --output hello.ppm
```

## Step 3: View the output

Open `hello.ppm` in any image viewer that supports PPM format, or convert it:

```bash
convert hello.ppm hello.png
```

## Understanding the output

Vibrowser currently renders to a static image. The rendering pipeline:

1. **Fetch** — Load the HTML file
2. **Parse** — Build the DOM tree
3. **Style** — Apply CSS and compute styles
4. **Layout** — Calculate element positions
5. **Paint** — Rasterize to pixels
6. **Output** — Save as PPM image

Each stage produces inspectable artifacts so you can see exactly what's happening.
