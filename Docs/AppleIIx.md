# Apple IIx

What the IIgs "could have been"

Goals:
* virtual memory / especially memory protection
* Microkernel OS, with preemptive multitasking, that supports Unix-like (GNO 3) as well as IIgs Desktop Apps
* higher resolution video display

I think a gradual approach, where we start with the virtual memory add-on; then implement the microkernel; then start bringing in the ability to run existing GS/OS applications.

GS/OS will, in true microkernel fashion, be a personality layer on top of that.

And Unixy can be an personality layer also - i.e., GNO 3 is at a similar layer as "GS/OS" but doesn't rely on it. There has been plenty of work showing that GS/OS is simple enough to model via paravirtualization (GoldenGate, Host FST, etc.)

Start from fundamentals- establish the base, get the MM, GPU, and multitasking OS layer up, then add bits at a time until we can run a GS/OS app.

We'll develop inside GSSquared as a framework, to ensure we don't stray too far from something that might work on a real GS.

## MMU

To support everything a modern OS needs, requires an MMU. For real hardware, this will require an accelerator that plugs in to the IIgs CPU socket.

We'll also want some framebuffer access for text, and perhaps up to 1024x768 resolution (1MB memory space).

Build as a new MMU construct. Like current GS MMU, allocates an internal Mega II for some level of compatibility. But handles mapping differently.

## GPU

For the display, the 1MHz Apple II bus is too slow for video. If we have any hope of running this in existing IIgs hardware (with accelerator+mmu addons) then a "send a GPU a sequence of commands to do the heavy lifting" approach is required. And that's fine.

That implies modifications to QuickDraw to work with that type of backend.

This will be secondsight with the GPU extension.

## GS/OS Compatibility

Our design goal is GS/OS compatibility - at the application level - not necessarily Apple II hardware compatibility.

Legacy Apple II compatibility maybe could be handled with a mode switcher of some sort. Those apps really take over the hardware. So set up a virtualization environment helped along by the MMU. So a virtual Apple II thinks it has access to bare h/w but doesn't.


# OS Architecture

# Modern Desktop OS Architecture: Beyond the Global Unix Model

## 1. Core Architectural Shift

Traditional Unix-style operating systems were built for multi-user, time-sharing mainframes. On a modern single-user desktop, the primary security threat is no longer **User A vs. User B**, but **User A vs. Untrusted Application X**.

| Dimension | Legacy Unix Paradigm | Modern Desktop Reality |
| :--- | :--- | :--- |
| **Primary Boundary** | User identity (`UID` / `GID`) | Application domain & sandbox |
| **Authority** | Ambient (Process inherits full user rights) | Capability-based (Explicit, ephemeral handles) |
| **Storage Model** | Global hierarchical root (`/`) | Isolated per-app storage spaces |
| **Inter-Process Model** | File-path reads & global IPC | Event bus, Pub/Sub, and RPC services |

---

## 2. Structural Principles

### A. Banishment of the Global Root (`/`)
* **No Shared Tree:** The concept of a single global filesystem root (`/`) accessible to all processes is eliminated.
* **Hermetic App Domains:** Applications exist in encapsulated, isolated storage spaces containing only their binaries, assets, configuration, and scratch space.
* **Complete Uninstalls:** Removing an application simply deletes its isolated domain—leaving zero orphan files in shared system folders.

### B. Intent-Driven Data Access
* Applications hold zero ambient access to user documents.
* Access to external data is granted exclusively through **User Intent Actions** (e.g., File Pickers, Drag & Drop, Copy/Paste).
* The OS acts as a broker, handing the application an ephemeral, byte-range-limited capability token rather than exposing file system paths.

### C. System-Level Event Bus (Pub/Sub & RPC)
Rather than inspecting a shared disk or navigating local file paths, applications interact with the system and each other through an explicit event-driven communication fabric:

* **Ephemeral Pub/Sub:** Handles user-driven events (Clipboard transfers, Drag & Drop payloads).
* **RPC Services:** Applications expose specific capabilities (e.g., `image.transform.v1`) to the OS broker.
* **Event Channels:** Hardware changes, system status updates, and notifications are broadcast across restricted, capability-gated topics.

---

## 3. The Unix Sub-Universe as a Personality

The power of Unix—text streams, `stdin`/`stdout`, and pipeline composition (`|`)—remains an invaluable paradigm for software development, but it no longer serves as the host-level operating system architecture.

* **Bounded Environments:** Unix exists as a self-contained "personality" or workspace container (a developer environment).
* **Controlled Access:** Inside its boundary, POSIX tools, pipes, and standard permissions function freely.
* **Isolation from Host:** The Unix container cannot reach out to inspect isolated desktop application spaces or host configuration.
