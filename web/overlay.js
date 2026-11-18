export const effects = {
  none: [{opacity:1},{opacity:1}], fade:[{opacity:0},{opacity:1}],
  left:[{opacity:0,transform:'translateX(-100%)'},{opacity:1,transform:'translateX(0)'}],
  right:[{opacity:0,transform:'translateX(100%)'},{opacity:1,transform:'translateX(0)'}],
  bottom:[{opacity:0,transform:'translateY(100%)'},{opacity:1,transform:'translateY(0)'}],
  scale:[{opacity:0,transform:'scale(.8)'},{opacity:1,transform:'scale(1)'}]
};
export class Compositor {
  constructor(canvas, {preview=false,onError=()=>{},onReady=()=>{}}={}) {
    this.canvas=canvas;this.nodes=new Map();this.preview=preview;this.onError=onError;this.onReady=onReady;
    this.width=1920;this.height=1080;
    this.resize=()=>{const parent=this.canvas.parentElement;
      const scale=Math.min(parent.clientWidth/this.width,parent.clientHeight/this.height);
      this.canvas.style.transform=`translate(-50%,-50%) scale(${scale})`;};
    this.observer=new ResizeObserver(this.resize);this.observer.observe(canvas.parentElement);
  }
  animate(entry,exit=false) {
    entry.animation?.cancel();
    const layout=entry.item.layout||{};const mode=layout.transition||'none';
    const frames=effects[mode]||effects.none;const duration=mode==='none'?0:Math.min(3000,Math.max(0,Number(layout.duration)||0));
    entry.animation=entry.media.animate(exit?[...frames].reverse():frames,{duration,easing:'ease-out',fill:'both'});
    return entry.animation.finished.catch(()=>{});
  }
  apply(state,selectedId='') {
    this.width=Number(state.width)||1920;this.height=Number(state.height)||1080;
    this.canvas.style.width=this.width+'px';this.canvas.style.height=this.height+'px';this.resize();
    const items=Array.isArray(state.items)?state.items:[];const keep=new Set(items.map(x=>x.id));
    for(const [id,entry] of this.nodes)if(!keep.has(id)&&!entry.removing){
      entry.removing=true;
      this.animate(entry,true).then(()=>{if(entry.removing&&this.nodes.get(id)===entry){entry.media.pause?.();entry.wrapper.remove();this.nodes.delete(id);}});
    }
    for(const item of items){
      if(!item.id||typeof item.url!=='string')continue;
      // Media must stay same origin and inside the library route, including in preview messages.
      const url=new URL(item.url,location.href);if(url.origin!==location.origin||!url.pathname.startsWith('/media/'))continue;
      let entry=this.nodes.get(item.id);
      if(entry&&entry.item.mime==='image/png'&&item.mime==='image/png'&&entry.item.url!==item.url&&entry.item.generation===item.generation){
        entry.media.src=item.url;entry.item=item;
      }
      if(entry&&(entry.item.url!==item.url||entry.item.generation!==item.generation||entry.item.mime!==item.mime)){
        entry.animation?.cancel();entry.media.pause?.();entry.wrapper.remove();this.nodes.delete(item.id);entry=null;
      }
      let fresh=false;
      if(!entry){
        fresh=true;const wrapper=document.createElement('div');wrapper.className='layer';wrapper.dataset.id=item.id;
        const video=item.mime?.startsWith('video/');const media=document.createElement(video?'video':'img');
        if(video){media.muted=true;media.autoplay=true;media.playsInline=true;media.preload='auto';}
        else media.alt=item.name||'';
        media.addEventListener('error',()=>this.onError(item,'O navegador não conseguiu decodificar esta mídia. Verifique o arquivo e o codec.'));
        media.addEventListener(video?'loadeddata':'load',()=>this.onReady(item,media),{once:true});
        media.src=item.url;wrapper.append(media);this.canvas.append(wrapper);
        entry={wrapper,media,item,removing:false};this.nodes.set(item.id,entry);
      }
      const returning=entry.removing;entry.removing=false;entry.item=item;
      const l=item.layout||{};
      Object.assign(entry.wrapper.style,{left:`${Number(l.x)||0}px`,top:`${Number(l.y)||0}px`,width:`${Math.max(1,Number(l.width)||1)}px`,height:`${Math.max(1,Number(l.height)||1)}px`,zIndex:String(Math.round(Number(l.z)||0))});
      entry.wrapper.classList.toggle('selected',this.preview&&item.id===selectedId);
      if(entry.media.tagName==='VIDEO'){entry.media.loop=l.loop!==false;if(fresh)entry.media.play().catch(()=>this.onError(item,'Não foi possível iniciar o vídeo. Verifique o codec e a reprodução automática.'));}
      if(fresh||returning)this.animate(entry);
    }
  }
  destroy(){this.observer.disconnect();for(const e of this.nodes.values()){e.animation?.cancel();e.media.pause?.();e.wrapper.remove();}this.nodes.clear();}
}

const canvas=document.querySelector('#canvas');
if(canvas){
  const key=location.pathname.split('/').pop();const preview=new URLSearchParams(location.search).get('preview')==='1';
  if(preview)document.body.dataset.preview='true';
  const renderer=new Compositor(canvas,{preview,
    onError:(item,error)=>{if(preview)parent.postMessage({type:'media-error',id:item.id,error},location.origin);else console.error('[JP Fachina Graphics]',item.id,error);},
    onReady:(item,media)=>{if(!preview)return;try{
      const w=media.videoWidth||media.naturalWidth,h=media.videoHeight||media.naturalHeight;if(!w||!h)return;
      const thumb=document.createElement('canvas');const ratio=Math.min(320/w,180/h,1);thumb.width=Math.max(1,Math.round(w*ratio));thumb.height=Math.max(1,Math.round(h*ratio));
      thumb.getContext('2d').drawImage(media,0,0,thumb.width,thumb.height);
      parent.postMessage({type:'thumbnail',id:item.id,png:thumb.toDataURL('image/png'),width:w,height:h},location.origin);
    }catch(error){console.warn('Miniatura indisponível',error);}}
  });
  if(preview){
    addEventListener('message',event=>{if(event.origin===location.origin&&event.source===parent&&event.data?.type==='preview')renderer.apply(event.data.state,event.data.selectedId);});
    parent.postMessage({type:'preview-ready'},location.origin);
  }else{
    // EventSource reconnects automatically; snapshots make reconnect idempotent.
    const source=new EventSource(`/events/${encodeURIComponent(key)}`);
    source.onmessage=e=>{try{renderer.apply(JSON.parse(e.data));}catch(error){console.error('Estado de overlay inválido',error);}};
    source.onerror=()=>console.warn('[JP Fachina Graphics] Reconectando ao servidor local; último estado preservado.');
    addEventListener('pagehide',()=>{source.close();renderer.destroy();},{once:true});
  }
}
