import {test,expect} from '@playwright/test';
const base='http://127.0.0.1:28749';
const key='a'.repeat(64);
function fixture(){return {schema:1,active:'p1',baseUrl:base,projects:[{id:'p1',key,name:'Transmissão principal',width:1920,height:1080,categories:['Rodapés','Logos','Avisos','Animações','Outros'],media:[{id:'m1',name:'Logo oficial',mime:'image/png',bytes:80000,category:'Logos',draft:{x:10,y:20,width:600,height:237,z:0,transition:'fade',duration:100,loop:true},live:null,generation:0}]}]};}
async function panel(page){
  let state=fixture();const commands=[];
  await page.route('**/api/state',route=>route.fulfill({json:state}));
  await page.route('**/control-events',route=>route.fulfill({status:200,contentType:'text/event-stream',body:': connected\n\n'}));
  await page.route('**/api/command',async route=>{
    const input=route.request().postDataJSON();commands.push(input);const p=state.projects.find(x=>x.id===input.projectId),m=p?.media.find(x=>x.id===input.mediaId);
    if(input.action==='media.update')Object.assign(m,input.patch);
    if(input.action==='media.on'){m.live=structuredClone(m.draft);m.generation++;}
    if(input.action==='media.off')m.live=null;
    if(input.action==='live.clear')for(const m of p.media)m.live=null;
    if(input.action==='project.select')state.active=input.projectId;
    if(input.action==='project.create'){const id='p'+(state.projects.length+1);state.projects.push({id,key:'b'.repeat(64),name:input.name,width:1920,height:1080,categories:['Outros'],media:[]});state.active=id;}
    if(input.action==='project.delete'){state.projects=state.projects.filter(p=>p.id!==input.projectId);state.active=state.projects[0]?.id||'';}
    if(input.action==='media.thumbnail')m.thumbnail='thumb.png';
    await route.fulfill({json:{ok:true}});
  });
  await page.addInitScript(()=>localStorage.setItem('jp-welcome','1'));
  await page.goto(base+'/panel#test-session');await expect(page.locator('#connection')).toContainText('conectado');
  return {commands,get state(){return state;}};
}
test('panel uses original logo and renders safely at monitor and dock widths',async({page})=>{
  const errors=[];page.on('pageerror',e=>errors.push(e.message));await panel(page);
  const logo=page.locator('.brand img');await expect(logo).toBeVisible();
  expect(await logo.evaluate(x=>[x.naturalWidth,x.naturalHeight])).toEqual([2048,809]);
  await page.screenshot({path:'artifacts/panel-monitor.png',fullPage:true});
  await page.setViewportSize({width:500,height:920});await page.screenshot({path:'artifacts/panel-dock.png',fullPage:true});
  expect(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth)).toBeTruthy();
  expect(errors).toEqual([]);
});
test('preparing, dragging and saving never sends live commands; take/out remain independent',async({page})=>{
  const f=await panel(page);await page.getByRole('button',{name:'PRÉ-VISUALIZAR',exact:true}).click();
  await page.locator('#x').fill('245');await page.locator('#x').dispatchEvent('input');
  expect(f.commands.filter(x=>x.action==='media.on')).toHaveLength(0);
  await page.locator('#save').click();expect(f.state.projects[0].media[0].draft.x).toBe(245);expect(f.state.projects[0].media[0].live).toBeNull();
  await page.locator('#take').click();await expect(page.locator('#selected-status')).toContainText('NO AR');
  expect(f.state.projects[0].media[0].live.x).toBe(245);
  await page.locator('#x').fill('700');await page.locator('#x').dispatchEvent('input');await page.locator('#save').click();
  expect(f.state.projects[0].media[0].live.x).toBe(245);
  await page.locator('#out').click();await expect(page.locator('#live-count')).toHaveText('0 artes no ar');
});
test('drag and resize map preview coordinates to 1920x1080',async({page})=>{
  const f=await panel(page);await page.getByRole('button',{name:'PRÉ-VISUALIZAR',exact:true}).click();
  const box=await page.locator('#selection-box').boundingBox();
  await page.mouse.move(box.x+20,box.y+20);await page.mouse.down();await page.mouse.move(box.x+50,box.y+40);await page.mouse.up();
  expect(Number(await page.locator('#x').inputValue())).toBeGreaterThan(10);
  expect(Number(await page.locator('#y').inputValue())).toBeGreaterThan(20);
  const width=Number(await page.locator('#width').inputValue());const handle=await page.locator('#resize-handle').boundingBox();
  await page.mouse.move(handle.x+5,handle.y+5);await page.mouse.down();await page.mouse.move(handle.x+35,handle.y+25);await page.mouse.up();
  expect(Number(await page.locator('#width').inputValue())).toBeGreaterThan(width);expect(f.commands.some(c=>c.action==='media.on')).toBeFalsy();
});
test('clear requires explicit confirmation and project switching isolates library',async({page})=>{
  const f=await panel(page);await page.locator('#clear').click();await page.locator('#confirm-dialog button[value="cancel"]').click();
  expect(f.commands.some(x=>x.action==='live.clear')).toBeFalsy();
  await page.locator('#clear').click();await page.locator('#confirm-dialog button[value="ok"]').click();
  expect(f.commands.find(x=>x.action==='live.clear')).toMatchObject({projectId:'p1',confirm:true});
  await page.locator('#new-project').click();await page.locator('#dialog-input').fill('Segundo evento');await page.locator('#dialog-ok').click();
  await expect(page.locator('#media-count')).toHaveText('0 MÍDIAS');
  await page.locator('#projects').selectOption('p1');await expect(page.locator('#media-count')).toHaveText('1 MÍDIAS');
});
test('media names are inserted as text, never executed as HTML',async({page})=>{
  await panel(page);await page.getByRole('button',{name:'PRÉ-VISUALIZAR',exact:true}).click();
  await page.locator('#media-name').fill('<img src=x onerror="window.injected=true">');await page.locator('#save').click();
  await expect(page.locator('.card-copy strong')).toHaveText('<img src=x onerror="window.injected=true">');
  expect(await page.evaluate(()=>window.injected)).toBeUndefined();
});
test('compositor remains transparent and preserves unrelated video DOM and playback position',async({page})=>{
  await page.goto(base+'/overlay/'+key+'?preview=1');
  const result=await page.evaluate(async()=>{
    const {Compositor}=await import('/overlay.js');const canvas=document.createElement('div');canvas.id='test-canvas';document.body.append(canvas);
    const c=new Compositor(canvas);const layout={x:0,y:0,width:1920,height:1080,z:0,transition:'none',duration:0};
    const a={id:'video',mime:'video/webm',url:'/media/key/video',generation:1,layout};
    c.apply({items:[a]});const node=c.nodes.get('video').media;let plays=0;node.play=()=>{plays++;return Promise.resolve();};
    const b={id:'logo',mime:'image/png',url:'/media/key/logo',generation:1,layout};
    c.apply({items:[a,b]});c.apply({items:[{...a,layout:{...layout,x:30}},b]});c.apply({items:[a]});
    const same=node===c.nodes.get('video').media;const count=plays;
    c.destroy();return {same,count};
  });expect(result).toEqual({same:true,count:0});
  await page.goto(base+'/overlay/'+key);expect(await page.locator('body').evaluate(x=>getComputedStyle(x).backgroundColor)).toBe('rgba(0, 0, 0, 0)');
});
test('quick off/on cancels pending exit without removing reactivated element',async({page})=>{
  await page.goto(base+'/overlay/'+key+'?preview=1');
  const exists=await page.evaluate(async()=>{
    const {Compositor}=await import('/overlay.js');const canvas=document.createElement('div');document.body.append(canvas);const c=new Compositor(canvas);
    const item={id:'logo',mime:'image/png',url:'/media/key/logo',generation:1,layout:{x:0,y:0,width:100,height:40,z:0,transition:'fade',duration:50}};
    c.apply({items:[item]});c.apply({items:[]});c.apply({items:[item]});await new Promise(r=>setTimeout(r,130));
    const exists=c.nodes.has('logo')&&c.nodes.get('logo').wrapper.isConnected;c.destroy();return exists;
  });expect(exists).toBeTruthy();
});
