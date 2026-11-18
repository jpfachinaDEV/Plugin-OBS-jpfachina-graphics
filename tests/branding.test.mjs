import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
test('original embedded SYNC PNG is byte-identical and retains dimensions',()=>{
  const png=readFileSync(new URL('../assets/branding/jpfachina-sync-original.png',import.meta.url));
  assert.equal(createHash('sha256').update(png).digest('hex'),'49e0bd162507b25133d594a42729b2d711fcbc8fe34d475d163f17e6b8c309ff');
  assert.equal(png.subarray(0,8).toString('hex'),'89504e470d0a1a0a');
  assert.equal(png.readUInt32BE(16),2048);assert.equal(png.readUInt32BE(20),809);
});
