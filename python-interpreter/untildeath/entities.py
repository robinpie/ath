"""Entity implementations for !~ATH."""

import asyncio
import os
from abc import ABC, abstractmethod
from typing import Optional


class Entity(ABC):
    """Base class for all entities."""

    def __init__(self, name: str):
        self.name = name
        self._dead = False
        self._death_event = asyncio.Event()
        self._task: Optional[asyncio.Task] = None

    @property
    def is_dead(self) -> bool:
        return self._dead

    @property
    def is_alive(self) -> bool:
        return not self._dead

    def die(self):
        """Mark this entity as dead and notify waiters."""
        if not self._dead:
            self._dead = True
            self._death_event.set()
            if self._task and not self._task.done():
                self._task.cancel()

    async def wait_for_death(self):
        """Wait until this entity dies."""
        await self._death_event.wait()

    @abstractmethod
    async def start(self):
        """Start the entity's lifecycle."""
        pass


class ThisEntity(Entity):
    """The program entity (THIS)."""

    def __init__(self):
        super().__init__('THIS')

    async def start(self):
        # THIS doesn't do anything by itself - it dies when explicitly killed
        pass


class TimerEntity(Entity):
    """Timer that dies after a duration."""

    def __init__(self, name: str, duration_ms: int):
        super().__init__(name)
        self.duration_ms = duration_ms

    async def start(self):
        """Start the timer countdown."""
        try:
            await asyncio.sleep(self.duration_ms / 1000.0)
            self.die()
        except asyncio.CancelledError:
            pass


class ProcessEntity(Entity):
    """Process that dies when the subprocess exits."""

    def __init__(self, name: str, command: str, args: list):
        super().__init__(name)
        self.command = command
        self.args = args
        self._process: Optional[asyncio.subprocess.Process] = None

    async def start(self):
        """Start the subprocess and wait for it to exit."""
        try:
            self._process = await asyncio.create_subprocess_exec(
                self.command, *self.args,
                stdout=asyncio.subprocess.DEVNULL,
                stderr=asyncio.subprocess.DEVNULL
            )
            await self._process.wait()
            self.die()
        except (OSError, asyncio.CancelledError) as e:
            self.die()


class ConnectionEntity(Entity):
    """TCP connection that dies when closed."""

    def __init__(self, name: str, host: str, port: int):
        super().__init__(name)
        self.host = host
        self.port = port
        self._reader = None
        self._writer = None

    async def start(self):
        """Open connection and wait for it to close."""
        try:
            self._reader, self._writer = await asyncio.open_connection(
                self.host, self.port
            )
            # Wait until the connection is closed
            while True:
                data = await self._reader.read(1024)
                if not data:
                    break
            self.die()
        except (OSError, asyncio.CancelledError, ConnectionRefusedError):
            self.die()
        finally:
            if self._writer:
                self._writer.close()
                try:
                    await self._writer.wait_closed()
                except Exception:
                    pass


class WatcherEntity(Entity):
    """File watcher that dies when the file is deleted."""

    def __init__(self, name: str, filepath: str):
        super().__init__(name)
        self.filepath = filepath
        self._poll_interval = 0.1  # 100ms polling
        self.exports: dict = {}       # populated by interpreter after module execution
        self.is_module: bool = False  # True if filepath ends with .~ath

    async def start(self):
        """Watch the file and die when it's deleted."""
        try:
            # Check if file exists initially
            if not os.path.exists(self.filepath):
                # File doesn't exist, die immediately (via event loop)
                await asyncio.sleep(0)
                self.die()
                return

            # Poll for file deletion
            while os.path.exists(self.filepath):
                await asyncio.sleep(self._poll_interval)

            self.die()
        except asyncio.CancelledError:
            pass


class BranchEntity(Entity):
    """Branch entity created by bifurcation."""

    def __init__(self, name: str):
        super().__init__(name)
        self._complete = asyncio.Event()

    async def start(self):
        # Branch entities complete when their code finishes
        pass

    def complete(self):
        """Mark the branch as complete (its code has finished)."""
        self._complete.set()
        self.die()

    async def wait_for_completion(self):
        """Wait for the branch to complete."""
        await self._complete.wait()


class CompositeEntity(Entity):
    """Entity combining multiple entities with AND/OR/NOT."""

    def __init__(self, name: str, op: str, entities: list):
        super().__init__(name)
        self.op = op  # 'AND', 'OR', 'NOT'
        self.entities = entities

    async def start(self):
        """Wait based on the composition logic."""
        try:
            if self.op == 'AND':
                # Wait for all entities to die
                await asyncio.gather(
                    *[e.wait_for_death() for e in self.entities]
                )
                self.die()
            elif self.op == 'OR':
                # Wait for any entity to die
                done, pending = await asyncio.wait(
                    [asyncio.create_task(e.wait_for_death()) for e in self.entities],
                    return_when=asyncio.FIRST_COMPLETED
                )
                for task in pending:
                    task.cancel()
                self.die()
            elif self.op == 'NOT':
                # Die immediately (the entity exists)
                # The 'NOT' entity dies when the entity is created
                await asyncio.sleep(0)
                self.die()
        except asyncio.CancelledError:
            pass
