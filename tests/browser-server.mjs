// Static fixture host for browser tests only. This is not a replacement for the native Qt server.
import http from 'node:http';
import {readFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
const root=fileURLToPath(new URL('..',import.meta.url));
const allow=new Map([['/panel','web/panel.html'],['/panel.js','web/panel.js'],['/panel.css','web/panel.css'],['/overlay.js','web/overlay.js'],['/overlay.css','web/overlay.css'],['/branding.png','assets/branding/jpfachina-sync-original.png']]);
http.createServer(async(req,res)=>{try{
  const url=new URL(req.url,'http://127.0.0.1');let file=allow.get(url.pathname);
  if(url.pathname.startsWith('/overlay/'))file='web/overlay.html';
  if(url.pathname.startsWith('/media/'))file='assets/branding/jpfachina-sync-original.png';
  if(!file){res.writeHead(404);res.end();return;}
  const bytes=await readFile(path.join(root,file));const mime=file.endsWith('.js')?'text/javascript':file.endsWith('.css')?'text/css':file.endsWith('.png')?'image/png':'text/html';
  res.writeHead(200,{'Content-Type':mime,'Cache-Control':'no-store'});res.end(bytes);
}catch{res.writeHead(500);res.end();}}).listen(28749,'127.0.0.1');
