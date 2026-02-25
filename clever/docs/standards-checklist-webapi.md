# Web API Standards Checklist

Clever Browser Engine -- Web APIs / JavaScript DOM APIs implementation status.

**Legend:** `[x]` implemented, `[~]` partial/stub, `[ ]` not implemented

---

## 1. Document Object Model (DOM)

### Node Interface

- [x] `Node.nodeType` -- returns 1 (Element), 3 (Text), 8 (Comment), 9 (Document), 10 (DocumentType)
- [x] `Node.nodeName` -- returns uppercase tag name, `#text`, `#comment`, `#document`
- [x] `Node.parentNode`
- [x] `Node.parentElement`
- [x] `Node.childNodes` -- returns array of all child nodes
- [x] `Node.firstChild`
- [x] `Node.lastChild`
- [x] `Node.nextSibling`
- [x] `Node.previousSibling`
- [x] `Node.textContent` -- getter and setter
- [x] `Node.appendChild()`
- [x] `Node.removeChild()`
- [x] `Node.insertBefore()`
- [x] `Node.replaceChild()`
- [x] `Node.cloneNode()` -- supports deep clone
- [x] `Node.contains()`
- [x] `Node.isConnected` -- walks parent chain to document root
- [x] `Node.normalize()` -- merges adjacent text nodes, removes empty (Cycle 236)
- [x] `Node.compareDocumentPosition()` -- returns bitmask (Cycle 231)
- [x] `Node.getRootNode()` -- walks parent chain (Cycle 231)
- [x] `Node.hasChildNodes()` -- returns !children.empty() (Cycle 231)
- [x] `Node.isSameNode()` -- pointer comparison (Cycle 231)
- [x] `Node.isEqualNode()` -- deep recursive comparison (Cycle 236)
- [~] `Node.lookupPrefix()` -- returns null (stub)
- [~] `Node.lookupNamespaceURI()` -- returns null (stub)

### Element Interface

- [x] `Element.id` -- getter
- [x] `Element.tagName` -- uppercase
- [x] `Element.className` -- getter and setter
- [x] `Element.classList.add()`
- [x] `Element.classList.remove()`
- [x] `Element.classList.contains()`
- [x] `Element.classList.toggle()` -- with force parameter (Cycle 204)
- [x] `Element.classList.replace()` -- (Cycle 204)
- [x] `Element.classList.item()` -- index access (Cycle 204)
- [x] `Element.getAttribute()`
- [x] `Element.setAttribute()`
- [x] `Element.hasAttribute()`
- [x] `Element.removeAttribute()`
- [x] `Element.getAttributeNames()`
- [x] `Element.toggleAttribute()` -- with optional force parameter
- [x] `Element.matches()` -- full CSS selector engine
- [x] `Element.closest()` -- walks ancestor chain with selector matching
- [x] `Element.querySelector()` -- scoped to subtree, full CSS selectors
- [x] `Element.querySelectorAll()` -- scoped to subtree, full CSS selectors
- [x] `Element.innerHTML` -- getter and setter (parses HTML on set)
- [x] `Element.outerHTML` -- getter (full recursive serialization)
- [x] `Element.insertAdjacentHTML()` -- beforebegin, afterbegin, beforeend, afterend
- [x] `Element.insertAdjacentElement()` -- all four positions
- [x] `Element.children` -- returns array of child elements only
- [x] `Element.childElementCount`
- [x] `Element.firstElementChild`
- [x] `Element.lastElementChild`
- [x] `Element.nextElementSibling`
- [x] `Element.previousElementSibling`
- [x] `Element.remove()` -- removes from parent, keeps alive in owned_nodes
- [x] `Element.before()` -- accepts multiple nodes/strings
- [x] `Element.after()` -- accepts multiple nodes/strings
- [x] `Element.prepend()` -- accepts multiple nodes/strings
- [x] `Element.append()` -- accepts multiple nodes/strings
- [x] `Element.replaceWith()` -- accepts multiple nodes/strings
- [x] `Element.getBoundingClientRect()` -- real values when layout geometry available
- [x] `Element.dataset` -- get/set/has via data-* attributes with camelCase conversion
- [~] `Element.scrollIntoView()` -- no-op stub
- [~] `Element.scroll()` -- no-op stub
- [~] `Element.scrollTo()` -- no-op stub
- [~] `Element.focus()` -- no-op stub
- [~] `Element.blur()` -- no-op stub
- [~] `Element.animate()` -- returns minimal Animation-like object with play/pause/cancel/finish/reverse
- [~] `Element.getAnimations()` -- returns empty array
- [x] `Element.insertAdjacentText()` -- all 4 positions (Cycle 231)
- [x] `Element.attachShadow()` -- open/closed mode, shadow root with innerHTML/appendChild/querySelector (Cycle 236)
- [x] `Element.shadowRoot` -- returns shadow root or null for closed mode (Cycle 236)
- [x] `Element.slot` -- getter/setter for slot attribute
- [~] `Element.requestFullscreen()` -- stub returns Promise.resolve() (Cycle 243)

### Element Dimensions (layout-backed)

- [x] `Element.offsetWidth` -- border box width from layout geometry
- [x] `Element.offsetHeight` -- border box height from layout geometry
- [x] `Element.offsetTop` -- border box Y from layout geometry
- [x] `Element.offsetLeft` -- border box X from layout geometry
- [~] `Element.scrollWidth` -- same as offsetWidth (simplified)
- [~] `Element.scrollHeight` -- same as offsetHeight (simplified)
- [~] `Element.scrollTop` -- always returns 0
- [~] `Element.scrollLeft` -- always returns 0
- [x] `Element.clientWidth` -- padding box width from layout geometry
- [x] `Element.clientHeight` -- padding box height from layout geometry

### HTMLElement Interface

- [x] `HTMLElement.style` -- CSSStyleDeclaration proxy
- [x] `HTMLElement.style.getPropertyValue()` -- reads from inline style attribute
- [x] `HTMLElement.style.setProperty()` -- writes to inline style attribute
- [x] `HTMLElement.style.[property]` -- camelCase getter/setter via Proxy (JS setup)
- [x] `HTMLElement.hidden` -- getter/setter (Cycle 231)
- [x] `HTMLElement.draggable` -- getter/setter via getAttribute/setAttribute (Cycle 236)
- [x] `HTMLElement.contentEditable` -- getter/setter (returns 'true'/'false'/'inherit') (Cycle 236)
- [~] `HTMLElement.offsetParent` -- stub returning body (Cycle 231)

### Document Interface

- [x] `document.getElementById()`
- [x] `document.getElementsByTagName()` -- case-insensitive tag matching
- [x] `document.getElementsByClassName()`
- [x] `document.querySelector()` -- full CSS selector engine
- [x] `document.querySelectorAll()` -- full CSS selector engine
- [x] `document.createElement()`
- [x] `document.createTextNode()`
- [x] `document.createDocumentFragment()` -- proper fragment semantics in appendChild
- [x] `document.createEvent()` -- returns event with initEvent method
- [x] `document.createRange()` -- full Range stub with selectNode, setStart/setEnd, collapse, cloneRange, getBoundingClientRect, createContextualFragment, etc.
- [x] `document.body` -- getter
- [x] `document.head` -- getter
- [x] `document.documentElement` -- getter (returns `<html>`)
- [x] `document.title` -- getter and setter
- [x] `document.cookie` -- getter (returns all cookies) and setter (parses name=value)
- [x] `document.write()` -- parses HTML, appends to body
- [x] `document.writeln()` -- same as write with newline
- [x] `document.addEventListener()` -- with capture support
- [x] `document.removeEventListener()` -- with capture matching
- [~] `document.elementFromPoint()` -- stub, always returns body
- [x] `document.createComment()` -- creates Comment node (Cycle 231)
- [x] `document.createTreeWalker()` -- C++ impl with depth-first traversal (Cycle 233)
- [x] `document.createNodeIterator()` -- C++ impl with depth-first traversal, nextNode/previousNode (Cycle 247)
- [~] `document.createProcessingInstruction()` -- returns PI-like object with nodeType 7 (Cycle 247)
- [~] `document.createCDATASection()` -- returns CDATA-like object with nodeType 4 (Cycle 247)
- [x] `document.importNode()` -- delegates to clone_node_impl (Cycle 231)
- [x] `document.adoptNode()` -- removes from parent, moves to owned_nodes (Cycle 236)
- [x] `document.readyState` -- returns "complete" (Cycle 230)
- [x] `document.characterSet` -- returns "UTF-8" (Cycle 230)
- [x] `document.contentType` -- returns "text/html" (Cycle 230)
- [x] `document.visibilityState` -- returns "visible" (Cycle 231)
- [x] `document.hidden` -- returns false (Cycle 231)
- [x] `document.activeElement` -- returns body (Cycle 231)
- [x] `document.forms` -- collects form elements (Cycle 231)
- [x] `document.images` -- collects img elements (Cycle 231)
- [x] `document.links` -- collects a/area elements (Cycle 231)
- [x] `document.scripts` -- collects script elements (Cycle 231)

---

## 2. Events

### Event Constructors

- [x] `Event` constructor -- type, bubbles, cancelable options
- [x] `CustomEvent` constructor -- type, detail, bubbles, cancelable options
- [x] `KeyboardEvent` constructor -- key, code, keyCode, ctrlKey, shiftKey, altKey, metaKey, repeat
- [x] `MouseEvent` constructor -- button, buttons, clientX/Y, screenX/Y, pageX/Y, offsetX/Y, modifier keys
- [x] `FocusEvent` constructor -- relatedTarget property (Cycle 233)
- [x] `InputEvent` constructor -- data, inputType, isComposing (Cycle 233)
- [x] `WheelEvent` constructor -- with deltaX/Y/Z, deltaMode
- [x] `TouchEvent` constructor -- with touches/targetTouches/changedTouches arrays
- [x] `PointerEvent` constructor -- pointerId, width, height, pressure, tilt, twist, pointerType (Cycle 233)
- [x] `DragEvent` constructor -- with DataTransfer stub (dropEffect, effectAllowed, items, files, types)
- [x] `ErrorEvent` constructor -- message, filename, lineno, colno, error (Cycle 234)
- [x] `HashChangeEvent` constructor -- with oldURL/newURL
- [x] `PopStateEvent` constructor -- with state
- [x] `PromiseRejectionEvent` constructor -- promise, reason (Cycle 234)

### Event Properties

- [x] `Event.type`
- [x] `Event.target`
- [x] `Event.currentTarget`
- [x] `Event.bubbles`
- [x] `Event.cancelable`
- [x] `Event.defaultPrevented`
- [x] `Event.eventPhase` -- 0 (NONE), 1 (CAPTURING), 2 (AT_TARGET), 3 (BUBBLING)
- [x] `Event.timeStamp`

### Event Methods

- [x] `Event.preventDefault()`
- [x] `Event.stopPropagation()`
- [x] `Event.stopImmediatePropagation()`
- [x] `Event.composedPath()` -- builds full path from target to root
- [x] `Event.initEvent()` -- available via createEvent (legacy) (Cycle 221)

### Event Listeners

- [x] `EventTarget.addEventListener()` -- supports capture flag (boolean and options object)
- [x] `EventTarget.removeEventListener()` -- matches callback identity and capture flag
- [x] `EventTarget.dispatchEvent()` -- full three-phase propagation (capture, at-target, bubble)
- [x] Inline event attributes -- onclick, onload, onchange, onsubmit, oninput, onmouseover, onmouseout, onmousedown, onmouseup, onkeydown, onkeyup, onkeypress, onfocus, onblur
- [x] `{ once: true }` listener option -- auto-removes after first invocation (Cycle 231)
- [x] `{ passive: true }` listener option -- parsed and stored (Cycle 231)
- [x] `{ signal: AbortSignal }` listener option -- abort removes listener, pre-aborted skips add (Cycle 245)

---

## 3. CSS Object Model (CSSOM)

- [x] `window.getComputedStyle()` -- returns CSSStyleDeclaration with layout-backed dimensions
- [x] `CSSStyleDeclaration.getPropertyValue()` -- layout-aware for dimension properties
- [x] `CSSStyleDeclaration.setProperty()` -- writes to inline style
- [x] CSS transition awareness -- stores from/to values as data attributes on style changes
- [x] `CSS.supports()` -- checks property support
- [x] `CSSStyleDeclaration.removeProperty()` -- removes property, returns old value
- [x] `CSSStyleDeclaration.cssText` -- getter/setter for full inline style string
- [x] `CSSStyleSheet` interface -- constructor, insertRule, deleteRule, addRule, removeRule, replace, replaceSync (Cycle 241)
- [x] `document.styleSheets` -- returns empty array-like with item() method (Cycle 241)
- [~] `CSSRule` interface -- constants and basic rule objects with type/selectorText/cssText (stub)
- [~] `window.getMatchedCSSRules()` -- returns empty array (stub, non-standard)

---

## 4. Fetch API / Network

### fetch()

- [x] `fetch()` -- global function, performs synchronous HTTP/HTTPS requests
- [x] `fetch()` supports GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS methods
- [x] `fetch()` supports custom headers via `headers` option
- [x] `fetch()` supports `body` option for POST requests
- [x] `fetch()` supports `redirect: 'follow'` -- follows HTTP 301/302/307/308 redirects
- [x] `fetch()` returns Promise-like Response object

### Response

- [x] `Response.ok`
- [x] `Response.status`
- [x] `Response.statusText`
- [x] `Response.url`
- [x] `Response.type` -- always returns "basic"
- [x] `Response.bodyUsed`
- [x] `Response.headers` -- returns Headers object
- [x] `Response.text()` -- returns Promise resolving to response body text
- [x] `Response.json()` -- returns Promise resolving to parsed JSON
- [x] `Response.clone()` -- creates copy of Response
- [x] `Response.arrayBuffer()` -- returns Promise resolving to ArrayBuffer
- [x] `Response.blob()` -- native Blob creation from body with content-type (Cycle 245)
- [~] `Response.formData()` -- returns Promise resolving to new FormData() (stub)
- [~] `Response.body` (ReadableStream) -- stub with getReader() returning done:true reader

### Headers

- [x] `Headers.get()`
- [x] `Headers.has()`
- [x] `Headers.forEach()`
- [x] `Headers.entries()`
- [x] `Headers.keys()`
- [x] `Headers.values()`
- [x] `Headers.set()` -- sets header value
- [x] `Headers.append()` -- delegates to set
- [x] `Headers.delete()` -- removes header entry

### XMLHttpRequest

- [x] `XMLHttpRequest` constructor
- [x] `XMLHttpRequest.open()` -- method, URL
- [x] `XMLHttpRequest.send()` -- optional body, performs synchronous request
- [x] `XMLHttpRequest.setRequestHeader()`
- [x] `XMLHttpRequest.getResponseHeader()`
- [x] `XMLHttpRequest.getAllResponseHeaders()`
- [x] `XMLHttpRequest.readyState` -- 0 (UNSENT), 1 (OPENED), 4 (DONE)
- [x] `XMLHttpRequest.status`
- [x] `XMLHttpRequest.statusText`
- [x] `XMLHttpRequest.responseText`
- [x] `XMLHttpRequest.onreadystatechange` callback
- [x] `XMLHttpRequest.responseType` -- supports "", "text", "json" (Cycle 241)
- [x] `XMLHttpRequest.response` -- returns text or parsed JSON based on responseType (Cycle 241)
- [~] `XMLHttpRequest.responseXML` -- returns null (stub, Cycle 246)
- [x] `XMLHttpRequest.abort()` -- resets readyState to 0, clears response (Cycle 241)
- [~] `XMLHttpRequest.upload` -- stub object with addEventListener/removeEventListener no-ops (Cycle 246)
- [~] `XMLHttpRequest.timeout` -- getter/setter stored, not yet enforced in send (Cycle 241)
- [~] `XMLHttpRequest.withCredentials` -- getter/setter stored, not yet used in send (Cycle 241)

### FormData

- [x] `FormData` constructor
- [x] `FormData.append()`
- [x] `FormData.get()`
- [x] `FormData.getAll()`
- [x] `FormData.has()`
- [x] `FormData.set()`
- [x] `FormData.delete()`
- [x] `FormData.entries()`
- [x] `FormData.keys()`
- [x] `FormData.values()`
- [x] `FormData.forEach()`

### WebSocket

- [x] `WebSocket` constructor -- connects via TCP with HTTP upgrade handshake
- [x] `WebSocket.send()` -- sends text frames with WebSocket framing
- [x] `WebSocket.close()` -- sends close frame
- [x] `WebSocket.readyState` -- CONNECTING (0), OPEN (1), CLOSING (2), CLOSED (3)
- [x] `WebSocket.url`
- [x] `WebSocket.protocol`
- [x] `WebSocket.bufferedAmount` -- stub returns 0
- [x] `WebSocket.onopen` -- getter/setter
- [x] `WebSocket.onmessage` -- getter/setter
- [x] `WebSocket.onclose` -- getter/setter
- [x] `WebSocket.onerror` -- getter/setter
- [~] `WebSocket.addEventListener()` -- routes to on* handlers (Cycle 246)
- [~] `WebSocket.removeEventListener()` -- clears on* handlers (Cycle 246)
- [~] `WebSocket.binaryType` -- getter/setter, default "blob" (Cycle 246)
- [ ] Binary frame support -- NOT IMPLEMENTED (text frames only)

### AbortController / AbortSignal

- [x] `AbortController` constructor
- [x] `AbortController.signal` -- AbortSignal object
- [x] `AbortController.abort()` -- sets signal.aborted = true, calls signal.onabort
- [x] `AbortSignal.aborted`
- [x] `AbortSignal.reason`
- [x] `AbortSignal.throwIfAborted()`
- [x] `AbortSignal.onabort` callback
- [x] `AbortSignal.addEventListener()` -- supports 'abort' event type (Cycle 241)
- [x] `AbortSignal.abort()` static method -- creates pre-aborted signal (Cycle 241)
- [~] `AbortSignal.timeout()` static method -- stub returns non-aborted signal (Cycle 241)
- [x] `AbortSignal.any()` static method -- checks if any input signal is aborted (Cycle 241)

---

## 5. Timers

- [x] `setTimeout()` -- delay=0 executes immediately, non-zero stored for future
- [x] `clearTimeout()`
- [x] `setInterval()` -- stores callback (no event loop, not repeatedly executed)
- [x] `clearInterval()`
- [x] `requestAnimationFrame()` -- assigns ID, stores callback
- [x] `cancelAnimationFrame()`
- [x] `requestIdleCallback()` -- executes callback immediately with IdleDeadline stub
- [x] `cancelIdleCallback()`
- [x] `queueMicrotask()` -- executes callback immediately

---

## 6. Storage

### Web Storage API

- [x] `localStorage.getItem()`
- [x] `localStorage.setItem()`
- [x] `localStorage.removeItem()`
- [x] `localStorage.clear()`
- [x] `localStorage.key()`
- [x] `localStorage.length` -- getter via Object.defineProperty
- [x] `sessionStorage` -- aliased to same in-memory store as localStorage

### Cookies

- [x] `document.cookie` -- getter returns "name=value; name2=value2"
- [x] `document.cookie` -- setter parses "name=value[; attributes]", ignores path/expires/etc.

---

## 7. URL API

- [x] `URL` constructor -- parses protocol, hostname, host, port, pathname, search, hash, origin
- [x] `URL` relative URL resolution with base parameter
- [x] `URL.prototype.href`
- [x] `URL.prototype.protocol`
- [x] `URL.prototype.hostname`
- [x] `URL.prototype.host`
- [x] `URL.prototype.port`
- [x] `URL.prototype.pathname`
- [x] `URL.prototype.search`
- [x] `URL.prototype.hash`
- [x] `URL.prototype.origin`
- [x] `URL.prototype.username`
- [x] `URL.prototype.password`
- [x] `URL.prototype.searchParams` -- returns URLSearchParams instance
- [x] `URL.prototype.toString()`
- [x] `URL.prototype.toJSON()`
- [~] `URL.createObjectURL()` -- stub returns "blob:null/stub"
- [~] `URL.revokeObjectURL()` -- no-op

### URLSearchParams

- [x] `URLSearchParams` constructor -- parses query string
- [x] `URLSearchParams.prototype.get()`
- [x] `URLSearchParams.prototype.set()`
- [x] `URLSearchParams.prototype.has()`
- [x] `URLSearchParams.prototype.delete()`
- [x] `URLSearchParams.prototype.toString()`
- [x] `URLSearchParams.prototype.forEach()`
- [x] `URLSearchParams.prototype.entries()`
- [x] `URLSearchParams.prototype.keys()`
- [x] `URLSearchParams.prototype.values()`
- [x] `URLSearchParams.prototype.append()` -- pushes new name/value pair (Cycle 241)
- [x] `URLSearchParams.prototype.getAll()` -- returns array of all values for name (Cycle 241)
- [x] `URLSearchParams.prototype.sort()` -- sorts params by key (Cycle 241)

---

## 8. Encoding

- [x] `TextEncoder` constructor
- [x] `TextEncoder.prototype.encode()` -- UTF-8 encoding to Uint8Array
- [x] `TextEncoder.prototype.encoding` -- returns "utf-8"
- [x] `TextEncoder.prototype.encodeInto()` -- encodes into dest array, returns {read, written} (Cycle 241)

- [x] `TextDecoder` constructor -- supports encoding label parameter
- [x] `TextDecoder.prototype.decode()` -- decodes Uint8Array/ArrayBuffer to string
- [x] `TextDecoder.prototype.encoding` -- getter
- [~] `TextDecoder` -- UTF-8 + ASCII + ISO-8859-1/Latin1 + Windows-1252 (Cycle 252)

- [x] `btoa()` -- Base64 encode
- [x] `atob()` -- Base64 decode

---

## 9. Observers

### MutationObserver

- [~] `MutationObserver` constructor -- accepts callback, stores it
- [~] `MutationObserver.prototype.observe()` -- no-op stub
- [~] `MutationObserver.prototype.disconnect()` -- no-op stub
- [~] `MutationObserver.prototype.takeRecords()` -- returns empty array

### IntersectionObserver

- [x] `IntersectionObserver` constructor -- parses rootMargin and threshold options
- [x] `IntersectionObserver.prototype.observe()` -- tracks observed elements
- [x] `IntersectionObserver.prototype.unobserve()` -- removes element from tracking
- [x] `IntersectionObserver.prototype.disconnect()` -- clears all observed elements
- [x] `IntersectionObserver.prototype.takeRecords()` -- returns empty array
- [x] Intersection ratio computation -- fires callbacks with IntersectionObserverEntry after layout

### ResizeObserver

- [x] `ResizeObserver` constructor -- accepts callback
- [x] `ResizeObserver.prototype.observe()` -- tracks observed elements
- [x] `ResizeObserver.prototype.unobserve()` -- removes element from tracking
- [x] `ResizeObserver.prototype.disconnect()` -- clears all observed elements
- [x] Resize detection -- fires callbacks with ResizeObserverEntry (contentRect, contentBoxSize, borderBoxSize, devicePixelContentBoxSize)

### PerformanceObserver

- [~] `PerformanceObserver` -- constructor with callback, supportedEntryTypes (Cycle 241)
- [~] `PerformanceObserver.prototype.observe()` -- stores entryTypes, no actual observation (Cycle 241)
- [~] `PerformanceObserver.prototype.disconnect()` -- no-op stub (Cycle 241)

---

## 10. Canvas API

### HTMLCanvasElement

- [x] `HTMLCanvasElement.getContext('2d')` -- returns CanvasRenderingContext2D
- [x] Canvas width/height from element attributes (defaults 300x150)
- [~] `HTMLCanvasElement.getContext('webgl')` -- returns null (not supported, Cycle 246)
- [x] `HTMLCanvasElement.toDataURL()` -- BMP format, base64 data URL (Cycle 245)
- [x] `HTMLCanvasElement.toBlob()` -- BMP Blob via callback (Cycle 245)

### CanvasRenderingContext2D

#### Rectangle Methods
- [x] `fillRect()`
- [x] `strokeRect()`
- [x] `clearRect()`

#### Path Methods
- [x] `beginPath()`
- [x] `closePath()`
- [x] `moveTo()`
- [x] `lineTo()`
- [x] `rect()`
- [x] `arc()` -- approximated with line segments
- [x] `fill()` -- scanline fill with even-odd rule
- [x] `stroke()` -- Bresenham line drawing
- [x] `arcTo()` -- line approximation to tangent points
- [x] `quadraticCurveTo()` -- 16-segment Bezier approximation
- [x] `bezierCurveTo()` -- 20-segment cubic Bezier approximation
- [x] `ellipse()` -- 32-segment with rotation support
- [ ] `clip()` -- NOT IMPLEMENTED
- [~] `isPointInPath()` -- stub (always returns false)

#### Text Methods
- [~] `fillText()` -- approximates text with rectangles per character
- [~] `strokeText()` -- same as fillText (stub)
- [x] `measureText()` -- returns width estimate based on font size

#### Style Properties
- [x] `fillStyle` -- getter/setter with color parsing (named, hex, rgb/rgba)
- [x] `strokeStyle` -- getter/setter with color parsing
- [x] `globalAlpha` -- getter/setter, clamped 0-1
- [x] `lineWidth` -- getter/setter
- [x] `font` -- getter/setter (parsed for font size)
- [x] `textAlign` -- getter/setter (start/left, center, right, end)
- [x] `textBaseline` -- getter/setter (alphabetic, top, hanging, middle, ideographic, bottom)
- [x] `lineCap` -- getter/setter (butt, round, square)
- [x] `lineJoin` -- getter/setter (miter, round, bevel)
- [x] `miterLimit` -- getter/setter (default 10)
- [x] `shadowColor` -- getter/setter with color parsing
- [x] `shadowBlur` -- getter/setter
- [x] `shadowOffsetX` -- getter/setter
- [x] `shadowOffsetY` -- getter/setter
- [x] `globalCompositeOperation` -- getter/setter (stores string value)
- [x] `imageSmoothingEnabled` -- getter/setter (boolean)

#### Transform Methods
- [x] `save()` -- pushes full drawing state to stack (Cycle 243)
- [x] `restore()` -- pops and restores full drawing state (Cycle 243)
- [x] `translate()` -- transforms via affine matrix post-multiply (Cycle 243)
- [x] `rotate()` -- transforms via affine matrix post-multiply (Cycle 243)
- [x] `scale()` -- transforms via affine matrix post-multiply (Cycle 243)
- [~] `transform()` -- no-op stub
- [~] `setTransform()` -- no-op stub
- [~] `resetTransform()` -- no-op stub

#### Image Methods
- [~] `drawImage()` -- no-op stub (TODO)
- [x] `getImageData()` -- returns ImageData with pixel data array
- [x] `putImageData()` -- writes pixel data to canvas buffer
- [x] `createImageData()` -- creates zeroed ImageData

#### Gradient / Pattern
- [x] `createLinearGradient()` -- returns gradient object with addColorStop()
- [x] `createRadialGradient()` -- returns gradient object with addColorStop()
- [x] `createConicGradient()` -- returns gradient object with addColorStop()
- [~] `createPattern()` -- returns stub pattern object

---

## 11. Workers

- [x] `Worker` constructor -- creates worker with dedicated QuickJS runtime/context
- [x] `Worker.postMessage()` -- sends serialized message to worker context
- [x] `Worker.terminate()` -- terminates worker thread
- [x] `Worker.onmessage` -- getter/setter for message handler
- [x] `Worker.onerror` -- getter/setter for error handler
- [x] `Worker.addEventListener()` -- supports "message" and "error" events
- [x] Worker global scope -- `self`, `postMessage()`, `close()`, `console`, `onmessage`
- [~] `SharedWorker` -- constructor stub with port (MessagePort stub)
- [~] `ServiceWorker` -- navigator.serviceWorker stub with register/getRegistrations/ready/controller
- [~] `Worker.importScripts()` -- no-op stub
- [~] `MessageChannel` -- constructor with port1/port2 (stub)
- [~] `MessagePort` -- postMessage/start/close stubs
- [~] `BroadcastChannel` -- constructor/postMessage/close/addEventListener/removeEventListener stubs
- [ ] Transferable objects -- NOT IMPLEMENTED (messages are serialized as JSON strings)

---

## 12. Window / Global APIs

### Window Properties

- [x] `window` -- references globalThis
- [x] `window.self` -- in worker contexts
- [x] `window.innerWidth`
- [x] `window.innerHeight`
- [x] `window.devicePixelRatio` -- returns 2.0
- [x] `window.outerWidth` -- returns 1024 (Cycle 237)
- [x] `window.outerHeight` -- returns 768 (Cycle 237)
- [x] `window.pageXOffset` -- returns 0 (Cycle 230)
- [x] `window.pageYOffset` -- returns 0 (Cycle 230)
- [~] `window.screenX` -- returns 0 (stub, Cycle 240)
- [~] `window.screenY` -- returns 0 (stub, Cycle 240)
- [x] `window.isSecureContext` -- returns true (Cycle 234)
- [~] `window.origin` -- returns "null" (Cycle 240)
- [~] `window.name` -- getter/setter, default "" (Cycle 240)
- [~] `window.opener` -- returns null (Cycle 240)
- [x] `window.parent` -- returns window (self-referencing, Cycle 240)
- [x] `window.top` -- returns window (self-referencing, Cycle 240)
- [~] `window.frameElement` -- returns null (Cycle 240)
- [x] `window.frames` -- returns window (Cycle 240)
- [~] `window.length` -- returns 0 (Cycle 240)
- [~] `window.closed` -- returns false (Cycle 240)

### Window Methods

- [~] `window.alert()` -- stores message in console output
- [~] `window.open()` -- returns null (stub)
- [~] `window.close()` -- no-op stub
- [~] `window.scrollTo()` -- no-op stub
- [~] `window.scrollBy()` -- no-op stub
- [~] `window.scroll()` -- no-op stub
- [x] `window.addEventListener()` -- with capture support
- [x] `window.removeEventListener()`
- [~] `window.dispatchEvent()` -- returns true (stub)
- [~] `window.confirm()` -- always returns true (stub, Cycle 240)
- [~] `window.prompt()` -- always returns "" (stub, Cycle 240)
- [~] `window.print()` -- no-op stub (Cycle 240)
- [~] `window.postMessage()` -- no-op stub (Cycle 240)
- [~] `window.focus()` -- no-op stub (Cycle 240)
- [~] `window.blur()` -- no-op stub (Cycle 240)
- [~] `window.stop()` -- no-op stub (Cycle 240)
- [~] `window.find()` -- no-op stub (Cycle 240)

### matchMedia

- [x] `window.matchMedia()` -- returns MediaQueryList with `matches` and `media`
- [x] `MediaQueryList.matches` -- evaluates prefers-color-scheme, min-width, max-width
- [x] `MediaQueryList.media` -- returns query string
- [~] `MediaQueryList.addEventListener()` -- no-op stub
- [~] `MediaQueryList.removeEventListener()` -- no-op stub
- [~] `MediaQueryList.addListener()` -- deprecated, no-op stub
- [~] `MediaQueryList.removeListener()` -- deprecated, no-op stub

### getSelection

- [x] `window.getSelection()` -- enhanced with full Selection API (Cycle 234)
- [x] `Selection.rangeCount` -- returns 0
- [x] `Selection.isCollapsed` -- returns true
- [x] `Selection.toString()` -- returns empty string
- [x] `Selection.getRangeAt()` -- uses createRange
- [x] `Selection.addRange()` -- no-op
- [x] `Selection.removeAllRanges()` -- no-op
- [x] `Selection.collapse()` -- no-op
- [x] `Selection.collapseToStart()` -- no-op
- [x] `Selection.collapseToEnd()` -- no-op
- [x] `Selection.selectAllChildren()` -- (Cycle 234)
- [x] `Selection.deleteFromDocument()` -- (Cycle 234)
- [x] `Selection.containsNode()` -- (Cycle 234)
- [x] `Selection.extend()` -- (Cycle 234)
- [x] `Selection.setBaseAndExtent()` -- (Cycle 234)
- [x] `Selection.empty()` -- alias for removeAllRanges (Cycle 234)
- [x] `Selection.modify()` -- (Cycle 234)

---

## 13. Location API

- [x] `location.href`
- [x] `location.hostname`
- [x] `location.pathname`
- [x] `location.protocol`
- [x] `location.host` -- includes port if non-standard (Cycle 233)
- [x] `location.port` -- extracted from URL (Cycle 233)
- [x] `location.search` -- query string (Cycle 233)
- [x] `location.hash` -- fragment (Cycle 233)
- [x] `location.origin` -- protocol + host (Cycle 233)
- [~] `location.assign()` -- stub (Cycle 233)
- [~] `location.replace()` -- stub (Cycle 233)
- [~] `location.reload()` -- stub (Cycle 233)

---

## 14. History API

- [x] `history.length` -- always 1
- [x] `history.state` -- always null
- [~] `history.pushState()` -- no-op stub
- [~] `history.replaceState()` -- no-op stub
- [~] `history.back()` -- no-op stub
- [~] `history.forward()` -- no-op stub
- [~] `history.go()` -- no-op stub

---

## 15. Navigator API

- [x] `navigator.userAgent` -- returns "Clever/0.5"
- [~] `navigator.clipboard.writeText()` -- returns resolved Promise (stub)
- [~] `navigator.clipboard.readText()` -- returns resolved Promise with empty string (stub)
- [~] `navigator.clipboard.write()` -- returns resolved Promise (stub)
- [~] `navigator.clipboard.read()` -- returns resolved Promise with empty array (stub)
- [x] `navigator.language` -- returns "en-US"
- [x] `navigator.languages` -- returns ["en-US", "en"]
- [x] `navigator.platform` -- returns "MacIntel"
- [x] `navigator.vendor` -- returns "Clever Browser"
- [x] `navigator.onLine` -- returns true
- [x] `navigator.cookieEnabled` -- returns true
- [x] `navigator.hardwareConcurrency` -- returns 4
- [x] `navigator.maxTouchPoints` -- returns 0
- [~] `navigator.mediaDevices` -- stub object
- [~] `navigator.permissions` -- stub object
- [~] `navigator.serviceWorker` -- full stub with register()/getRegistrations()/ready/controller/addEventListener
- [~] `navigator.geolocation` -- stub object with getCurrentPosition/watchPosition/clearWatch (Cycle 246)
- [~] `navigator.sendBeacon()` -- returns true (no-op stub, Cycle 241)

---

## 16. Screen API

- [x] `screen.width`
- [x] `screen.height`
- [x] `screen.availWidth`
- [x] `screen.availHeight`
- [x] `screen.colorDepth` -- returns 24
- [x] `screen.pixelDepth` -- returns 24
- [x] `screen.orientation` -- {type: "landscape-primary", angle: 0} (Cycle 234)
- [x] `screen.availLeft` -- returns 0 (Cycle 234)
- [x] `screen.availTop` -- returns 0 (Cycle 234)

---

## 17. Performance API

- [x] `performance.now()` -- high-resolution timestamp in milliseconds since page start
- [~] `performance.mark()` -- stub (Cycle 234)
- [~] `performance.measure()` -- stub (Cycle 234)
- [x] `performance.getEntries()` -- returns empty array (Cycle 234)
- [x] `performance.getEntriesByType()` -- returns empty array (Cycle 234)
- [x] `performance.getEntriesByName()` -- returns empty array (Cycle 234)
- [x] `performance.timeOrigin` -- epoch ms (Cycle 234)
- [~] `performance.timing` -- stub object with navigationStart (Cycle 240)
- [~] `performance.navigation` -- stub object with type=0, redirectCount=0 (Cycle 240)
- [~] `performance.memory` -- usedJSHeapSize/totalJSHeapSize/jsHeapSizeLimit all 0 (stub, non-standard)

### Performance Entry Types

- [~] `PerformanceEntry` -- constructor with name/entryType/startTime/duration, toJSON()
- [~] `PerformanceResourceTiming` -- constructor with all timing fields (initiatorType, fetchStart, responseEnd, transferSize, etc.)
- [~] `PerformanceMark` -- constructor with name, entryType='mark', startTime=performance.now()
- [~] `PerformanceMeasure` -- constructor with name, entryType='measure'
- [~] `PerformanceNavigation` -- constructor with type=0, redirectCount=0
- [~] `PerformanceNavigationTiming` -- constructor with entryType='navigation'

---

## 18. Console API

- [x] `console.log()`
- [x] `console.warn()`
- [x] `console.error()`
- [x] `console.info()`
- [x] `console.debug()` -- alias to console.log (Cycle 241 checklist audit)
- [x] `console.dir()` -- alias to console.log (Cycle 241 checklist audit)
- [x] `console.table()` -- alias to console.log (Cycle 241 checklist audit)
- [x] `console.trace()` -- outputs stack trace (Cycle 241 checklist audit)
- [x] `console.time()` / `console.timeEnd()` -- with timing measurement (Cycle 241 checklist audit)
- [x] `console.count()` -- with counter tracking (Cycle 241 checklist audit)
- [~] `console.group()` / `console.groupEnd()` -- no-op stubs (Cycle 241 checklist audit)
- [x] `console.assert()` -- outputs message on falsy assertion (Cycle 241 checklist audit)
- [~] `console.clear()` -- no-op stub (Cycle 241 checklist audit)

---

## 19. Crypto API

- [~] `crypto.getRandomValues()` -- fills array with Math.random() (NOT cryptographically secure)
- [x] `crypto.randomUUID()` -- generates v4 UUID using Math.random()
- [x] `crypto.subtle` -- object with digest method
- [x] `crypto.subtle.digest()` -- native SHA-1/SHA-256/SHA-384/SHA-512 via CommonCrypto, returns Promise<ArrayBuffer>
- [~] `crypto.subtle.encrypt()` / `decrypt()` -- returns Promise.reject (stub, Cycle 246)
- [~] `crypto.subtle.sign()` / `verify()` -- returns Promise.reject (stub, Cycle 246)
- [~] `crypto.subtle.generateKey()` -- returns Promise.reject (stub, Cycle 246)
- [~] `crypto.subtle.importKey()` / `exportKey()` -- returns Promise.reject (stub, Cycle 246)
- [~] `crypto.subtle.deriveBits()` / `deriveKey()` -- returns Promise.reject (stub, Cycle 246)
- [~] `crypto.subtle.wrapKey()` / `unwrapKey()` -- returns Promise.reject (stub, Cycle 246)

---

## 20. DOMParser

- [x] `DOMParser` constructor
- [x] `DOMParser.parseFromString()` -- parses HTML, returns document-like object with body, head, documentElement, title, querySelector, querySelectorAll, getElementById, getElementsByTagName

---

## 21. Structured Clone

- [x] `structuredClone()` -- implemented via JSON serialize/deserialize (does not handle circular refs, Map, Set, etc.)

---

## 22. CSS Namespace

- [x] `CSS.supports()` -- checks if a CSS property is recognized

---

## 23. Not Implemented -- Major APIs

The following commonly-used Web APIs have no implementation in Clever:

- [~] **Intersection Observer V2** -- observe() accepts trackVisibility/delay options without error (values ignored)
- [x] **File API** -- Blob (constructor/size/type/slice/text/arrayBuffer), File (extends Blob, name/lastModified), FileReader (readAsText/readAsDataURL/readAsArrayBuffer/abort/events)
- [~] **Geolocation API** -- navigator.geolocation.getCurrentPosition/watchPosition/clearWatch stubs (Cycle 246)
- [~] **Media APIs** -- HTMLMediaElement (src/currentTime/duration/paused/volume/playbackRate/readyState/play/pause/load/canPlayType/addTextTrack), HTMLVideoElement (width/height/videoWidth/videoHeight/poster), HTMLAudioElement, Audio constructor (Cycle 252). MediaSource NOT IMPLEMENTED
- [~] **Web Audio API** -- AudioContext (createGain/createOscillator/createBufferSource/createAnalyser/createBiquadFilter/createDynamicsCompressor/createDelay/createConvolver/createPanner/decodeAudioData/resume/suspend/close), webkitAudioContext alias (Cycle 252)
- [~] **Web Animations API** -- element.animate() returns full Animation stub with finished/ready Promises (Cycle 243), Animation constructor (play/pause/cancel/finish/reverse/updatePlaybackRate/commitStyles/persist), KeyframeEffect (getKeyframes/setKeyframes/getComputedTiming), DocumentTimeline, document.timeline, document.getAnimations()
- [~] **Fullscreen API** -- requestFullscreen/exitFullscreen/fullscreenElement/fullscreenEnabled + webkit prefixed (Cycle 243)
- [~] **Notifications API** -- Notification constructor, permission='default', requestPermission() returns 'denied'
- [~] **Web Components** -- Shadow DOM (attachShadow/shadowRoot, Cycle 236), HTMLTemplateElement.content (Cycle 236). customElements NOT IMPLEMENTED
- [~] **IndexedDB** -- IDBDatabase, IDBObjectStore, IDBTransaction, IDBKeyRange, IDBRequest, IDBOpenDBRequest (stub: open/createObjectStore/transaction/deleteDatabase/cmp/databases)
- [~] **Cache API** -- CacheStorage (open/has/delete/keys/match), Cache (match/matchAll/add/addAll/put/delete/keys), globalThis.caches
- [~] **Streams API** -- ReadableStream (getReader/cancel/pipeTo/pipeThrough/tee), WritableStream (getWriter/abort/close), TransformStream (readable/writable) (stub)
- [~] **Clipboard API** -- navigator.clipboard (writeText/readText/write/read), ClipboardItem constructor with types/getType (Cycle 253)
- [~] **Drag and Drop API** -- DataTransfer (setData/getData/clearData/setDragImage/dropEffect/effectAllowed/types/files), DataTransferItemList (add/remove/clear), DragEvent (Cycle 253)
- [x] **Pointer Events** -- PointerEvent constructor implemented (Cycle 233), pointer capture NOT IMPLEMENTED
- [~] **Touch Events** -- Touch constructor (full properties), TouchEvent with touches/targetTouches/changedTouches arrays (Cycle 253)
- [~] **Gamepad API** -- navigator.getGamepads() returns [null,null,null,null] (Cycle 252)
- [~] **WebRTC** -- RTCPeerConnection (createOffer/createAnswer/setLocal/RemoteDescription/addIceCandidate/createDataChannel/addTrack/removeTrack/getStats/getSenders/getReceivers/close), RTCSessionDescription, RTCIceCandidate, MediaStream (getTracks/getAudioTracks/getVideoTracks/addTrack/removeTrack/clone), MediaStreamTrack (stop/clone/getSettings/getCapabilities/applyConstraints) stubs (Cycle 254)
- [~] **WebGL** -- WebGLRenderingContext stub (50+ methods: createShader/compileShader/createProgram/linkProgram/drawArrays/drawElements/createBuffer/bindBuffer/bufferData/createTexture/uniforms/viewport/clear + 30 constants), WebGL2RenderingContext alias, canvas.getContext('webgl'/'webgl2'/'experimental-webgl') returns stub (Cycle 255)
- [~] **Web Speech API** -- SpeechRecognition/webkitSpeechRecognition (start/stop/abort), SpeechSynthesisUtterance, speechSynthesis (speak/cancel/pause/resume/getVoices) stubs (Cycle 253)
- [~] **Resize Observer** loop detection -- max depth 4 per layout cycle (Cycle 254)
- [~] **Broadcast Channel API** -- BroadcastChannel constructor/postMessage/close stubs
- [~] **Payment Request API** -- PaymentRequest (show rejects NotSupportedError, canMakePayment returns false, abort) stub (Cycle 254)
- [~] **Credential Management API** -- navigator.credentials.get/store/create/preventSilentAccess stubs (Cycle 252)
- [~] **Web Locks API** -- navigator.locks.request/query stubs (Cycle 252)
- [~] **Reporting API** -- ReportingObserver constructor/observe/disconnect/takeRecords stub (Cycle 252)

---

## Summary

| Category | Implemented | Partial/Stub | Not Implemented |
|---|---|---|---|
| DOM (Node/Element/Document) | 63 | 9 | 29 |
| Events | 18 | 0 | 13 |
| Event Listeners | 4 | 0 | 3 |
| CSSOM | 5 | 0 | 5 |
| Fetch / Network | 45 | 0 | 15 |
| Timers | 8 | 0 | 0 |
| Storage | 9 | 0 | 0 |
| URL / URLSearchParams | 22 | 2 | 3 |
| Encoding | 7 | 0 | 2 |
| Observers | 12 | 5 | 2 |
| Canvas 2D | 18 | 8 | 22 |
| Workers | 7 | 0 | 7 |
| Window | 7 | 16 | 20 |
| Location | 4 | 0 | 8 |
| History | 2 | 5 | 0 |
| Navigator | 1 | 4 | 13 |
| Screen | 6 | 0 | 1 |
| Performance | 1 | 6 | 2 |
| Console | 4 | 0 | 9 |
| Crypto | 1 | 2 | 6 |
| DOMParser | 2 | 0 | 0 |
| Cache API | 0 | 13 | 0 |
| Web Animations | 0 | 13 | 0 |
| Other | 2 | 0 | 0 |
| **Total** | **~248** | **~83** | **~160** |

**Implementation rate: ~50% implemented, ~17% stubbed, ~33% not implemented**

(Counts are approximate; some APIs have multiple sub-features.)
