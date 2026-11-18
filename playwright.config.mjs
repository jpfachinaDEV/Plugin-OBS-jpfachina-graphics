import {defineConfig} from '@playwright/test';
export default defineConfig({
  testDir:'./tests',testMatch:'**/*.spec.mjs',fullyParallel:false,workers:1,timeout:30000,
  reporter:[['list'],['json',{outputFile:'test-results/browser-results.json'}]],
  use:{browserName:'chromium',channel:'chrome',headless:true,viewport:{width:1440,height:1000},screenshot:'only-on-failure'},
  webServer:{command:'node tests/browser-server.mjs',url:'http://127.0.0.1:28749/panel',reuseExistingServer:false,timeout:10000}
});
