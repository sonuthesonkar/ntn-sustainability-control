<script lang="ts">
  import { page } from '$app/stores';
</script>
<!-- Simple and minimal error page -->
<div class="error-container">
  <h1>Status: {$page.status}</h1>
  <div class="h-divider"></div>
  
  {#if $page.status === 404}
    <p>The page or resource you are looking for doesn't exist.</p>
  {:else if $page.status === 500}
    <p>Server is down at the moment.</p>
  {:else}
    <p>{$page.error?.message || 'An unexpected error occurred.'}</p>
  {/if}

  <span>Please try after some time.</span>
</div>

<style>
  .error-container {
    text-align: center;
    padding: 2rem;
    font-family: sans-serif;
  }
  
  h1 { color: #777; }
  
  .h-divider {
		height: 1px;
		width: 100%;
		justify-self: center;
		background: linear-gradient(to right, transparent, #1a1a1a, transparent);
	}
</style>
