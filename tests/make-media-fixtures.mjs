import {chromium} from '@playwright/test';
import {mkdir,writeFile} from 'node:fs/promises';
const directory='tests/fixtures';await mkdir(directory,{recursive:true});
await writeFile(`${directory}/sample.gif`,Buffer.from('47494638396101000100800000ff000000ff0021ff0b4e45545343415045322e30030100000021f904000a0000002c000000000100010000020244010021f904000a0000002c00000000010001000002024c01003b','hex'));
const browser=await chromium.launch({channel:'chrome',headless:true});
try{
  const page=await browser.newPage();
  for(const [extension,mime]of [['webm','video/webm;codecs=vp8'],['mp4','video/mp4;codecs=avc1.42001E']]){
    const result=await page.evaluate(async mime=>{
      if(!MediaRecorder.isTypeSupported(mime))return null;
      const canvas=document.createElement('canvas');canvas.width=80;canvas.height=40;const context=canvas.getContext('2d');
      const recorder=new MediaRecorder(canvas.captureStream(20),{mimeType:mime,videoBitsPerSecond:100000});const chunks=[];
      recorder.ondataavailable=event=>chunks.push(event.data);const stopped=new Promise(resolve=>recorder.onstop=resolve);
      context.fillStyle='#00ff00';context.fillRect(0,0,80,40);recorder.start();let tick=0;
      const timer=setInterval(()=>{context.fillStyle=tick++%2?'#00ff00':'#0000ff';context.fillRect(0,0,80,40);},50);
      await new Promise(resolve=>setTimeout(resolve,1800));clearInterval(timer);recorder.stop();await stopped;
      return Array.from(new Uint8Array(await new Blob(chunks,{type:mime}).arrayBuffer()));
    },mime);
    if(result){await writeFile(`${directory}/sample.${extension}`,Buffer.from(result));console.log(`Created local ${extension} fixture (${result.length} bytes)`);}
    else console.log(`SKIP unsupported MediaRecorder codec: ${mime}`);
  }
}finally{await browser.close();}
