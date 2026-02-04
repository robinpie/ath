#!/usr/bin/env node

import fs from 'fs';
import { TildeAth } from './src/index.js';
import { Debugger } from './src/debugger.js';

const args = process.argv.slice(2);
const stepIndex = args.findIndex(arg => arg === '--step' || arg === '-d' || arg === '--debug');
let debugMode = false;
let filename = null;

if (stepIndex !== -1) {
  debugMode = true;
  args.splice(stepIndex, 1);
}

if (args.length > 0) {
  filename = args[0];
}

async function main() {
  if (!filename) {
    console.log('Usage: node ath.js [--step] <file>');
    // TODO: Implement REPL
    return;
  }

  if (!fs.existsSync(filename)) {
    console.error(`File not found: ${filename}`);
    process.exit(1);
  }

  const source = fs.readFileSync(filename, 'utf-8');
  
  let dbg = null;
  if (debugMode) {
    dbg = new Debugger(source);
    console.log(`Debugger enabled for ${filename}`);
  }

  const runtime = new TildeAth({
    debugger: dbg,
    onOutput: (text) => console.log(text),
  });

  try {
    await runtime.run(source);
  } catch (e) {
    if (e.message !== 'DEBUGGER_QUIT') {
        console.error(e);
        process.exit(1);
    } else {
        console.log('Debugger quit.');
    }
  }
}

main();
