const $=id=>document.getElementById(id);
let token=location.hash.slice(1)||sessionStorage.getItem('jp-session')||'';
if(token){sessionStorage.setItem('jp-session',token);history.replaceState(null,'',location.pathname);}
let state={projects:[],active:''},selectedId='',draft=null,dirty=false,showLive=false,frameKey='',frameReady=false;
let toastTimer,refreshTimer,loading=false,reloadQueued=false,connected=false,working=false;
const thumbPending=new Set();
const project=()=>state.projects.find(p=>p.id===state.active);
const selected=()=>project()?.media.find(m=>m.id===selectedId);
const mediaUrl=m=>`/media/${project().key}/${m.id}`;
const layoutFields=['x','y','width','height','z','duration'];
function notify(message,error=false){$('toast').textContent=message;$('toast').classList.toggle('error',error);$('toast').hidden=false;clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('toast').hidden=true,error?10000:4500);}
function connection(on){connected=on;$('connection').textContent=on?'● Serviço local conectado':'● Sem conexão — reconectando';$('connection').classList.toggle('online',on);document.body.dataset.connected=String(on);}
async function request(path,options={}){
  const response=await fetch(path,{...options,headers:{Authorization:`Bearer ${token}`,'Content-Type':'application/json',...options.headers}});
  let data;try{data=await response.json();}catch{throw Error('Resposta inválida do serviço local.');}
  if(!response.ok)throw Error(data.error||`Falha no serviço local (${response.status}).`);return data;
}
async function command(action,data={},refresh=true){
  if(!connected)throw Error('Serviço local desconectado. Aguarde a reconexão antes de operar.');
  const result=await request('/api/command',{method:'POST',body:JSON.stringify({action,projectId:state.active,...data})});
  if(refresh)await load();return result;
}
async function run(fn){if(working)return;working=true;try{await fn();}catch(e){notify(e.message,true);}finally{working=false;}}
function scheduleLoad(){clearTimeout(refreshTimer);refreshTimer=setTimeout(()=>load().catch(e=>notify(e.message,true)),60);}
async function load(){
  if(loading){reloadQueued=true;return;}loading=true;
  try{
    const next=await request('/api/state');
    const changedProject=next.active!==state.active;
    state=next;connection(true);
    if(changedProject){selectedId='';draft=null;dirty=false;showLive=false;}
    if(selectedId&&!selected()){selectedId='';draft=null;dirty=false;}
    if(selected()&&!dirty)draft=structuredClone(selected().draft);
    render();
  }catch(e){connection(false);throw e;}finally{loading=false;if(reloadQueued){reloadQueued=false;scheduleLoad();}}
}
function options(select,values,current){select.replaceChildren();for(const [value,label]of values){const option=document.createElement('option');option.value=value;option.textContent=label;select.append(option);}select.value=current??'';}
function render(){
  const p=project();options($('projects'),state.projects.map(x=>[x.id,x.name]),state.active);
  for(const id of ['rename-project','delete-project','source','copy-url','import','download','categories','folder','clear'])$(id).disabled=!p;
  const filter=$('filter').value;
  options($('filter'),[['','Todas as categorias'],...(p?.categories||[]).map(x=>[x,x])],filter);
  if($('filter').selectedIndex<0)$('filter').value='';
  const live=(p?.media||[]).filter(m=>m.live);
  $('live-count').textContent=`${live.length} arte${live.length===1?'':'s'} no ar`;
  $('live-names').textContent=live.map(x=>x.name).join(' · ')||'Aguardando operação';
  $('media-count').textContent=`${p?.media.length||0} MÍDIAS`;
  renderCards();renderEditor();
  if(p&&frameKey!==p.key){frameKey=p.key;frameReady=false;$('preview').src=`/overlay/${p.key}?preview=1`;}
  if(!p){frameKey='';frameReady=false;$('preview').removeAttribute('src');}
  updatePreview();
}
function renderCards(){
  const p=project();const filter=$('filter').value,query=$('search').value.toLocaleLowerCase('pt-BR');
  const list=(p?.media||[]).filter(m=>(!filter||m.category===filter)&&m.name.toLocaleLowerCase('pt-BR').includes(query));
  const cards=$('cards');cards.replaceChildren();$('library-empty').hidden=!!list.length;
  if(!list.length&&p?.media.length){$('library-empty').querySelector('strong').textContent='Nenhuma arte encontrada';}
  else $('library-empty').querySelector('strong').textContent='Uma biblioteca pronta para a sua live';
  for(const m of list){
    const card=document.createElement('article');card.className='media-card';card.dataset.id=m.id;card.classList.toggle('selected',m.id===selectedId);
    const thumb=document.createElement('div');thumb.className='thumb';thumb.tabIndex=0;thumb.setAttribute('role','button');thumb.setAttribute('aria-label',`Pré-visualizar ${m.name}`);
    if(m.thumbnail||m.mime.startsWith('image/')){const img=document.createElement('img');img.loading='lazy';img.alt=m.name;img.src=m.thumbnail?`/thumb/${p.key}/${m.id}`:mediaUrl(m);thumb.append(img);}
    else{const icon=document.createElement('span');icon.className='video-icon';icon.textContent='▶';thumb.append(icon);}
    const badge=document.createElement('span');badge.className='badge'+(m.live?' on-air':'');badge.textContent=m.live?'● NO AR':m.mime.split('/')[1].toUpperCase();thumb.append(badge);
    const copy=document.createElement('div');copy.className='card-copy';const title=document.createElement('strong');title.textContent=m.name;title.title=m.name;
    const meta=document.createElement('small');meta.textContent=`${m.category} · ${(m.bytes/1024/1024).toFixed(1)} MB`;copy.append(title,meta);
    const actions=document.createElement('div');actions.className='card-actions';
    const actionButton=(text,className,fn)=>{const b=document.createElement('button');b.textContent=text;b.className=className;b.addEventListener('click',()=>run(fn));actions.append(b);return b;};
    actionButton('PRÉ-VISUALIZAR','preview-action',()=>choose(m.id));
    actionButton('COLOCAR NO AR','on',async()=>{if(m.id===selectedId)await saveDraft(false);await command('media.on',{mediaId:m.id});notify(`“${m.name}” no ar.`);});
    const off=actionButton('RETIRAR DO AR','',async()=>{await command('media.off',{mediaId:m.id});notify(`“${m.name}” retirada do ar.`);});off.disabled=!m.live;
    thumb.addEventListener('click',()=>run(()=>choose(m.id)));thumb.addEventListener('keydown',event=>{if(event.key==='Enter'||event.key===' '){event.preventDefault();run(()=>choose(m.id));}});
    card.append(thumb,copy,actions);cards.append(card);
  }
}
function renderEditor(){
  const m=selected();$('editor-empty').hidden=!!m;$('editor-form').hidden=!m;
  $('selected-status').textContent=m?.live?'● NO AR':'EM ESPERA';$('selected-status').classList.toggle('on-air',!!m?.live);
  if(!m)return;
  options($('media-category'),project().categories.map(x=>[x,x]),dirty?$('media-category').value:m.category);
  if(!dirty){$('media-name').value=m.name;fillGeometry();$('transition').value=draft.transition;$('loop').checked=draft.loop!==false;}
  $('media-details').textContent=`${m.mime} · ${(m.bytes/1024/1024).toFixed(2)} MB · ${m.live?'No ar; alterações aguardam aplicação':'Fora do ar'}${m.mime==='video/mp4'?' · MP4 comum não tem transparência':''}`;
  $('out').disabled=!m.live;updateDirtyLabel();
}
function fillGeometry(){if(draft)for(const field of layoutFields)$(field).value=Math.round(draft[field]);}
function updateDirtyLabel(){$('unsaved').textContent=dirty?'Preparação alterada. Salve ou coloque no ar para aplicar.':'Preparação salva. Só “Colocar no ar” envia os ajustes à transmissão.';}
async function ask({title,description='',value='',multiline=false,note='',label='Nome',ok='Salvar'}){
  const dialog=$('text-dialog');$('dialog-title').textContent=title;$('dialog-description').textContent=description;
  $('dialog-label').textContent=label;$('dialog-note').textContent=note;$('dialog-ok').textContent=ok;
  $('dialog-input').hidden=multiline;$('dialog-input').required=!multiline;$('dialog-textarea').hidden=!multiline;
  const input=multiline?$('dialog-textarea'):$('dialog-input');input.value=value;
  dialog.returnValue='';dialog.showModal();input.focus();input.select();
  return new Promise(resolve=>dialog.addEventListener('close',()=>resolve(dialog.returnValue==='ok'?input.value.trim():null),{once:true}));
}
async function confirm(title,description){const d=$('confirm-dialog');$('confirm-title').textContent=title;$('confirm-description').textContent=description;d.returnValue='';d.showModal();return new Promise(resolve=>d.addEventListener('close',()=>resolve(d.returnValue==='ok'),{once:true}));}
async function leaveDraft(){return !dirty||await confirm('Descartar preparação não salva?','Os ajustes ainda não salvos serão descartados. A composição no ar continua como está.');}
async function choose(id){
  if(id!==selectedId&&!(await leaveDraft()))return;
  selectedId=id;draft=structuredClone(selected().draft);dirty=false;showLive=false;renderCards();renderEditor();updatePreview();
}
function previewState(){
  const p=project();if(!p)return {width:1920,height:1080,items:[]};
  const items=showLive?p.media.filter(m=>m.live).map(m=>({...m,layout:m.live,url:mediaUrl(m)})):selected()?[{...selected(),layout:{...draft,transition:'none'},url:mediaUrl(selected())}]:[];
  return {width:p.width,height:p.height,items};
}
function updatePreview(){
  const items=previewState();if(frameReady)$('preview').contentWindow.postMessage({type:'preview',state:items,selectedId:showLive?'':selectedId},location.origin);
  $('preview-empty').hidden=items.items.length>0;
  $('show-live').textContent=showLive?'Voltar à preparação':'Ver composição no ar';
  const box=$('selection-box');box.style.display=selected()&&!showLive?'block':'none';
  if(draft)Object.assign(box.style,{left:`${draft.x/19.2}%`,top:`${draft.y/10.8}%`,width:`${draft.width/19.2}%`,height:`${draft.height/10.8}%`});
}
function readEditor(){
  if(!draft)return;for(const field of layoutFields){const v=Number($(field).value);if(Number.isFinite(v))draft[field]=v;}
  draft.transition=$('transition').value;draft.loop=$('loop').checked;dirty=true;showLive=false;updateDirtyLabel();updatePreview();
}
async function saveDraft(message=true){
  if(!selected())return;if(!$('editor-form').reportValidity())throw Error('Revise os valores da preparação.');
  if(dirty){
    await command('media.update',{mediaId:selectedId,patch:{name:$('media-name').value,category:$('media-category').value,draft}},false);
    dirty=false;await load();
  }
  if(message)notify('Preparação salva. A composição no ar não foi alterada.');
}
addEventListener('message',event=>{
  if(event.origin!==location.origin||event.source!==$('preview').contentWindow)return;
  if(event.data?.type==='preview-ready'){frameReady=true;updatePreview();}
  if(event.data?.type==='media-error')notify(event.data.error,true);
  if(event.data?.type==='thumbnail'){
    const p=project(),m=p?.media.find(x=>x.id===event.data.id);if(!m||m.thumbnail||thumbPending.has(m.id))return;
    thumbPending.add(m.id);
    command('media.thumbnail',{projectId:p.id,mediaId:m.id,png:event.data.png},false).then(()=>scheduleLoad()).catch(e=>notify(e.message,true)).finally(()=>thumbPending.delete(m.id));
  }
});
let pointer=null;
$('selection-box').addEventListener('pointerdown',event=>{
  if(!draft)return;event.preventDefault();const surface=$('drag-surface').getBoundingClientRect();
  pointer={id:event.pointerId,x:event.clientX,y:event.clientY,start:{...draft},sx:1920/surface.width,sy:1080/surface.height,resize:event.target===$('resize-handle')};
  $('selection-box').setPointerCapture(event.pointerId);
});
$('selection-box').addEventListener('pointermove',event=>{
  if(!pointer||pointer.id!==event.pointerId)return;const dx=(event.clientX-pointer.x)*pointer.sx,dy=(event.clientY-pointer.y)*pointer.sy;
  if(pointer.resize){const ratio=pointer.start.width/pointer.start.height;draft.width=Math.round(Math.min(10000,Math.max(1,pointer.start.width+dx)));draft.height=Math.round(Math.min(10000,Math.max(1,event.shiftKey?draft.width/ratio:pointer.start.height+dy)));}
  else{draft.x=Math.round(Math.min(10000,Math.max(-10000,pointer.start.x+dx)));draft.y=Math.round(Math.min(10000,Math.max(-10000,pointer.start.y+dy)));}
  dirty=true;fillGeometry();updateDirtyLabel();updatePreview();
});
for(const event of ['pointerup','pointercancel','lostpointercapture'])$('selection-box').addEventListener(event,()=>pointer=null);
$('editor-form').addEventListener('input',readEditor);$('editor-form').addEventListener('change',readEditor);
$('editor-form').addEventListener('submit',event=>{event.preventDefault();run(()=>saveDraft());});
$('take').addEventListener('click',()=>run(async()=>{await saveDraft(false);await command('media.on',{mediaId:selectedId});notify(`“${selected().name}” no ar.`);}));
$('out').addEventListener('click',()=>run(async()=>{await command('media.off',{mediaId:selectedId});notify('Arte retirada do ar.');}));
$('delete-media').addEventListener('click',()=>run(async()=>{if(await confirm('Excluir esta mídia?',`“${selected().name}” será removida da biblioteca e do overlay desta transmissão.`))await command('media.delete',{mediaId:selectedId,confirm:true});}));
document.querySelectorAll('[data-align]').forEach(button=>button.addEventListener('click',()=>{
  if(!draft)return;switch(button.dataset.align){case'left':draft.x=0;break;case'center':draft.x=(1920-draft.width)/2;break;case'right':draft.x=1920-draft.width;break;case'top':draft.y=0;break;case'middle':draft.y=(1080-draft.height)/2;break;case'bottom':draft.y=1080-draft.height;break;}
  dirty=true;showLive=false;fillGeometry();updateDirtyLabel();updatePreview();
}));
$('show-live').addEventListener('click',()=>{showLive=!showLive;updatePreview();});
$('projects').addEventListener('change',()=>run(async()=>{const id=$('projects').value;if(await leaveDraft())await command('project.select',{projectId:id});else $('projects').value=state.active;}));
$('new-project').addEventListener('click',()=>run(async()=>{if(!(await leaveDraft()))return;const name=await ask({title:'Nova transmissão',description:'Uma biblioteca independente, com URL e controles próprios.',value:'Nova transmissão'});if(name)await command('project.create',{name});}));
$('rename-project').addEventListener('click',()=>run(async()=>{const name=await ask({title:'Renomear transmissão',value:project().name});if(name)await command('project.rename',{name});}));
$('delete-project').addEventListener('click',()=>run(async()=>{if(await confirm('Excluir transmissão?',`A biblioteca e o overlay de “${project().name}” serão desativados. Outras transmissões e as cenas do OBS não serão alteradas.`))await command('project.delete',{confirm:true});}));
$('clear').addEventListener('click',()=>run(async()=>{if(await confirm('Limpar transmissão?',`Retirar todas as artes de “${project().name}” do ar? As mídias continuam na biblioteca.`)){await command('live.clear',{confirm:true});notify('Todas as artes desta transmissão foram retiradas do ar.');}}));
$('copy-url').addEventListener('click',()=>run(async()=>{const url=`${state.baseUrl}/overlay/${project().key}`;try{await navigator.clipboard.writeText(url);notify('URL copiada. Use uma fonte de navegador 1920 × 1080.');}catch{await ask({title:'URL do overlay',value:url,label:'Selecione e copie a URL',ok:'Concluir',note:'Fonte de navegador · largura 1920 · altura 1080 · fundo transparente. Esta URL permite ler as mídias desta transmissão.'});}}));
for(const [id,action]of [['source','native.source'],['folder','native.folder'],['about','native.about']])$(id).addEventListener('click',()=>run(()=>command(action,{},false)));
$('import').addEventListener('click',()=>run(()=>command('native.import',{category:$('filter').value||project().categories[0]},false)));
$('download').addEventListener('click',()=>run(async()=>{const url=await ask({title:'Adicionar mídia por URL',description:'O arquivo será baixado uma vez e guardado na biblioteca.',label:'URL direta HTTP(S)',ok:'Baixar',note:'Até 256 MB · somente endereços públicos · sem redirecionamentos.'});if(url){const parsed=new URL(url);if(!['http:','https:'].includes(parsed.protocol))throw Error('Use uma URL HTTP ou HTTPS.');await command('media.download',{url,category:$('filter').value||project().categories[0]},false);notify('Download iniciado. Você receberá o resultado neste painel.');}}));
$('categories').addEventListener('click',()=>run(async()=>{const result=await ask({title:'Categorias da biblioteca',description:'Uma categoria por linha. Para renomear, edite a linha correspondente.',value:project().categories.join('\n'),multiline:true,label:'Categorias',note:'As mídias acompanham a categoria da mesma linha. Ao remover linhas, revise a organização da biblioteca.'});if(result)await command('category.save',{categories:result.split('\n').map(x=>x.trim()).filter(Boolean)});}));
$('search').addEventListener('input',renderCards);$('filter').addEventListener('change',renderCards);
async function watch(){
  for(;;){try{
    const response=await fetch('/control-events',{headers:{Authorization:`Bearer ${token}`}});
    if(!response.ok||!response.body)throw Error('Serviço indisponível.');connection(true);scheduleLoad();
    const reader=response.body.getReader(),decoder=new TextDecoder();let buffer='';
    for(;;){const {value,done}=await reader.read();if(done)break;buffer+=decoder.decode(value,{stream:true});let end;
      while((end=buffer.indexOf('\n\n'))>=0){const block=buffer.slice(0,end);buffer=buffer.slice(end+2);const line=block.split('\n').find(x=>x.startsWith('data: '));if(!line)continue;
        try{const event=JSON.parse(line.slice(6));if(event.changed)scheduleLoad();if(event.message)notify(event.message,event.error);}catch(e){console.warn(e);}}
    }
  }catch(e){console.warn('[JP Fachina Graphics]',e.message);}connection(false);await new Promise(resolve=>setTimeout(resolve,2000));}
}
addEventListener('beforeunload',event=>{if(dirty){event.preventDefault();event.returnValue='';}});
try{await load();watch();if(!localStorage.getItem('jp-welcome')){$('welcome').showModal();$('welcome').addEventListener('close',()=>localStorage.setItem('jp-welcome','1'),{once:true});}}
catch(e){notify(token?e.message:'Abra este painel pelo dock JP Fachina Graphics no OBS para iniciar uma sessão autenticada.',true);watch();}
