<div class="app-container">
  <header class="header">
    <div class="header-group">
      <!-- Title and Copyright Row -->
      <div class="text-row">
        <h1>NTN Sustainability Control</h1>
        <span class="copyright">(&copy; {new Date().getFullYear()} Sonu Sonkar)</span>
      </div>
      
      <!-- Horizontal Divider Underneath -->
      <div class="h-divider"></div>
    </div>
  </header>

  <main class="content">
    <slot />
  </main>
</div>

<style>
  :global(body, html) {
    margin: 0;
    padding: 0;
    height: 100vh;
    overflow: hidden;
    background: #ffffff;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  }

  .app-container {
    display: flex;
    flex-direction: column;
    height: 100vh;
    height: 100dvh;
  }

  .header {
    flex: 0 0 auto;
    display: flex;
    justify-content: center;
    padding: 15px 0 0 0;
  }

  .header-group {
    display: flex;
    flex-direction: column;
    align-items: center;
    width: fit-content;
  }

  .text-row {
    display: flex;
    align-items: baseline;
    gap: 8px; /* Space between title, and copyright */
  }

  .header h1 {
    margin: 0;
    font-size: 1rem;
    color: #555;
    white-space: nowrap;
  }

  .copyright {
    margin: 0;
    font-size: 0.75rem;
    color: #777;
    white-space: nowrap;
  }

  /* Horizontal Divider */
  .h-divider {
    height: 1px;
    width: 120%; /* Extends slightly beyond the whole row */
    background: linear-gradient(to right, transparent, #1a1a1a, transparent);
  }

  .content {
    flex: 1;
    overflow: auto;
    width: 100%;
    height: 100%;
    min-height: 0;
  }
</style>
