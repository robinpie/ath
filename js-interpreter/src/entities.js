/**
 * Entity implementations for !~ATH.
 * Uses Promise-based death signaling instead of asyncio.Event.
 */

/**
 * DeathEvent - Promise-based equivalent of asyncio.Event for death notification.
 */
class DeathEvent {
  constructor() {
    this._resolved = false;
    this._promise = new Promise((resolve) => {
      this._resolve = resolve;
    });
  }

  set() {
    if (!this._resolved) {
      this._resolved = true;
      this._resolve();
    }
  }

  isSet() {
    return this._resolved;
  }

  async wait() {
    return this._promise;
  }
}

/**
 * Base class for all entities.
 */
export class Entity {
  constructor(name) {
    this.name = name;
    this._dead = false;
    this._deathEvent = new DeathEvent();
    this._timeoutId = null;
  }

  get isDead() {
    return this._dead;
  }

  get isAlive() {
    return !this._dead;
  }

  die() {
    if (!this._dead) {
      this._dead = true;
      this._deathEvent.set();
      if (this._timeoutId !== null) {
        clearTimeout(this._timeoutId);
        this._timeoutId = null;
      }
    }
  }

  async waitForDeath() {
    await this._deathEvent.wait();
  }

  async start() {
    // Override in subclasses
  }
}

/**
 * The program entity (THIS).
 */
export class ThisEntity extends Entity {
  constructor() {
    super('THIS');
  }

  async start() {
    // THIS doesn't do anything by itself - it dies when explicitly killed
  }
}

/**
 * Timer that dies after a duration.
 */
export class TimerEntity extends Entity {
  constructor(name, durationMs) {
    super(name);
    this.durationMs = durationMs;
    this._startResolve = null;
  }

  async start() {
    return new Promise((resolve) => {
      this._startResolve = resolve;
      this._timeoutId = setTimeout(() => {
        this._timeoutId = null;
        this._startResolve = null;
        this.die();
        resolve();
      }, this.durationMs);
    });
  }

  die() {
    const resolveStart = this._startResolve;
    this._startResolve = null;
    super.die();
    if (resolveStart) {
      resolveStart();
    }
  }
}

/**
 * Branch entity created by bifurcation.
 */
export class BranchEntity extends Entity {
  constructor(name) {
    super(name);
    this._completeEvent = new DeathEvent();
  }

  async start() {
    // Branch entities complete when their code finishes
  }

  complete() {
    this._completeEvent.set();
    this.die();
  }

  async waitForCompletion() {
    await this._completeEvent.wait();
  }
}

/**
 * Entity combining multiple entities with AND/OR/NOT.
 */
export class CompositeEntity extends Entity {
  constructor(name, op, entities) {
    super(name);
    this.op = op;  // 'AND', 'OR', 'NOT'
    this.entities = entities;
  }

  async start() {
    try {
      if (this.op === 'AND') {
        // Wait for all entities to die
        await Promise.all(this.entities.map(e => e.waitForDeath()));
        this.die();
      } else if (this.op === 'OR') {
        // Wait for any entity to die
        await Promise.race(this.entities.map(e => e.waitForDeath()));
        this.die();
      } else if (this.op === 'NOT') {
        // Die immediately (the entity exists)
        // The 'NOT' entity dies when the entity is created
        await Promise.resolve();
        this.die();
      }
    } catch (e) {
      // Cancelled or other error
    }
  }
}
