<script lang="ts">
  import { onMount } from 'svelte';
  import * as echarts from 'echarts';

  export let data;

  let chart: any;
  let mermaid: any;

  let mode = data.mode;
  let history = data.history;
  const latest = history.at(-1);
  // ---------------- STATE ----------------
  let kpis = {
    Congestion: latest.congestion,
    PRB_Util: latest.prb_util,
    Traffic_Load: latest.traffic_load,
    RAN_Energy: latest.ran_energy,
    Carbon_Intensity: latest.carbon_intensity,
    ISAC_Quality: latest.isac_quality,
    Mobility_Rate: latest.mobility_rate
  };

  let currentState = latest.ntn_state;
  const ntn_states = [" ", "! NTNStart !", "! NTNCross !", "! NTNFull !"];
  
  let err_msg = '';

  // ---------------- BACKEND UPDATE ----------------
  /**
   * Update db kpi, infer crisis score, update history, refresh chart and mermaid layout
   * @param updated_kpi
   */
  async function pushUpdate(updated_kpi) {
    err_msg = ''; // Reset error.
    try {
      const u_res = await fetch('/api/update', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ history, kpis, updated_kpi })
      });

      if ([401, 403].includes(u_res.status)) {
        mode = 'observer';
        return;
      } else if (u_res.status != 200) {
        const body = await u_res.json();
        throw new Error(body.message || "Server error!");
      };

      // fetch latest history
      const res = await fetch('/api/history');
      history = await res.json();

      currentState = history.at(-1).ntn_state;
      updateChart();
      renderMermaid();
    } catch (err) {
      err_msg = "Update failed - server error!";
      console.error(err_msg, err);
    }
  }

  // ---------------- CHART ----------------
  /**
   * Update chart
   */
  function updateChart() {
    chart.setOption({
      xAxis: [
        { gridIndex: 0, data: history.map(h => h.ts) },
        { gridIndex: 1, data: history.map(h => h.ts) }
      ],
      series: [
        { data: history.map(h => h.crisis_score) }, // True Crisis
        {}, // Placeholder for the markLine series (no data update needed)
        { data: history.map(h => h.ntn_state) }    // NTN State
      ]
    });
  }

  /**
   * Set color (for mermaid blocks), based on ntn state
   * @param s
   */
  function ntnColor(s: number) {
    return ['#eeeeee', '#fff2cc', '#cfe2f3', '#d9ead3'][s];
  }

  // ---------------- MERMAID ----------------
  /**
   * Update mermaid block diagram
   */
  async function renderMermaid() {
    if (!mermaid) return;

    let mmd = "";
    try {
      // Fetch the file from the static folder
      const response = await fetch('/system-view-6g.mmd');
      mmd = await response.text();
    } catch (error) {
      console.error("Failed to load or render mermaid file:", error);
    }
    
    const { svg } = await mermaid.render('ntn', mmd);
    const el = document.getElementById('mermaid');
    if (el) el.innerHTML = svg;
  }

  /**
   * Release db lock
  */
  async function releaseLock(){
    const res = await fetch('/api/release', { method: 'POST' });
    if (res.ok) {
      window.location.reload();
    }
  }

  // ---------------- INIT ----------------
  /**
   * Initializations
  */
  onMount(async () => {
    chart = echarts.init(document.getElementById('chart')!);
    const option = {
      // Sync tooltips across both plots
      tooltip: { trigger: 'axis', axisPointer: { animation: false } },
      
      // Create two grid areas (Top: 65% height, Bottom: 20% height)
      grid: [
        { left: '10%', width: '85%', top: '5%', height: '60%' }, // ax1 (Crisis)
        { left: '10%', width: '85%', top: '75%', height: '20%' } // ax2 (NTN States)
      ],

      xAxis: [
        { 
          type: 'category', 
          data: history.map(h => h.ts), 
          gridIndex: 0, 
          show: false // Hide x-axis for top plot to share with bottom
        },
        { 
          type: 'category', 
          data: history.map(h => h.ts), 
          gridIndex: 1, 
          name: 'Time', 
          nameLocation: 'middle', 
          nameGap: 25
        }
      ],

      yAxis: [
        // ax1 Y-Axis
        { 
          type: 'value', 
          name: 'Sustainability Crisis Score', 
          gridIndex: 0, 
          min: 0, 
          max: 1,
          nameLocation: 'middle',
          nameRotate: 90,
          nameGap: 50 
        },
        // ax2 Y-Axis (State Labels)
        { 
          type: 'value', 
          gridIndex: 1, 
          min: 0, 
          max: 3, 
          interval: 1,
          axisLabel: {
            formatter: (value: number) => {
              return ["No NTN", "NTN Start", "NTN Cross", "Full Fallback"][value];
            }
          }
        }
      ],

      series: [
        // ax1: True Crisis (Solid line)
        {
          name: 'Sustainability Crisis',
          type: 'line',
          xAxisIndex: 0,
          yAxisIndex: 0,
          data: history.map(h => h.crisis_score),
          symbol: 'none',
          lineStyle: { width: 2 }
        },
        // ax1: Thresholds (Horizontal lines using markLine)
        {
          type: 'line',
          xAxisIndex: 0,
          yAxisIndex: 0,
          markLine: {
            symbol: 'none',
            label: { position: 'insideEndTop' },
            data: [
              { yAxis: 0.6, label: { formatter: 'NTN Start' }, lineStyle: { type: [10, 5, 2, 5] } }, //dashdotted
              { yAxis: 0.8, label: { formatter: 'NTN Cross' }, lineStyle: { type: [2, 4] } } //dotted
            ]
          }
        },
        // ax2: NTN States (Step line)
        {
          name: 'NTN State',
          type: 'line',
          step: 'post', // Matches matplotlib' where="post"
          xAxisIndex: 1,
          yAxisIndex: 1,
          data: history.map(h => h.ntn_state),
          areaStyle: { opacity: 0.1 } // Subtle fill like a state chart
        }
      ]
    };

    chart.setOption(option); //Set chart

    mermaid = (await import('mermaid')).default;
    mermaid.initialize({ startOnLoad: false, theme: 'neutral', htmlLabels: true });
    renderMermaid();
  });
  $: activeColorN = ntnColor(currentState); // Reactive color for ntn blocks
  $: activeColorR = ntnColor(3 - currentState); // Reactive color for terrestrial blocks
</script>

<style>
	:global(.activeN rect), :global(.activeN .node-arm) {
    fill: var(--active-colorN) !important;
    transition: fill 0.3s ease; /* Smooth color transition! */
  }

  :global(.activeR rect), :global(.activeR .node-arm) {
    fill: var(--active-colorR) !important;
    transition: fill 0.3s ease; /* Smooth color transition! */
  }

  :global(.dynamic-status, .filler) {
    display: inline-block;
    min-width: 12ch; /* Pre-allocate width so it doesn't "jerk" */
    background-color: var(--active-colorN) !important;
    transition: fill 0.3s ease; /* Smooth color transition! */
    text-align: center;
  }

  :global(.dynamic-status::after) {
    content: var(--ntn-text);
  }

	.dashboard {
		display: grid;
		/* Col 1: Left content | Col 2: Divider | Col 3: Right content */
		grid-template-columns: 1.5fr auto 1fr; 
		height: 100vh; /*not 100% */
		padding: 5px 20px;
		box-sizing: border-box;
	}

	.left-col {
		display: grid;
		/* Row 1: Sliders | Row 2: Divider | Row 3: Graph */
		grid-template-rows: 1fr auto 1.5fr;
		height: 100%;
    min-height: 0;
	}

	section, .right-col {
		display: flex;
		flex-direction: column;
		padding: 10px;
    min-height: 0;
	}

	.label {
    display:grid;
    grid-template-columns: 1fr 1fr 1fr;
    align-items: center;
    width: 100%;
    margin: 0 0 15px 0;
		font-size: 0.75rem;
		font-weight: 600;
		color: #777;
		letter-spacing: 0.05em;
    text-align: center;
  }

  .label-left {
    display: flex;
    gap: 4px;
    align-items: flex-start;
    justify-content: left;
  }

  .label-right {
    display: flex;
    gap: 4px;
    align-items: flex-end;
    justify-content: right;
  }

  .label h3 {
    grid-column: 2;
  }

  .mode {
    padding: 0px 4px;
    box-sizing: border-box;
    background-color: #5b5b5b;
    color: #f5f5f5;
  }
  
  /* Small Button */ 
  .label button {
    padding: 0px 4px;
    height: auto;
    border: 1px solid #777;
    border-radius: 4px;
    background-color: #f0f0f0;
    cursor: pointer;
    transition: background 0.2s ease;
  }
  .label button:hover {
    background-color: #e0e0e0;
    border-color: #666;
  }

	/* Gradient Dividers */
	.v-divider {
		width: 1px;
		height: 90%; /* Don't touch the very edges */
		align-self: center;
		background: linear-gradient(to bottom, transparent, #1a1a1a, transparent);
	}

	.h-divider {
		height: 1px;
		width: 90%;
		justify-self: center;
		background: linear-gradient(to right, transparent, #1a1a1a, transparent);
	}

	/* Sliders Customization */
	.slider-grid {
		display: flex;
		justify-content: space-around;
		align-items: center;
		flex: 1;
	}

	.slider-container {
		display: flex;
		flex-direction: column;
		align-items: center;
		height: 80%;
		gap: 10px;
	}

	input[type="range"] {
		writing-mode: vertical-lr;
    direction: rtl;
		cursor: pointer;
		accent-color: #3b82f6; /* Modern Blue */
	}

	.caption {
		font-size: 0.7rem;
		font-weight: 500;
		color: #64748b;
	}

  .content-area {
		flex: 1;
		display: flex;
		justify-content: center;
		align-items: center;
	}
</style>

<div class="dashboard">
  <!-- SLIDERS -->
  <div class="left-col">
    <section class="sliders-section">
      <div class="label">
        <div class="label-left">
          {#if mode === 'observer'}
            <span class="mode">Observer</span>
            <span>(view-only)</span>
          {:else}
            <span class="mode">Controller</span>
            <button on:click={releaseLock}>
              Release Lock
            </button>
          {/if}
        </div>
        <h3>KPIs</h3>
        <div class="label-right">
          {#if err_msg}
            <span>&#x274C; {err_msg}</span>
          {/if}
        </div>
      </div>
      <div class="slider-grid">
        {#each Object.keys(kpis) as k}
          <div class="slider-container">
            <span class="caption">{k}</span>
            <input
              disabled={mode === 'observer'}
              type="range"
              min="0"
              max="1"
              step="0.01"
              bind:value={kpis[k]}
              on:change={() => pushUpdate(k)}
            />
            <span class="caption">{kpis[k].toFixed(2)}</span>
          </div>
        {/each}
      </div>
    </section>
    <!-- HORIZONTAL DIVIDER (Gradient Line) -->
    <div class="h-divider"></div>
    
    <!-- HISTORY -->
    <section class="graph-section">
      <div class="label">
        <h3>NTN Fallback History</h3>
      </div>
      <div class="content-area">  
        <div id="chart" style="width:100%; height:100%;"></div>
      </div>
    </section>
  </div>

  <!-- VERTICAL DIVIDER (Gradient Line) -->
	<div class="v-divider"></div>
  
  <!-- MERMAID -->
  <div class="right-col">
    <div class="label">
      <h3>System View</h3>
    </div>
    <div class="content-area" style="--active-colorN: {activeColorN}; --active-colorR: {activeColorR}; 
                              --ntn-text: '{ntn_states[currentState]}'">
      <div id="mermaid"></div>
    </div>
  </div>
</div>
