// Integration against the real compiled Qt service. All data and processes are isolated.
import {spawn} from 'node:child_process';
import {mkdir,writeFile,readFile,copyFile} from 'node:fs/promises';
import path from 'node:path';
import assert from 'node:assert/strict';
import {chromium} from '@playwright/test';
const root=path.resolve('.deps/native-e2e-data');await mkdir(root+'/media',{recursive:true});
await copyFile('assets/branding/jpfachina-sync-original.png',root+'/media/m1.png');
const key='e'.repeat(64),key2='f'.repeat(64);
const layout={x:20,y:30,width:600,height:237,z:1,transition:'none',duration:0,loop:true};
const seed={schema:1,port:28746,active:'p1',projects:[{id:'p1',key,name:'Teste nativo isolado',width:1920,height:1080,categories:['Logos','Outros'],media:[{id:'m1',name:'Logo SYNC original',category:'Logos',mime:'image/png',file:'media/m1.png',bytes:(await readFile(root+'/media/m1.png')).length,origin:'isolated-test',draft:layout,live:null,generation:0}]},{id:'p2',key:key2,name:'Outro evento',width:1920,height:1080,categories:['Outros'],media:[]}]};
await writeFile(root+'/library.json',JSON.stringify(seed));
const child=spawn(path.resolve('build/jp-graphics-dev.exe'),['--data',root,'--no-browser'],{windowsHide:true,env:{...process.env,QT_QPA_PLATFORM:'minimal',QT_FORCE_STDERR_LOGGING:'1'},stdio:['ignore','pipe','pipe']});
let browser;
try{
  const panelUrl=await new Promise((resolve,reject)=>{let output='';const timeout=setTimeout(()=>reject(Error('Native startup timeout')),10000);child.on('exit',code=>{clearTimeout(timeout);reject(Error('Native process exited '+code));});child.stderr.on('data',data=>process.stderr.write(data));child.stdout.on('data',data=>{output+=data;const match=output.match(/http:\/\/127\.0\.0\.1:\d+\/panel#[a-f0-9]+/);if(match){clearTimeout(timeout);resolve(match[0]);}});});
  const url=new URL(panelUrl),token=url.hash.slice(1),base=url.origin;
  const headers={Authorization:'Bearer '+token,'Content-Type':'application/json'};
  const command=async(action,data={})=>{const r=await fetch(base+'/api/command',{method:'POST',headers,body:JSON.stringify({action,projectId:'p1',...data})});const result=await r.json();assert.equal(r.status,200,JSON.stringify(result));return result;};
  assert.equal((await fetch(base+'/api/state')).status,401);
  assert.equal((await fetch(base+'/api/state',{headers:{...headers,Origin:'https://untrusted.invalid'}})).status,403);
  await command('media.on',{mediaId:'m1'});
  const denied=await fetch(base+'/api/command',{method:'POST',headers,body:JSON.stringify({action:'live.clear',projectId:'p1'})});assert.equal(denied.status,400);
  assert.equal((await (await fetch(base+'/state/'+key)).json()).items.length,1);
  assert.equal((await (await fetch(base+'/state/'+key2)).json()).items.length,0);
  browser=await chromium.launch({channel:'chrome',headless:true});const page=await browser.newPage({viewport:{width:1440,height:1000}});const notices=[];
  await page.exposeFunction('testNotice',message=>notices.push(message));
  await page.addInitScript(()=>addEventListener('DOMContentLoaded',()=>new MutationObserver(()=>window.testNotice(document.querySelector('#toast').textContent)).observe(document.querySelector('#toast'),{childList:true,subtree:true})));
  await page.addInitScript(()=>localStorage.setItem('jp-welcome','1'));await page.goto(panelUrl);
  await page.waitForFunction(()=>document.querySelector('#connection').textContent.includes('conectado'));
  await page.getByRole('button',{name:'PRÉ-VISUALIZAR',exact:true}).click();
  const overlay=await browser.newPage();await overlay.goto(base+'/overlay/'+key);await overlay.locator('.layer').waitFor();
  assert.equal(await overlay.locator('.layer').evaluate(x=>x.style.left),'20px');
  await page.locator('#x').fill('420');await page.locator('#x').dispatchEvent('input');await page.locator('#save').click();
  await page.waitForFunction(()=>document.querySelector('#unsaved').textContent.startsWith('Preparação salva'));
  assert.equal(await overlay.locator('.layer').evaluate(x=>x.style.left),'20px');
  await page.locator('#take').click();await overlay.waitForFunction(()=>document.querySelector('.layer').style.left==='420px');
  await page.screenshot({path:'artifacts/native-panel.png',fullPage:true});await overlay.screenshot({path:'artifacts/native-overlay.png',omitBackground:true});
  await page.locator('#projects').selectOption('p2');await page.waitForFunction(()=>document.querySelector('#media-count').textContent==='0 MÍDIAS');
  await page.locator('#clear').click();await page.locator('#confirm-dialog button[value="ok"]').click();
  assert.equal((await (await fetch(base+'/state/'+key)).json()).items.length,1);
  // Public HTTPS download exercises DNS validation, pinned IP, TLS and persistent local copy.
  await command('media.download',{projectId:'p2',url:'https://raw.githubusercontent.com/obsproject/obs-studio/32.1.0/frontend/cmake/linux/icons/obs-logo-128.png',category:'Outros'});
  const deadline=Date.now()+40000;let downloaded;
  while(Date.now()<deadline){const state=await(await fetch(base+'/api/state',{headers})).json();downloaded=state.projects.find(x=>x.id==='p2').media[0];if(downloaded)break;await new Promise(r=>setTimeout(r,250));}
  assert.ok(downloaded,'HTTPS download did not populate isolated library; notices: '+notices.join(' | '));assert.equal(downloaded.mime,'image/png');
  const cached=await fetch(base+'/media/'+key2+'/'+downloaded.id);assert.equal(cached.status,200);assert.ok((await cached.arrayBuffer()).byteLength>100);
  await command('media.off',{mediaId:'m1'});await overlay.waitForFunction(()=>document.querySelectorAll('.layer').length===0);
  await command('live.clear',{projectId:'p2',confirm:true});
  const saved=JSON.parse(await readFile(root+'/library.json','utf8'));assert.equal(saved.projects[0].media[0].draft.x,420);assert.equal(saved.projects[0].media[0].live,null);
  await writeFile('artifacts/native-e2e-results.json',JSON.stringify({passed:true,checks:['real Qt authentication and Origin enforcement','confirmation required for clear','independent project overlays','private preview and draft persistence','real SSE live updates','download via HTTPS with pinned DNS','cached local media','manual removal'],dataRoot:root},null,2));
  console.log('PASS native end-to-end: real Qt service, browser UI, SSE, isolation, HTTPS download and offline cache');
}finally{await browser?.close();child.kill();await new Promise(resolve=>{if(child.exitCode!==null)resolve();else{child.once('exit',resolve);setTimeout(resolve,3000);}});}
