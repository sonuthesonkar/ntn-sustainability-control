<div class="app-container">
  <!-- Top Center Title Overlay -->
  <header class="overlay-header">
    <h1>NTN Sustainability Control</h1>
    <div class="h-divider"></div>
  </header>

  <main>
    <slot /> <!-- or {@render children()} in Svelte 5 -->
  </main>

  <!-- Bottom Right Footer Overlay -->
  <footer class="overlay-footer">
    &copy; {new Date().getFullYear()} Sonu Sonkar.
  </footer>
</div>

<style>
  /* Light Theme Foundation */
	:global(body, html) {
		margin: 0;
		padding: 0;
		height: 100vh;
		width: 100vw;
		background: #ffffff;
		color: #334155;
		font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
	}
  
  /* Container stays exactly 100vh and doesn't scroll */
  .app-container {
    position: relative;
    height: 100vh;
    height: 100dvh; /* Mobile support */
    width: 100%;
    overflow: auto; /* Scrolling enabled */
  }

  /* Top Center Title */
  .overlay-header {
    position: absolute;
    top: 0px;
    left: 50%;
    transform: translateX(-50%); /* Perfectly centers the element */
    z-index: 100;
    pointer-events: none; /* Clicks pass through to dashboard */
  }

  .overlay-header h1 {
    margin: 0;
    font-size: 1rem;
    color: #777;
    white-space: nowrap;
  }

  .h-divider {
		height: 1px;
		width: 125%;
		justify-self: center;
		background: linear-gradient(to right, transparent, #1a1a1a, transparent);
	}

  /* Bottom Right Copyright */
  .overlay-footer {
    position: absolute;
    bottom: 8px;
    right: 12px;
    z-index: 100;
    font-size: 0.75rem;
    color: #777;
    pointer-events: none;
  }
</style>
