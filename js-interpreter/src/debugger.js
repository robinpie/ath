/**
 * Stepping debugger for the JS interpreter.
 */

import * as readline from 'node:readline/promises';
import { stdin as input, stdout as output } from 'node:process';

export const DebuggerState = {
  RUNNING: 'RUNNING',
  STEPPING: 'STEPPING',
  PAUSED: 'PAUSED',
  QUIT: 'QUIT',
};

const STEP_TYPES = new Set([
  'ImportStmt', 'BifurcateStmt', 'AthLoop', 'DieStmt',
  'VarDecl', 'ConstDecl', 'Assignment', 'RiteDef', 'Conditional',
  'AttemptSalvage', 'CondemnStmt', 'BequeathStmt', 'ExprStmt'
]);

class AsyncInputHandler {
  constructor() {
    this.rl = readline.createInterface({ input, output });
  }

  async getInput(prompt) {
    return await this.rl.question(prompt);
  }

  close() {
    this.rl.close();
  }
}

export class Debugger {
  constructor(sourceCode) {
    this.state = DebuggerState.STEPPING;
    this.sourceLines = sourceCode.split('\n');
    this.inputHandler = new AsyncInputHandler();
    this.lastCommand = 'step';
  }

  async stepHook(node, scope, interpreter, branchContext) {
    // Only pause on statements
    if (!STEP_TYPES.has(node.type)) {
      return true;
    }

    if (this.state === DebuggerState.RUNNING) {
      return true;
    }

    if (this.state === DebuggerState.STEPPING) {
      this.state = DebuggerState.PAUSED;
      
      const stepInfo = this._createStepInfo(node, branchContext);
      this._displayStep(stepInfo, scope, interpreter);

      while (this.state === DebuggerState.PAUSED) {
        try {
          let cmdInput = await this.inputHandler.getInput('(step) ');
          cmdInput = cmdInput.trim();
          
          let cmd;
          if (!cmdInput) {
            cmd = this.lastCommand;
          } else {
            cmd = cmdInput.toLowerCase();
            // Only update lastCommand for execution actions
            if (['s', 'step', 'c', 'continue'].includes(cmd)) {
              this.lastCommand = cmd;
            }
          }

          await this.processCommand(cmd, scope, interpreter);
        } catch (e) {
            // Handle EOF or close
            this.state = DebuggerState.QUIT;
            return false;
        }
      }
    }

    return this.state !== DebuggerState.QUIT;
  }

  close() {
    this.inputHandler.close();
  }

  _createStepInfo(node, branchContext) {
    const line = node.line;
    const column = node.column;
    
    let sourceLine = '';
    if (line > 0 && line <= this.sourceLines.length) {
      sourceLine = this.sourceLines[line - 1];
    }

    return {
      line,
      column,
      nodeType: node.type,
      description: this._describeNode(node),
      branch: branchContext || 'MAIN',
      sourceLine
    };
  }

  _describeNode(node) {
    switch (node.type) {
      case 'ImportStmt':
        return `Importing ${node.entityType} entity '${node.name}'`;
      case 'BifurcateStmt':
        return `Bifurcating '${node.entity}' into '${node.branch1}' and '${node.branch2}'`;
      case 'AthLoop':
        return `~ATH loop waiting on entity`;
      case 'DieStmt':
        return `Invoking .DIE()`;
      case 'VarDecl':
        return `Declaring variable '${node.name}'`;
      case 'ConstDecl':
        return `Declaring constant '${node.name}'`;
      case 'Assignment':
        return `Assignment`;
      case 'RiteDef':
        return `Defining rite '${node.name}'`;
      case 'Conditional':
        return `Conditional check (SHOULD)`;
      case 'AttemptSalvage':
        return `Entering ATTEMPT block`;
      case 'CondemnStmt':
        return `Throwing error (CONDEMN)`;
      case 'BequeathStmt':
        return `Returning value (BEQUEATH)`;
      case 'ExprStmt':
        return `Expression statement`;
      default:
        return node.type;
    }
  }

  _displayStep(info, scope, interpreter) {
    console.log('='.repeat(80));
    console.log(`Step | Branch: ${info.branch} | Line ${info.line}, Col ${info.column}`);
    console.log('-'.repeat(80));
    
    if (info.sourceLine) {
      console.log('SOURCE:');
      console.log(`   ${info.line} | ${info.sourceLine}`);
      const indent = info.sourceLine.length - info.sourceLine.trimStart().length;
      const markerIndent = Math.max(0, info.column - 1);
      console.log(' '.repeat(6 + markerIndent) + '^^^^^');
    }
    
    console.log(`\nSTATEMENT: ${info.nodeType}`);
    console.log(`  ${info.description}`);
    
    console.log('\nSCOPE VARIABLES:');
    let count = 0;
    
    const vars = scope.variables instanceof Map ? Object.fromEntries(scope.variables) : scope.variables;
    
    const entries = Object.entries(vars);
    if (entries.length === 0) {
      console.log('  (empty)');
    } else {
      for (const [name, value] of entries) {
        let valStr = String(value);
        if (typeof value === 'string') {
          valStr = `"${value}"`;
        }
        // Basic type display
        let typeName = typeof value;
        if (value && value.constructor) {
            typeName = value.constructor.name;
        }
        
        console.log(`  ${name} = ${valStr} (${typeName})`);
        count++;
        if (count >= 5) {
          console.log('  ... (use \'v\' to see all)');
          break;
        }
      }
    }

    console.log(`\nPENDING TASKS: ${interpreter._pendingPromises.length}`);
    console.log('='.repeat(80));
    console.log('Commands: [Enter]=step  [c]=continue  [v]=variables  [e]=entities  [q]=quit');
  }

  async processCommand(cmd, scope, interpreter) {
    if (['s', 'step'].includes(cmd)) {
      this.state = DebuggerState.STEPPING;
    } else if (['c', 'continue'].includes(cmd)) {
      this.state = DebuggerState.RUNNING;
    } else if (['q', 'quit'].includes(cmd)) {
      this.state = DebuggerState.QUIT;
    } else if (['v', 'vars', 'variables'].includes(cmd)) {
      console.log('\n--- ALL VARIABLES ---');
      let current = scope;
      let depth = 0;
      while (current) {
        console.log(`Scope Level ${depth}:`);
        const vars = current.variables instanceof Map ? Object.fromEntries(current.variables) : current.variables;
        const entries = Object.entries(vars);
        if (entries.length === 0) {
            console.log('  (empty)');
        }
        for (const [name, value] of entries) {
            let valStr = String(value);
            if (typeof value === 'string') valStr = `"${value}"`;
            console.log(`  ${name} = ${valStr}`);
        }
        current = current.parent;
        depth++;
      }
      console.log('---------------------');
      this.state = DebuggerState.PAUSED;
    } else if (['e', 'entities'].includes(cmd)) {
      console.log('\n--- ENTITIES ---');
      for (const [name, entity] of interpreter.entities) {
        const status = entity.isAlive ? 'ALIVE' : 'DEAD';
        const kind = entity.constructor.name;
        console.log(`  ${name.padEnd(15)} : ${status.padEnd(5)} (${kind})`);
      }
      console.log('----------------');
      this.state = DebuggerState.PAUSED;
    } else if (['t', 'tasks'].includes(cmd)) {
        console.log('\n--- PENDING TASKS ---');
        interpreter._pendingPromises.forEach((p, i) => {
            console.log(`  Task ${i}: ${p}`);
        });
        console.log('---------------------');
        this.state = DebuggerState.PAUSED;
    } else if (['h', 'help', '?'].includes(cmd)) {
      console.log('\n--- DEBUGGER HELP ---');
      console.log('  (Enter) / s / step   : Execute next statement');
      console.log('  c / continue         : Resume execution until next breakpoint or end');
      console.log('  v / variables        : Show all variables in current scope chain');
      console.log('  e / entities         : Show all entities and their status');
      console.log('  t / tasks            : Show pending async tasks');
      console.log('  q / quit             : Stop execution');
      console.log('---------------------');
      this.state = DebuggerState.PAUSED;
    } else {
      console.log(`Unknown command: ${cmd}`);
      this.state = DebuggerState.PAUSED;
    }
  }
}